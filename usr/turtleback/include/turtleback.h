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

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/inthandler.h>

#include <builtins.h>

#define TURTLEBACK_VERSION_MAJOR    0
#define TURTLEBACK_VERSION_MINOR    0
#define TURTLEBACK_PATCH_LEVEL      1

#define IRQ_ID_UART                 106

#define INPUT_BUFFER_LENGTH         5000

#define TURTLEBACK_DEFAULT_PROMPT   ">\033[33mTurtleBack\033[0m$ \0"

typedef void (*shell_cmd_handler)(int argc, char **argv);

struct shell_cmd {
    char *cmd;
    char *help_text;
    shell_cmd_handler invoke;
};

#endif /* _TURTLEBACK_H_ */
