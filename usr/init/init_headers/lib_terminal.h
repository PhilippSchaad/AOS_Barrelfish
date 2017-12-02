#ifndef LIB_TERMINAL_H
#define LIB_TERMINAL_H

#include <stdlib.h>
#include <stdio.h>

#include <aos/aos.h>
#include <aos/inthandler.h>

#define IRQ_ID_UART             106
#define FIFO_CIRC_QUEUE_SIZE    100
#define LINE_BUFF_LINE_SIZE     500

struct fifo_input_queue {
    char *buff;
    int head;
    int tail;
    int size;
};

struct line_input_queue {
    char *write_buff;
    char *read_buff;
    int write_pos;
    int read_pos;
    int line_capacity;
};

enum input_feed_mode {
    FEED_MODE_DIRECT,
    FEED_MODE_LINE
};

void terminal_getchar(char *c);

void terminal_write(const char *buffer);
void terminal_write_c(const char c);
void terminal_write_l(const char *buffer, size_t length);

void set_feed_mode_line(void);
void set_feed_mode_direct(void);

void terminal_destroy(void);
void terminal_init(coreid_t core);

#endif /* LIB_TERMINAL_H */
