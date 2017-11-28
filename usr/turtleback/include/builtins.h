/**
 * \file
 * \brief TurtleBack Shell builtin functions
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

#ifndef _BUILTINS_H_
#define _BUILTINS_H_

#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>

void shell_echo(int argc, char **argv);

#endif /* _BUILTINS_H_ */
