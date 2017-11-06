#include "lib_multicore.h"

static errval_t core_allocate_memory(struct arm_core_data **ret_core_data,
                                     coreid_t core_id,
                                     coreid_t current_core_id) {
    size_t retsize;

    // I: Allocate a new KCB.
    struct capref kcb_ram, kcb_frame;
    struct frame_identity kcb_frame_identity;
    CHECK(ram_alloc(&kcb_ram, OBJSIZE_KCB));
    CHECK(slot_alloc(&kcb_frame));
    CHECK(cap_retype(kcb_frame, kcb_ram, 0, ObjType_KernelControlBlock,
                     OBJSIZE_KCB, 1));
    CHECK(frame_identify(kcb_frame, &kcb_frame_identity));

    // II: Allocate coreboot's core_data struct.
    struct capref core_data_frame;
    struct arm_core_data *core_data;
    CHECK(frame_alloc(&core_data_frame, OBJSIZE_KCB, &retsize));
    CHECK(invoke_kcb_clone(kcb_frame, core_data_frame));
    CHECK(paging_map_frame(get_current_paging_state(), (void *) &core_data,
                           retsize, core_data_frame, NULL, NULL));

    // III: Allocate memory to load coreboot's relocatable segment. TODO
    // Idk what exactly this has to be?...

    // IV: Allocate space to load the init process.
    struct capref init_frame;
    struct frame_identity init_frame_identity;
    CHECK(frame_alloc(&init_frame, ARM_CORE_DATA_PAGES * BASE_PAGE_SIZE,
                      &retsize));
    CHECK(frame_identify(init_frame, &init_frame_identity));

    // V: Identify the provided urpc frame (no need to alloc it, yay!)
    struct frame_identity urpc_frame_identity;
    CHECK(frame_identify(cap_urpc, &urpc_frame_identity));

    // Fill in the core_data struct.
    core_data->kcb = kcb_frame_identity.base;
    core_data->memory_base_start = init_frame_identity.base;
    core_data->memory_bytes = init_frame_identity.bytes;
    core_data->urpc_frame_base = urpc_frame_identity.base;
    core_data->urpc_frame_size = urpc_frame_identity.bytes;
    core_data->dst_core_id = core_id;
    core_data->src_core_id = current_core_id;

    *ret_core_data = core_data;

    return SYS_ERR_OK;
}

static errval_t load_and_relocate_driver(struct bootinfo *bootinfo,
                                         struct arm_core_data *core_data)
{
    struct mem_region *init_module = multiboot_find_module(bootinfo, "init");
    if (!init_module) {
        DBG(ERR, "Failed to load init module\n");
        return SPAWN_ERR_FIND_MODULE;
    }

    struct multiboot_modinfo module_info = {
        .mod_start = init_module->mr_base,
        .mod_end = init_module->mr_base + init_module->mrmod_size,
        .string = init_module->mrmod_data,
        .reserved = 0
    };
    core_data->monitor_module = module_info;

    core_data->init_name[0] = 'i';
    core_data->init_name[1] = 'n';
    core_data->init_name[2] = 'i';
    core_data->init_name[3] = 't';
    core_data->init_name[4] = '\0';

    return SYS_ERR_OK;
}

static errval_t clean_cache(void) {
    return LIB_ERR_NOT_IMPLEMENTED;
}

static errval_t invoke_kernel_cap(void) {
    return LIB_ERR_NOT_IMPLEMENTED;
}

errval_t wake_core(coreid_t core_id, coreid_t current_core_id,
                   struct bootinfo *bootinfo)
{
    // If the core id is 0, this core is already up and does not need to be
    // woken up.
    assert(core_id != 0);

    // If the core id is our own current core id, we are obviously already
    // awake too.
    assert(core_id != current_core_id);

    struct arm_core_data *core_data;

    CHECK(core_allocate_memory(&core_data, core_id, current_core_id));

    CHECK(load_and_relocate_driver(bootinfo, core_data));

    CHECK(clean_cache());

    CHECK(invoke_kernel_cap());

    return LIB_ERR_NOT_IMPLEMENTED;
}
