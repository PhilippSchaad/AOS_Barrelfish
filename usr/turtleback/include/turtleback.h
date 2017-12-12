/**
 * \file
 * \brief TurtleBack Shell functions
 */

/*
 * Copyright (c) 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems
 * Group.
 */

#ifndef _TURTLEBACK_H_
#define _TURTLEBACK_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/inthandler.h>

#include <barrelfish_kpi/asm_inlines_arch.h>

#define TURTLEBACK_VERSION_MAJOR    0
#define TURTLEBACK_VERSION_MINOR    0
#define TURTLEBACK_PATCH_LEVEL      1

#define IRQ_ID_UART                 106

#define INPUT_BUFFER_LENGTH         5000

#define TURTLEBACK_DEFAULT_PROMPT   ">\033[33mTurtleBack\033[0m$ "

#define HELP_USAGE                  "help [cmd]"
#define ONCORE_USAGE                "oncore [0|1] [cmd [..]]"
#define MEMTEST_USAGE               "memtest [size (MB)]"
#define TIME_USAGE                  "time [cmd [..]]"

#define CLOCK_FREQUENCY             1200000000 // PB_ES CLK Frequency (Hz)

typedef void (*shell_cmd_handler)(int argc, char **argv);

struct shell_cmd {
    char *cmd;
    char *help_text;
    char *usage;
    shell_cmd_handler invoke;
};

// Declaration of TurtleBack builtin functions.
void shell_invalid_command(char *cmd);
void shell_help(int argc, char **argv);
void shell_clear(int argc, char **argv);
void shell_echo(int argc, char **argv);
void shell_oncore(int argc, char **argv);
void shell_ps(int argc, char **argv);
void shell_led(int argc, char **argv);
void shell_memtest(int argc, char **argv);
void shell_run_testsuite(int argc, char **argv);
void shell_time(int argc, char **argv);

// List of TurtleBack builtin functions.
static struct shell_cmd shell_builtins[] = {
    {
        .cmd = "echo",
        .help_text = "Display a line of text",
        .usage = "echo ..",
        .invoke = shell_echo
    },
    {
        .cmd = "clear",
        .help_text = "Clear the terminal screen",
        .usage = "clear",
        .invoke = shell_clear
    },
    {
        .cmd = "help",
        .help_text = "Display the help text",
        .usage = HELP_USAGE,
        .invoke = shell_help
    },
    {
        .cmd = "oncore",
        .help_text = "Run a process on a specific core [0|1]",
        .usage = ONCORE_USAGE,
        .invoke = shell_oncore
    },
    {
        .cmd = "ps",
        .help_text = "Report a snapshot of the current processes",
        .usage = "ps",
        .invoke = shell_ps
    },
    {
        .cmd = "led",
        .help_text = "Toggle the D2 LED",
        .usage = "led",
        .invoke = shell_led
    },
    {
        .cmd = "memtest",
        .help_text = "Test a region of memory for read/write errors",
        .usage = MEMTEST_USAGE,
        .invoke = shell_memtest
    },
    {
        .cmd = "testsuite",
        .help_text = "Run the included testsuite",
        .usage = "testsuite",
        .invoke = shell_run_testsuite
    },
    {
        .cmd = "time",
        .help_text = "Time a certain program or command",
        .usage = TIME_USAGE,
        .invoke = shell_time
    },
    // Builtins list terminator.
    {
        .cmd = NULL,
        .help_text = NULL,
        .usage = NULL,
        .invoke = NULL
    }
};

#endif /* _TURTLEBACK_H_ */
