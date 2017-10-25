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

errval_t aos_rpc_send_number(struct aos_rpc *chan, uintptr_t val)
{
    // TODO: implement functionality to send a number ofer the channel
    // given channel and wait until the ack gets returned.
    return SYS_ERR_OK;
}

errval_t aos_rpc_send_string(struct aos_rpc *chan, const char *string)
{
    // TODO: implement functionality to send a string over the given channel
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

static errval_t putchar_recv_handler(void *args)
{
    DBG(DETAILED, "putchar_recv_handler\n");

    struct aos_rpc *rpc = (struct aos_rpc *) args;
    // Get the message from the child.
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;
    struct capref child_cap;
    errval_t err = lmp_chan_recv(&rpc->chan, &msg, &child_cap);
    // Regegister if failed.
    if (err_is_fail(err) && lmp_err_is_transient(err)) {
        CHECK(lmp_chan_register_recv(args, get_default_waitset(),
                                     MKCLOSURE((void *) putchar_recv_handler,
                                               args)));
    }

    assert(msg.words[0] == RPC_TYPE_ACK);

    DBG(DETAILED, "ACK Received\n");

    rpc->char_put = true;

    return SYS_ERR_OK;
}

static errval_t putchar_send_handler(uintptr_t *args)
{
    assert((char *) args[1] != NULL);
    DBG(DETAILED, "putchar_send_handler\n");

    struct aos_rpc *rpc = (struct aos_rpc *) args;
    printf("The arg is: %c\n", *(char *)args[1]);

    errval_t err;
    printf("hi\n");
    // XXX: Can't get past here!
    err = lmp_chan_send2(&rpc->chan, LMP_FLAG_SYNC, rpc->chan.local_cap,
                         RPC_TYPE_PUTCHAR, *((char *)args[1]));
    printf("hi\n");
    if (err_is_fail(err)){
        // Reregister if failed.
        CHECK(lmp_chan_register_send(&rpc->chan, get_default_waitset(),
                                     MKCLOSURE((void *) putchar_send_handler,
                                               args)));
    }

    return SYS_ERR_OK;
}

errval_t aos_rpc_serial_putchar(struct aos_rpc *chan, char c)
{
    assert(chan != NULL);
    chan->char_put = false;

    struct waitset *ws = get_default_waitset();

    CHECK(lmp_chan_register_recv(&chan->chan, ws,
                                 MKCLOSURE((void*) putchar_recv_handler,
                                           chan)));
    uintptr_t sendargs[2];
    sendargs[0] = (uintptr_t) &chan;
    sendargs[1] = (uintptr_t) &c;
    CHECK(lmp_chan_register_send(&chan->chan, ws,
                                 MKCLOSURE((void *) putchar_send_handler,
                                           sendargs)));

    while (!chan->char_put) {
        CHECK(event_dispatch(ws));
    }

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


static errval_t handshake_recv_handler(void* args)
{
    DBG(DETAILED, "handshake_recv_handler\n");

    struct aos_rpc *rpc = (struct aos_rpc *) args;
    // Get the message from the child.
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;
    struct capref child_cap;
    errval_t err = lmp_chan_recv(&rpc->chan, &msg, &child_cap);
    // Regegister if failed.
    if (err_is_fail(err) && lmp_err_is_transient(err)) {
        CHECK(lmp_chan_register_recv(args, get_default_waitset(),
                                     MKCLOSURE((void *) handshake_recv_handler,
                                               args)));
    }

    assert(msg.words[0] == RPC_TYPE_ACK);

    DBG(DETAILED, "ACK Received\n");

    rpc->init = true;
    return SYS_ERR_OK;
}

static errval_t handshake_send_handler(void* args)
{
    DBG(DETAILED, "handshake_send_handler\n");
    struct aos_rpc *rpc = (struct aos_rpc *) args;

    errval_t err;
    err = lmp_chan_send1(&rpc->chan, LMP_FLAG_SYNC, rpc->chan.local_cap,
                         RPC_TYPE_HANDSHAKE);
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
                                 MKCLOSURE((void*) handshake_recv_handler,
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
    return NULL;
}

/**
 * \brief Returns the channel to the process manager
 */
struct aos_rpc *aos_rpc_get_process_channel(void)
{
    return NULL;
}

/**
 * \brief Returns the channel to the serial console
 */
struct aos_rpc *aos_rpc_get_serial_channel(void)
{
    return NULL;
}
