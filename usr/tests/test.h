#ifndef _TESTS_TESTS_H_
#define _TESTS_TESTS_H_

#define MAX_N_TESTS 500

#define TEST_PRINT(message)                         \
    debug_printf("%s: %s\n", __func__, message)
#define TEST_PRINT_INFO(message)                    \
    debug_printf("\033[33m %s: %s \033[0m\n",       \
                 __func__, message)
#define TEST_PRINT_SUCCESS()                        \
    debug_printf("\033[33m Test: %s: "              \
                 "\033[0m \033[32m OK "             \
                 "\033[0m\n\n\n", __func__);        \
    return 0
#define TEST_PRINT_FAIL()                           \
    DEBUG_ERR(err,"err: ");                         \
    debug_printf("\033[33m Test: %s: "              \
                 "\033[0m \033[31m FAILED "         \
                 "\033[0m\n\n\n", __func__);        \
    USER_PANIC("test failed");                      \
    return 1

#include "mm_tests.h"
#include "spawn_tests.h"

struct tester {
    int (*tests[MAX_N_TESTS])(void);
    int num_tests;
};

static void register_test(struct tester *t, int(*test)(void))
{
    t->tests[t->num_tests] = test;
    t->num_tests += 1;
}

static void init_testing(struct tester *t)
{
    t->num_tests = 0;
}

static void tests_run(struct tester *t)
{
    debug_printf("\n");
    debug_printf("\033[33m ###################### \033[0m\n");
    debug_printf("\033[33m Running %d tests \033[0m\n", t->num_tests);
    debug_printf("\033[33m ###################### \033[0m\n\n\n");

    int fails = 0;
    for (int i = 0; i < t->num_tests; i++) {
        if (t->tests[i]()) {
            fails += 1;
        }
    }
    int passes = t->num_tests - fails;

    debug_printf("\033[33m ############################ \033[0m\n");
    debug_printf("\033[33m Tests have run to completion \033[0m\n");
    debug_printf("\033[33m %d/%d passes\033[0m\n", passes, t->num_tests);
    debug_printf("\033[33m %d/%d fails\033[0m\n", fails, t->num_tests);
    debug_printf("\033[33m ############################ \033[0m\n\n\n");
}

static void register_memory_tests(struct tester *t)
{
    // mm tests
    register_test(t, mm_alloc_300f);
    register_test(t, mm_alloc_free_600);
    register_test(t, mm_alloc_free_20);
    register_test(t, mm_alloc_free_10);
    register_test(t, mm_alloc_and_map_10f);
    register_test(t, mm_alloc_and_map_large_10f);
    register_test(t, mm_paging_map_fixed_attr_cursize_test);
    register_test(t, mm_paging_alloc_aligned_allignment_test);
}

static void register_spawn_tests(struct tester *t)
{
    register_test(t, spawn_hello1);
    register_test(t, spawn_hello5);
}

#endif /* _TESTS_TESTS_H_ */
