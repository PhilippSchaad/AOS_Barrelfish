/*
 * Copyright (c) 2016 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, CAB F.78, Universitaetstr. 6, CH-8092 Zurich,
 * Attn: Systems Group.
 */


#include <stdio.h>

#include <aos/aos.h>
#include <aos/aos_rpc.h>

#if 0

/*
  XXX the following two funcions are not suitable for 2016's AOS course
      as the student were not required to implement the wait function
 */

static void
spawn_and_wait(struct aos_rpc *spawnd, char *cmdline) {
    domainid_t mtpid;
    errval_t err;
    int code;

    debug_printf("Spawning \"%s\"\n", cmdline);
    err = aos_rpc_process_spawn(spawnd, cmdline, 0, &mtpid);
    assert(err_is_ok(err));
    assert(mtpid);

    debug_printf("Waiting for PID %d\n", (int)mtpid);
    err= aos_rpc_process_wait(spawnd, mtpid, &code);
    assert(err_is_ok(err));
    debug_printf("PID %d exited with code %d\n", (int)mtpid, code);
}

static void
spawn_concurrent(struct aos_rpc *spawnd, size_t n, char *cmdline) {
    domainid_t *mtpid;
    errval_t err;

    mtpid= malloc(n * sizeof(domainid_t));
    assert(mtpid);

    debug_printf("Spawning %zu concurrent \"%s\"\n", n, cmdline);

    for(size_t i= 0; i < n; i++) {
        err = aos_rpc_process_spawn(spawnd, cmdline, 0, &mtpid[i]);
        assert(err_is_ok(err));
        assert(mtpid[i]);
    }

    for(size_t i= 0; i < n; i++) {
        int code;

        debug_printf("Waiting for PID %d\n", (int)mtpid[i]);
        err= aos_rpc_process_wait(spawnd, mtpid[i], &code);
        assert(err_is_ok(err));
        debug_printf("PID %d exited with code %d\n", (int)mtpid[i], code);
    }

    free(mtpid);
}
#endif


#define EXPECT_SUCCESS(err, test) \
    do{ \
        if(err_is_fail(err)){ \
            DEBUG_ERR(err, test); \
        } else { \
            debug_printf("SUCCESS: " test "\n");\
        }\
   } while(0);\

static void wait(void)
{
    for (int i = 0; i < 0xffff; i++) {
        sys_nop();
    }
}

#define SPAWN_CHILD_NAME "m2_echo_args"
#define SPAWN_NUM 16

__attribute__((unused))
static void test_spawn_many(struct aos_rpc *spawnd, size_t n)
{
    domainid_t *mtpid;
    errval_t err;

    mtpid= malloc(n * sizeof(domainid_t));
    assert(mtpid);

    debug_printf("#############################################\n");
    debug_printf("Spawning %zu concurrent \"%s\"\n", n, SPAWN_CHILD_NAME);
    debug_printf("#############################################\n");

    for(size_t i= 0; i < n; i++) {
        err = aos_rpc_process_spawn(spawnd, SPAWN_CHILD_NAME, 0, &mtpid[i]);
        EXPECT_SUCCESS(err, "Spawning " SPAWN_CHILD_NAME);
        assert(mtpid[i]);
    }
    free(mtpid);

    wait();

    debug_printf("#############################################\n");
}


__attribute__((unused))
static void test_spawn_args(struct aos_rpc *spawnd)
{
    domainid_t mtpid;
    errval_t err;

    debug_printf("#############################################\n");
    debug_printf("Spawning with args \"%s\"\n", SPAWN_CHILD_NAME);
    debug_printf("#############################################\n");

    err = aos_rpc_process_spawn(spawnd, "m2_echo_args arg1", 0, &mtpid);
    EXPECT_SUCCESS(err, "Spawning 'm2_echo_args arg1'");

    wait();

    err = aos_rpc_process_spawn(spawnd, "m2_echo_args arg1", 1, &mtpid);
    EXPECT_SUCCESS(err, "Spawning 'm2_echo_args arg1' cross core");

    wait();

    err = aos_rpc_process_spawn(spawnd, "m2_echo_args arg1 arg2 arg3 arg4 arg5", 0, &mtpid);
    EXPECT_SUCCESS(err, "Spawning 'm2_echo_args arg1 arg2 arg3 arg4 arg5'");

    wait();

    err = aos_rpc_process_spawn(spawnd, "m2_echo_args arg1 arg2 arg3 arg4 arg5", 1, &mtpid);
    EXPECT_SUCCESS(err, "Spawning 'm2_echo_args arg1 arg2 arg3 arg4 arg5' cross core");

    wait();

    err = aos_rpc_process_spawn(spawnd, "m2_echo_args \"arg 1\" \"arg 2\" \""
                                          "arg 3\" \"arg 4\" \"arg 5\"", 0, 
                                          &mtpid);
    EXPECT_SUCCESS(err, "Spawning 'm2_echo_args \"arg 1\" \"arg 2\" \"arg 3\" "
                        "\"arg 4\" \"arg 5\"'");

    wait();

    err = aos_rpc_process_spawn(spawnd, "m2_echo_args "
                           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 0, &mtpid);
    EXPECT_SUCCESS(err, "Spawning 'm2_echo_args "
                           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'");

    wait();
}


int
main(int argc, char *argv[]) {
    struct aos_rpc *spawnd = aos_rpc_get_process_channel();

    #if TEST == 1
    test_spawn_many(spawnd, SPAWN_NUM);
    #elif TEST == 2 
    test_spawn_args(spawnd);
    #else
    if(argc == 1){
        debug_printf("Usage: %s [many|args]\n", argv[0]);
    } else if(argc == 2) {
        if(strcmp("many", argv[1]) == 0) {
            test_spawn_many(spawnd, SPAWN_NUM);
        } else if(strcmp("args", argv[1]) == 0) {
            test_spawn_args(spawnd);
        }
    }
    #endif

    return EXIT_SUCCESS;
}
