#include <lib_multicore.h>

static errval_t load_and_relocate_driver(struct bootinfo *bootinfo,
                                         struct arm_core_data *core_data)
{
    size_t retsize;

    // Load the cpu driver module.
    struct mem_region *cpu_driver_module =
        multiboot_find_module(bootinfo, "cpu_omap44xx");
    if (!cpu_driver_module) {
        DBG(ERR, "Failed to load cpu driver module\n");
        return SPAWN_ERR_FIND_MODULE;
    }

    // Identify our cpu-driver memory frame to obtain the size of our driver.
    struct frame_identity cpu_driver_identity;
    struct capref cpu_driver = {.cnode = cnode_module,
                                .slot = cpu_driver_module->mrmod_slot};
    frame_identify(cpu_driver, &cpu_driver_identity);

    // 7.3.1(III): Allocate memory to load coreboot's relocatable segment.
    struct capref relocatable_segment_frame;
    struct frame_identity reloc_identity;
    void *relocatable_segment_addr;
    CHECK(frame_alloc(&relocatable_segment_frame, cpu_driver_identity.bytes,
                      &retsize));
    CHECK(frame_identify(relocatable_segment_frame, &reloc_identity));
    CHECK(paging_map_frame(get_current_paging_state(),
                           &relocatable_segment_addr, retsize,
                           relocatable_segment_frame, NULL, NULL));

    void *elf_addr;
    CHECK(paging_map_frame(get_current_paging_state(), &elf_addr,
                           cpu_driver_identity.bytes, cpu_driver, NULL, NULL));

    // Relocate the cpu driver.
    CHECK(load_cpu_relocatable_segment(
        elf_addr, relocatable_segment_addr, reloc_identity.base,
        core_data->kernel_load_base, &core_data->got_base));

    return SYS_ERR_OK;
}

static void clean_cache(struct frame_identity frame)
{
    MEMORY_BARRIER;

    // Clean cache.
    sys_armv7_cache_clean_poc(
        (void *) (uint32_t) frame.base,
        (void *) (uint32_t)(frame.base + frame.bytes - 1));
    // Invalidate cache.
    sys_armv7_cache_invalidate(
        (void *) (uint32_t) frame.base,
        (void *) (uint32_t)(frame.base + frame.bytes - 1));

    MEMORY_BARRIER;
}

errval_t create_urpc_frame(void **buf, size_t bytes)
{
    size_t retsize;

    CHECK(frame_alloc(&cap_urpc, bytes, &retsize));
    if (retsize != bytes)
        return LIB_ERR_NO_SIZE_MATCH;

    CHECK(paging_map_frame(get_current_paging_state(), buf, bytes, cap_urpc,
                           NULL, NULL));
    return SYS_ERR_OK;
}

errval_t wake_core(coreid_t core_id, coreid_t current_core_id,
                   struct bootinfo *bootinfo)
{
    size_t retsize;

    // If the core id is 0, this core is already up and does not need to be
    // woken up.
    if (core_id == 0)
        return SYS_ERR_OK;

    // If the core id is our own current core id, we are obviously already
    // awake too.
    if (core_id == current_core_id)
        return SYS_ERR_OK;

    // 7.3.1: Allocate memory.
    // I: Allocate a new KCB.
    struct capref kcb_ram, kcb_frame;
    struct frame_identity kcb_frame_identity;
    CHECK(ram_alloc(&kcb_ram, OBJSIZE_KCB));
    CHECK(slot_alloc(&kcb_frame));
    CHECK(cap_retype(kcb_frame, kcb_ram, 0, ObjType_KernelControlBlock,
                     OBJSIZE_KCB, 1));
    CHECK(frame_identify(kcb_frame, &kcb_frame_identity));

    // II: Allocate coreboot's core_data struct.
    struct arm_core_data *core_data;
    struct capref core_data_frame;
    struct frame_identity core_data_identity;
    CHECK(frame_alloc(&core_data_frame, OBJSIZE_KCB, &retsize));
    CHECK(invoke_kcb_clone(kcb_frame, core_data_frame));
    CHECK(paging_map_frame(get_current_paging_state(), (void *) &core_data,
                           retsize, core_data_frame, NULL, NULL));
    CHECK(frame_identify(core_data_frame, &core_data_identity));

    // III: Allocate memory to load coreboot's relocatable segment.
    // Note: This was moved into load_and_relocate_driver, because
    // we need the cpu-driver size or it.

    // IV: Allocate space to load the init process.
    struct capref init_frame;
    struct frame_identity init_frame_identity;
    CHECK(frame_alloc(&init_frame, ARM_CORE_DATA_PAGES * BASE_PAGE_SIZE,
                      &retsize));
    CHECK(frame_identify(init_frame, &init_frame_identity));

    // V: Identify the provided urpc frame (no need to alloc it, yay!)
    struct frame_identity urpc_frame_identity;
    CHECK(frame_identify(cap_urpc, &urpc_frame_identity));

    // 7.3.2: Load and relocate the CPU driver.
    CHECK(load_and_relocate_driver(bootinfo, core_data));

    // Load the init module for the core_data->monitor_module.
    struct mem_region *init_module = multiboot_find_module(bootinfo, "init");
    if (!init_module) {
        DBG(ERR, "Failed to load init module\n");
        return SPAWN_ERR_FIND_MODULE;
    }
    // Information about our monitore-module (the init module), for core_data.
    struct multiboot_modinfo module_info = {.mod_start = init_module->mr_base,
                                            .mod_end = init_module->mr_base +
                                                       init_module->mrmod_size,
                                            .string = init_module->mrmod_data,
                                            .reserved = 0};

    // 7.3.3: Fill in the core_data struct.
    core_data->monitor_module = module_info;
    core_data->init_name[0] = 'i';
    core_data->init_name[1] = 'n';
    core_data->init_name[2] = 'i';
    core_data->init_name[3] = 't';
    core_data->init_name[4] = '\0';
    core_data->kcb = kcb_frame_identity.base;
    core_data->memory_base_start = init_frame_identity.base;
    core_data->memory_bytes = init_frame_identity.bytes;
    core_data->urpc_frame_base = urpc_frame_identity.base;
    core_data->urpc_frame_size = urpc_frame_identity.bytes;
    core_data->dst_core_id = core_id;
    core_data->src_core_id = current_core_id;
    core_data->cmdline =
        offsetof(struct arm_core_data, cmdline_buf) + core_data_identity.base;

    // 7.3.4: Clean the cache.
    clean_cache(kcb_frame_identity);
    clean_cache(core_data_identity);
    clean_cache(init_frame_identity);
    clean_cache(urpc_frame_identity);

    // 7.3.5: Invoke the kernel cap.
    CHECK(invoke_monitor_spawn_core(core_id, CPU_ARM7,
                                    (forvaddr_t) core_data_identity.base));

    return SYS_ERR_OK;
}
