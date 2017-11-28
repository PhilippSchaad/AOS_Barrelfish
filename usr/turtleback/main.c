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

static char *shell_prompt;

static struct shell_cmd shell_builtins[] = {
    {
        .cmd = "echo",
        .help_text = "display a line of text",
        .invoke = shell_echo
    },
    {
        .cmd = NULL,
        .help_text = NULL,
        .invoke = NULL
    }
};

static inline bool is_endl(char c)
{
    // 4 = EOL | 10 = NL | 13 = CR
    return (c == 4 || c == 10 || c == 13);
}

static inline bool is_backspace(char c)
{
    // 127 = Backspace
    return c == 127;
}

static inline bool is_space(char c)
{
    // 32 = Space
    return c == 32;
}

static void shell_mem_err(void)
{
    USER_PANIC("The TurtleBack Shell has run out of memory! Aborting..\n");
}

static void shell_new_prompt(void)
{
    printf(shell_prompt);
    fflush(stdout);
}

static void parse_line(void)
{
    // Check if the buffer is empty. If so, nothing to do.
    if (buffer_pos == 0)
        return;

    // If the first buffer position is a space, we have an invalid cmd.
    if (is_space(input_buffer[0])) {
        // TODO: print invalid use
        return;
    }

    // Gather all tokens from the input buffer.
    int token_length = 0;
    int n_tokens = 0;
    char *token_start = NULL;
    char *iter = input_buffer;
    char **tokens = malloc(sizeof(char *));
    if (!tokens)
        shell_mem_err();
    for (int i = 0; i < buffer_pos + 1; i++) {
        if (is_space(input_buffer[i]) || input_buffer[i] == '\0') {
            if (!is_space(*(iter - 1))) {
                tokens[n_tokens] = malloc((token_length + 1) * sizeof(char));
                if (tokens[n_tokens] == NULL)
                    shell_mem_err();
                strncpy(tokens[n_tokens], token_start, token_length);
                tokens[n_tokens][token_length] = '\0';
                n_tokens++;
                tokens = realloc(tokens, (n_tokens + 1) * sizeof(char *));
            }
            token_start = NULL;
            token_length = 0;
        } else {
            if (!token_start)
                token_start = &input_buffer[i];
            token_length++;
        }
    }

    bool handled = false;

    // Try matching against builtins on the first token.
    for (int i = 0; shell_builtins[i].cmd != NULL; i++) {
        if (strncmp(tokens[0], shell_builtins[i].cmd,
                    strlen(tokens[0])) == 0) {
            handled = true;
            shell_builtins[i].invoke(n_tokens, tokens);
            break;
        }
    }

    // Try spawning it as an application if it has not yet been handled.
    /*
    if (!handled) {
    }
    */
    
    // Cleanup.
    for (int i = 0; i < n_tokens; i++) {
        free(tokens[i]);
    }
    free(tokens);
}

static void handle_getchar_interrupt(void *args)
{
    char new_char;
    aos_rpc_serial_getchar(init_rpc, &new_char);
    if (is_endl(new_char)) {
        input_buffer[buffer_pos] = '\0';
        parse_line();
        printf("\n");
        shell_new_prompt();
        buffer_pos = 0;
        input_buffer[buffer_pos] = '\0';
    } else if (is_backspace(new_char)) {
        // If we try to delete beyond the first char, we are in kernel space.
        // So let's not do this and return instead.
        if (buffer_pos == 0)
            return;

        aos_rpc_serial_putchar(init_rpc, '\b');
        aos_rpc_serial_putchar(init_rpc, ' ');
        aos_rpc_serial_putchar(init_rpc, '\b');
        input_buffer[buffer_pos] = '\0';
        buffer_pos--;
    } else if (buffer_pos < INPUT_BUFFER_LENGTH) {
        aos_rpc_serial_putchar(init_rpc, new_char);
        input_buffer[buffer_pos] = new_char;
        buffer_pos++;
    }
    // If the buffer is full, ignore the char.
}

int main(int argc, char **argv)
{
    debug_printf("TurtleBack version.. who am I kidding, this isn't even alpha..\n");

    shell_prompt = TURTLEBACK_DEFAULT_PROMPT;

    init_rpc = aos_rpc_get_init_channel();
    if (!init_rpc)
        USER_PANIC("init RPC channel NULL?\n");

    CHECK(aos_rpc_get_irq_cap(init_rpc, &cap_irq));
    CHECK(inthandler_setup_arm(handle_getchar_interrupt, NULL, IRQ_ID_UART));

    shell_new_prompt();

    struct waitset *ws = get_default_waitset();
    while (true)
        CHECK(event_dispatch(ws));

    return 0;
}
