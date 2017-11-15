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

//#undef DEBUG_LEVEL
//#define DEBUG_LEVEL 100

errval_t aos_rpc_send_number(struct aos_rpc *chan, uintptr_t val)
{
    DBG(VERBOSE, "rpc_send_number\n");

    uintptr_t *sendargs =malloc(1* sizeof(uintptr_t));

    sendargs[0] = (uintptr_t) val;
    CHECK(send(&chan->chan,NULL_CAP,RPC_MESSAGE(RPC_TYPE_NUMBER),1,sendargs,MKCLOSURE(free,sendargs),request_fresh_id(RPC_MESSAGE(RPC_TYPE_NUMBER))));
    CHECK(event_dispatch(get_default_waitset()));
    return SYS_ERR_OK;
}
errval_t aos_rpc_send_string(struct aos_rpc *rpc, const char *string)
{
    CHECK(persist_send_cleanup_wrapper(&rpc->chan,NULL_CAP,RPC_MESSAGE(RPC_TYPE_STRING),strlen(string),(void*)string,NULL_EVENT_CLOSURE,request_fresh_id(RPC_MESSAGE(RPC_TYPE_STRING))));
    CHECK(event_dispatch(get_default_waitset()));
    return SYS_ERR_OK;
}

static errval_t ram_send_handler(uintptr_t *args)
{
    DBG(DETAILED, "rpc_ram_send_handler\n");

    struct aos_rpc *rpc = (struct aos_rpc *) args[0];
    size_t size = *((size_t *) args[1]);
    size_t align = *((size_t *) args[2]);

    DBG(DETAILED, "The size is %zu and alignment is %zu\n", size, align);
    //dirty hacks
    unsigned int first_byte = (RPC_MESSAGE(RPC_TYPE_RAM) << 24) + (0 << 16) + 2;
    //end dirty hacks
    errval_t err;
    do {
        DBG(DETAILED, "calling lmp_chan_send3 in rpc_ram_send_handler\n");
        // Check if sender is currently busy
        // TODO: could we implement some kind of buffer for this?
        err = lmp_chan_send3(&rpc->chan, LMP_FLAG_SYNC, NULL_CAP,
                             first_byte, size, align);
    } while (err == LIB_ERR_CHAN_ALREADY_REGISTERED);
    if(!err_is_ok(err))
        debug_printf("tried to send ram request, ran into issue: %s\n",err_getstring(err));

    return SYS_ERR_OK;
}
//ugly hack until we have a proper system for associating sends with recvs
static struct capref ram_cap;
static size_t ram_size;
//end ugly hack until we have a proper system for associating sends with recvs

errval_t aos_rpc_get_ram_cap(struct aos_rpc *chan, size_t size, size_t align,
                             struct capref *retcap, size_t *ret_size)
{
    thread_mutex_lock_nested(&chan->mutex);
    DBG(-1, "rpc_get_ram_cap\n");

    struct waitset *ws = get_default_waitset();

    uintptr_t sendargs[3];
    DBG(VERBOSE, "rpc_get_ram_cap 2\n");

    sendargs[0] = (uintptr_t) chan;
    sendargs[1] = (uintptr_t) &size;
    sendargs[2] = (uintptr_t) &align;
    DBG(VERBOSE, "rpc_get_ram_cap 3\n");

//    CHECK(lmp_chan_alloc_recv_slot(&chan->chan));
    DBG(-1, "rpc_get_ram_cap 4\n");
    ram_size = 0;

    errval_t err = lmp_chan_register_send(&chan->chan, ws,
                                 MKCLOSURE((void *) ram_send_handler,
                                           sendargs));
    while(err == LIB_ERR_CHAN_ALREADY_REGISTERED) {
        CHECK(event_dispatch(ws));
        err = lmp_chan_register_send(&chan->chan, ws,
                                     MKCLOSURE((void *) ram_send_handler,
                                               sendargs));
    }
    DBG(-1, "rpc_get_ram_cap 5\n");
    CHECK(err);


    DBG(-1, "rpc_get_ram_cap 6\n");


