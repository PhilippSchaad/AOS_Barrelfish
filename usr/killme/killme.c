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
    printf("Hello, I am the killme domain. I don't want to die, instead I'd "
           "like to spam you with messages.\n");
    while (true) {
        debug_printf("muahaha... I am still alive :D\n");
        volatile int i;
        for (i = 0; i < 2 << 20; ++i)
            ;
    }
}
