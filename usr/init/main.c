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

struct domain {
    struct lmp_chan chan;
    struct capability identification_cap;
    int id;
    char* name;
    struct domain *next;
};

struct domain_list {
    struct domain *head;
};

struct domain_list *active_domains = NULL;

/// Try to find the correct domain identified by cap.
static struct domain * find_domain(struct capref *cap)
{
    struct capability identification_cap;
    CHECK(debug_cap_identify(*cap, &identification_cap));

    assert(active_domains != NULL);

    struct domain *current = active_domains->head;
    while (current != NULL) {
        if (current->identification_cap.u.endpoint.epoffset ==
                identification_cap.u.endpoint.epoffset &&
                current->identification_cap.u.endpoint.listener ==
                identification_cap.u.endpoint.listener) {
            return current;
        }
        current = current->next;
    }

    DBG(DETAILED, "Domain not found\n");

    return NULL;
}

static errval_t number_send_handler(void *args)
{
/* how about we don't throw a number ack into our pipeline where it rots forever
  DBG(DETAILED, "init sends ACK for number\n");

    struct lmp_chan *chan = (struct lmp_chan *) args;

    CHECK(lmp_chan_send1(chan, LMP_FLAG_SYNC, NULL_CAP,
                         RPC_ACK_MESSAGE(RPC_TYPE_NUMBER)));*/
    return SYS_ERR_OK;
}

static errval_t ram_send_handler(void **args)
{
    DBG(VERBOSE, "ram_send_handler sending ack\n");
    struct lmp_chan *chan = (struct lmp_chan *) args[0];
    struct capref *cap = (struct capref *) args[1];
    size_t *size = (size_t *) args[2];

    CHECK(lmp_chan_send2(chan, LMP_FLAG_SYNC, *cap,
                         RPC_ACK_MESSAGE(RPC_TYPE_RAM), *size));

    free(args[0]);
    free(args[1]);
    free(args[2]);
    free(args);

    return SYS_ERR_OK;
}

static errval_t handshake_send_handler(void *args)
{
    DBG(DETAILED, "init sends ACK\n");

    struct lmp_chan *chan = (struct lmp_chan *) args;

    CHECK(lmp_chan_send1(chan, LMP_FLAG_SYNC, NULL_CAP, 
                         RPC_ACK_MESSAGE(RPC_TYPE_HANDSHAKE)));
    return SYS_ERR_OK;
}

static errval_t number_recv_handler(void *args, struct lmp_recv_msg *msg,
                                    struct capref *cap)
{
    DBG(RELEASE, "We got the number %u via RPC\n", (uint32_t) msg->words[1]);

    struct domain *dom = find_domain(cap);
    if (dom == NULL) {
        assert(!"Failed to find active domain!");
    }

    CHECK(lmp_chan_register_send(&dom->chan, get_default_waitset(),
                                 MKCLOSURE((void *) number_send_handler,
                                           &dom->chan)));
    return SYS_ERR_OK;
}

