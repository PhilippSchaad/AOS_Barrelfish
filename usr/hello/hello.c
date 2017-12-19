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

#include <aos/aos.h>

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    printf("Hello, world! from userspace\n");
    if (argc > 0) {
        for (int i = 0; i < argc; ++i) {
            printf("Found command line argument %d: %s \n", i, argv[i]);
        }
    }
}
