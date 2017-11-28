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

int main(int argc, char *argv[])
{
    printf("Welcome. I am your connection to the world.\n");

    // map the required memory
    void* uart_device_mapping;
    DBG(-1, "aos_rpc_get_dev_cap: paddr:0x%"PRIxLVADDR" size:%u\n", OMAP44XX_MAP_L4_PER_UART4, OMAP44XX_MAP_L4_PER_UART4_SIZE);
    map_device_register(OMAP44XX_MAP_L4_PER_UART4, OMAP44XX_MAP_L4_PER_UART4_SIZE, (lvaddr_t *) &uart_device_mapping);
}
