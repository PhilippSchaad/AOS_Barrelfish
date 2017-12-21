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
#include <maps/omap44xx_map.h>
#include <driverkit/driverkit.h>
#include <netutil/user_serial.h>
#include <aos/aos_rpc.h>
#include "message_buffer.h"
#include "slip.h"
#include "udp.h"
#include <aos/domain_network_interface.h>
#include <nameserver.h>

// message buffer
static struct net_msg_buf message_buffer;

// handle the interrupt from serial
void serial_input(uint8_t *buf, size_t len){
    //debug_printf("I received %#04x,\n", *buf);
    CHECK(net_msg_buf_write(&message_buffer, buf, len));
    //debug_printf("call, buffer length is %d\n", net_msg_buf_length(&message_buffer));
}

static void message_handler(void* payload, size_t bytes){
    struct network_register_deregister_port_message* message = payload;
    struct network_message_transfer_message* transfer_message = payload;

    //debug_printf("I received a new message \n");

    // handle the different message types
    switch(message->message_type){
        case NETWORK_REGISTER_PORT:
            switch(message->protocol){
                case PROTOCOL_UDP:
                    udp_register_port(message->port, message->pid, message->core);
                    break;
                default:
                    printf("Protocol %d not supported\n", message->protocol);
            }
            break;
        case NETWORK_DEREGISTER_PORT:
            switch(message->protocol){
                case PROTOCOL_UDP:
                    udp_deregister_port(message->port);
                    break;
                default:
                    printf("Protocol %d not supported\n", message->protocol);
            }
            break;
        case NETWORK_TRANSFER_MESSAGE:
            // new message that needs sending
            switch(transfer_message->protocol){
                case PROTOCOL_UDP:
                    udp_send(transfer_message->port_from, transfer_message->port_to, transfer_message->payload, transfer_message->payload_size, transfer_message->ip_to);
                    break;
                default:
                    printf("Protocol %d not supported\n", transfer_message->protocol);
            }
            break;
        default:
            break;
    }
}

static void sample_fun(int* arg){}

/*
static errval_t testfun(uint8_t* payload, size_t size, uint32_t src, uint16_t source_port, uint16_t dest_port){
    debug_printf("The server received the message \n");
    udp_send(dest_port, source_port, payload, size, src);
    return SYS_ERR_OK;
}
*/

static void parse_and_set_ip(char* ip_string){
    uint8_t digits[4];
    char buff[4];
    int done = 0;
    int currpos = 0;
    for(int i = 0; i<=strlen(ip_string); ++i){
        if(ip_string[i] == '.'||ip_string[i] == '\0'||ip_string[i] == ' '){
            digits[done++] = (uint8_t) strtoul(buff, NULL, 0);
            currpos = 0;
            memset(buff,0x0,4);
        } else {
            buff[currpos++] = ip_string[i];
        }
    }
    if(done == 4){
        ip_set_ip((digits[0]<<24) + (digits[1]<<16) + (digits[2]<<8) + digits[3]);
        printf("Our IP address is: %d.%d.%d.%d\n", digits[0], digits[1], digits[2], digits[3]);
    } else {
        printf("Error: unable to parse ip address\n");
        printf("Usage: network ip_addr\n");
        exit(EXIT_FAILURE);
    }
}


int main(int argc, char *argv[])
{
    if(argc != 2){
        printf("Usage: network ip_addr\n");
        return EXIT_FAILURE;
    }

    // check that there is no previous network domain
    struct nameserver_query nsq;
    nsq.tag = nsq_name;
    nsq.name = "Network";
    struct nameserver_info *nsia;
    CHECK(lookup(&nsq,&nsia));
    if(nsia != NULL) {
        printf("ERROR: There is already a network domain running on this core. exit.");
        return EXIT_FAILURE;
    }

    parse_and_set_ip(argv[1]);


    printf("\nWelcome. I am your connection to the world.\n");

    errval_t err;

    // init the buffer
    net_msg_buf_init(&message_buffer);
    *message_buffer.start = 8;
    // map the required memory
    lvaddr_t uart_device_mapping;
    CHECK(map_device_register(OMAP44XX_MAP_L4_PER_UART4, OMAP44XX_MAP_L4_PER_UART4_SIZE, &uart_device_mapping));

    // get the interrupt cap
    CHECK(aos_rpc_get_irq_cap(aos_rpc_get_init_channel(), &cap_irq));

    // init the driver
    CHECK(serial_init(uart_device_mapping, UART4_IRQ));

    printf("UART4 up\n");

    // enable udp
    // TODO: something to do here??
    udp_init();
    // TODO: remove from here
    // open new udp port
    //udp_register_port(55,testfun);

    struct nameserver_info nsi;
    struct lmp_chan recv_chan;
    struct nameserver_properties props;
    char result[6];
    props.prop_name="pid";
    sprintf(result, "%u", disp_get_domain_id());
    props.prop_attr=result;
    nsi.props = &props;
    nsi.name = "Network";
    nsi.type = "Network";
    nsi.nsp_count = 1;
    nsi.coreid = disp_get_core_id();
    //init_rpc_server(NULL,&recv_chan);
    nsi.chan_cap = recv_chan.local_cap;
    CHECK(register_service(&nsi));

    int retval;
    thread_join(thread_create((thread_func_t) sample_fun, NULL), &retval);

    // start slip (sending and receiveing of packets)
    slip_init(&message_buffer);

    // init message handler
    rpc_register_process_message_handler(aos_rpc_get_init_channel(), message_handler);

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
