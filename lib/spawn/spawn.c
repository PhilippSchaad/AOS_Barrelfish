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
    // Create an L1 CNode according to the process's spawn-info.
    struct cnoderef l1_cnode;
    CHECK(cnode_create_l1(&si->l1_cnode, &l1_cnode));

    // Go over all root-CNode slots and create L2 CNodes (foreign) in them.
    for (int i = 0; i < ROOTCN_SLOTS_USER; i++) {
        CHECK(cnode_create_foreign_l2(si->l1_cnode, i, &si->l2_cnode_list[i]));
    }

    // TASKCN contains information about the process. Set its SLOT_ROOTCN
    // (which contains a capability for the process's root L1 CNode) to point
    // to our L1 CNode.
    struct capref taskcn_slot_rootcn = {
        .cnode = si->l2_cnode_list[ROOTCN_SLOT_TASKCN],
        .slot = TASKCN_SLOT_ROOTCN
    };
    CHECK(cap_copy(taskcn_slot_rootcn, si->l1_cnode));


    // Give the SLOT_BASE_PAGE_CN some memory by iterating over all L2 slots.
    struct capref rootcn_slot_base_page_cn = {
        .cnode = si->l2_cnode_list[ROOTCN_SLOT_BASE_PAGE_CN]
    };
    for (rootcn_slot_base_page_cn.slot = 0;
            rootcn_slot_base_page_cn.slot < L2_CNODE_SLOTS;
            rootcn_slot_base_page_cn.slot++) {
        struct capref memory;

        // Allocate the memory.
        CHECK(ram_alloc(&memory, BASE_PAGE_SIZE));

        // Copy the memory capability into our SLOT_BASE_PAGE_CN slot.
        CHECK(cap_copy(rootcn_slot_base_page_cn, memory));

        // Cleanup. Destroy the memory capability again.
        CHECK(cap_destroy(memory));
    }

    return SYS_ERR_OK;
}

errval_t slot_callback(struct spawninfo* si, struct capref cap);
errval_t slot_callback(struct spawninfo* si, struct capref cap){
    debug_printf("Copy slot to child\n");
    struct capref child = {
        .cnode = si->l2_cnode_list[ROOTCN_SLOT_PAGECN],
        .slot = si->next_slot++
    };
    return cap_copy(child, cap);
}

/// Initialize the vspace for a given module.
static errval_t init_vspace(struct spawninfo *si)
{
    // create l1 page table in the current space
    // cap
    struct capref l1_pt;
    CHECK(slot_alloc(&l1_pt));

    // vnode
    CHECK(vnode_create(l1_pt, ObjType_VNode_ARM_l1));
   
    // set up the new process' capability
    si->process_l1_pt.cnode = si->l2_cnode_list[ROOTCN_SLOT_PAGECN];
    si->process_l1_pt.slot  = PAGECN_SLOT_VROOT;


    // TODO:
    // do we need to prefill the table?
    
    // copy the page table to the new process
    CHECK(cap_copy(si->process_l1_pt, l1_pt));

    // Set the spawned process's paging state.
    CHECK(paging_init_state(&si->paging_state, /*XXX: what do we need here??? */(1 << 25),
                            l1_pt, get_default_slot_allocator()));

    // add the callback function
    si->slot_callback=slot_callback;
    si->paging_state.spawninfo = si;

    return SYS_ERR_OK;
    
}

