#include <aos/aos.h>
#include <spawn/spawn.h>

#include <elf/elf.h>
#include <aos/dispatcher_arch.h>
#include <barrelfish_kpi/paging_arm_v7.h>
#include <barrelfish_kpi/domain_params.h>
#include <spawn/multiboot.h>

extern struct bootinfo *bi;

/// Initialize the cspace for a given module.
static errval_t init_cspace(struct spawninfo *si)
{
    errval_t err;

    // Create an L1 CNode according to the process's spawn-info.
    struct cnoderef l1_cnode;
    err = cnode_create_l1(&si->l1_cnode, &l1_cnode);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "cnode_create_l1 in init_cspace");
        return err;
    }

    // Go over all root-CNode slots and create L2 CNodes (foreign) in them.
    for (int i = 0; i < ROOTCN_SLOTS_USER; i++) {
        err = cnode_create_foreign_l2(si->l1_cnode, i, &si->l2_cnode_list[i]);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "cnode_create_foreign_l2 in init_cspace, in "
                      "iteration %d", i);
            return err;
        }
    }

    // TASKCN contains information about the process. Set its SLOT_ROOTCN
    // (which contains a capability for the process's root L1 CNode) to point
    // to our L1 CNode.
    struct capref taskcn_slot_rootcn = {
        .cnode = si->l2_cnode_list[ROOTCN_SLOT_TASKCN],
        .slot = TASKCN_SLOT_ROOTCN
    };
    err = cap_copy(taskcn_slot_rootcn, si->l1_cnode);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "cap_copy in init_cspace");
        return err;
    }

    // Give the SLOT_BASE_PAGE_CN some memory by iterating over all L2 slots.
    struct capref rootcn_slot_base_page_cn = {
        .cnode = si->l2_cnode_list[ROOTCN_SLOT_BASE_PAGE_CN]
    };
    for (rootcn_slot_base_page_cn.slot = 0;
            rootcn_slot_base_page_cn.slot < L2_CNODE_SLOTS;
            rootcn_slot_base_page_cn.slot++) {
        struct capref memory;

        // Allocate the memory.
        err = ram_alloc(&memory, BASE_PAGE_SIZE);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "ram_alloc in init_cspace");
            return err;
        }

        // Copy the memory capability into our SLOT_BASE_PAGE_CN slot.
        err = cap_copy(rootcn_slot_base_page_cn, memory);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "cap_copy in init_cspace");
            return err;
        }

        // Cleanup. Destroy the memory capability again.
        err = cap_destroy(memory);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "cap_destroy in init_cspace");
            return err;
        }
    }

    return SYS_ERR_OK;
}

/// Initialize the vspace for a given module.
static errval_t init_vspace(struct spawninfo *si)
{
    // TODO: Implement
    return LIB_ERR_NOT_IMPLEMENTED;
}

/// Initialize the dispatcher for a given module.
static errval_t init_dispatcher(struct spawninfo *si)
{
    // TODO: Implement
    return LIB_ERR_NOT_IMPLEMENTED;
}

/// Initialize the environment for a given module.
static errval_t init_env(struct spawninfo *si)
{
    // TODO: Implement
    return LIB_ERR_NOT_IMPLEMENTED;
}

/// Callback for elf_load.
static errval_t elf_alloc_sect_func(void *state, genvaddr_t base, size_t size,
                                    uint32_t flags, void **ret)
{
    // TODO: Implement
    return LIB_ERR_NOT_IMPLEMENTED;
}

// TODO(M4): Build and pass a messaging channel to your child process
errval_t spawn_load_by_name(void * binary_name, struct spawninfo * si)
{
    printf("spawn start_child: starting: %s\n", binary_name);

    errval_t err;

    // Init spawninfo.
    memset(si, 0, sizeof(*si));
    si->binary_name = binary_name;

    // I: Getting the binary from the multiboot image.
    struct mem_region *module = multiboot_find_module(bi, binary_name);
    if (module == NULL) {
        debug_printf("multiboot: Could not find module %s\n", binary_name);
        return SPAWN_ERR_FIND_MODULE;
    }

    // II: Mapping the multiboot module into our address space.
    struct capref child_frame = {
        .cnode = cnode_module,
        .slot = module->mrmod_slot
    };

    struct frame_identity frame_id;
    err = frame_identify(child_frame, &frame_id);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "frame_identify in spawn_load_by_name");
        return err;
    }

    lvaddr_t elf_addr;
    err = paging_map_frame(get_current_paging_state(), (void **)&elf_addr,
                           frame_id.bytes, child_frame, NULL, NULL);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "paging_map_frame in spawn_load_by_name");
        return err;
    }

    // III: Set up the child's cspace.
    err = init_cspace(si);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "init_cspace in spawn_load_by_name");
        return err;
    }

    // IV: Set up the child's vspace.
    err = init_vspace(si);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "init_vspace in spawn_load_by_name");
        return err;
    }

    // V: Load the ELF binary.
    err = elf_load(EM_ARM, elf_alloc_sect_func, (void *)si, elf_addr,
                   frame_id.bytes, &si->entry_addr);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "elf_load in spawn_load_by_name");
        return err;
    }

    struct Elf32_Shdr *global_offset_table =
        elf32_find_section_header_name(elf_addr, frame_id.bytes, ".got");
    if (global_offset_table == NULL) {
        debug_printf("libspawn: Unable to load ELF for binary %s\n",
                     binary_name);
        return SPAWN_ERR_LOAD;
    }
    // Store the uspace base.
    si->u_base = global_offset_table->sh_addr;

    // VI: Initialize the dispatcher.
    err = init_dispatcher(si);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "init_dispatcher in spawn_load_by_name");
        return err;
    }
    
    // VII: Initialize the environment.
    err = init_env(si);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "init_env in spawn_load_by_name");
        return err;
    }
    
    // VIII: Make the dispatcher runnable.
    // TODO: Implement.

    return SYS_ERR_OK;
}
