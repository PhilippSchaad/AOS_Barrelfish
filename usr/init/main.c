/**
 * \file
 * \brief init process for child spawning
 */

/*
 * Copyright (c) 2007, 2008, 2009, 2010, 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/morecore.h>
#include <aos/paging.h>
#include <aos/waitset.h>

#include <barrelfish_kpi/asm_inlines_arch.h>

#include <mm/mm.h>
#include <spawn/spawn.h>

#include <lib_multicore.h>
#include <lib_rpc.h>
#include <lib_urpc.h>
#include <lib_procman.h>
#include <lib_terminal.h>
#include <mem_alloc.h>

#include "../tests/test.h"
#include "../networking/slip.h"

//#define PERF_MEASUREMENT
#define NDTESTS

coreid_t my_core_id;
struct bootinfo *bi;

int main(int argc, char *argv[])
{
    errval_t err;

    // Set the core id in the disp_priv struct.
    err = invoke_kernel_get_core_id(cap_kernel, &my_core_id);
    assert(err_is_ok(err));
    disp_set_core_id(my_core_id);

#if DEBUG_LEVEL >= VERBOSE
    debug_printf("init: on core %" PRIuCOREID " invoked as:", my_core_id);
    for (int i = 0; i < argc; i++) {
        printf(" %s", argv[i]);
    }
    printf("\n");
#endif

    // First argument contains the bootinfo location, if it's not set.
    if (my_core_id == 0) {
        bi = (struct bootinfo *) strtol(argv[1], NULL, 10);
        if (!bi) {
            assert(my_core_id > 0);
        }
    }

    if (my_core_id == 0) {
        CHECK(initialize_ram_alloc(NULL));
    }

    // init urpc channel to 2nd core
    void *buf;
    if (my_core_id == 0) {
        CHECK(create_urpc_frame(&buf, BASE_PAGE_SIZE));
        memset(buf, 0, BASE_PAGE_SIZE);

        // wake up 2nd core
        CHECK(wake_core(1, my_core_id, bi));
    }

    // create the init ep
    CHECK(cap_retype(cap_selfep, cap_dispatcher, 0, ObjType_EndPoint, 0, 1));

    // Structure to keep track of domains.
    active_domains = malloc(sizeof(struct domain_list));

    init_rpc();

    CHECK(procman_init());

    // Initialize the terminal driver.
    terminal_init(my_core_id);
    debug_printf("t driver online\n");


    if (my_core_id == 0) {
        // Initialize the master URPC server (aka core 0).
        urpc_master_init_and_run(buf);

        struct spawninfo *si_ns =
                (struct spawninfo *) malloc(sizeof(struct spawninfo));
        spawn_load_by_name("nameserver",si_ns);
        debug_printf("spawned ns\n");
        while(!nameserver_online()) event_dispatch(get_default_waitset());
        debug_printf("received nameserver registration\n");

        // Spawn the TurtleBack Shell.
        struct spawninfo *si = malloc(sizeof(struct spawninfo));
        CHECK(spawn_load_by_name("turtleback", si));
        debug_printf("turtleback online\n");
    } else {
        // Register ourselves as a slave server on the URPC master server.
        debug_printf("pre this\n");
        urpc_slave_init_and_run();
        debug_printf("post this and pre that\n");

        // Wait until we have received the URPC telling us to initialiize our
        // ram allocator and have done so successfully.
        while (!urpc_ram_is_initalized())
            ;

        debug_printf("post that\n");
        // Register ourselves (init on core 1) with the process manager.
        procman_register_process("init", 1, NULL);

        struct spawninfo *si_ns =
                (struct spawninfo *) malloc(sizeof(struct spawninfo));
        spawn_load_by_name("nameserver",si_ns);
        debug_printf("spawned ns\n");
        while(!nameserver_online()) event_dispatch(get_default_waitset());
        debug_printf("received nameserver registration\n");
    }
    debug_printf("all systems operational\n");

    // Hang around
    struct waitset *default_ws = get_default_waitset();
    while (true) {
        err = event_dispatch(default_ws);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }

    free(active_domains);

    return EXIT_SUCCESS;
}
