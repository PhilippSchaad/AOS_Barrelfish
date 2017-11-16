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
#include <aos/generic_threadsafe_queue.h>

struct thread_mutex aos_rpc_mutex;

struct rpc_call {
    unsigned char id;
    unsigned char type;
    bool *done;
    void (*recv_handling)(void *arg1, struct recv_list *data);
    void *arg1;
    struct rpc_call *next;
};

struct rpc_call *rpc_call_buffer_head = NULL;

bool rpc_type_ram_call_buffer_in_use[10];
struct rpc_call rpc_type_ram_call_buffer[10];

static void register_recv(unsigned char id, unsigned char type, bool *done,
                          void (*inst_recv_handling)(void *arg1,
                                                     struct recv_list *data),
                          void *recv_handling_arg1)
{
    struct rpc_call *call = NULL;
    if (type == RPC_ACK_MESSAGE(RPC_TYPE_RAM)) {
        synchronized(aos_rpc_mutex)
        {
            for (int i = 0; i < 10; i++) {
                if (!rpc_type_ram_call_buffer_in_use[i]) {
                    rpc_type_ram_call_buffer_in_use[i] = true;
                    call = &rpc_type_ram_call_buffer[i];
                    break;
                }
            }
            if (call == NULL)
                DBG(ERR, "we ran out of empty rpc_type_ram_call_buffer, this "
                         "should never happen");
        }
    } else {
        call = malloc(sizeof(struct rpc_call));
    }
    call->id = id;
    call->type = type;
    call->done = done;
    call->recv_handling = inst_recv_handling;
    call->arg1 = recv_handling_arg1;

    // TODO: consider that this might not need a mutex
    synchronized(aos_rpc_mutex)
    {
        call->next = rpc_call_buffer_head;
        rpc_call_buffer_head = call;
    }
}

static void
rpc_framework(void (*inst_recv_handling)(void *arg1, struct recv_list *data),
              void *recv_handling_arg1, unsigned char type,
              struct lmp_chan *chan, struct capref cap, size_t payloadsize,
              uintptr_t *payload, struct event_closure callback_when_done)
{
    unsigned char id = request_fresh_id(RPC_MESSAGE(type));

    bool done = false;
    register_recv(id, RPC_ACK_MESSAGE(type), &done, inst_recv_handling,
                  recv_handling_arg1);
    send(chan, cap, RPC_MESSAGE(type), payloadsize, payload,
         callback_when_done, id);
    struct waitset *ws = get_default_waitset();

    while (!done)
        event_dispatch(ws);
}

errval_t aos_rpc_send_number(struct aos_rpc *chan, uintptr_t val)
{
    DBG(VERBOSE, "rpc_send_number\n");

    uintptr_t *sendargs = malloc(1 * sizeof(uintptr_t));

    sendargs[0] = (uintptr_t) val;
    rpc_framework(NULL, NULL, RPC_TYPE_NUMBER, &chan->chan, NULL_CAP, 1,
                  sendargs, MKCLOSURE(free, sendargs));
    DBG(DETAILED, "ACK Received (number)\n");
    return SYS_ERR_OK;
}

errval_t aos_rpc_send_string(struct aos_rpc *rpc, const char *string)
{
    size_t tempsize = strlen(string);
    uintptr_t *payload2;
    size_t payloadsize2;
    convert_charptr_to_uintptr_with_padding_and_copy(string, tempsize,
                                                     &payload2, &payloadsize2);
    rpc_framework(NULL, NULL, RPC_TYPE_STRING, &rpc->chan, NULL_CAP,
                  payloadsize2, payload2, NULL_EVENT_CLOSURE);
    free(payload2);
    DBG(DETAILED, "ACK Receied (string)\n");
    return SYS_ERR_OK;
}

