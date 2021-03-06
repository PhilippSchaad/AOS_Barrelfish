#include <lib_terminal.h>
#include <aos/dispatch.h>

static bool is_master = false;
static struct fifo_input_queue *input_buff;
static struct line_input_queue *line_buff;
static struct waitset *ws;
static enum input_feed_mode feed_mode;

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

static inline void do_feeding_input_buff(char c) {
    input_buff->buff[input_buff->head] = c;

    if (is_master)
        urpc_term_sendchar(&input_buff->buff[input_buff->head]);

    input_buff_advance_head();
}

static bool direct_esc_init = false;
static bool direct_ctrl_init = false;
static void feed_input_buff(char c)
{
    // If the buffer is full, just discard the character.
    if (input_buff_is_full())
        return;

    if (direct_esc_init && c == ASCII_CODE_OPENING_SQUARE_BRACKET) {
        direct_ctrl_init = true;
        direct_esc_init = false;
        return;
    }

    if (direct_ctrl_init) {
        switch (c) {
        case ASCII_CODE_A:
            do_feeding_input_buff(ASCII_CODE_ARROW_U);
            break;
        case ASCII_CODE_B:
            do_feeding_input_buff(ASCII_CODE_ARROW_D);
            break;
        case ASCII_CODE_C:
            do_feeding_input_buff(ASCII_CODE_ARROW_R);
            break;
        case ASCII_CODE_D:
            do_feeding_input_buff(ASCII_CODE_ARROW_L);
            break;
        }
        direct_ctrl_init = false;
        return;
    }

    if (c == ASCII_CODE_ESC) {
        direct_esc_init = true;
        return;
    }

    do_feeding_input_buff(c);
}

static inline bool line_buff_is_empty(void)
{
    return line_buff->read_pos == -1;
}

static inline bool line_buff_is_full(void)
{
    return line_buff->write_pos == line_buff->line_capacity - 1;
}

static inline void line_buff_switch_lines(void)
{
    char *tmp = line_buff->read_buff;
    line_buff->read_buff = line_buff->write_buff;
    line_buff->write_buff = tmp;
}

static bool escape_sequence_initializer = false;
static bool escape_sequence_finalizer = false;
static void feed_line_buff(char c)
{
    // If the write-line in the buffer is full, discard the character.
    if (line_buff_is_full())
        return;

    if (escape_sequence_initializer &&
            c == ASCII_CODE_OPENING_SQUARE_BRACKET) {
        escape_sequence_finalizer = true;
        escape_sequence_initializer = false;
        return;
    }

    if (escape_sequence_finalizer) {
        // TODO: process escape sequence.
        escape_sequence_finalizer = false;
        return;
    }

    if (is_endl(c)) {
        // If the read-line is not yet empty, we just do nothing, essentially
        // forcing the user to re-press new-line, giving the reader time to
        // finish reading.
        if (!line_buff_is_empty())
            return;

        line_buff->write_buff[line_buff->write_pos] = '\0';
        line_buff_switch_lines();
        line_buff->write_pos = 0;
        line_buff->read_pos = 0;
        terminal_write_c('\n');
    } else if (c == ASCII_CODE_DEL) {
        // Disallow deleting more than what we have written.
        if (line_buff->write_pos == 0)
            return;

        line_buff->write_buff[line_buff->write_pos] = '\0';
        line_buff->write_pos--;
        terminal_write("\b \b");
    } else if (c == ASCII_CODE_ESC) {
        escape_sequence_initializer = true;
        return;
    } else {
        line_buff->write_buff[line_buff->write_pos] = c;
        line_buff->write_pos++;
        line_buff->write_buff[line_buff->write_pos] = '\0';
        terminal_write_c(c);
    }

    if (is_master)
        urpc_term_sendchar(&line_buff->write_buff[line_buff->write_pos]);
}

void terminal_feed_buffer(char c)
{
    switch (feed_mode) {
    case FEED_MODE_DIRECT:
        feed_input_buff(c);
        break;
    case FEED_MODE_LINE:
        feed_line_buff(c);
        break;
    }
}

static void handle_uart_getchar_interrupt(void *args)
{
    char c;
    sys_getchar(&c);

    terminal_feed_buffer(c);
}

static void feed_mode_direct_getchar(char *c)
{
    if (is_master)
        while (input_buff_is_empty())
            event_dispatch(ws);
    else
        while (input_buff_is_empty())
            thread_yield();

    *c = input_buff->buff[input_buff->tail];
    input_buff_advance_tail();
}

static void feed_mode_line_getchar(char *c)
{
    while (line_buff_is_empty())
        event_dispatch(ws);

    *c = line_buff->read_buff[line_buff->read_pos];

    if (*c == '\0')
        line_buff->read_pos = -1;
    else
        line_buff->read_pos++;
}

void terminal_getchar(char *c)
{
    switch (feed_mode) {
    case FEED_MODE_DIRECT:
        feed_mode_direct_getchar(c);
        break;
    case FEED_MODE_LINE:
        feed_mode_line_getchar(c);
        break;
    }
    // Tell the other instance to advance its read buffer too.
    urpc_term_consume();
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

void set_feed_mode_line(void)
{
    feed_mode = FEED_MODE_LINE;

    // Reset the line buffer.
    line_buff->write_pos = 0;
    line_buff->read_pos = -1;
    line_buff->write_buff[line_buff->write_pos] = '\0';
    line_buff->read_buff[0] = '\0';

    urpc_term_set_line_mode();
}

void set_feed_mode_direct(void)
{
    feed_mode = FEED_MODE_DIRECT;

    // Reset the input buffer.
    input_buff->head = 0;
    input_buff->tail = 0;
    input_buff->buff[input_buff->head] = '\0';

    urpc_term_set_direct_mode();
}

void terminal_destroy(void)
{
    free(input_buff->buff);
    free(input_buff);

    free(line_buff->write_buff);
    free(line_buff->read_buff);
    free(line_buff);
}

void terminal_buffer_consume_char(void)
{
    switch (feed_mode) {
    case FEED_MODE_DIRECT:
        input_buff_advance_tail();
        break;
    case FEED_MODE_LINE:
        if (line_buff->read_buff[line_buff->read_pos] == '\0')
            line_buff->read_pos = 0;
        else
            line_buff->read_pos++;
        break;
    }
}

// TODO (phschaad): Make line_capacity reallocate on capacity maxing out maybe?
void terminal_init(coreid_t core)
{
    // Set up the FIFO input buffer, used for FEED_MODE_DIRECT.
    // We use an extra position in the buffer to detect if it is full.
    input_buff = malloc(sizeof(struct fifo_input_queue));
    input_buff->size = FIFO_CIRC_QUEUE_SIZE + 1;
    input_buff->buff = malloc(input_buff->size * sizeof(char));
    input_buff->head = 0;
    input_buff->tail = 0;

    // Set up the line buffer, used for FEED_MODE_LINE.
    line_buff = malloc(sizeof(struct line_input_queue));
    line_buff->line_capacity = LINE_BUFF_LINE_SIZE;
    line_buff->write_buff = malloc(line_buff->line_capacity * sizeof(char));
    line_buff->read_buff = malloc(line_buff->line_capacity * sizeof(char));
    line_buff->write_pos = 0;
    line_buff->read_pos = -1;

    ws = get_default_waitset();
    if (!ws)
        USER_PANIC("Waitset in terminal driver NULL?\n");

    feed_mode = FEED_MODE_DIRECT;

    if (core == 0)
        is_master = true;

    if (is_master)
        CHECK(inthandler_setup_arm(handle_uart_getchar_interrupt, NULL,
                                   IRQ_ID_UART));
}
