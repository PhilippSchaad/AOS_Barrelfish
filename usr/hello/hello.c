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
    debug_printf("Hello, world! from userspace\n");
    if (argc > 0){
        for (int i=0; i<argc; ++i){
            debug_printf("Found command line argument %d: %s \n", i, argv[i]);
        }
    }

    char *invalid_address = (char *)0xffff1232;
    char letter = *invalid_address;
    printf("%c\n", letter);
    return 0;
}
