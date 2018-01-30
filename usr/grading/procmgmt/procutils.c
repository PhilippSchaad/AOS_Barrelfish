#include <aos/aos.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <aos/aos_rpc.h>

int main(int argc, char *argv[])
{
    debug_printf("procutils started: testing process spawning\n");

    struct aos_rpc *spawnd = aos_rpc_get_process_channel();

    debug_printf("list running processes");
    domainid_t *pids;
    size_t pid_count;
    errval_t err;
    err = aos_rpc_process_get_all_pids(spawnd, &pids, &pid_count);
    assert(err_is_ok(err));

    for (int i = 0; i < pid_count; i++) {
        char *name;
        err = aos_rpc_process_get_name(spawnd, pids[i], &name);
        assert(err_is_ok(err));
        debug_printf("%d\t%s\n", pids[i], name);
        free(name);
    }

    free(pids);

    //TODO: check if rpc_lookup_pid was part of exercise.

    return 0;
}
