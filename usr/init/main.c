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

#include <mm/mm.h>
#include <spawn/spawn.h>

#include <lib_multicore.h>
#include <lib_rpc.h>
#include <mem_alloc.h>

#include "../tests/test.h"

coreid_t my_core_id;
struct bootinfo *bi;

int main(int argc, char *argv[])
{
    errval_t err;

    // Set the core id in the disp_priv struct.
    err = invoke_kernel_get_core_id(cap_kernel, &my_core_id);
    assert(err_is_ok(err));
    disp_set_core_id(my_core_id);

    debug_printf("init: on core %" PRIuCOREID " invoked as:", my_core_id);
    for (int i = 0; i < argc; i++) {
        printf(" %s", argv[i]);
    }
    printf("\n");

    // First argument contains the bootinfo location, if it's not set.
    bi = (struct bootinfo *) strtol(argv[1], NULL, 10);
    if (!bi) {
        assert(my_core_id > 0);
    }

    err = initialize_ram_alloc();
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "initialize_ram_alloc");
    }

    // init urpc channel to 2nd core
    void *buf;
    CHECK(create_urpc_frame(&buf, BASE_PAGE_SIZE));

    // wake up 2nd core
    CHECK(wake_core(1, my_core_id, bi));

    // create the init ep
    CHECK(cap_retype(cap_selfep, cap_dispatcher, 0, ObjType_EndPoint, 0, 1));

    // Structure to keep track of domains.
    active_domains = malloc(sizeof(struct domain_list));

    // create channel to receive child eps
    struct lmp_chan chan;
    CHECK(lmp_chan_accept(&chan, DEFAULT_LMP_BUF_WORDS, NULL_CAP));
    CHECK(lmp_chan_alloc_recv_slot(&chan));
    CHECK(cap_copy(cap_initep, chan.local_cap));
    CHECK(lmp_chan_register_recv(
        &chan, get_default_waitset(),
        MKCLOSURE((void *) general_recv_handler, &chan)));

    /*
    if (my_core_id == 0) {
        // run tests
        struct tester t;
        init_testing(&t);
        register_memory_tests(&t);
        register_spawn_tests(&t);
        tests_run(&t);
    } else {
        debug_printf("I am the other core\n");
    }
    */

    debug_printf("Message handler loop\n");
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
