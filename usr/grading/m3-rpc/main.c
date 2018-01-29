/**
 * \file
 * \brief Hello world application
 */

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


#define TINY_STRING "Lorem"

#define SMALL_STRING "Lorem ipsum dolor sit amet amet."

#define MEDIUM_STRING "Lorem ipsum dolor sit amet, consectetur adipiscing elit. " \
                      "Donec id volutpat neque. Mauris gravida mi in tortor rhoncus,"\
                      "sed ultrices nulla scelerisque. Nullam auctor nunc in purus "\
                      "convallis pretium at in arcu. Donec ut sem blandit erat "\
                      "elementum porta amet."

#define LARGE_STRING "Lorem ipsum dolor sit amet, consectetur adipiscing elit." \
                     "Duis convallis elit ac turpis hendrerit imperdiet. Quisque "\
                     "sodales mauris in urna rutrum tristique. Vivamus a efficitur "\
                     "elit. Vestibulum consequat lacus ultrices lorem aliquet, at "\
                     "finibus ante hendrerit. Vestibulum tempor risus vitae neque "\
                     "blandit, ut laoreet tortor egestas. Phasellus ipsum nulla, "\
                     "ultricies id gravida nec, volutpat sed elit. Mauris varius "\
                     "mauris et nunc pharetra, vel blandit ipsum varius."\
                     "Vivamus cursus tellus id finibus interdum. Donec consectetur "\
                     "facilisis sem nec sollicitudin. Vivamus id mauris interdum, "\
                     "cursus turpis sit amet, maximus nulla. Suspendisse potenti. "\
                     "Cras interdum porta iaculis. Phasellus condimentum urna purus,"\
                     " id sollicitudin risus aliquet ac. Aenean id ante vitae purus "\
                     "efficitur lacinia. Mauris vulputate faucibus vestibulum. "\
                     "Donec non eros sed elit porttitor elementum. Nulla sagittis "\
                     "elit sed velit vestibulum, vitae sagittis risus convallis. "\
                     "Donec et ipsum sed nibh venenatis interdum id vitae lectus. "\
                     "Sed posuere metus."


