#include "lib_multicore.h"

static errval_t core_allocate_memory(void) {
    size_t retsize;

    // I: Allocate a new KCB.
    struct capref kcb_ram, kcb_frame;
    CHECK(ram_alloc(&kcb_ram, OBJSIZE_KCB));
    CHECK(slot_alloc(&kcb_frame));
    CHECK(cap_retype(kcb_frame, kcb_ram, 0, ObjType_KernelControlBlock,
                     OBJSIZE_KCB, 1));

    // II: Allocate coreboot's core_data struct.
    struct capref core_data_memframe;
    struct arm_core_data *core_data;
    CHECK(frame_alloc(&core_data_memframe, OBJSIZE_KCB, &retsize));
    CHECK(invoke_kcb_clone(kcb_frame, core_data_memframe));
    CHECK(paging_map_frame(get_current_paging_state(), (void *) &core_data,
                           retsize, core_data_memframe, NULL, NULL));

    // III: Allocate memory to load coreboot's relocatable segment. TODO

    // IV: Allocate space to load the init process.
    struct capref init_frame;
    struct frame_identity init_frame_identity;
    CHECK(frame_alloc(&init_frame, ARM_CORE_DATA_PAGES * BASE_PAGE_SIZE,
                      &retsize));
    CHECK(frame_identify(init_frame, &init_frame_identity));
    core_data->memory_base_start = init_frame_identity.base;
    core_data->memory_bytes = init_frame_identity.bytes;

    // V: Allocate an URPC frame. TODO

    return LIB_ERR_NOT_IMPLEMENTED;
}

static errval_t load_and_relocate_driver(void)
{
    return LIB_ERR_NOT_IMPLEMENTED;
}

static errval_t fill_core_data(void) {
    return LIB_ERR_NOT_IMPLEMENTED;
}

static errval_t clean_cache(void) {
    return LIB_ERR_NOT_IMPLEMENTED;
}

static errval_t invoke_kernel_cap(void) {
    return LIB_ERR_NOT_IMPLEMENTED;
}

errval_t wake_core(coreid_t core_id, coreid_t current_core_id)
{
    // If the core id is 0, this core is already up and does not need to be
    // woken up.
    assert(core_id != 0);

    // If the core id is our own current core id, we are obviously already
    // awake too.
    assert(core_id != current_core_id);

    CHECK(core_allocate_memory());

    CHECK(load_and_relocate_driver());

    CHECK(fill_core_data());

    CHECK(clean_cache());

    CHECK(invoke_kernel_cap());

    return LIB_ERR_NOT_IMPLEMENTED;
}