static errval_t ram_send_handler(uintptr_t *args)
{
    DBG(DETAILED, "rpc_ram_send_handler\n");

    struct aos_rpc *rpc = (struct aos_rpc *) args[0];
    size_t size = *((size_t *) args[1]);
    size_t align = *((size_t *) args[2]);

    DBG(DETAILED, "The size is %zu and alignment is %zu\n", size, align);

    // mildly dirty hacks
    unsigned int first_byte = (RPC_MESSAGE(RPC_TYPE_RAM) << 24) +
                              (((unsigned char) args[3]) << 16) + 2;
    DBG(DETAILED, "Sending message with: type %u id %u\n",
        RPC_MESSAGE(RPC_TYPE_RAM), args[3]);
    // end mildly dirty hacks

    errval_t err;
    do {
        DBG(DETAILED, "calling lmp_chan_send3 in rpc_ram_send_handler\n");
        // Check if sender is currently busy
        // TODO: could we implement some kind of buffer for this?
        err = lmp_chan_send3(&rpc->chan, LMP_FLAG_SYNC, NULL_CAP, first_byte,
                             size, align);
    } while (err == LIB_ERR_CHAN_ALREADY_REGISTERED);
    if (!err_is_ok(err))
        debug_printf("tried to send ram request, ran into issue: %s\n",
                     err_getstring(err));

    return SYS_ERR_OK;
}

struct aos_rpc_ram_recv_struct {
    struct capref ram_cap;
    size_t size;
};

static void aos_rpc_ram_recv(void *arg1, struct recv_list *data)
{
    struct aos_rpc_ram_recv_struct *arrrs =
        (struct aos_rpc_ram_recv_struct *) arg1;
    arrrs->ram_cap = data->cap;
    arrrs->size = data->payload[1];
}

errval_t aos_rpc_get_ram_cap(struct aos_rpc *chan, size_t size, size_t align,
                             struct capref *retcap, size_t *ret_size)
{
    // we duplicate the rpc_framework into this as we have to make a
    // non-mallocing send call, which we dospecialcased atm
    thread_mutex_lock_nested(&chan->mutex);

    DBG(DETAILED, "rpc_get_ram_cap\n");

    struct waitset *ws = get_default_waitset();

    uintptr_t sendargs[4];

    sendargs[0] = (uintptr_t) chan;
    sendargs[1] = (uintptr_t) &size;
    sendargs[2] = (uintptr_t) &align;

    // CHECK(lmp_chan_alloc_recv_slot(&chan->chan));
    unsigned char id = request_fresh_id(RPC_MESSAGE(RPC_TYPE_RAM));
    sendargs[3] = (uintptr_t) id;

    bool done = false;
    struct aos_rpc_ram_recv_struct retvals;
    register_recv(id, RPC_ACK_MESSAGE(RPC_TYPE_RAM), &done, aos_rpc_ram_recv,
                  &retvals);

    errval_t err = lmp_chan_register_send(
        &chan->chan, ws, MKCLOSURE((void *) ram_send_handler, sendargs));
    while (err == LIB_ERR_CHAN_ALREADY_REGISTERED) {
        CHECK(event_dispatch(ws));
        err = lmp_chan_register_send(
            &chan->chan, ws, MKCLOSURE((void *) ram_send_handler, sendargs));
    }
    CHECK(err);

    while (!done)
        event_dispatch(ws);

    *retcap = retvals.ram_cap;
    *ret_size = retvals.size;

    thread_mutex_unlock(&chan->mutex);

    return SYS_ERR_OK;
}

errval_t aos_rpc_serial_getchar(struct aos_rpc *chan, char *retc)
{
    // TODO implement functionality to request a character from
    // the serial driver.
    return SYS_ERR_OK;
}

errval_t aos_rpc_serial_putchar(struct aos_rpc *rpc, char c)
{
    assert(rpc != NULL);
    uintptr_t payload = c;
    rpc_framework(NULL, NULL, RPC_TYPE_PUTCHAR, &rpc->chan, NULL_CAP, 1,
                  &payload, NULL_EVENT_CLOSURE);
    DBG(DETAILED, "ACK Received (putchar) \n");
    return SYS_ERR_OK;
}

errval_t aos_rpc_kill_me(struct aos_rpc *chan, struct capref disp)
{
    DBG(VERBOSE, "aos_rpc_kill_me\n");
    rpc_framework(NULL, NULL, RPC_TYPE_PROCESS_KILL_ME, &chan->chan, disp, 0,
                  NULL, NULL_EVENT_CLOSURE);
    DBG(ERR, "we should be dead by now\n");
    return SYS_ERR_OK;
}

