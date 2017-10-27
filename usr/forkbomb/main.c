
#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/waitset.h>
#include <aos/paging.h>


int main(int argc, char *argv[])
{
    struct aos_rpc init_rpc;
    domainid_t ret;
    debug_printf("Hi, I'm a forkbomb and I only print a single message... and spawn myself twice again.");
    aos_rpc_init(&init_rpc);
    aos_rpc_process_spawn(&init_rpc,"forkbomb",1,&ret);
    aos_rpc_process_spawn(&init_rpc,"forkbomb", ret % MAX_COREID,&ret);
    return 0;
}
