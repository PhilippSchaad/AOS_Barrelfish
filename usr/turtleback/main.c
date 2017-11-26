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

static char input_buffer[INPUT_BUFFER_LENGTH];
static int buffer_pos = 0;

static inline bool is_endl(char c)
{
    // 4 = EOL | 10 = NL | 13 = CR
    if (c == 4 || c == 10 || c == 13)
        return true;
    return false;
}

static inline bool is_backspace(char c)
{
    // 127 = Backspace
    if (c == 127)
        return true;
    return false;
}

static void parse_input(void)
{
    printf(input_buffer);
    printf("\n");
}

static void handle_getchar_interrupt(void *args)
{
    char new_char;
    aos_rpc_serial_getchar(init_rpc, &new_char);
    if (is_endl(new_char)) {
        input_buffer[buffer_pos] = '\0';
        printf("\n");
        parse_input();
        buffer_pos = 0;
    } else if (is_backspace(new_char)) {
        // If we try to delete beyond the first char, we are in kernel space.
        // So let's not do this and return instead.
        if (buffer_pos == 0)
            return;

        printf("\b \b");
        fflush(stdout);
        buffer_pos--;
    } else if (buffer_pos < INPUT_BUFFER_LENGTH) {
        printf("%c", new_char);
        fflush(stdout);
        input_buffer[buffer_pos] = new_char;
        buffer_pos++;
    }
    // If the buffer is full, ignore the char.
}

int main(int argc, char **argv)
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
