/**
 * \file
 * \brief Top-level header for convenient inclusion of standard
 * libbarrelfish headers.
 */

/*
 * Copyright (c) 2007, 2008, 2009, 2010, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef LIBBARRELFISH_BARRELFISH_H
#define LIBBARRELFISH_BARRELFISH_H

/* standard libc types and assert */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

/* utility macros */
#include <bitmacros.h>

/* Barrelfish kernel interface definitions */
#include <errors/errno.h>
#include <barrelfish_kpi/types.h>
#include <barrelfish_kpi/capabilities.h>
#include <barrelfish_kpi/cpu.h> // XXX!?
#include <barrelfish_kpi/paging_arch.h>
#include <barrelfish_kpi/syscalls.h>
#include <barrelfish_kpi/init.h> // kernel-defined part of cspace
#include <barrelfish_kpi/registers_arch.h>
#include <barrelfish_kpi/dispatcher_handle.h>

/* libbarrelfish API */
#include <aos/types.h>
#include <aos/capabilities.h>
#include <aos/slab.h>
#include <aos/threads.h>
#include <aos/slot_alloc.h>
#include <aos/ram_alloc.h>
#include <aos/syscalls.h>
#include <aos/cspace.h>
#include <aos/domain.h>
#include <aos/debug.h>
#include <aos/static_assert.h>
#include <aos/paging.h>
#include <aos/lmp_chan.h>
#include <aos/lmp_endpoints.h>

#define TOGGLE_BIT(base, offset, bit) (*((volatile uint32_t *)                \
            (base + offset)) ^= 0x1 << bit)
#define CLR_BIT(base, offset, bit) (*((volatile uint32_t *)                   \
            (base + offset)) &= ~(0x1 << bit))

#define PAGING_STATE_PADDR      0x1000
#define PAGING_VADDR_START      (1 << 25) //< = 0x2000000

#define MEMORY_BARRIER          __asm volatile("dmb");__asm volatile("isb")

#define GPIO_1_BASE             0x4A310000
#define OFFSET_GPIO_OE          0x134
#define OFFSET_GPIO_DATAOUT     0x13c
#define LED_D2_BIT              0x8

#define OS_NAME                 "Clownfish"
#define OS_VERSION_MAJOR        0
#define OS_VERSION_MINOR        1
#define OS_PATCH_LEVEL          0
#define OS_AUTHORS              "Fania Christian, Mueller Jan, Schaad Philipp"
#define OS_GROUP                "Group D"

/* Duplicate memory */
static inline void * memdup(const void *ptr, size_t size) {
    void *res = malloc(size);
    assert(res);
    memcpy(res, ptr, size);
    return res;
}

/* XXX: glue junk for old IDC system, to be removed!! */

void messages_wait_and_handle_next(void);
void __attribute__((noreturn)) messages_handler_loop(void);

#endif
