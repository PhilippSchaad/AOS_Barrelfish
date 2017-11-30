#include <lib_terminal.h>

void terminal_write(char *buffer)
{
    printf("%s", buffer);
    fflush(stdout);
}

void terminal_write_c(char c)
{
    printf("%c", c);
    fflush(stdout);
}

void terminal_write_l(char *buffer, size_t length)
{
    printf("%.*s", length, buffer);
    fflush(stdout);
}