/// Initialize the dispatcher for a given module.
static errval_t init_dispatcher(struct spawninfo *si)
{
    // XXX: Note, untested!

    debug_printf(" Allocate a capability for the dispatcher.\n");
    CHECK(slot_alloc(&si->dispatcher));

    debug_printf(" Create the dispatcher.\n");
    CHECK(dispatcher_create(si->dispatcher));

    debug_printf(" Set an endpoint for the dispatcher.\n");
    struct capref dispatcher_end;
    CHECK(slot_alloc(&dispatcher_end));
    CHECK(cap_retype(dispatcher_end, si->dispatcher, 0, ObjType_EndPoint,
                     0, 1));

    debug_printf(" Create a memory frame for the dispatcher.\n");
    size_t retsize;
    struct capref dispatcher_memframe;
    CHECK(frame_alloc(&dispatcher_memframe, DISPATCHER_SIZE, &retsize));
    
    assert(retsize == DISPATCHER_SIZE);

    debug_printf(" Copy the dispatcher into the spawned process's VSpace.\n");
    struct capref spawned_dispatcher = {
        .cnode = si->l2_cnode_list[ROOTCN_SLOT_TASKCN],
        .slot = TASKCN_SLOT_DISPATCHER
    };
    CHECK(cap_copy(spawned_dispatcher, si->dispatcher));

    debug_printf(" Copy the endpoint into the spawned process's VSpace.\n");
    struct capref spawned_endpoint = {
        .cnode = si->l2_cnode_list[ROOTCN_SLOT_TASKCN],
        .slot = TASKCN_SLOT_SELFEP
    };
    CHECK(cap_copy(spawned_endpoint, dispatcher_end));

    debug_printf(" Copy the dispatcher's mem frame into the new process's VSpace.\n");
    si->spawned_disp_memframe.cnode = si->l2_cnode_list[ROOTCN_SLOT_TASKCN];
    si->spawned_disp_memframe.slot = TASKCN_SLOT_DISPFRAME;
    
    CHECK(cap_copy(si->spawned_disp_memframe, dispatcher_memframe));

    debug_printf(" Map the dispatcher's memory frame into the current VSpace.\n");
    lvaddr_t disp_current_vaddr;
    CHECK(paging_map_frame(get_current_paging_state(),
                           (void *)&disp_current_vaddr,
                           DISPATCHER_SIZE, dispatcher_memframe, NULL,
                           NULL));

    debug_printf(" Map the dispatcher's memory frame into the spawned VSpace.\n");
    lvaddr_t disp_spawn_vaddr;
    CHECK(paging_map_frame(&si->paging_state,
                           (void *)&disp_spawn_vaddr, DISPATCHER_SIZE,
                           dispatcher_memframe, NULL, NULL));

    debug_printf(" Finalize the dispatcher: (ref. book 4.15)\n");
    // Get a reference to the dispatcher, name it, set it to disabled at first,
    // set the dispatcher memframe address in the new VSpace (spawned process),
    // and set it to trap on FPU instructions.
    struct dispatcher_shared_generic *disp =
        get_dispatcher_shared_generic((dispatcher_handle_t)disp_current_vaddr);
    disp->udisp = disp_spawn_vaddr;
    disp->disabled = 1;
    disp->fpu_trap = 1;
    strncpy(disp->name, si->binary_name, DISP_NAME_LEN);

    debug_printf(" Set the core ID of the process and zero the frame(/size) and header.\n");
    struct dispatcher_generic *disp_gen =
        get_dispatcher_generic((dispatcher_handle_t)disp_current_vaddr);
    disp_gen->core_id = 0;
    disp_gen->eh_frame = 0;
    disp_gen->eh_frame_size = 0;
    disp_gen->eh_frame_hdr = 0;
    disp_gen->eh_frame_hdr_size = 0;

    debug_printf(" Set the base address of the GOT in the new VSpace.\n");
    struct dispatcher_shared_arm *disp_arm =
        get_dispatcher_shared_arm((dispatcher_handle_t)disp_current_vaddr);
    disp_arm->got_base = si->u_got;

    arch_registers_state_t *enabled_area =
        dispatcher_get_enabled_save_area(
                (dispatcher_handle_t)disp_current_vaddr);
    arch_registers_state_t *disabled_area =
        dispatcher_get_disabled_save_area(
                (dispatcher_handle_t)disp_current_vaddr);
    enabled_area->regs[REG_OFFSET(PIC_REGISTER)] = si->u_got;
    disabled_area->regs[REG_OFFSET(PIC_REGISTER)] = si->u_got;
    disabled_area->named.pc = si->entry_addr;
    enabled_area->named.cpsr = CPSR_F_MASK | ARM_MODE_USR;
    disabled_area->named.cpsr = CPSR_F_MASK | ARM_MODE_USR;

    debug_printf(" Store enabled area in spawn info, because we need it to init the env.\n");
    si->enabled_area = enabled_area;

    return SYS_ERR_OK;
}