#define HUGE_STRING "Lorem ipsum dolor sit amet, consectetur adipiscing elit. " \
        "Mauris pharetra risus non orci viverra accumsan. Maecenas " \
        "eu lacus cursus, finibus ligula accumsan, hendrerit massa. " \
        "Quisque tincidunt non nisi nec fermentum. Vivamus sagittis " \
        "euismod dui, vitae dignissim nisl porttitor ut. Nulla " \
        "ullamcorper non ipsum ut placerat. Duis in tincidunt sapien. " \
        "Sed sollicitudin lorem nec neque laoreet pretium. Pellentesque " \
        "vitae cursus turpis. Maecenas aliquet feugiat massa non elementum. " \
        "Vivamus vel massa lacinia, ultrices sem vel, viverra nunc. " \
        "Cras sit amet arcu vel enim congue facilisis quis eget dui. " \
        "Pellentesque habitant morbi tristique senectus et netus et " \
        "malesuada fames ac turpis egestas. Nunc urna tortor, " \
        "lobortis sed sagittis sit amet, accumsan euismod nulla. " \
        "Cras aliquam ullamcorper augue, eu maximus ligula placerat " \
        "facilisis. Sed non convallis arcu. Nam sollicitudin et elit et iaculis." \
        "Nam feugiat aliquet elit eget venenatis. Proin vitae nunc justo. " \
        "Proin eget laoreet velit. Sed turpis dui, molestie eu elit vel, " \
        "congue sagittis metus. Sed id pharetra ante. Interdum et " \
        "malesuada fames ac ante ipsum primis in faucibus. Aenean " \
        "leo massa, ultrices vitae volutpat at, ultricies eu " \
        "tortor. Cras consequat bibendum ex ut ultrices. Nam " \
        "convallis tempus leo, ut gravida quam lacinia sed. " \
        "Integer risus magna, interdum quis ipsum eget, tempus " \
        "fringilla quam." \
        "In hac habitasse platea dictumst. Vivamus porta, lorem " \
        "quis imperdiet laoreet, ex magna facilisis nulla, ut " \
        "suscipit nisl arcu at justo. Praesent interdum tempus " \
        "libero eget volutpat. Etiam pellentesque, ex scelerisque " \
        "gravida feugiat, sem mauris facilisis enim, eget imperdiet " \
        "est quam venenatis diam. Nullam sit amet pellentesque lectus, " \
        "nec tempor sapien. Integer maximus a arcu a aliquet. Etiam " \
        "et pharetra ex. Praesent eget nibh erat. Suspendisse " \
        "finibus porttitor pellentesque. Aliquam tincidunt orci " \
        "sit amet ipsum tincidunt, et imperdiet tellus faucibus. " \
        "Praesent sodales lectus massa, nec egestas risus ornare congue." \
        "Cras consequat tristique mattis. Vivamus rhoncus eget " \
        "dolor ut tincidunt. Ut metus urna, placerat eu maximus " \
        "in, faucibus quis augue. In facilisis odio quis consequat " \
        "pharetra. Nam sodales rhoncus nulla, non volutpat lacus " \
        "iaculis quis. Nulla euismod orci ut odio venenatis, quis " \
        "ultrices justo pellentesque. Aenean viverra consectetur " \
        "mi ac molestie. Suspendisse in libero dolor. Maecenas " \
        "at odio a urna malesuada luctus. Vestibulum consequat " \
        "elementum arcu, vitae ornare dui luctus at. Pellentesque " \
        "vestibulum laoreet lorem et ultricies. Suspendisse vel " \
        "faucibus purus, ut suscipit tellus. Cras a lacus felis. " \
        "Donec dui lorem, feugiat non tincidunt id, sollicitudin " \
        "dapibus sem. Proin hendrerit scelerisque tempus." \
        "Duis eu ex augue. Aenean non risus eget lacus fringilla " \
        "malesuada id sit amet sapien. Sed sed pellentesque dolor, " \
        "non laoreet risus. Cras lacinia mattis erat, nec maximus " \
        "tellus sollicitudin a. Ut mattis, mauris sit amet consequat " \
        "venenatis, est sapien facilisis nibh, a accumsan magna" \
        " massa sit amet urna. Nunc ornare lacus sagittis diam " \
        "euismod, a maximus sapien commodo. Donec lacinia turpis " \
        "sit amet eros tempus eleifend. Praesent sit amet neque ex. " \
        "Nulla vel vestibulum lectus. Aenean tempor, erat at faucibus " \
        "lacinia, risus ante posuere dui, et pretium neque mauris sit " \
        "amet lectus. Fusce sed nibh finibus, laoreet libero non, " \
        "aliquam neque. Praesent vel sagittis nisi. Nulla eu " \
        "cursus magna, in bibendum nibh. Duis mattis, nisl nec " \
        "tempus vulputate, arcu sapien porttitor ante, vel " \
        "consectetur libero dolor sed ante." \
        "Integer scelerisque condimentum iaculis. Vivamus nulla " \
        "nunc, consequat sed consectetur id, finibus eu urna. " \
        "Morbi varius sollicitudin risus sit amet tempor. In " \
        "tincidunt, sapien ut consequat bibendum, tellus metus " \
        "scelerisque tortor, vitae porta diam enim quis ex. " \
        "Donec felis lectus, sodales in nisi eget, tempor lacinia " \
        "metus. In hac habitasse platea dictumst. Vestibulum " \
        "ullamcorper tincidunt finibus. Sed fermentum odio quam, " \
        "a molestie nulla pulvinar ac. Aenean nec massa porttitor, " \
        "convallis dui ac, porta velit. Sed malesuada nunc eget " \
        "purus porta maximus nullam."

#define EXPECT_SUCCESS(err, test) \
    do{ \
        if(err_is_fail(err)){ \
            DEBUG_ERR(err, test); \
        } else { \
            debug_printf("SUCCESS: " test "\n");\
        }\
   } while(0);\


#define PRINT_PREFIX "<grading> "

#define PRINT_TEST_PREAMBLE(test) \
    printf(PRINT_PREFIX "---------------------------------------\n"); \
    printf(PRINT_PREFIX "Test:  %s\n", test);

#define PRINT_TEST_STEP(step, size) \
    do { \
        printf(PRINT_PREFIX " - %s: %" PRIu32 "\n", step, size); \
    } while(0);

#define PRINT_TEST_STEP_RESULT(step, result) \
    do { \
        if (err_is_ok(result)) { \
            printf(PRINT_PREFIX " - %s: OK\n", step, (err_is_ok(result) ? "OK" : "FAIL: ")); \
        } else { \
            printf(PRINT_PREFIX " - %s: FAIL (%s, %s)\n", step, err_getcode(result), err_getstring(result)); \
        } \
    } while(0);


static void test_lmp(void)
{
    PRINT_TEST_PREAMBLE("LMP-SIMPLE");
    /* get the init ep */
}

static void test_simple_rpc(void)
{
    errval_t err;

    PRINT_TEST_PREAMBLE("RPC-SIMPLE");

    err = aos_rpc_send_number(get_init_rpc(), 42);
    PRINT_TEST_STEP_RESULT("send-number-rpc", err);

    err = aos_rpc_send_string(get_init_rpc(), "hello");
    PRINT_TEST_STEP_RESULT("send-string-rpc", err);
}

