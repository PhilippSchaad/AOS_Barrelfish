#ifndef _TESTS_TESTS_H_
#define _TESTS_TESTS_H_

#define TEST_PRINT(message) debug_printf("%s: %s\n", __func__, message)
#define TEST_PRINT_INFO(message) debug_printf("\033[33m %s: %s \033[0m\n", __func__, message)
#define TEST_PRINT_SUCCESS() debug_printf("\033[33m Test: %s: \033[0m \033[32m OK \033[0m\n", __func__); return
#define TEST_PRINT_FAIL() debug_printf("\033[33m Test: %s: \033[0m \033[31m FAILED \033[0m\n", __func__); return

#include <stdio.h>
#include <aos/aos.h>

//  add the tests
#include "mm_tests.h"

static void tests_run(void){
    debug_printf("\033[33m###################### \033[0m\n");
    debug_printf("\033[33mRunning tests \033[0m\n");
    debug_printf("\033[33m###################### \033[0m\n");

    // mm tests
    mm_tests_run();
}

#endif /* _TESTS_TESTS_H_ */