/// Initialize the environment for a given module.
static errval_t init_env(struct spawninfo *si, struct mem_region *module)
{
    debug_printf(" Retrieve arguments from the module and allocate memory for them.\n");
    const char *args = multiboot_module_opts(module);
    size_t region_size = ROUND_UP(sizeof(struct spawn_domain_params) +
                                  strlen(args) + 1, BASE_PAGE_SIZE);
    struct capref mem_frame;
    size_t retsize;
    CHECK(frame_alloc(&mem_frame, region_size, &retsize));

    assert(retsize == region_size);

    debug_printf(" Map the arguments into the current VSpace.\n");
    lvaddr_t args_addr;
    CHECK(paging_map_frame(get_current_paging_state(), (void *)&args_addr,
                           retsize, mem_frame, NULL, NULL));

    debug_printf(" Map the arguments into the spawned process's CSpace.\n");
    struct capref spawn_args = {
        .cnode = si->l2_cnode_list[ROOTCN_SLOT_TASKCN],
        .slot = TASKCN_SLOT_ARGSPAGE
    };
    CHECK(cap_copy(spawn_args, mem_frame));

    debug_printf(" Map the arguments into the spawned process's VSpace.\n");
    lvaddr_t spawn_args_addr;
    CHECK(paging_map_frame(&si->paging_state,
                           (void *)&spawn_args_addr, retsize, mem_frame,
                           NULL, NULL));

    debug_printf(" Complete spawn_domain_params.\n");
    struct spawn_domain_params *parameters =
        (struct spawn_domain_params *)args_addr;
    memset(&parameters->argv[0], 0, sizeof(parameters->argv));
    memset(&parameters->envp[0], 0, sizeof(parameters->envp));

    debug_printf(" Add the arguments into the spawned process's VSpace.\n");
    char *param_base =
        (char *) parameters + sizeof(struct spawn_domain_params);
    char *param_last = param_base;
    lvaddr_t spawn_param_base =
        spawn_args_addr + sizeof(struct spawn_domain_params);
    strcpy(param_base, args);

    debug_printf(" Set the arguments correctly.\n");
    char *current_param = param_base;
    size_t n_args = 0;
    while (*current_param != 0) {
        if (*current_param == ' ') {
            parameters->argv[n_args] =
                (void *)spawn_param_base + (param_last - param_base);
            *current_param = 0;
            n_args += 1;
            current_param += 1;
            param_last = current_param;
        }
        current_param += 1;
    }
    parameters->argv[n_args] =
        (void *)spawn_param_base + (param_last - param_base);
    n_args += 1;
    parameters->argc = n_args;
    si->enabled_area->named.r0 = spawn_args_addr;

    debug_printf(" Remaining arguments unset.\n");
    parameters->vspace_buf = NULL;
    parameters->vspace_buf_len = 0;
    parameters->tls_init_base = NULL;
    parameters->tls_init_len = 0;
    parameters->tls_total_len = 0;
    parameters->pagesize = 0;

    return SYS_ERR_OK;
}

