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

#include <turtleback.h>

static struct aos_rpc *init_rpc;

static void handle_getchar_interrupt(void *args)
{
    char c;
    aos_rpc_serial_getchar(init_rpc, &c);
    printf("got char: %c\n", c);
}

int main(int argc, char *argv[])
{
    debug_printf("TurtleBack version.. who am I kidding, this isn't even alpha..\n");

    init_rpc = aos_rpc_get_init_channel();
    if (!init_rpc)
        USER_PANIC("init RPC channel NULL?\n");

    CHECK(aos_rpc_get_irq_cap(init_rpc, &cap_irq));
    CHECK(inthandler_setup_arm(handle_getchar_interrupt, NULL, IRQ_ID_UART));

    struct waitset *ws = get_default_waitset();
    while (true)
        CHECK(event_dispatch(ws));

    return 0;
}
