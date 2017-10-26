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

static errval_t handshake_send_handler(void *args)
{
    DBG(DETAILED, "init sends ACK\n");
    struct lmp_chan* chan =(struct lmp_chan*) args;
    CHECK(lmp_chan_send1(chan, LMP_FLAG_SYNC, NULL_CAP, RPC_ACK_MESSAGE(RPC_TYPE_HANDSHAKE)));
    return SYS_ERR_OK;
}

static errval_t number_recv_handler(void *args, struct lmp_recv_msg *msg,
                                    struct capref *cap)
{
    // TODO: Implement.
    return LIB_ERR_NOT_IMPLEMENTED;
}

char* string_recv_buff = NULL;
static errval_t string_recv_handler(void *args, struct lmp_recv_msg *msg,
                                    struct capref *cap)
{
    DBG(DETAILED, "receive string\n");
    char current_char;

    uint32_t count=0;

    if(string_recv_buff != NULL){
        //debug_printf("find count\n");
        assert(msg->words[0] == RPC_MESSAGE(RPC_TYPE_STRING_DATA));
        // find the counter
        while(true){
            if((uint8_t) string_recv_buff[count] == 0){
                break;
            }
            ++count;
        }
    } else {
        assert(msg->words[0] == RPC_MESSAGE(RPC_TYPE_STRING));
    }

    // we might receive a splitted message
    for (uint8_t i=1; i<9; ++i){
        if (i==1 && msg->words[0] == RPC_MESSAGE(RPC_TYPE_STRING)){
            // allocate memory for the string
            string_recv_buff = malloc((uint32_t) msg->words[i] * sizeof(char));
            memset(string_recv_buff, 0, msg->words[i] * sizeof(char));
            count=0;
            //printf("str with size %d allocated at 0x%p\n", (uint32_t) msg->words[i], string_recv_buff);
            continue;
        }
        for (uint8_t j=0; j<4; ++j){
            current_char = (char) ((uint32_t) msg->words[i] >> 8*j);
            if(current_char == '\0'){
                // we reached the end of the string
                // TODO: do something with the string
                printf("%s\n", string_recv_buff);
                free(string_recv_buff);
                string_recv_buff=NULL;
                return SYS_ERR_OK;
            }
            string_recv_buff[count++] = current_char;
        }
    }
    return SYS_ERR_OK;
}

static errval_t ram_recv_handler(void *args, struct lmp_recv_msg *msg,
                                 struct capref *cap)
{
    // TODO: Implement.
    return LIB_ERR_NOT_IMPLEMENTED;
}

static errval_t putchar_recv_handler(void *args, struct lmp_recv_msg *msg,
                                     struct capref *cap)
{
    DBG(DETAILED, "putchar request received\n");

    // Print the character.
    debug_printf("%c", (char) msg->words[1]);

    return SYS_ERR_OK;
}

static errval_t handshake_recv_handler(void *args, struct capref *child_cap)
{
    DBG(DETAILED, "init received cap\n");

    struct lmp_chan* chan =(struct lmp_chan*) args;

    // set the remote cap we just got from the child
    chan->remote_cap = *child_cap;

    // Send ACK to the child
    CHECK(lmp_chan_register_send(chan, get_default_waitset(),
                                 MKCLOSURE((void *)handshake_send_handler,
                                           chan)));

    DBG(DETAILED, "successfully received cap\n");
    return SYS_ERR_OK;
}

static errval_t general_recv_handler(void *args)
{

    struct lmp_chan *chan = (struct lmp_chan *) args;
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;
    struct capref cap;

    CHECK(lmp_chan_recv(chan, &msg, &cap));
    CHECK(lmp_chan_alloc_recv_slot(chan));
    CHECK(lmp_chan_register_recv(chan, get_default_waitset(),
                                 MKCLOSURE((void *) general_recv_handler,
                                           args)));

    assert(msg.buf.msglen > 0);
    DBG(VERBOSE, "Handling RPC receive event (type %d)\n", msg.words[0]);

    // Check the message type and handle it accordingly.
    switch (msg.words[0]) {
        case RPC_MESSAGE(RPC_TYPE_NUMBER):
            CHECK(number_recv_handler(args, &msg, &cap));
            break;
        case RPC_MESSAGE(RPC_TYPE_STRING):
            CHECK(string_recv_handler(args, &msg, &cap));
            break;
        case RPC_MESSAGE(RPC_TYPE_STRING_DATA):
            CHECK(string_recv_handler(args, &msg, &cap));
            break;
        case RPC_MESSAGE(RPC_TYPE_RAM):
            CHECK(ram_recv_handler(args, &msg, &cap));
            break;
        case RPC_MESSAGE(RPC_TYPE_PUTCHAR):
            CHECK(putchar_recv_handler(args, &msg, &cap));
            break;
        case RPC_MESSAGE(RPC_TYPE_HANDSHAKE):
            CHECK(handshake_recv_handler(args, &cap));
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