    while(ram_size == 0) event_dispatch(ws); //kind of an ugly hack
    *retcap = ram_cap;
    *ret_size = ram_size;
    ram_size = 0;

    thread_mutex_unlock(&chan->mutex);
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
    DBG(VERBOSE, "putchar_send_handler\n");

    struct aos_rpc *rpc = (struct aos_rpc *) args[0];

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
    } while (err == LIB_ERR_CHAN_ALREADY_REGISTERED);

    return SYS_ERR_OK;
}

send_handler_0(process_get_pids_send_handler,
               RPC_MESSAGE(RPC_TYPE_PROCESS_GET_PIDS))
send_handler_1(process_get_name_send_handler,
               RPC_MESSAGE(RPC_TYPE_PROCESS_GET_NAME), args[1])

static errval_t kill_me_send_handler(void *args)
{
    DBG(VERBOSE, "kill_me_send_handler\n");

    uintptr_t *uargs = (uintptr_t *) args;

    struct aos_rpc *rpc = (struct aos_rpc *) uargs[0];
    struct capref *cap = (struct capref *) uargs[1];

    errval_t err;
    err = lmp_chan_send1(&rpc->chan, LMP_FLAG_SYNC, *cap,
                         RPC_MESSAGE(RPC_TYPE_PROCESS_KILL_ME));
    if (err_is_fail(err)){
        // Reregister if failed.
        CHECK(lmp_chan_register_send(&rpc->chan, get_default_waitset(),
                                     MKCLOSURE((void *) kill_me_send_handler,
                                               args)));
    }
    thread_mutex_unlock(&rpc->mutex);

    return SYS_ERR_OK;
}

errval_t aos_rpc_kill_me(struct aos_rpc *chan, struct capref disp)
{
    DBG(VERBOSE, "aos_rpc_kill_me\n");
    thread_mutex_lock(&chan->mutex);

    struct waitset *ws = get_default_waitset();

    uintptr_t sendargs[2];

    sendargs[0] = (uintptr_t) chan;
    sendargs[1] = (uintptr_t) &disp;

    CHECK(lmp_chan_register_send(&chan->chan, ws,
                                 MKCLOSURE((void *) kill_me_send_handler,
                                           sendargs)));

    CHECK(event_dispatch(ws));

    return SYS_ERR_OK;
}

static errval_t process_spawn_receive_handler(uintptr_t* args) {
    recv_handler_fleshy_bits(process_spawn_receive_handler,
                             -1, RPC_TYPE_PROCESS_SPAWN);
    args[8] = msg.words[1];
    return SYS_ERR_OK;
}

send_handler_8(process_spawn_send_handler, RPC_MESSAGE(RPC_TYPE_PROCESS_SPAWN),
               args[1], args[2], args[3], args[4], args[5], args[6], args[7],
               args[8]);

errval_t aos_rpc_process_spawn(struct aos_rpc *chan, char *name,
                               coreid_t core, domainid_t *newpid)
{
    return SYS_ERR_OK; //todo: implement this;
    struct waitset *ws = get_default_waitset();

    uintptr_t sendargs[9]; // 1+1

    sendargs[0] = (uintptr_t) chan;

    size_t totalcount = strlen(name);
    sendargs[1] = (uintptr_t)totalcount;

    if (totalcount > 6 * 4)
        return LRPC_ERR_WRONG_WORDCOUNT;

    for (int i = 0; i < totalcount; i++) {
        sendargs[(i >> 2) + 2] = (i % 4 ? sendargs[(i >> 2) + 2] : 0) +
            (name[i] << (8 * (i % 4)));
    }

    impl(process_spawn_send_handler, process_spawn_receive_handler, true);
    *newpid = (domainid_t) sendargs[8];

    return SYS_ERR_OK;
}

struct pgnrp_buf {
    size_t cur;
    size_t totalcount;
    char* name;
};

static errval_t process_get_name_receive_handler(uintptr_t *args) {
    recv_handler_fleshy_bits(process_get_name_receive_handler, -1,
                             RPC_TYPE_PROCESS_GET_NAME);

    int totalcount = (int) msg.words[1];
    char* temp = malloc(sizeof(char) * (totalcount + 1));

    if (totalcount > 4 * 6)
        return LRPC_ERR_WRONG_WORDCOUNT;

    for (int i = 0; i < totalcount; i++) {
        char *c = (char*) &(msg.words[(i >> 2) + 2]);
        temp[i] = c[i % 4];
    }

    args[2] = (uintptr_t) temp;
    return SYS_ERR_OK;
}

