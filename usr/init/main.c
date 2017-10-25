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
#include <aos/waitset.h>
#include <aos/morecore.h>
#include <aos/paging.h>

#include <mm/mm.h>
#include "mem_alloc.h"
#include <spawn/spawn.h>


#include "../tests/test.h"

#undef DEBUG_LEVEL
#define DEBUG_LEVEL DETAILED

coreid_t my_core_id;
struct bootinfo *bi;

static errval_t handshake_send_handler(void *arg){
    DBG(DETAILED, "init sends ACK\n");
    struct lmp_chan* chan =(struct lmp_chan*) arg;
    CHECK(lmp_chan_send1(chan, LMP_FLAG_SYNC, NULL_CAP, RPC_TYPE_ACK));
    return SYS_ERR_OK;
}

static errval_t handshake_recv_handler(void *arg, struct capref *child_cap){
    DBG(DETAILED, "init received cap\n");

    struct lmp_chan* chan =(struct lmp_chan*) arg;

    // set the remote cap we just got from the child
    chan->remote_cap = *child_cap;

    // send ACK to the child
    CHECK(lmp_chan_register_send(chan, get_default_waitset(),
                                 MKCLOSURE((void *)handshake_send_handler,
                                           chan)));

    DBG(DETAILED, "successfully received cap\n");
    return SYS_ERR_OK;
}

static errval_t general_recv_handler(void *args)
{
    DBG(VERBOSE, "Handling RPC-receipt\n");

    struct lmp_chan *chan = (struct lmp_chan *) args;
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;
    struct capref cap;

    CHECK(lmp_chan_recv(chan, &msg, &cap));
    CHECK(lmp_chan_alloc_recv_slot(chan));
    CHECK(lmp_chan_register_recv(chan, get_default_waitset(),
                                 MKCLOSURE((void *) general_recv_handler,
                                           args)));

    assert(msg.buf.msglen > 0);

    // Check the message type and handle it accordingly.
    switch (msg.words[0]) {
        case RPC_TYPE_HANDSHAKE:
            handshake_recv_handler(args, &cap);
            break;
        default:
            DBG(WARN, "Unable to handle RPC-receipt, expect badness!\n");
            // TODO: Maybe return an error instead of continuing?
    }

    return SYS_ERR_OK;
}

int main(int argc, char *argv[])
{
    errval_t err;

    /* Set the core id in the disp_priv struct */
    err = invoke_kernel_get_core_id(cap_kernel, &my_core_id);
    assert(err_is_ok(err));
    disp_set_core_id(my_core_id);

    debug_printf("init: on core %" PRIuCOREID " invoked as:", my_core_id);
    for (int i = 0; i < argc; i++) {
       printf(" %s", argv[i]);
    }
    printf("\n");

    /* First argument contains the bootinfo location, if it's not set */
    bi = (struct bootinfo*)strtol(argv[1], NULL, 10);
    if (!bi) {
        assert(my_core_id > 0);
    }

    err = initialize_ram_alloc();
    if(err_is_fail(err)){
        DEBUG_ERR(err, "initialize_ram_alloc");
    }

    // create the init ep
    CHECK(cap_retype(cap_selfep, cap_dispatcher,0, ObjType_EndPoint, 0, 1));

    // create channel to receive child eps
    struct lmp_chan chan;
    CHECK(lmp_chan_accept(&chan, DEFAULT_LMP_BUF_WORDS, NULL_CAP));
    CHECK(lmp_chan_alloc_recv_slot(&chan));
    CHECK(cap_copy(cap_initep, chan.local_cap));
    CHECK(lmp_chan_register_recv(&chan, get_default_waitset(),
                                 MKCLOSURE((void *)general_recv_handler,
                                           &chan)));

    // run tests
    struct tester t;
    init_testing(&t);
    register_memory_tests(&t);
    register_spawn_tests(&t);
    tests_run(&t);

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

    return EXIT_SUCCESS;
}
