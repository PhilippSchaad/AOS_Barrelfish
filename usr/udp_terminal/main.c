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
#include <nameserver.h>

static domainid_t network_pid;
static coreid_t network_coreid;
static uint16_t my_port;
static uint16_t foreign_port;
static uint32_t my_ip;
static uint32_t foreign_ip;

static void message_handler(void* payload, size_t bytes){
    struct network_message_transfer_message* message = payload;
    if(message->message_type == NETWORK_PRINT_MESSAGE){
        struct network_print_message* pmessage = payload;
        // message from one of my children
        // forward to the network
        debug_printf("received message from child (len is %d): %s\n", pmessage->payload_size, pmessage->payload);
        network_message_transfer(my_port, foreign_port, my_ip, foreign_ip, PROTOCOL_UDP,(uint8_t*) pmessage->payload, pmessage->payload_size, network_pid, network_coreid);
        return;
    }

    if (message->message_type != NETWORK_TRANSFER_MESSAGE){
        printf("ERROR: wrong message type. was %d\n", message->message_type);
        return;
    }
    if (message->protocol != PROTOCOL_UDP){
        printf("ERROR: wrong protocol, was %d\n", message->protocol);
        return;
    }

    // the payload does not change. return the exact same message
    // print the output
    printf((char*) message->payload);
    //network_message_transfer(message->port_to, message->port_from, message->ip_to, message->ip_from, PROTOCOL_UDP, message->payload, message->payload_size, network_pid, network_coreid);
    foreign_port = message->port_from;
    foreign_ip = message->ip_from;
    my_ip = message->ip_to;

    domainid_t ret;

    if(message->payload_size >= 200){
        network_message_transfer(my_port, foreign_port, my_ip, foreign_ip, PROTOCOL_UDP, (uint8_t*) "Error: max input length is 200.", 31, network_pid, network_coreid);
    }

    //remove newline character from the string
    message->payload[message->payload_size-1] = 0x0;

    if(strcmp((char*) message->payload, "") == 0 || (strlen((char*) message->payload) >= 3 && strncmp((char*) message->payload, "->:", 3) == 0)){
        return;
    }

    char result[214];
    sprintf(result, "%s __terminalized",  message->payload);
    debug_printf("try to spawn: %s\n", result);
    aos_rpc_process_spawn(aos_rpc_get_init_channel(), result, disp_get_core_id(), &ret);
    //TODO: catch errors
    // TODO: catch output triggered inputs. maybe use preamble on output that is checked and ignored here
}


int main(int argc, char *argv[])
{
    // TODO: check args
    // TODO: remove all args except port
    if(argc != 2){
        printf("Usage: udp_terminal port\n");
        return EXIT_FAILURE;
    }

    struct nameserver_query nsq;
    nsq.tag = nsq_name;
    nsq.name = "Network";
    struct nameserver_info *nsi;
    CHECK(lookup(&nsq,&nsi));
    if(nsi != NULL) {
        //printf("NSI with core id %u, pid %u, and propcount %d\n", nsi->coreid, strtoul(nsi->props->prop_attr, NULL, 0), nsi->nsp_count);
    }else{
        printf("Nameserver did not know the network. Start the network domain first!\n");
        return EXIT_FAILURE;
    }

    network_pid = strtoul(nsi->props->prop_attr, NULL, 0);
    network_coreid = nsi->coreid;
    my_port = strtoul(argv[1], NULL, 0);

    if(my_port == 0){
        printf("Usage: udp_terminal port\n");
        return EXIT_FAILURE;
    }

    // register self at nameserver
    struct nameserver_info nsis;
    struct lmp_chan recv_chan;
    struct nameserver_properties props;
    char result[6];
    props.prop_name="pid";
    sprintf(result, "%u", disp_get_domain_id());
    props.prop_attr=result;
    nsis.props = &props;
    nsis.name = "Network_Terminal";
    nsis.type = "Network_Terminal";
    nsis.nsp_count = 1;
    nsis.coreid = disp_get_core_id();
    init_rpc_server(NULL,&recv_chan);
    nsis.chan_cap = recv_chan.local_cap;
    CHECK(register_service(&nsis));

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
