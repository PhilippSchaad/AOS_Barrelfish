#include <aos/aos.h>
#include <spawn/spawn.h>

#include <elf/elf.h>
#include <aos/dispatcher_arch.h>
#include <barrelfish_kpi/paging_arm_v7.h>
#include <barrelfish_kpi/domain_params.h>
#include <spawn/multiboot.h>

extern struct bootinfo *bi;

// TODO(M2): Implement this function such that it starts a new process
// TODO(M4): Build and pass a messaging channel to your child process
errval_t spawn_load_by_name(void * binary_name, struct spawninfo * si) {
    printf("spawn start_child: starting: %s\n", binary_name);

    errval_t err = SYS_ERR_OK;

    // Init spawninfo
    memset(si, 0, sizeof(*si));
    si->binary_name = binary_name;

    // I: Get the binary from the multiboot image
    struct mem_region *module = multiboot_find_module(bi, binary_name);
    if (module == NULL) {
        debug_printf("multiboot: Could not find module %s\n", binary_name);
        return SPAWN_ERR_FIND_MODULE;
    }

    // TODO: Implement me
    // - Get the binary from multiboot image
    // - Map multiboot module in your address space
    // - Setup childs cspace
    // - Setup childs vspace
    // - Load the ELF binary
    // - Setup dispatcher
    // - Setup environment
    // - Make dispatcher runnable

    return err;
}
