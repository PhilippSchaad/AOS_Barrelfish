#include <aos/aos.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <aos/aos_rpc.h>
#include "../common/get_rpc.h"

int main(int argc, char *argv[])
{
    debug_printf("spawntest started: testing process spawning\n");

    struct aos_rpc *spawnd = grading_get_init_rpc();

    domainid_t mtpid;
    errval_t err;
    debug_printf("spawning memtest");
    err = aos_rpc_process_spawn(spawnd, "memtest", 0, &mtpid);
    assert(err_is_ok(err));
    assert(mtpid);
    debug_printf("spawning multithreaded memtest");
    err = aos_rpc_process_spawn(spawnd, "memtest_mt", 0, &mtpid);
    assert(err_is_ok(err));
    assert(mtpid);

    return 0;
}