char* string_recv_buff = NULL;
unsigned int current_id;
static errval_t string_recv_handler(void *args, struct lmp_recv_msg *msg,
                                    struct capref *cap)
{
    // TODO: this is currently not thread save....
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
        current_id = msg->words[1];
        DBG(DETAILED, "id %u is printing now\n", current_id);
    }

    // we might receive a splitted message
    for (uint8_t i=2; i<9; ++i){
        if (i==2 && msg->words[0] == RPC_MESSAGE(RPC_TYPE_STRING)){
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
                printf("Terminal: %s\n", string_recv_buff);
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
    DBG(VERBOSE, "ram request received\n");

    assert(msg->buf.msglen >= 3);

    struct domain *dom = find_domain(cap);
    if (dom == NULL) {
        assert(!"Failed to find active domain!");
    }

    size_t size = (size_t) msg->words[1];
    size_t align = (size_t) msg->words[2];

    struct capref ram_cap;
    CHECK(aos_ram_alloc_aligned(&ram_cap, size, align));

    // Fix size based on BASE_PAGE_SIZE, the way we're retrieving it.
    if (size % BASE_PAGE_SIZE != 0) {
        size += (size_t) BASE_PAGE_SIZE - (size % BASE_PAGE_SIZE);
    }
    DBG(DETAILED, "We got ram with size %zu\n", size);

    // Send the response:
    void **sendargs = (void **) malloc(sizeof(void *) * 3);
    sendargs[0] = (void *) malloc(sizeof(struct lmp_chan));
    sendargs[1] = (void *) malloc(sizeof(struct capref));
    sendargs[2] = (void *) malloc(sizeof(size_t));
    *((struct lmp_chan *) sendargs[0]) = dom->chan;
    *((struct capref *) sendargs[1]) = ram_cap;
    *((size_t *) sendargs[2]) = size;
    CHECK(lmp_chan_register_send(&dom->chan, get_default_waitset(),
                                 MKCLOSURE((void *) ram_send_handler,
                                           sendargs)));

    return SYS_ERR_OK;
}

static errval_t putchar_recv_handler(void *args, struct lmp_recv_msg *msg,
                                     struct capref *cap)
{
    DBG(DETAILED, "putchar request received\n");

    // Print the character.
    sys_print((char *) &msg->words[1], 1);

    return SYS_ERR_OK;
}
static int pid;
static errval_t handshake_recv_handler(void *args, struct capref *child_cap)
{
    DBG(DETAILED, "init received cap\n");

    // Check if the domain/channel already exists. If so, no need to create one
    struct domain *dom = find_domain(child_cap);
    if (dom == NULL) {
        dom = (struct domain *) malloc(sizeof(struct domain));
        dom->id = pid++;
        dom->next = active_domains->head;
        active_domains->head = dom;

        struct capability identification_cap;
        CHECK(debug_cap_identify(*child_cap, &identification_cap));
        dom->identification_cap = identification_cap;

        CHECK(lmp_chan_accept(&dom->chan, DEFAULT_LMP_BUF_WORDS, *child_cap));

        // set the remote cap we just got from the child
        dom->chan.remote_cap = *child_cap;

        DBG(DETAILED, "Created new channel\n");
    }

    // Send ACK to the child
    CHECK(lmp_chan_register_send(&dom->chan, get_default_waitset(),
                                 MKCLOSURE((void *)handshake_send_handler,
                                           &dom->chan)));

    DBG(DETAILED, "successfully received cap\n");
    return SYS_ERR_OK;
}

struct adhoc_process_table {
    int id;
    char* name;
    struct spawninfo *si;
    struct adhoc_process_table* next;
};

static struct adhoc_process_table *adhoc_process_table = NULL;

static errval_t spawn_recv_handler(void *args, struct lmp_recv_msg *msg, struct capref* cap) {
//    struct lmp_chan *chan = (struct lmp_chan *) args;
    int totalcount = (int)msg->words[1];
    if(totalcount > 4*6)
        return -1; //todo: real error number
    char *name = (char*) malloc(sizeof(char) * (4*6+1));
    for(int i = 0; i < totalcount; i++) {
        char *c = (char*)&(msg->words[(i>>2)+2]);
        name[i] = c[i%4];
    }
    name[totalcount] = '\0';
    struct spawninfo* si = (struct spawninfo*) malloc(sizeof(struct spawninfo));
    errval_t err;
    err = spawn_load_by_name(name, si);
    size_t ret_id = si->paging_state.debug_paging_state_index;
    struct adhoc_process_table *apt = (struct adhoc_process_table*) malloc(sizeof(struct adhoc_process_table));
    apt->id = ret_id;
    apt->name = name;
    apt->si = si;
    apt->next = adhoc_process_table;
    adhoc_process_table = apt;
    struct domain *dom = find_domain(cap);
    if (dom == NULL) {
        assert(!"Failed to find active domain!");
    }
    CHECK(lmp_chan_send2(&dom->chan, LMP_FLAG_SYNC, NULL_CAP,
                         RPC_ACK_MESSAGE(RPC_TYPE_PROCESS_SPAWN),(uintptr_t)ret_id));
    return SYS_ERR_OK;
}

static errval_t process_get_name_recv_handler(struct capref* cap, struct lmp_recv_msg* msg) {

    int id = (int)msg->words[1];
    struct adhoc_process_table *apt = adhoc_process_table;
    char* name;
    bool found = false;
    while(apt) {
        if(apt->id == id) {
            name = apt->name;
            found = true;
            break;
        }
        apt = apt->next;
    }
    if(!found)
        name = "No Such Id, Sorry.";

    uintptr_t sendargs[9]; //1+1
    size_t totalcount = strlen(name);
    sendargs[1] = (uintptr_t)totalcount;
    if(totalcount > 4*6)
        return -1; //TODO: Real error message
    for(int i = 0; i < totalcount; i++) {
        sendargs[(i >> 2)+2] = (i % 4 ? sendargs[(i >> 2)+2] : 0) + (name[i] << (8*(i % 4)));
    }
    struct domain *dom = find_domain(cap);
    if (dom == NULL) {
        assert(!"Failed to find active domain!");
    }
    CHECK(lmp_chan_send9(&dom->chan, LMP_FLAG_SYNC, NULL_CAP,
                         RPC_ACK_MESSAGE(RPC_TYPE_PROCESS_GET_NAME),sendargs[1],sendargs[2],sendargs[3],sendargs[4],sendargs[5],sendargs[6],sendargs[7],sendargs[8]));
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
        case RPC_MESSAGE(RPC_TYPE_PROCESS_GET_NAME):
            CHECK(process_get_name_recv_handler(&cap,&msg));
            break;
        case RPC_MESSAGE(RPC_TYPE_PROCESS_SPAWN):
            CHECK(spawn_recv_handler(args,&msg,&cap));
            break;
        case RPC_MESSAGE(RPC_TYPE_PROCESS_GET_PIDS):
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

    // Structure to keep track of domains.
    active_domains = malloc(sizeof(struct domain_list));

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

    free(active_domains);

    return EXIT_SUCCESS;
}
