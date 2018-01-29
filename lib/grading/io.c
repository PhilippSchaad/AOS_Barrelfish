#include <stdarg.h>
#include <stdio.h>

#include <aos/aos.h>
#include <aos/sys_debug.h>
#include <grading.h>

static void
raw_vprintf(const char *fmt, va_list argptr) {
    char str[256];

    vsnprintf(str, sizeof(str), fmt, argptr);
    sys_print(str, sizeof(str));
}

static void
grading_vprintf(const char *fmt, va_list argptr) {
    const char str[]= "<grading> ";
    sys_print(str, sizeof(str));
    raw_vprintf(fmt, argptr);
}

void
grading_test_fail(const char *test, const char *fmt, ...) {
    va_list argptr;

    grading_printf("TEST % 5s FAILED: ", test);

    va_start(argptr, fmt);
    raw_vprintf(fmt, argptr);
    va_end(argptr);
}

void
grading_test_pass(const char *test, const char *fmt, ...) {
    va_list argptr;

    grading_printf("TEST % 5s PASSED: ", test);

    va_start(argptr, fmt);
    raw_vprintf(fmt, argptr);
    va_end(argptr);
}

void
grading_panic(const char *fmt, ...) {
    va_list argptr;

    grading_printf("terminated: ");

    va_start(argptr, fmt);
    raw_vprintf(fmt, argptr);
    va_end(argptr);

    while(1);
}

void
grading_printf(const char *fmt, ...) {
    va_list argptr;

    va_start(argptr, fmt);
    grading_vprintf(fmt, argptr);
    va_end(argptr);
}

void
grading_printf_nb(const char *fmt, ...) {
    va_list argptr;

    va_start(argptr, fmt);
    raw_vprintf(fmt, argptr);
    va_end(argptr);
}
