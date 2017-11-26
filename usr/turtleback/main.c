/**
 * \file
 * \brief The TurtleBack Shell application
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

#include <aos/aos.h>
#include <aos/aos_rpc.h>

#include <stdio.h>
#include <stdlib.h>

static struct aos_rpc *init_rpc;

static void wait_for_input(void)
{
    while (true) {
        char c;
        aos_rpc_serial_getchar(init_rpc, &c);
        debug_printf("got char: %c\n", c);
    }
}

int main(int argc, char *argv[])
{
    debug_printf("TurtleBack version.. who am I kidding, this isn't even alpha..\n");

    init_rpc = aos_rpc_get_init_channel();
    if (!init_rpc)
        USER_PANIC("init RPC channel NULL?\n");

    wait_for_input();

    return 0;
}
