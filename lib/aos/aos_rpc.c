/**
 * \file
 * \brief Implementation of AOS rpc-like messaging
 */

/*
 * Copyright (c) 2013 - 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <aos/aos_rpc.h>

#undef DEBUG_LEVEL
#define DEBUG_LEVEL DETAILED

static errval_t rpc_receive_handler(void *args){
    DBG(DETAILED, "rpc_receive_handler\n");

    struct aos_rpc *rpc = (struct aos_rpc *) args;
    // Get the message from the child.
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;
    struct capref child_cap;
    errval_t err = lmp_chan_recv(&rpc->chan, &msg, &child_cap);
    // Regegister if failed.
    if (err_is_fail(err) && lmp_err_is_transient(err)) {
        CHECK(lmp_chan_register_recv(args, get_default_waitset(),
                                     MKCLOSURE((void *) rpc_receive_handler,
                                               args)));
        return err;
    }

    // do actions depending on the message type
    // Check the message type and handle it accordingly.
    switch (msg.words[0]) {
        case RPC_ACK_MESSAGE(RPC_TYPE_HANDSHAKE):
            DBG(DETAILED, "ACK Received (handshake) \n");
            rpc->init = true;
            break;
        case RPC_ACK_MESSAGE(RPC_TYPE_PUTCHAR):
            DBG(DETAILED, "ACK Received (putchar) \n");
            break;
        default:
            assert(!"NOT IMPLEMENTED");
            DBG(WARN, "Unable to handle RPC-receipt, expect badness!\n");
    }

    return SYS_ERR_OK;

}

errval_t aos_rpc_send_number(struct aos_rpc *chan, uintptr_t val)
{
    // TODO: implement functionality to send a number over the channel
    // given channel and wait until the ack gets returned.
    return SYS_ERR_OK;
}

static errval_t string_send_handler(uintptr_t *args)
{
    DBG(DETAILED, "string_send_handler (%d)\n", args[9]);

    struct aos_rpc *rpc = (struct aos_rpc *) args[0];

    //printf("send:\n");
    //for (uint8_t i=1; i<9; ++i){
    //    for (uint8_t j=0; j<4; ++j){
    //        printf("%c", (char) ((uint32_t) args[i] >> 8*j) );
    //   }
    //   printf("\n\n");
    //}

    errval_t err;
    err = lmp_chan_send9(&rpc->chan, LMP_FLAG_SYNC, NULL_CAP ,
                         RPC_MESSAGE((uint32_t) args[9]), args[1], args[2],args[3],args[4],args[5],args[6],args[7],args[8]);
    if (err_is_fail(err)){
        // Reregister if failed.
        CHECK(lmp_chan_register_send(&rpc->chan, get_default_waitset(),
                                     MKCLOSURE((void *) string_send_handler,
                                               args)));
        CHECK(event_dispatch(get_default_waitset()));
    }

    return SYS_ERR_OK;
}

errval_t aos_rpc_send_string(struct aos_rpc *rpc, const char *string)
{
    assert(rpc != NULL);
    // we probably have to split the string into smaller pieces
    // Reminder: string is null terminated ('\0')
    // we can send a max of 9 messages where we need one to identify the message type
    // so we can send 8 32 bit/4 char chunks at once

    struct waitset *ws = get_default_waitset();

    uint32_t args[10];
    args[0] = (uint32_t) rpc; // cast pointer to int
    args[9] = (uint32_t) RPC_TYPE_STRING;

    // get the length of the string (including the terminating 0)
    uint32_t len = strlen(string) + 1;

    uint8_t count = 0; // we init with 0 to be able to add 1 at the beginning

    // we send the size of the string in the first message
    args[++count] = len;
    for (uint32_t i=0; i<=len;++i){
        // we go through the array and pack 4 of them together
        if (i%4==0){
            // increase count
            // the first of 4 should reset the value
            args[++count] = string[i];
        } else {
            // shift and add the other 3 values
            args[count] += string[i] << ((i%4)*8);
        }
        // send if full
        if(count == 8 && i%4==3){
            errval_t err;
            do {
                // check if sender is currently busy
                err = lmp_chan_register_send(&rpc->chan, ws,
                                 MKCLOSURE((void *) string_send_handler,
                                           args));
                CHECK(event_dispatch(ws));
            } while(err == LIB_ERR_CHAN_ALREADY_REGISTERED);
            args[9] = (uint32_t) RPC_TYPE_STRING_DATA;

            // reset counter
            count = 0;
        }
    }

    // send the final chunk if needed
    if (len%32 != 0){
        errval_t err;
        do {
            // check if sender is currently busy
            err = lmp_chan_register_send(&rpc->chan, ws,
                             MKCLOSURE((void *) string_send_handler,
                                       args));
            CHECK(event_dispatch(ws));
        } while(err == LIB_ERR_CHAN_ALREADY_REGISTERED);
    }
    // and wait for a response.
    return SYS_ERR_OK;
}

errval_t aos_rpc_get_ram_cap(struct aos_rpc *chan, size_t size, size_t align,
                             struct capref *retcap, size_t *ret_size)
{
    // TODO: implement functionality to request a RAM capability over the
    // given channel and wait until it is delivered.
    return SYS_ERR_OK;
}

errval_t aos_rpc_serial_getchar(struct aos_rpc *chan, char *retc)
{
    // TODO implement functionality to request a character from
    // the serial driver.
    return SYS_ERR_OK;
}

static errval_t putchar_send_handler(uintptr_t *args)
{
    assert((char *) args[1] != NULL);
    DBG(DETAILED, "putchar_send_handler\n");

    struct aos_rpc *rpc = (struct aos_rpc *) args[0];
    DBG(DETAILED, "The arg is: %c\n", *(char *)args[1]);

    errval_t err;
    err = lmp_chan_send2(&rpc->chan, LMP_FLAG_SYNC, rpc->chan.local_cap,
                         RPC_MESSAGE(RPC_TYPE_PUTCHAR), *((char *)args[1]));
    if (err_is_fail(err)){
        // Reregister if failed.
        CHECK(lmp_chan_register_send(&rpc->chan, get_default_waitset(),
                                     MKCLOSURE((void *) putchar_send_handler,
                                               args)));
    }

    return SYS_ERR_OK;
}

errval_t aos_rpc_serial_putchar(struct aos_rpc *rpc, char c)
{
    assert(rpc != NULL);

    struct waitset *ws = get_default_waitset();

    uintptr_t sendargs[2];
    sendargs[0] = (uintptr_t) rpc;
    sendargs[1] = (uintptr_t) &c;

    errval_t err;
    do {
        // check if sender is currently busy
        // TODO: could we implement some kind of buffer for this?
        err = lmp_chan_register_send(&rpc->chan, ws,
                                 MKCLOSURE((void *) putchar_send_handler,
                                           sendargs));
        CHECK(event_dispatch(ws));
    } while(err == LIB_ERR_CHAN_ALREADY_REGISTERED);
    return SYS_ERR_OK;
}

errval_t aos_rpc_process_spawn(struct aos_rpc *chan, char *name,
                               coreid_t core, domainid_t *newpid)
{
    // TODO (milestone 5): implement spawn new process rpc
    return SYS_ERR_OK;
}

errval_t aos_rpc_process_get_name(struct aos_rpc *chan, domainid_t pid,
                                  char **name)
{
    // TODO (milestone 5): implement name lookup for process given a process
    // id
    return SYS_ERR_OK;
}

errval_t aos_rpc_process_get_all_pids(struct aos_rpc *chan,
                                      domainid_t **pids, size_t *pid_count)
{
    // TODO (milestone 5): implement process id discovery
    return SYS_ERR_OK;
}

errval_t aos_rpc_get_device_cap(struct aos_rpc *rpc,
                                lpaddr_t paddr, size_t bytes,
                                struct capref *frame)
{
    return LIB_ERR_NOT_IMPLEMENTED;
}



static errval_t handshake_send_handler(void* args)
{
    DBG(DETAILED, "handshake_send_handler\n");
    struct aos_rpc *rpc = (struct aos_rpc *) args;

    errval_t err;
    err = lmp_chan_send1(&rpc->chan, LMP_FLAG_SYNC, rpc->chan.local_cap,
                         RPC_MESSAGE(RPC_TYPE_HANDSHAKE));
    if (err_is_fail(err)){
        // Reregister if failed.
        CHECK(lmp_chan_register_send(&rpc->chan, get_default_waitset(),
                                     MKCLOSURE((void *) handshake_send_handler,
                                               args)));
    }
    return SYS_ERR_OK;
}

static errval_t rpc_handshake_helper(struct aos_rpc *rpc, struct capref dest)
{
    assert(rpc != NULL);
    rpc->init = false;
    struct waitset *ws = get_default_waitset();

    /* allocate lmp channel structure */
    /* create local endpoint */
    /* set remote endpoint to dest's endpoint */
    CHECK(lmp_chan_accept(&rpc->chan, DEFAULT_LMP_BUF_WORDS, cap_initep));
    /* set receive handler */
    CHECK(lmp_chan_register_recv(&rpc->chan, ws,
                                 MKCLOSURE((void*) rpc_receive_handler,
                                           rpc)));
    /* send local ep to init and wait for init to acknowledge receiving the endpoint */
    /* set send handler */
    CHECK(lmp_chan_register_send(&rpc->chan, ws,
                                 MKCLOSURE((void *)handshake_send_handler,
                                           rpc)));

    /* wait for ACK */
    while (!rpc->init){
        CHECK(event_dispatch(ws));
    }
    return SYS_ERR_OK;
}

errval_t aos_rpc_init(struct aos_rpc *rpc)
{
    CHECK(rpc_handshake_helper(rpc, cap_initep));
    // store rpc
    set_init_rpc(rpc);
    return SYS_ERR_OK;
}

/**
 * \brief Returns the RPC channel to init.
 */
struct aos_rpc *aos_rpc_get_init_channel(void)
{
    //TODO check if was created
    return get_init_rpc();
}

/**
 * \brief Returns the channel to the memory server
 */
struct aos_rpc *aos_rpc_get_memory_channel(void)
{
    // TODO check if we want to create a separate mem server
    return get_init_rpc();
}

/**
 * \brief Returns the channel to the process manager
 */
struct aos_rpc *aos_rpc_get_process_channel(void)
{
    // TODO check if we want to create a separate process server
    return get_init_rpc();
}

/**
 * \brief Returns the channel to the serial console
 */
struct aos_rpc *aos_rpc_get_serial_channel(void)
{
    // TODO check if we want to create a separate serial server
    return get_init_rpc();
}