static void aos_rpc_process_spawn_recv(void *arg1, struct recv_list *data)
{
    // TODO: can this be removed or is there something useful that we should do here?
    // domainid_t *newpid = (domainid_t *) arg1;
    debug_printf("spawned requested process\n");
    //*newpid = data->payload[1];
}

errval_t aos_rpc_process_spawn(struct aos_rpc *chan, char *name, coreid_t core,
                               domainid_t *newpid)
{
    DBG(DETAILED, "rpc call: spawn new process %s on core %d\n", name, core);
    // this has the same issue like string sending, so we just manually solve
    // it for now. TODO: make this nicer and consistent with string sending
    size_t tempsize = strlen(name);

    // add core to the sendstring
    char* new_name = malloc((tempsize + 5)*4);
    sprintf(new_name, "%s_%d", name, core);
    tempsize = strlen(new_name);

    // convert
    uintptr_t *payload2;
    size_t payloadsize2;
    convert_charptr_to_uintptr_with_padding_and_copy(new_name, tempsize, &payload2,
                                                     &payloadsize2);

    rpc_framework(aos_rpc_process_spawn_recv, newpid, RPC_TYPE_PROCESS_SPAWN,
                  &chan->chan, NULL_CAP, payloadsize2, payload2,
                  NULL_EVENT_CLOSURE);
    free(payload2);
    free(new_name);
    return SYS_ERR_OK;
}


static void aos_rpc_process_register_recv(void *arg1, struct recv_list *data)
{
    uint32_t *combinedArg = (uint32_t*) data->payload;
    DBG(DETAILED, "registered self. received core %d pid %d\n", combinedArg[1], combinedArg[0]);
    // TODO: store somewhere
    //domainid_t pid = combinedArg[0];
    coreid_t coreid = combinedArg[1];

    disp_set_core_id(coreid);
    // printfs are correctly prefixed from now on
    // registration finished
}

errval_t aos_rpc_process_register(struct aos_rpc *chan, char *name)
{
    DBG(DETAILED, "rpc call: register self\n");
    size_t tempsize = strlen(name);

    // convert
    uintptr_t *payload2;
    size_t payloadsize2;
    convert_charptr_to_uintptr_with_padding_and_copy(name, tempsize, &payload2,
                                                     &payloadsize2);

    rpc_framework(aos_rpc_process_register_recv, chan, RPC_TYPE_PROCESS_REGISTER,
                  &chan->chan, NULL_CAP, payloadsize2, payload2,
                  NULL_EVENT_CLOSURE);
    free(payload2);
    return SYS_ERR_OK;
}

static void aos_rpc_process_get_name_recv(void *arg1, struct recv_list *data)
{
    // this shares code with lib_rpc.c/new_spawn_recv_handler
    char **ret_name = (char **) arg1;
    char *recv_name = (char *) &data->payload[1];
    size_t length = strlen(recv_name); // TODO: consider that this could be
                                       // made much faster by doing data->size
                                       // * 4 - padding
    char *name = malloc(length + 1);
    strcpy(name, recv_name);
    name[length] = '\0';
    *ret_name = name;
    debug_printf("in here we have: size %u and name %s\n", data->size,
                 (char *) &data->payload[1]);
}

errval_t aos_rpc_process_get_name(struct aos_rpc *chan, domainid_t pid,
                                  char **name)
{
    uintptr_t *payload = malloc(sizeof(uintptr_t));
    *payload = pid;
    debug_printf("wanting to get the name of process with pid %u\n", *payload);
    rpc_framework(aos_rpc_process_get_name_recv, name,
                  RPC_TYPE_PROCESS_GET_NAME, &chan->chan, NULL_CAP, 1, payload,
                  NULL_EVENT_CLOSURE);
    free(payload);
    return SYS_ERR_OK;
}

errval_t aos_rpc_process_get_all_pids(struct aos_rpc *chan, domainid_t **pids,
                                      size_t *pid_count)
{
    /*    uintptr_t sendargs[3]; // 0+2
        sendargs[0] = (uintptr_t) chan;

        impl(process_get_pids_send_handler, process_get_pids_receive_handler,
             true);

        *pid_count = (size_t) sendargs[1];
        *pids = (domainid_t *) sendargs[2];
    */
    return LIB_ERR_NOT_IMPLEMENTED;
}

