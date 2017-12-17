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

int main(int argc, char *argv[])
{
    // TODO: check args
    // TODO: remove all args except port
    if(argc != 4){
        printf("Usage: udp_echo network-pid network-core port\n");
        return EXIT_FAILURE;
    }
    //debug_printf("Try to open port at %d, %d\n",strtoul(argv[1], NULL, 0), strtoul(argv[2], NULL, 0));
    network_register_port(strtoul(argv[3], NULL, 0), PROTOCOL_UDP,strtoul(argv[1], NULL, 0), strtoul(argv[2], NULL, 0));
    return EXIT_SUCCESS;
}