errval_t aos_rpc_process_get_name(struct aos_rpc *chan, domainid_t pid,
                                  char **name)
{
    struct waitset *ws = get_default_waitset();
    uintptr_t sendargs[3]; // 1 to send, 1 to receive
    sendargs[0] = (uintptr_t) chan;
    sendargs[1] = (uintptr_t) pid;

    impl(process_get_name_send_handler, process_get_name_receive_handler,
         true);

    *name = (char*) sendargs[2];

    return SYS_ERR_OK;
}

static errval_t process_get_pids_receive_handler(uintptr_t *args) {
    recv_handler_fleshy_bits(process_get_pids_receive_handler, -1,
                             RPC_TYPE_PROCESS_GET_PIDS);
    return LIB_ERR_NOT_IMPLEMENTED;
}

errval_t aos_rpc_process_get_all_pids(struct aos_rpc *chan,
                                      domainid_t **pids, size_t *pid_count)
{
    struct waitset *ws = get_default_waitset();
    uintptr_t sendargs[3]; // 0+2
    sendargs[0] = (uintptr_t) chan;

    impl(process_get_pids_send_handler, process_get_pids_receive_handler,
         true);

    *pid_count = (size_t) sendargs[1];
    *pids = (domainid_t *) sendargs[2];

    return LIB_ERR_NOT_IMPLEMENTED;
}

errval_t aos_rpc_get_device_cap(struct aos_rpc *rpc,
                                lpaddr_t paddr, size_t bytes,
                                struct capref *frame)
{
    return LIB_ERR_NOT_IMPLEMENTED;
}


static void aos_rpc_recv_handler(struct recv_list* data) {
    // do actions depending on the message type
    // Check the message type and handle it accordingly.
    switch (data->type) {
        case RPC_ACK_MESSAGE(RPC_TYPE_HANDSHAKE):
            DBG(-1, "ACK Received (handshake) \n");
            data->chan->remote_cap = data->cap;
            get_init_rpc()->init = true;
            break;
        case RPC_ACK_MESSAGE(RPC_TYPE_PUTCHAR):
            DBG(DETAILED, "ACK Received (putchar) \n");
            break;
        case RPC_ACK_MESSAGE(RPC_TYPE_RAM):
            // TODO: Handle in non-hacky way
            ram_cap = data->cap;
            ram_size = data->size;
            break;
        case RPC_ACK_MESSAGE(RPC_TYPE_NUMBER):
            DBG(DETAILED, "ACK Received (number)\n");
            break;
        case RPC_ACK_MESSAGE(RPC_TYPE_STRING):
            DBG(DETAILED, "ACK Receied (string)\n");
            break;
        default:
            debug_printf("got message type %d\n", data->type);
            DBG(WARN, "Unable to handle RPC-receipt, expect badness!\n");
            //assert(!"NOT IMPLEMENTED");
    }
}

unsigned int id = 1337;
errval_t aos_rpc_init(struct aos_rpc *rpc)
{
    assert(rpc != NULL);
    rpc->init = false;
    CHECK(init_rpc_client(aos_rpc_recv_handler,&rpc->chan, cap_initep));
    // Send local ep to init and wait for init to acknowledge receiving
    // the endpoint.
    // Set send handler.
    struct waitset* ws = get_default_waitset();
    CHECK(send(&rpc->chan,rpc->chan.local_cap,RPC_MESSAGE(RPC_TYPE_HANDSHAKE),0,NULL,NULL_EVENT_CLOSURE,request_fresh_id(RPC_MESSAGE(RPC_TYPE_HANDSHAKE))));
    rpc->id = id;
    id++;
    set_init_rpc(rpc);
    // Wait for ACK.
    while (!rpc->init) {
        CHECK(event_dispatch(ws));
    }
    debug_printf("yes\n");

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