errval_t aos_rpc_get_device_cap(struct aos_rpc *rpc, lpaddr_t paddr,
                                size_t bytes, struct capref *frame)
{
    return LIB_ERR_NOT_IMPLEMENTED;
}

static void aos_rpc_recv_handler(struct recv_list *data)
{
    // do actions depending on the message type
    // Check the message type and handle it accordingly.
    if (data->type & 1) { // ACK, so we check the recv list
        struct rpc_call *foodata = NULL;
        // TODO: consider that this might not need a mutex lock
        synchronized(aos_rpc_mutex)
        {
            struct rpc_call *prev = NULL;
            foodata = rpc_call_buffer_head;
            while (foodata != NULL) {
                if (foodata->type == data->type &&
                    foodata->id == data->payload[0])
                    break;
                prev = foodata;
                foodata = foodata->next;
            }
            if (foodata != NULL) {
                if (prev == NULL) {
                    rpc_call_buffer_head = rpc_call_buffer_head->next;
                } else {
                    prev->next = foodata->next;
                }
            }
        }
        if (foodata == NULL) {
            DBG(WARN, "did not have a RPC receiver registered for the ACK - "
                      "type %u id %u\n",
                (unsigned int) data->type, (unsigned int) data->payload[0]);
            DBG(WARN, "Dumping foodata structure: \n");
            foodata = rpc_call_buffer_head;
            int counter = 0;
            while (foodata != NULL) {
                DBG(WARN, "%d - type %u id % u\n", counter,
                    (unsigned int) foodata->type, (unsigned int) foodata->id);
                counter++;
                foodata = foodata->next;
            }
        } else {
            if (foodata->recv_handling != NULL)
                foodata->recv_handling(foodata->arg1, data);
            MEMORY_BARRIER;
            *foodata->done = true;
            MEMORY_BARRIER;
            if (foodata->type != RPC_ACK_MESSAGE(RPC_TYPE_RAM)) {
                free(foodata);
            } else {
                // jup, this is a thing we are doing
                size_t slot = (((size_t) foodata) -
                               ((size_t) rpc_type_ram_call_buffer)) /
                              sizeof(struct rpc_call);
                synchronized(aos_rpc_mutex)
                {
                    assert(slot < 10 && rpc_type_ram_call_buffer_in_use[slot]);
                    rpc_type_ram_call_buffer_in_use[slot] = false;
                }
            }
        }
    } else {
        switch (data->type) {
        default:
            debug_printf("got message type %d\n", data->type);
            DBG(WARN, "Unable to handle RPC-receipt, expect badness!\n");
        }
    }
}

static void handshake_recv(void *arg1, struct recv_list *data)
{
    struct aos_rpc *rpc = (struct aos_rpc *) arg1;
    DBG(VERBOSE, "ACK Received (handshake) \n");
    data->chan->remote_cap = data->cap;
    rpc->init = true;
}

unsigned int id = 1337;
errval_t aos_rpc_init(struct aos_rpc *rpc)
{
    thread_mutex_init(&aos_rpc_mutex);

    for (int i = 0; i < 10; i++) {
        rpc_type_ram_call_buffer_in_use[i] = false;
    }

    assert(rpc != NULL);
    rpc->init = false;
    CHECK(init_rpc_client(aos_rpc_recv_handler, &rpc->chan, cap_initep));

    // Send local ep to init and wait for init to acknowledge receiving
    // the endpoint.
    // Set send handler.
    rpc_framework(handshake_recv, rpc, RPC_TYPE_HANDSHAKE, &rpc->chan,
                  rpc->chan.local_cap, 0, NULL, NULL_EVENT_CLOSURE);
    rpc->id = id;
    id++;

    // Wait for ACK.
    struct waitset *ws = get_default_waitset();
    while (!rpc->init)
        CHECK(event_dispatch(ws));

    set_init_rpc(rpc);

    return SYS_ERR_OK;
}

/**
 * \brief Returns the RPC channel to init.
 */
struct aos_rpc *aos_rpc_get_init_channel(void)
{
    // TODO check if was created
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
