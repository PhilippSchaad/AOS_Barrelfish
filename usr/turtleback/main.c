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

/*
static char **shell_history;
static int shell_history_max_length;
static int shell_history_length;
*/

static char *shell_prompt;

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

static void print_fill_length(const char *format, ...)
{
    int colwidth = 80;

    va_list p_args;
    va_start(p_args, format);

    char pre_formatted[colwidth];
    vsprintf(pre_formatted, format, p_args);

    char p_line[colwidth];
    sprintf(p_line, "* %.*s\0", colwidth - 4, pre_formatted);

    int p_line_length = strlen(p_line);
    int spaces_to_fill = colwidth;
    spaces_to_fill -= 2 + p_line_length;

    char *filler =
        "                                                                     "
        "        ";

    printf("%s%.*s *\n", p_line, spaces_to_fill, filler);

    va_end(p_args);
}

static void shell_welcome_msg(void)
{
    assert(OS_VERSION_MAJOR >= 0);
    assert(OS_VERSION_MINOR >= 0);
    assert(OS_PATCH_LEVEL >= 0);

    shell_clear(0, NULL);

    printf("**************************************"
           "******************************************\n");
    printf("*********************************  Welcome  "
           "************************************\n");
    printf("**************************************"
           "******************************************\n");
    print_fill_length("%.20s v%u.%u.%u", OS_NAME, OS_VERSION_MAJOR,
                      OS_VERSION_MINOR, OS_PATCH_LEVEL);
    print_fill_length("Authors: %s", OS_AUTHORS);
    print_fill_length("%s", OS_GROUP);
    print_fill_length("");
    printf("**************************************"
           "******************************************\n");
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
    char **tokens = malloc(sizeof(char *));
    if (!tokens)
        shell_mem_err();
    for (int i = 0; i < buffer_pos + 1; i++) {
        if (is_space(input_buffer[i]) || input_buffer[i] == '\0') {
            if (token_length > 0) {
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
        if (strcmp(tokens[0], shell_builtins[i].cmd) == 0) {
            handled = true;
            shell_builtins[i].invoke(n_tokens, tokens);
            break;
        }
    }

    // Try spawning it as an application if it has not yet been handled.
    if (!handled) {
        domainid_t pid;
        CHECK(aos_rpc_process_spawn(init_rpc, input_buffer, disp_get_core_id(),
                                    &pid));
        // A PID of UINT32_MAX means the process was not found.
        if (pid != UINT32_MAX)
            handled = true;
    }

    if (!handled)
        shell_invalid_command(tokens[0]);
    
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
        printf("\n");
        parse_line();
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
    shell_prompt = TURTLEBACK_DEFAULT_PROMPT;

    init_rpc = aos_rpc_get_init_channel();
    if (!init_rpc)
        USER_PANIC("init RPC channel NULL?\n");

    CHECK(aos_rpc_get_irq_cap(init_rpc, &cap_irq));
    CHECK(inthandler_setup_arm(handle_getchar_interrupt, NULL, IRQ_ID_UART));

    shell_welcome_msg();
    shell_new_prompt();
    /*
    domainid_t pid;
    CHECK(aos_rpc_process_spawn(init_rpc, "hello", 1, &pid));
    */

    struct waitset *ws = get_default_waitset();
    while (true)
        CHECK(event_dispatch(ws));

    return 0;
}
