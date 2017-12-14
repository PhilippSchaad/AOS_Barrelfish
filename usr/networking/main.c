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

// message buffer
static struct net_msg_buf message_buffer;

// handle the interrupt from serial
void serial_input(uint8_t *buf, size_t len){
    //debug_printf("I received %#04x,\n", *buf);
    CHECK(net_msg_buf_write(&message_buffer, buf, len));
    //debug_printf("call, buffer length is %d\n", net_msg_buf_length(&message_buffer));
}

static void sample_fun(int* arg){
}

static errval_t testfun(uint8_t* payload, size_t size, uint32_t src, uint16_t source_port, uint16_t dest_port){
    debug_printf("The server received the message \n");
    udp_send(dest_port, source_port, payload, size, src);
    return SYS_ERR_OK;
}

int main(int argc, char *argv[])
{
    printf("Welcome. I am your connection to the world.\n");

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
    udp_init();
    // TODO: remove from here
    // open new udp port
    udp_register_port(55,testfun);


    // TODO: fix this properly!!!
    int retval;
    thread_join(thread_create((thread_func_t) sample_fun, NULL), &retval);

    // start slip (sending and receiveing of packets)
    slip_init(&message_buffer);

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
