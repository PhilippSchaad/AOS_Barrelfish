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
#include <spawn/spawn.h>
#include <grading.h>

static errval_t
spawn_nowait(struct aos_rpc *spawnd, coreid_t core, char *cmdline) {
    domainid_t mtpid;
    errval_t err;

    debug_printf("Spawning \"%s\" on core %d\n", cmdline, core);
    err = aos_rpc_process_spawn(spawnd, cmdline, core, &mtpid);
    if(!err_is_ok(err)) return err;
    debug_printf("PID of \"%s\" is %d\n", cmdline, (int)mtpid);

    return SYS_ERR_OK;
}

#if 0

/*
  XXX the following two funcions are not suitable for 2016's AOS course
      as the student were not required to implement the wait function
 */

static errval_t
spawn_and_wait(struct aos_rpc *spawnd, coreid_t core, char *cmdline) {
    domainid_t mtpid;
    errval_t err;
    int code;

    debug_printf("Spawning \"%s\" on core %d\n", cmdline, core);
    err = aos_rpc_process_spawn(spawnd, cmdline, core, &mtpid);
    if(!err_is_ok(err)) return err;

    debug_printf("Waiting for PID %d\n", (int)mtpid);
    err= aos_rpc_process_wait(spawnd, mtpid, &code);
    if(!err_is_ok(err)) return err;

    debug_printf("PID %d exited with code %d\n", (int)mtpid, code);

    return SYS_ERR_OK;
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

static errval_t
count_processes(const char *proc_name, int *count) {
    domainid_t* pids;
    size_t len;
    errval_t err;

    *count= 0;

    err= aos_rpc_process_get_all_pids(get_init_rpc(), &pids, &len);
    if(err_is_fail(err)) return err;

    for(int i = 0; i < len; i++) {
        char *name;
        domainid_t pid= pids[i];

        err= aos_rpc_process_get_name(get_init_rpc(), pid, &name);
        if(err_is_fail(err)) {
            free(pids);
            return err;
        }

        if(!strcmp(name, proc_name)) (*count)++;
    }

    return SYS_ERR_OK;
}

int
main(int argc, char *argv[]) {
    struct aos_rpc *spawnd = aos_rpc_get_process_channel();
    errval_t err;

    if(argc > 1 && !strcmp(argv[1], "client")) {
        /* We are the client spawned on core 1. */
        debug_printf("Client process.\n");

        /* Verify that we can do a ps from core 1. */
        int spin_count;
        err= count_processes("spin", &spin_count);
        if(err_is_ok(err)) {
            if(spin_count == 2) {
                grading_test_pass("7, Global ps from core 1.", "n == 2\n");
            }
            else {
                grading_test_fail("7, Global ps from core 1.", "n != 2\n");
            }
        }
        else grading_test_fail("7, Global ps from core 1.",
                               "%s\n", err_getstring(err));

        /* Spawn on each core. */
        err= spawn_nowait(spawnd, 0, "spin");
        if(err_is_ok(err)) grading_test_pass("5, Spawn spin on 0 from 1.","\n");
        else grading_test_fail("5, Spawn spin on 0 from 1.",
                               "%s\n", err_getstring(err));

        spawn_nowait(spawnd, 1, "spin");
        if(err_is_ok(err)) grading_test_pass("6, Spawn spin on 1 from 1.","\n");
        else grading_test_fail("6, Spawn spin on 1 from 1.",
                               "%s\n", err_getstring(err));

        /* There should now be 4. */
        err= count_processes("spin", &spin_count);
        if(err_is_ok(err)) {
            if(spin_count == 4) {
                grading_test_pass("8, Global ps from core 1.", "n == 4\n");
            }
            else {
                grading_test_fail("8, Global ps from core 1.", "n != 4\n");
            }
        }
        else grading_test_fail("8, Global ps from core 1.",
                               "%s\n", err_getstring(err));
    }
    else {
        /* We are the original process on core 0. */

        /* Spawn a simple process on each core. */
        err= spawn_nowait(spawnd, 0, "spin");
        if(err_is_ok(err)) grading_test_pass("0, Spawn spin on 0 from 0.","\n");
        else grading_test_fail("0, Spawn spin on 0 from 0.",
                               "%s\n", err_getstring(err));

        spawn_nowait(spawnd, 1, "spin");
        if(err_is_ok(err)) grading_test_pass("1, Spawn spin on 1 from 0.","\n");
        else grading_test_fail("1, Spawn spin on 1 from 0.",
                               "%s\n", err_getstring(err));

        /* Verify that they both show up in ps. */
        int spin_count;
        err= count_processes("spin", &spin_count);
        if(err_is_ok(err)) {
            if(spin_count == 2) {
                grading_test_pass("2, Global ps from core 0.", "n == 2\n");
            }
            else {
                grading_test_fail("2, Global ps from core 0.", "n != 2\n");
            }
        }
        else grading_test_fail("2, Global ps from core 0.",
                               "%s\n", err_getstring(err));

#if 0
        /* Spawn a client on core 1, to spawn more children. */
        err= spawn_and_wait(spawnd, 1, "m6_test client");
        if(err_is_ok(err)) grading_test_pass("Spawn client on 1.","\n");
        else grading_test_fail("3, Spawn client on 1.",
                               "%s\n", err_getstring(err));
#endif

        /* There should now be 4 copies of spin running. */
        err= count_processes("spin", &spin_count);
        if(err_is_ok(err)) {
            if(spin_count == 4) {
                grading_test_pass("4, Global ps from core 0.", "n == 4\n");
            }
            else {
                grading_test_fail("4, Global ps from core 0.", "n != 4\n");
            }
        }
        else grading_test_fail("4, Global ps from core 0."
                               "%s\n", err_getstring(err));
    }

    return EXIT_SUCCESS;
}
