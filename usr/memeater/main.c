/**
 * \file
 * \brief init process for child spawning
 */

/*
 * Copyright (c) 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/waitset.h>
#include <aos/paging.h>

static struct aos_rpc init_rpc;

const char *str = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, "
                  "sed do eiusmod tempor incididunt ut labore et dolore magna "
                  "aliqua. Ut enim ad minim veniam, quis nostrud exercitation "
                  "ullamco laboris nisi ut aliquip ex ea commodo consequat. "
                  "Duis aute irure dolor in reprehenderit in voluptate velit "
                  "esse cillum dolore eu fugiat nulla pariatur. Excepteur "
                  "sint occaecat cupidatat non proident, sunt in culpa qui "
                  "officia deserunt mollit anim id est laborum.";

static errval_t request_and_map_memory(void)
{
    errval_t err;

    size_t bytes;
    struct frame_identity id;
    debug_printf("\033[33mtesting memory server...\033[0m\n");

    struct paging_state *pstate = get_current_paging_state();

    debug_printf("\033[33mobtaining cap of %" PRIu32 " bytes...\033[0m\n",
                 BASE_PAGE_SIZE);

    struct capref cap1;
    err = aos_rpc_get_ram_cap(&init_rpc, BASE_PAGE_SIZE, BASE_PAGE_SIZE, &cap1,
                              &bytes);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "\033[31mcould not get BASE_PAGE_SIZE cap\n\033[0m");
        return err;
    }

    struct capref cap1_frame;
    err = slot_alloc(&cap1_frame);
    assert(err_is_ok(err));

    err = cap_retype(cap1_frame, cap1, 0, ObjType_Frame, BASE_PAGE_SIZE, 1);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "\033[31mcould not retype RAM cap to frame "
                  "cap\n\033[0m");
        return err;
    }

    err = invoke_frame_identify(cap1_frame, &id);
    assert(err_is_ok(err));

    void *buf1;
    err = paging_map_frame(pstate, &buf1, BASE_PAGE_SIZE, cap1_frame, NULL,
                           NULL);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "\033[31mcould not map BASE_PAGE_SIZE cap\n\033[0m");
        return err;
    }

    debug_printf("\033[33mgot frame: 0x%" PRIxGENPADDR " mapped at %p\n"
                 "\033[0m", id.base, buf1);

    debug_printf("\033[33mperforming memset.\n\033[0m");
    memset(buf1, 0x00, BASE_PAGE_SIZE);



    debug_printf("\033[33mobtaining cap of %" PRIu32 " bytes using frame "
                 "alloc...\n\033[0m", LARGE_PAGE_SIZE);

    struct capref cap2;
    err = frame_alloc(&cap2, LARGE_PAGE_SIZE, &bytes);
    if (err_is_fail(err)) {
        debug_printf("\033[31merr code: %s\n\033[0m", err_getcode(err));
        DEBUG_ERR(err, "\033[31mcould not get LARGE_PAGE_SIZE cap\n\033[0m");
        return err;
    }

    err = invoke_frame_identify(cap2, &id);
    assert(err_is_ok(err));

    void *buf2;
    err = paging_map_frame(pstate, &buf2, LARGE_PAGE_SIZE, cap2, NULL, NULL);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "\033[31mcould not map LARGE_PAGE_SIZE cap\n\033[0m");
        return err;
    }

    debug_printf("\033[33mgot frame: 0x%" PRIxGENPADDR " mapped at %p\n"
                 "\033[0m", id.base, buf2);

    debug_printf("\033[33mperforming memset.\n\033[0m");
    memset(buf2, 0x00, LARGE_PAGE_SIZE);
    debug_printf("\033[32mSUCCESS\n\033[0m");

    return SYS_ERR_OK;
}

static errval_t test_basic_rpc(void)
{
    errval_t err;

    debug_printf("\033[33mRPC: testing basic RPCs...\033[0m\n");

    debug_printf("\033[33mRPC: sending number...\033[0m\n");
    err =  aos_rpc_send_number(&init_rpc, 42);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "\033[31mcould not send a string\033[0m\n");
        return err;
    }

    debug_printf("\033[33mRPC: sending small string...\033[0m\n");
    err =  aos_rpc_send_string(&init_rpc, "Hello init");
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "\033[31mcould not send a string\033[0m\n");
        return err;
    }

    debug_printf("\033[33mRPC: sending large string...\033[0m\n");
    err =  aos_rpc_send_string(&init_rpc, str);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "\033[31mcould not send a string\033[0m\n");
        return err;
    }

    debug_printf("\033[33mRPC: testing basic RPCs. \033[32mSUCCESS\033[0m\n");

    return SYS_ERR_OK;
}

int main(int argc, char *argv[])
{
    errval_t err;

    debug_printf("memeater started....\n");

    err = aos_rpc_init(&init_rpc);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "\033[31mcould not initialize RPC\033[0m\n");
    }

    err = test_basic_rpc();
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "\033[31mfailure in testing basic RPC\033[0m\n");
    }

    err = request_and_map_memory();
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "\033[31mcould not request and map memory\033[0m\n");
    }


    /* test printf functionality */
    debug_printf("\033[33mtesting terminal printf function...\033[0m\n");

    printf("Hello world using terminal service\n");


    domainid_t ret;
    aos_rpc_process_spawn(&init_rpc, "hello", 1, &ret);
    char *name;
    aos_rpc_process_get_name(&init_rpc, ret, &name);
    debug_printf("\033[33mWe spawned 'hello' and then requested the name of "
                 "the process with its idea, result: %s\n\033[0m", name);
    //disabled because it gets ugly, as the name would indicate
    //aos_rpc_process_spawn(&init_rpc,"forkbomb",1,&ret);

    struct capref slot[2000];
    debug_printf("\033[33mTry to allocate 2000 slots\n\033[0m");
    for (int i=1; i<=2000; ++i){
        err = slot_alloc(&slot[i-1]);
        if (err_is_fail(err)) {
            return err_push(err, LIB_ERR_SLOT_ALLOC);
        }
        /*
        if (i%20==0){
            debug_printf("Created %d slots\n", i);
        }
        */
    }
    debug_printf("\033[33mCreated them all\n\033[0m");
    for (int i = 1; i <= 2000; ++i){
        err = slot_free(slot[i - 1]);
        if (err_is_fail(err)) {
            return err_push(err, LIB_ERR_SLOT_ALLOC);
        }
    }
    debug_printf("\033[33mFreed them again...\n\033[0m");
    debug_printf("\033[32mSUCCESS\n\033[0m");

    debug_printf("\033[33mWe're now gonna alloc a large array and only access "
                 "a few tiny parts of it..\n\033[0m");
    char *large_array = (char *) malloc(sizeof(char) * 1000000000);
    debug_printf("\033[33mAllocated 1GB array\n\033[0m");
    int offset = 500000000;
    for (int i = 0; i < 100; i++) {
        large_array[i + offset] = 'x';
    }
    debug_printf("\033[33mAssigned 100 places towards the middle a value 'x'\n\033[0m");
    debug_printf("\033[33mNow printing those values:\n\033[0m");
    for (int i = 0; i < 100; i++) {
        printf("%c", large_array[i + offset]);
    }
    printf("\n");
    printf("\033[32mSUCCESS\033[0m");
    printf("\n");


    thread_create((thread_func_t) test_basic_rpc, NULL);


    debug_printf("seems I have done everything I should... =)\n");
    debug_printf("memeater terminated....\n");
    return EXIT_SUCCESS;
}
