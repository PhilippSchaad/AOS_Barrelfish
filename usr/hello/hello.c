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
#include <stdlib.h>

int main(int argc, char *argv[])
{
    printf("Hello, world! from userspace\n");
    if (argc > 0){
        for (int i=0; i<argc; ++i){
            printf("Found command line argument %d: %s \n", i, argv[i]);
        }
    }
    size_t st = *(size_t*)0x1000;
    printf("we are paging_state number: %u\n",st);
    void *t = malloc(0x2000);
    printf("we just malloc'd at the addr: %p\n",t);
    printf("and now we are writing to it!\n");
    for(int i = 0; i < 0x2000; i++) {
        ((char*)t)[i] = 'i';
    }
    return 0;
}