/// Callback for elf_load.
static errval_t elf_alloc_sect_func(void *state, genvaddr_t base, size_t size,
                                    uint32_t flags, void **ret)
{
    debug_printf("start elf_alloc_sect_funci at %"PRIxGENPADDR"\n", base);
    //size_t alignment_offset = BASE_PAGE_OFFSET(base);
    // Align base address and size.
    //genvaddr_t base_aligned = base - alignment_offset;
    //size_t size_aligned = ROUND_UP(size + alignment_offset, BASE_PAGE_SIZE);

    // Allocate memory frame for this ELF section.
    struct capref frame;
    size_t retsize;
    CHECK(frame_alloc(&frame, size, &retsize));

    //assert(retsize == size_aligned);

    // Map the frame into the spawned process's VSpace.
    CHECK(paging_map_fixed_attr(&((struct spawninfo *)state)->paging_state,
                                base, frame, retsize, flags));

    // Map it into the current VSpace.
    CHECK(paging_map_frame(get_current_paging_state(), ret, retsize,
                           frame, NULL, NULL));

    // Correct return to fit alignment.
    //*ret += alignment_offset;
    debug_printf("end elf_alloc_sect_func. I will return buffer at address 0x%"PRIxPTR"\n", *ret);
    return SYS_ERR_OK;
}

// TODO(M4): Build and pass a messaging channel to your child process
errval_t spawn_load_by_name(void * binary_name, struct spawninfo * si)
{
    printf("spawn start_child: starting: %s\n", binary_name);

    // Init spawninfo.
    memset(si, 0, sizeof(*si));
    si->binary_name = binary_name;

    debug_printf("I: Getting the binary from the multiboot image.\n");
    struct mem_region *module = multiboot_find_module(bi, binary_name);
    if (module == NULL) {
        debug_printf("multiboot: Could not find module %s\n", binary_name);
        return SPAWN_ERR_FIND_MODULE;
    }

    debug_printf("II: Mapping the multiboot module into our address space.\n");
    struct capref child_frame = {
        .cnode = cnode_module,
        .slot = module->mrmod_slot
    };

    struct frame_identity frame_id;
    CHECK(frame_identify(child_frame, &frame_id));

    lvaddr_t elf_addr;
    CHECK(paging_map_frame(get_current_paging_state(), (void **)&elf_addr,
                           frame_id.bytes, child_frame, NULL, NULL));

    debug_printf("Magic Number of elf: %i %c%c%c\n",*(char*)elf_addr,*(((char*)elf_addr)+1),*(((char*)elf_addr)+2),*(((char*)elf_addr)+3));

    debug_printf("III: Set up the child's cspace.\n");
    CHECK(init_cspace(si));

    debug_printf("IV: Set up the child's vspace.\n");
    CHECK(init_vspace(si));

    debug_printf("V: Load the ELF binary.\n");
    CHECK(elf_load(EM_ARM, elf_alloc_sect_func, (void *)si, elf_addr,
                   frame_id.bytes, &si->entry_addr));

    struct Elf32_Shdr *global_offset_table =
        elf32_find_section_header_name(elf_addr, frame_id.bytes, ".got");
    if (global_offset_table == NULL) {
        debug_printf("libspawn: Unable to load ELF for binary %s\n",
                     binary_name);
        return SPAWN_ERR_LOAD;
    }
    // Store the uspace base.
    si->u_got = global_offset_table->sh_addr;

    debug_printf("Magic Number of elf again: %i %c%c%c\n",*(char*)elf_addr,*(((char*)elf_addr)+1),*(((char*)elf_addr)+2),*(((char*)elf_addr)+3));

    debug_printf("VI: Initialize the dispatcher.\n");
    CHECK(init_dispatcher(si));

    debug_printf("VII: Initialize the environment.\n");
    CHECK(init_env(si, module));

    debug_printf("VIII: Make the dispatcher runnable.\n");
    CHECK(invoke_dispatcher(si->dispatcher, cap_dispatcher, si->l1_cnode,
                            si->process_l1_pt, si->spawned_disp_memframe,
                            true));

    return SYS_ERR_OK;
}