static void test_string_rpc(void)
{
    errval_t err;

    PRINT_TEST_PREAMBLE("RPC-STRING");

    printf(PRINT_PREFIX "strlen(TINY_STRING)=%zu\n", strlen(TINY_STRING));
    err = aos_rpc_send_string(get_init_rpc(), TINY_STRING);
    PRINT_TEST_STEP_RESULT("string-rpc-tiny", err);

    printf(PRINT_PREFIX "strlen(SMALL_STRING)=%zu\n", strlen(SMALL_STRING));
    err = aos_rpc_send_string(get_init_rpc(), SMALL_STRING);
    PRINT_TEST_STEP_RESULT("string-rpc-small", err);

    printf(PRINT_PREFIX "strlen(MEDIUM_STRING)=%zu\n", strlen(MEDIUM_STRING));
    err = aos_rpc_send_string(get_init_rpc(), MEDIUM_STRING);
    PRINT_TEST_STEP_RESULT("string-rpc-medium", err);

    printf(PRINT_PREFIX "strlen(LARGE_STRING)=%zu\n", strlen(LARGE_STRING));
    err = aos_rpc_send_string(get_init_rpc(), LARGE_STRING);
    PRINT_TEST_STEP_RESULT("string-rpc-large", err);

    printf(PRINT_PREFIX "strlen(HUGE_STRING)=%zu\n", strlen(HUGE_STRING));
    err = aos_rpc_send_string(get_init_rpc(), HUGE_STRING);
    PRINT_TEST_STEP_RESULT("string-rpc-huge", err);

}

static void test_syscall_rpc(void)
{
    PRINT_TEST_PREAMBLE("RPC-SYSCALL");

    errval_t err;
    size_t bytes;
    struct capref frame;
    err = aos_rpc_get_ram_cap(get_init_rpc(), BASE_PAGE_SIZE, BASE_PAGE_SIZE, &frame, &bytes);
    PRINT_TEST_STEP_RESULT("syscall-ramcap-rpc", err);

    err = aos_rpc_serial_putchar(get_init_rpc(), 'A');
    PRINT_TEST_STEP_RESULT("syscall-putchar-rpc", err);

    err = aos_rpc_serial_putchar(get_init_rpc(), 'O');
    PRINT_TEST_STEP_RESULT("syscall-putchar-rpc", err);

    err = aos_rpc_serial_putchar(get_init_rpc(), 'S');
    PRINT_TEST_STEP_RESULT("syscall-putchar-rpc", err);

    err = aos_rpc_serial_putchar(get_init_rpc(), '\n');
    PRINT_TEST_STEP_RESULT("syscall-putchar-rpc", err);

    domainid_t pid;
    err = aos_rpc_process_spawn(get_init_rpc(), "simplechild", 0, &pid);
    PRINT_TEST_STEP_RESULT("syscall-spawn-rpc", err);
}

#define NTHREAD_MAX 8

#define ITERATIONS 10

static int rpc_thread(void *arg)
{
    errval_t err;

   // uint32_t id = *((uint32_t *)arg);

    for (int i = 0; i < ITERATIONS; i++) {
        err = aos_rpc_send_number(get_init_rpc(), 42);
        PRINT_TEST_STEP_RESULT("concurrent-send-number", err);

        err = aos_rpc_send_string(get_init_rpc(), TINY_STRING);
        PRINT_TEST_STEP_RESULT("concurrent-send-string-tiny", err);

        err = aos_rpc_send_string(get_init_rpc(), SMALL_STRING);
        PRINT_TEST_STEP_RESULT("concurrent-send-string-small", err);

        size_t bytes;
        struct capref frame;
        err = aos_rpc_get_ram_cap(get_init_rpc(), BASE_PAGE_SIZE, BASE_PAGE_SIZE,
                                  &frame, &bytes);
        PRINT_TEST_STEP_RESULT("concurrent-spawn-rpc", err);
    }

    return 0;
}

static errval_t test_concurrent_rpc(void)
{
    PRINT_TEST_PREAMBLE("RPC-CONCURRENT");

    errval_t err;
    struct thread *t[NTHREAD_MAX];
    uint32_t ids[NTHREAD_MAX];

    for (int nthreads = 2; nthreads <= NTHREAD_MAX; nthreads++) {

        PRINT_TEST_STEP("concurrent-rpc", nthreads);

        for (int i = 1; i < nthreads; i++) {
            ids[i] = i;
            t[i] = thread_create(rpc_thread, &ids[i]);
            assert(t[i]);
        }

        rpc_thread(&ids[0]);

        for (int i = 1; i < nthreads; i++) {
            err = thread_join(t[i], NULL);
            assert(err_is_ok(err));
        }
    }

    return SYS_ERR_OK;
}


int main(int argc, char *argv[])
{
    PRINT_TEST_PREAMBLE("RPC-TEST-START");

    test_lmp();
    test_simple_rpc();
    test_string_rpc();
    test_syscall_rpc();
    test_concurrent_rpc();

    PRINT_TEST_PREAMBLE("RPC-TEST-DONE");

    return EXIT_SUCCESS;
}
