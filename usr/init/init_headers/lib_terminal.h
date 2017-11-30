#ifndef LIB_TERMINAL_H
#define LIB_TERMINAL_H

#include <stdlib.h>

#include <aos/aos.h>

void terminal_write(char *buffer);
void terminal_write_c(char c);
void terminal_write_l(char *buffer, size_t length);

#endif /* LIB_TERMINAL_H */
