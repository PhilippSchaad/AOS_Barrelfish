/**
 * \file
 * \brief ram allocator functions
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

#ifndef _INIT_MEM_ALLOC_H_
#define _INIT_MEM_ALLOC_H_

#include <aos/aos.h>
#include <stdio.h>

extern struct bootinfo *bi;
extern struct mm aos_mm;

errval_t initialize_ram_alloc(struct bootinfo *bi_override);
errval_t aos_ram_free(struct capref cap, size_t bytes);
errval_t aos_ram_alloc_aligned(struct capref *ret, size_t size,
                               size_t alignment);

#endif /* _INIT_MEM_ALLOC_H_ */
