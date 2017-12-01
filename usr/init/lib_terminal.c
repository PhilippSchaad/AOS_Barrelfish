#include <lib_terminal.h>
#include <aos/dispatch.h>

static struct fifo_input_queue *input_buff;

static inline int input_buff_capacity(void)
{
    return input_buff->size - 1;
}

static inline int input_buff_free_count(void)
{
    if (input_buff->head >= input_buff->tail)
        return input_buff_capacity() - (input_buff->head - input_buff->tail);
    else
        return (input_buff->tail - input_buff->head) - 1;
}

static inline bool input_buff_is_full(void)
{
    return input_buff_free_count() == 0;
}

static inline bool input_buff_is_empty(void)
{
    return input_buff_free_count() == input_buff_capacity();
}

static inline void input_buff_advance_head(void)
{
    input_buff->head = (input_buff->head + 1) % input_buff->size;
}

static inline void input_buff_advance_tail(void)
{
    input_buff->tail = (input_buff->tail + 1) % input_buff->size;
}

static void handle_uart_getchar_interrupt(void *args)
{
    debug_printf("interrupt received\n");
    char c;
    sys_getchar(&c);
    debug_printf("typed: %c\n", c);

    // If the buffer is full, just discard the caracter.
    if (input_buff_is_full()) {
        debug_printf("buffer is full\n");
        return;
    }

    terminal_write_c(c);

    input_buff->buff[input_buff->head] = c;

    input_buff_advance_head();
}

void terminal_getchar(char *c)
{
    debug_printf("trying to read\n");
    while (input_buff_is_empty())
        thread_yield();

    *c = input_buff->buff[input_buff->tail];

    input_buff_advance_tail();
    debug_printf("read\n");
}

void terminal_write(const char *buffer)
{
    sys_print(buffer, strlen(buffer));
    fflush(stdout);
}

void terminal_write_c(const char c)
{
    sys_print(&c, 1);
    fflush(stdout);
}

void terminal_write_l(const char *buffer, size_t length)
{
    sys_print(buffer, length);
    fflush(stdout);
}

errval_t terminal_init(coreid_t core)
{
    // We use an extra position in the buffer to detect if it is full.
    input_buff = malloc(sizeof(struct fifo_input_queue));
    input_buff->size = FIFO_CIRC_QUEUE_SIZE + 1;
    input_buff->buff = malloc(input_buff->size * sizeof(char));
    input_buff->head = 0;
    input_buff->tail = 0;

    // TODO: this is not true yet, but maybe that's what we need.
    // The terminal driver on core 0 gets interrupts initially. If
    // someone else wants to read on core 1, that one gets registered on
    // demand.
    if (core == 0)
        CHECK(inthandler_setup_arm(handle_uart_getchar_interrupt, NULL,
                                   IRQ_ID_UART));

    return SYS_ERR_OK;
}
