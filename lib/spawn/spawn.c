#include <aos/aos.h>
#include <spawn/spawn.h>

#include <aos/dispatcher_arch.h>
#include <barrelfish_kpi/domain_params.h>
#include <barrelfish_kpi/paging_arm_v7.h>
#include <elf/elf.h>
#include <spawn/multiboot.h>

extern struct bootinfo *bi;
extern domainid_t procman_register_process(char *name, coreid_t core_id,
                                           struct spawninfo *si);

/// Initialize the cspace for a given module.
static errval_t init_cspace(struct spawninfo *si)
{
    // Create an L1 CNode according to the process's spawn-info.
    struct cnoderef l1_cnode;
    CHECK(cnode_create_l1(&si->l1_cnode, &l1_cnode));

    // Go over all root-CNode slots and create L2 CNodes (foreign)
    // in them (using L1 created before.)
    for (int i = 0; i <= ROOTCN_SLOTS_USER; i++) {
        CHECK(cnode_create_foreign_l2(si->l1_cnode, i, &si->l2_cnode_list[i]));
    }

    DBG(DETAILED, "1. Map TASKCN");
    // TASKCN contains information about the process. Set its SLOT_ROOTCN
    // (which contains a capability for the process's root L1 CNode) to point
    // to our L1 CNode.
    struct capref taskcn_slot_rootcn = {
        .cnode = si->l2_cnode_list[ROOTCN_SLOT_TASKCN],
        .slot = TASKCN_SLOT_ROOTCN};
    CHECK(cap_copy(taskcn_slot_rootcn, si->l1_cnode));

    DBG(DETAILED, "2. Create RAM caps for SLOT_BASE_PAGE_CN");
    // Give the SLOT_BASE_PAGE_CN some memory by iterating over all L2 slots.
    struct capref rootcn_slot_base_page_cn = {
        .cnode = si->l2_cnode_list[ROOTCN_SLOT_BASE_PAGE_CN]};
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

static errval_t slot_callback(struct spawninfo *si, struct capref cap)
{
    DBG(DETAILED, "Copy slot to child\n");
    struct capref child = {.cnode = si->l2_cnode_list[ROOTCN_SLOT_PAGECN],
                           .slot = si->next_slot++};
    return cap_copy(child, cap);
}

/// Initialize the vspace for a given module.
static errval_t init_vspace(struct spawninfo *si)
{
    // Create an L1 pagetable in the current VSpace.
    struct capref l1_pt;
    CHECK(slot_alloc(&l1_pt));
    CHECK(vnode_create(l1_pt, ObjType_VNode_ARM_l1));

    // Set up the new process's capability.
    si->process_l1_pt.cnode = si->l2_cnode_list[ROOTCN_SLOT_PAGECN];
    si->process_l1_pt.slot = PAGECN_SLOT_VROOT;

    // Copy the page table to the new process.
    CHECK(cap_copy(si->process_l1_pt, l1_pt));

    // Set the spawned process's paging state.
    CHECK(paging_init_state(&si->paging_state, PAGING_VADDR_START, l1_pt,
                            get_default_slot_allocator()));

    // Add the callback function for slot allocation.
    si->slot_callback = slot_callback;
    si->paging_state.spawninfo = si;
    si->next_slot = PAGECN_SLOT_VROOT + 1;

    return SYS_ERR_OK;
}

/// Initialize the dispatcher for a given module.
static errval_t init_dispatcher(struct spawninfo *si)
{
    DBG(DETAILED, " Allocate a capability for the dispatcher.\n");
    CHECK(slot_alloc(&si->dispatcher));

    DBG(DETAILED, " Create the dispatcher.\n");
    CHECK(dispatcher_create(si->dispatcher));

    DBG(DETAILED, " Set an endpoint for the dispatcher.\n");
    struct capref dispatcher_end;
    CHECK(slot_alloc(&dispatcher_end));
    CHECK(
        cap_retype(dispatcher_end, si->dispatcher, 0, ObjType_EndPoint, 0, 1));

    DBG(DETAILED, " Create a memory frame for the dispatcher.\n");
    size_t retsize;
    struct capref dispatcher_memframe;
    CHECK(frame_alloc(&dispatcher_memframe, DISPATCHER_SIZE, &retsize));

    if (retsize != DISPATCHER_SIZE)
        return LIB_ERR_NO_SIZE_MATCH;

    DBG(DETAILED, " Copy the dispatcher into the spawned process's VSpace.\n");
    struct capref spawned_dispatcher = {
        .cnode = si->l2_cnode_list[ROOTCN_SLOT_TASKCN],
        .slot = TASKCN_SLOT_DISPATCHER};
    CHECK(cap_copy(spawned_dispatcher, si->dispatcher));

    DBG(DETAILED, " Copy the endpoint into the spawned process's VSpace.\n");
    struct capref spawned_endpoint = {
        .cnode = si->l2_cnode_list[ROOTCN_SLOT_TASKCN],
        .slot = TASKCN_SLOT_SELFEP};
    CHECK(cap_copy(spawned_endpoint, dispatcher_end));

    DBG(DETAILED, " Create the endpoint to the init process in the spawned "
                  "process' CSpace\n");
    struct capref init_endpoint = {.cnode =
                                       si->l2_cnode_list[ROOTCN_SLOT_TASKCN],
                                   .slot = TASKCN_SLOT_INITEP};
    CHECK(cap_copy(init_endpoint, cap_initep));

    DBG(DETAILED, " Copy the dispatcher's mem frame into the new "
                  "process's VSpace.\n");
    si->spawned_disp_memframe.cnode = si->l2_cnode_list[ROOTCN_SLOT_TASKCN];
    si->spawned_disp_memframe.slot = TASKCN_SLOT_DISPFRAME;

    CHECK(cap_copy(si->spawned_disp_memframe, dispatcher_memframe));

    DBG(DETAILED, " Map the dispatcher's memory frame into the "
                  "current VSpace.\n");
    void *disp_current_vaddr;
    CHECK(paging_map_frame(get_current_paging_state(), &disp_current_vaddr,
                           DISPATCHER_SIZE, dispatcher_memframe, NULL, NULL));

    DBG(DETAILED, " Map the dispatcher's memory frame into the "
                  "spawned VSpace.\n");
    void *disp_spawn_vaddr;
    CHECK(paging_map_frame(&si->paging_state, &disp_spawn_vaddr,
                           DISPATCHER_SIZE, dispatcher_memframe, NULL, NULL));

    DBG(DETAILED, " Finalize the dispatcher: (ref. book 4.15)\n");
    // Get a reference to the dispatcher, name it, set it to disabled at first,
    // set the dispatcher memframe address in the new VSpace (spawned process),
    // and set it to trap on FPU instructions.
    struct dispatcher_shared_generic *disp = get_dispatcher_shared_generic(
        (dispatcher_handle_t) disp_current_vaddr);
    disp->udisp = (lvaddr_t) disp_spawn_vaddr;
    disp->disabled = 1;
    disp->fpu_trap = 1;
    strncpy(disp->name, si->binary_name, DISP_NAME_LEN);
    DBG(DETAILED, " Set the core ID of the process and zero the frame(/size) "
                  "and header.\n");
    struct dispatcher_generic *disp_gen =
        get_dispatcher_generic((dispatcher_handle_t) disp_current_vaddr);
    disp_gen->domain_id = si->paging_state.debug_paging_state_index;
    disp_gen->core_id = 0;
    disp_gen->eh_frame = 0;
    disp_gen->eh_frame_size = 0;
    disp_gen->eh_frame_hdr = 0;
    disp_gen->eh_frame_hdr_size = 0;

    DBG(DETAILED, " Set the base address of the GOT in the new VSpace.\n");
    struct dispatcher_shared_arm *disp_arm =
        get_dispatcher_shared_arm((dispatcher_handle_t) disp_current_vaddr);
    disp_arm->got_base = si->u_got;

    arch_registers_state_t *enabled_area = dispatcher_get_enabled_save_area(
        (dispatcher_handle_t) disp_current_vaddr);
    arch_registers_state_t *disabled_area = dispatcher_get_disabled_save_area(
        (dispatcher_handle_t) disp_current_vaddr);
    enabled_area->regs[REG_OFFSET(PIC_REGISTER)] = si->u_got;
    disabled_area->regs[REG_OFFSET(PIC_REGISTER)] = si->u_got;
    disabled_area->named.pc = si->entry_addr;
    enabled_area->named.cpsr = CPSR_F_MASK | ARM_MODE_USR;
    disabled_area->named.cpsr = CPSR_F_MASK | ARM_MODE_USR;

    DBG(DETAILED, " Store enabled area in spawn info, because we need it to "
                  "init the env.\n");
    si->enabled_area = enabled_area;

    return SYS_ERR_OK;
}

/// Initialize the environment for a given module.
static errval_t init_env(struct spawninfo *si, struct mem_region *module,
                         char *arguments)
{
    char *args;
    if (arguments == NULL) {
        DBG(DETAILED, " Retrieve arguments from the module and allocate memory "
                      "for them.\n");
        args = (char *) multiboot_module_opts(module);
    } else {
        args = arguments;
    }

    DBG(DETAILED, " Found the following command line arguments: %s\n", args);
    size_t region_size = ROUND_UP(
        sizeof(struct spawn_domain_params) + strlen(args) + 1, BASE_PAGE_SIZE);
    struct capref mem_frame;
    size_t retsize;
    CHECK(frame_alloc(&mem_frame, region_size, &retsize));

    if (retsize != region_size)
        return LIB_ERR_NO_SIZE_MATCH;

    DBG(DETAILED, " Map the arguments into the current VSpace.\n");
    void *args_addr;
    CHECK(paging_map_frame(get_current_paging_state(), &args_addr, retsize,
                           mem_frame, NULL, NULL));

    DBG(DETAILED, " Map the arguments into the spawned process's CSpace.\n");
    struct capref spawn_args = {.cnode = si->l2_cnode_list[ROOTCN_SLOT_TASKCN],
                                .slot = TASKCN_SLOT_ARGSPAGE};
    CHECK(cap_copy(spawn_args, mem_frame));

    DBG(DETAILED, " Map the arguments into the spawned process's VSpace.\n");
    void *spawn_args_addr;
    CHECK(paging_map_frame(&si->paging_state, &spawn_args_addr, retsize,
                           mem_frame, NULL, NULL));

    DBG(DETAILED, " Complete spawn_domain_params.\n");
    struct spawn_domain_params *parameters =
        (struct spawn_domain_params *) args_addr;
    memset(&parameters->argv[0], 0, sizeof(parameters->argv));
    memset(&parameters->envp[0], 0, sizeof(parameters->envp));

    DBG(DETAILED, " Add the arguments into the spawned process's VSpace.\n");
    char *param_base =
        (char *) parameters + sizeof(struct spawn_domain_params);
    char *param_last = param_base;
    lvaddr_t spawn_param_base =
        (lvaddr_t) spawn_args_addr + sizeof(struct spawn_domain_params);
    strcpy(param_base, args);

    DBG(DETAILED, " Set the arguments correctly.\n");
    char *current_param = param_base;
    size_t n_args = 0;
    while (*current_param != 0) {
        if (*current_param == ' ') {
            parameters->argv[n_args] =
                (void *) spawn_param_base + (param_last - param_base);
            *current_param = 0;
            n_args += 1;
            current_param += 1;
            param_last = current_param;
        }
        current_param += 1;
    }
    parameters->argv[n_args] =
        (void *) spawn_param_base + (param_last - param_base);
    n_args += 1;
    parameters->argc = n_args;
    si->enabled_area->named.r0 = (uint32_t) spawn_args_addr;

    DBG(DETAILED, " Remaining arguments unset.\n");
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
    DBG(DETAILED, "start elf_alloc_sect_funci at %" PRIxGENPADDR "\n", base);
    size_t alignment_offset = BASE_PAGE_OFFSET(base);
    // Align base address and size.
    genvaddr_t base_aligned = base - alignment_offset;
    size_t size_aligned = ROUND_UP(size + alignment_offset, BASE_PAGE_SIZE);

    // Allocate memory frame for this ELF section.
    struct capref frame;
    size_t retsize;
    CHECK(frame_alloc(&frame, size_aligned, &retsize));

    if (retsize != size_aligned)
        return LIB_ERR_NO_SIZE_MATCH;

    // Map the frame into the spawned process's VSpace.
    CHECK(paging_map_fixed_attr(&((struct spawninfo *) state)->paging_state,
                                base_aligned, frame, retsize, flags));

    // Map it into the current VSpace.
    CHECK(paging_map_frame(get_current_paging_state(), ret, retsize, frame,
                           NULL, NULL));

    struct paging_frame_node *new_node =
        (struct paging_frame_node *) slab_alloc(
            &((struct spawninfo *) state)->paging_state.slab_alloc);
    new_node->base_addr = (lvaddr_t) *ret;
    new_node->region_size = retsize;
    new_node->next = ((struct spawninfo *) state)->allocated_sects;
    ((struct spawninfo *) state)->allocated_sects = new_node;

    // Correct return to fit alignment.
    *ret += alignment_offset;
    DBG(DETAILED, "end elf_alloc_sect_func. I will return buffer at "
                  "address 0x%" PRIxPTR "\n",
        *ret);
    return SYS_ERR_OK;
}

/// Helper function to pass the paging state to the spawned process.
static errval_t map_paging_state_to_child(struct paging_state *st)
{
    struct capref frame;
    size_t ret;
    size_t nodes = 0;
    struct paging_frame_node *x = &st->free_vspace;

    while (x->next != NULL) {
        nodes++;
        x = x->next;
    }

    CHECK(frame_alloc(&frame,
                      sizeof(struct paging_state) +
                          nodes * sizeof(struct paging_frame_node),
                      &ret));
    paging_map_fixed(st, PAGING_STATE_PADDR, frame, ret);

    void *our_side;
    paging_map_frame(get_current_paging_state(), &our_side, ret, frame, NULL,
                     NULL);
    struct paging_state *mapped_st = (struct paging_state *) our_side;
    *mapped_st = *st;

    // We move past the paging_state in the memblock now and then map our nodes
    struct paging_frame_node *mem =
        (struct paging_frame_node *) &(mapped_st[1]);
    x = &st->free_vspace;
    while (x->next != NULL) {
        *mem = *x;
        x = x->next;
        mem->next = &mem[1];
    }
    return SYS_ERR_OK;
}

/**
 * \brief Spawn a process loaded by its binary name.
 *
 * \param binary_name   Binary name of the process to spawn.
 * \param si            Struct holding information about the process to spawn.
 */
errval_t spawn_load_by_name(char *binary_name, struct spawninfo *si)
{
    DBG(VERBOSE, "spawn start_child: starting: %s\n", binary_name);

    // Init spawninfo.
    memset(si, 0, sizeof(*si));

    // Parse 'binary_name' to filter out program name and arguments.
    char *actual_name = binary_name;
    char *arguments = NULL;
    int binary_name_length = strlen(binary_name);
    for (int i = 0; i < binary_name_length; i++) {
        if (binary_name[i] == ' ') {
            actual_name = malloc((i + 1) * sizeof(char));
            strncpy(actual_name, binary_name, i);
            actual_name[i] = '\0';
            if (i + 1 < binary_name_length && binary_name[i + 1] != '\0')
                arguments = &binary_name[i + 1];
            break;
        }
    }
    si->binary_name = actual_name;

    DBG(DETAILED, "I: Getting the binary from the multiboot image.\n");
    struct mem_region *module;
    module = multiboot_find_module(bi, actual_name);
    if (module == NULL) {
        DBG(VERBOSE, "multiboot: Could not find module %s\n", binary_name);
        return SPAWN_ERR_FIND_MODULE;
    }

    DBG(DETAILED,
        "II: Mapping the multiboot module into our address space.\n");
    struct capref child_frame = {.cnode = cnode_module,
                                 .slot = module->mrmod_slot};

    struct frame_identity frame_id;
    CHECK(frame_identify(child_frame, &frame_id));

    lvaddr_t elf_addr;
    CHECK(paging_map_frame(get_current_paging_state(), (void **) &elf_addr,
                           frame_id.bytes, child_frame, NULL, NULL));

    DBG(VERBOSE, "Magic Number of elf: %i %c%c%c\n", *(char *) elf_addr,
        *(((char *) elf_addr) + 1), *(((char *) elf_addr) + 2),
        *(((char *) elf_addr) + 3));

    DBG(DETAILED, "III: Set up the child's cspace.\n");
    CHECK(init_cspace(si));

    DBG(DETAILED, "IV: Set up the child's vspace.\n");
    CHECK(init_vspace(si));

    DBG(DETAILED, "V: Load the ELF binary.\n");
    CHECK(elf_load(EM_ARM, elf_alloc_sect_func, (void *) si, elf_addr,
                   frame_id.bytes, &si->entry_addr));

    struct Elf32_Shdr *global_offset_table =
        elf32_find_section_header_name(elf_addr, frame_id.bytes, ".got");
    if (global_offset_table == NULL) {
        DBG(ERR, "libspawn: Unable to load ELF for binary %s\n", binary_name);
        return SPAWN_ERR_LOAD;
    }
    // Store the uspace Global Offset Table base.
    si->u_got = global_offset_table->sh_addr;

    DBG(VERBOSE, "Magic Number of elf again: %i %c%c%c\n", *(char *) elf_addr,
        *(((char *) elf_addr) + 1), *(((char *) elf_addr) + 2),
        *(((char *) elf_addr) + 3));

    DBG(DETAILED, "VI: Initialize the dispatcher.\n");
    CHECK(init_dispatcher(si));

    DBG(DETAILED, "VII: Initialize the environment.\n");
    CHECK(init_env(si, module, arguments));

    map_paging_state_to_child(&si->paging_state);

    // Check the registers...
    DBG(DETAILED, "dump stuff...\n");
    DBG(DETAILED, "Entry Address: 0x%" PRIxGENPADDR "\n", si->entry_addr);
    DBG(DETAILED, "VIII: Make the dispatcher runnable.\n");
    CHECK(invoke_dispatcher(si->dispatcher, cap_dispatcher, si->l1_cnode,
                            si->process_l1_pt, si->spawned_disp_memframe,
                            true));

    while (si->allocated_sects != NULL) {
        paging_unmap(get_current_paging_state(),
                     (void *) si->allocated_sects->base_addr);
        struct paging_frame_node *prev = si->allocated_sects;
        si->allocated_sects = si->allocated_sects->next;
        slab_free(&si->paging_state.slab_alloc, prev);
    }

    return SYS_ERR_OK;
}
