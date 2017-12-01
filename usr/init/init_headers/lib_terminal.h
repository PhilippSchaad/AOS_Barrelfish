#ifndef LIB_TERMINAL_H
#define LIB_TERMINAL_H

#include <stdlib.h>
#include <stdio.h>

#include <aos/aos.h>
#include <aos/inthandler.h>

#define IRQ_ID_UART             106
#define FIFO_CIRC_QUEUE_SIZE    100

struct fifo_input_queue {
    char *buff;
    int head;
    int tail;
    int size;
};

void terminal_write(const char *buffer);
void terminal_write_c(const char c);
void terminal_write_l(const char *buffer, size_t length);
void terminal_getchar(char *c);

errval_t terminal_init(coreid_t core);

#endif /* LIB_TERMINAL_H */
