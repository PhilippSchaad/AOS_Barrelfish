#include <aos/aos.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <aos/aos_rpc.h>
#include "../common/get_rpc.h"

static struct aos_rpc *serial;
static char my_getchar(void)
{
    char c;
    errval_t err = aos_rpc_serial_getchar(serial, &c);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "my_getchar");
        return 0;
    }
    return c;
}

int main(int argc, char *argv[])
{
    debug_printf("serialtest started: testing user level serial\n");

    serial = grading_get_init_rpc();

    char c;
    char buf[256];
    int i = 0;
    do {
        i = 0;
        while ((c = my_getchar()) && c != '\n') {
            buf[i++] = c;
        }
        buf[i] = 0;
        debug_printf("got %.*s\n", i, buf);
    } while (strncmp(buf, "exit", 5));

    aos_rpc_serial_putchar(serial, 'I');
    aos_rpc_serial_putchar(serial, '4');
    aos_rpc_serial_putchar(serial, 'a');
    aos_rpc_serial_putchar(serial, '?');

    return 0;
}
