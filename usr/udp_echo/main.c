/**
 * \file
 * \brief Hello world application
 */

/*
 * Copyright (c) 2016 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, CAB F.78, Universitaetstr. 6, CH-8092 Zurich,
 * Attn: Systems Group.
 */
#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <aos/domain_network_interface.h>
#include <aos/aos_rpc.h>

static domainid_t network_pid;
static coreid_t network_coreid;
static uint16_t my_port;

static void message_handler(void* payload, size_t bytes){
    struct network_message_transfer_message* message = payload;

    if (message->message_type != NETWORK_TRANSFER_MESSAGE){
        printf("ERROR: wrong message type. was %d\n", message->message_type);
        return;
    }
    if (message->protocol != PROTOCOL_UDP){
        printf("ERROR: wrong protocol, was %d\n", message->protocol);
        return;
    }

    // the payload does not change. return the exact same message
    network_message_transfer(message->port_to, message->port_from, message->ip_to, message->ip_from, PROTOCOL_UDP, message->payload, message->payload_size, network_pid, network_coreid);
}


int main(int argc, char *argv[])
{
    // TODO: check args
    // TODO: remove all args except port
    if(argc != 4){
        printf("Usage: udp_echo network-pid network-core port\n");
        return EXIT_FAILURE;
    }
    network_pid = strtoul(argv[1], NULL, 0);
    network_coreid = strtoul(argv[2], NULL, 0);
    my_port = strtoul(argv[3], NULL, 0);


    // init message handler
    rpc_register_process_message_handler(aos_rpc_get_init_channel(), message_handler);

    //debug_printf("Try to open port at %d, %d\n",strtoul(argv[1], NULL, 0), strtoul(argv[2], NULL, 0));
    network_register_port(my_port, PROTOCOL_UDP, network_pid, network_coreid);

    // Hang around
    errval_t err;
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
