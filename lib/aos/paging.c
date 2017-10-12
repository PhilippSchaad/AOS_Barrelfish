/**
 * \file
 * \brief AOS paging helpers.
 */

/*
 * Copyright (c) 2012, 2013, 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <aos/aos.h>
#include <aos/paging.h>
#include <aos/except.h>
#include <aos/slab.h>
#include "threads_priv.h"

#include <stdio.h>
#include <string.h>

//#define no_debug_printf
#ifdef no_debug_printf
#undef debug_printf
#define debug_printf(...)
#endif

//#define no_page_align_in_frame_alloc
#define no_paging_map_fixed_attr_debug_printf

static struct paging_state current;

/**
 * \brief Helper function that allocates a slot and
 *        creates a ARM l2 page table capability
 */
__attribute__((unused))
static errval_t arml2_alloc(struct paging_state * st, struct capref *ret)
{
    CHECK(st->slot_alloc->alloc(st->slot_alloc, ret));
debug_printf("arml2_alloc middle reached");
    CHECK(vnode_create(*ret, ObjType_VNode_ARM_l2));
    return SYS_ERR_OK;
}

static size_t ps_index = 1;

errval_t paging_init_state(struct paging_state *st, lvaddr_t start_vaddr,
        struct capref pdir, struct slot_allocator * ca)
{
    debug_printf("paging_init_state\n");

    //for debug purposes to keep track how many we got
    st->debug_paging_state_index = ps_index++;
    debug_printf("setting up paging_state #%u",st->debug_paging_state_index);

    // slot allocator
    st->slot_alloc = ca;

    // set the l1 page table
    st->l1_page_table = pdir;
    
    // TODO: I don't think this is really needed
    // init the l2_page_tables
    size_t i;
    for (i = 0; i < ARM_L1_MAX_ENTRIES; ++i) {
        st->l2_page_tables[i].init = false;
    }
    //set the start of the free space
    st->free_vspace.base_addr = start_vaddr;
    debug_printf("at time of init our base addr is at: %p \n",st->free_vspace.base_addr);
    st->free_vspace.region_size = 0xFFFFFFFF - start_vaddr;

    st->free_vspace.next = NULL;
    
    // TODO (M2): implement state struct initialization
    // TODO (M4): Implement page fault handler that installs frames when a page fault
    // occurs and keeps track of the virtual address space.
    return SYS_ERR_OK;
}

/**
 * \brief This function initializes the paging for this domain
 * It is called once before main.
 */
errval_t paging_init(void)
{
    debug_printf("paging_init\n");
    // TODO (M2): Call paging_init_state for &current
    // TODO (M4): initialize self-paging handler
    // TIP: use thread_set_exception_handler() to setup a page fault handler
    // TIP: Think about the fact that later on, you'll have to make sure that
    // you can handle page faults in any thread of a domain.
    // TIP: it might be a good idea to call paging_init_state() from here to
    // avoid code duplication.
    set_current_paging_state(&current);
    
    // according to the book, the L1 page table is at the following address
    struct capref l1_pagetable = {
        .cnode = cnode_page,
        .slot = 0,
    };
    
    paging_init_state(&current, /*TODO: Do you know what this offset is??*/ (1<<25),
                      l1_pagetable, get_default_slot_allocator());
    
    return SYS_ERR_OK;
}


/**
 * \brief Initialize per-thread paging state
 */
void paging_init_onthread(struct thread *t)
{
    // TODO (M4): setup exception handler for thread `t'.
}

/**
 * \brief return a pointer to a bit of the paging region `pr`.
 * This function gets used in some of the code that is responsible
 * for allocating Frame (and other) capabilities.
 */
errval_t paging_region_init(struct paging_state *st, struct paging_region *pr,
                            size_t size)
{
    void *base = NULL;
    CHECK(paging_alloc(st, &base, size));
    pr->base_addr    = (lvaddr_t)base;
    pr->current_addr = pr->base_addr;
    pr->region_size  = size;
    // TODO: maybe add paging regions to paging state?
    return SYS_ERR_OK;
}

/**
 * \brief return a pointer to a bit of the paging region `pr`.
 * This function gets used in some of the code that is responsible
 * for allocating Frame (and other) capabilities.
 */
errval_t paging_region_map(struct paging_region *pr, size_t req_size,
                           void **retbuf, size_t *ret_size)
{
    lvaddr_t end_addr = pr->base_addr + pr->region_size;
    ssize_t rem = end_addr - pr->current_addr;
    if (rem > req_size) {
        // ok
        *retbuf = (void*)pr->current_addr;
        *ret_size = req_size;
        pr->current_addr += req_size;
    } else if (rem > 0) {
        *retbuf = (void*)pr->current_addr;
        *ret_size = rem;
        pr->current_addr += rem;
        debug_printf("exhausted paging region, "
                "expect badness on next allocation\n");
    } else {
        return LIB_ERR_VSPACE_MMU_AWARE_NO_SPACE;
    }
    return SYS_ERR_OK;
}

/**
 * \brief free a bit of the paging region `pr`.
 * This function gets used in some of the code that is responsible
 * for allocating Frame (and other) capabilities.
 * NOTE: Implementing this function is optional.
 */
errval_t paging_region_unmap(struct paging_region *pr, lvaddr_t base,
                             size_t bytes)
{
    // XXX: should free up some space in paging region, however need to track
    //      holes for non-trivial case
    return SYS_ERR_OK;
}

/**
 * TODO(M2): Implement this function
 * \brief Find a bit of free virtual address space that is large enough to
 *        accomodate a buffer of size `bytes`.
 */

// TODO: consider wether guaranteeing page aligning is a good idea?
// In particular, if it is, find a way to impl this so that it doesn't lose
// memory through adjusting to page alignment
// TODO: It probably should not do page aligning but our current
// paging_map_fixed_frame impl doesn't like it when it's not page aligned.
// So that one needs fixing first
errval_t paging_alloc(struct paging_state *st, void **buf, size_t bytes)
{
//    *buf = (void*) (++COUNT * BASE_PAGE_SIZE);
    struct paging_free_frame_node *node = &st->free_vspace;
    while(node != NULL) {
        if(node->region_size >= bytes) {
            debug_printf("ps %u paging_alloc base: %p size: %u req: %u \n", st->debug_paging_state_index,
                    node->base_addr, node->region_size, bytes);
            *buf = (void*)node->base_addr;
            node->base_addr += bytes;
#ifndef no_page_align_in_frame_alloc
            lvaddr_t temp = ROUND_UP(node->base_addr, BASE_PAGE_SIZE); //this does the page aligning thing
            debug_printf("ps %u alignment change does: %u \n",st->debug_paging_state_index,(size_t)(temp-node->base_addr));
            node->region_size -= bytes + (temp-node->base_addr);
            node->base_addr = temp;
#else
            node->region_size -= bytes;
#endif
            debug_printf("ps %u paging_alloc new base: %p new size: %u\n",st->debug_paging_state_index,node->base_addr,node->region_size);
            if(node->region_size == 0) {
                // TODO: add removing, just move content of next node
                // into this and delete next node 
                // this does mean we'll eventually trail a size 0 node
                // at maxaddr, but that's still cheaper than doubly linked
                // list. And less of a perf issue than going from start to
                // remove properly from the list
                // random sidenote: it might be worth having pools, if aquiring
                // capabilities is so expensive, on the other hand that sounds
                // like a great way to get security issues later down the line.
            }
            return SYS_ERR_OK;
        }
        debug_printf("ps %u we requested %u bytes but only had %u available anymore on region %p \n", st->debug_paging_state_index, bytes, node->region_size, node->base_addr);
        node = node->next;
    }
    return LIB_ERR_OUT_OF_VIRTUAL_ADDR;
}

/**
 * \brief map a user provided frame, and return the VA of the mapped
 *        frame in `buf`.
 */
errval_t paging_map_frame_attr(struct paging_state *st, void **buf,
                               size_t bytes, struct capref frame,
                               int flags, void *arg1, void *arg2)
{
    CHECK_MSG(paging_alloc(st, buf, bytes), "to addr %p of size %i\n",
              *buf, bytes);
    return paging_map_fixed_attr(st, (lvaddr_t)(*buf), frame, bytes, flags);
}

errval_t
slab_refill_no_pagefault(struct slab_allocator *slabs, struct capref frame,
                         size_t minbytes)
{
    // Refill the two-level slot allocator without causing a page-fault
    void *buf;
    struct paging_state* st = get_current_paging_state();
    buf = NULL;
    size_t frame_size;
    debug_printf("allocing at least: %u\n", minbytes);
    CHECK(frame_alloc(&frame, minbytes, &frame_size));
    debug_printf("allocing in reality: %u\n", frame_size);
    CHECK(paging_map_frame_attr(st, &buf, frame_size, frame,
                                VREGION_FLAGS_READ_WRITE, NULL, NULL));
    slab_grow(slabs, buf, frame_size);

    return SYS_ERR_OK;
}

#ifdef no_paging_map_fixed_attr_debug_printf
#undef debug_printf
#define debug_printf(...)
#endif

/**
 * \brief map a user provided frame at user provided VA.
 * TODO(M2): General case
 */
errval_t paging_map_fixed_attr(struct paging_state *st, lvaddr_t vaddr,
        struct capref frame, size_t bytes, int flags)
{
    // create a L2 pagetable
    struct capref l2_pagetable;
    // get the index of the L2 table in the L1 table
    lvaddr_t l1_index = ARM_L1_OFFSET(vaddr);
    lvaddr_t end_vaddr = vaddr+bytes;
    lvaddr_t end_l1_index = ARM_L1_OFFSET(end_vaddr);
#ifndef no_debug_printf
    size_t debug_alloced_bytes = 0;
#endif
    bool first = true;
    debug_printf("fixed alloc: vaddr: %p, bytes: %u \n", vaddr, bytes);
    debug_printf("l1_index_start: %p, l1_index_end: %p", l1_index, end_l1_index);
    while(l1_index <= end_l1_index) {
        size_t curbytes;
        lvaddr_t curvaddr;
        debug_printf("first: %d \n",first);
#define l2_page_byte_storage_size (1 << 20)
        if(l1_index < end_l1_index) {
            if(first) {
                curbytes = l2_page_byte_storage_size - (vaddr % l2_page_byte_storage_size);
                curvaddr = vaddr;
                first = false;
            } else {
                curbytes = l2_page_byte_storage_size;
                curvaddr = l1_index << 20;
            }
        }else {
            if(first) {
                curbytes = bytes;
                curvaddr = vaddr;
            }else {
                curbytes = (bytes - (l2_page_byte_storage_size - (vaddr % l2_page_byte_storage_size))) % l2_page_byte_storage_size;
                curvaddr = l1_index << 20;
            }
        }
        debug_printf("curvaddr: %p, curbytes: %u \n",curvaddr,curbytes);
#ifndef no_debug_printf
        debug_alloced_bytes += curbytes;
        debug_printf("allocated so far: %u and still need to alloc: %u\n",debug_alloced_bytes, bytes - debug_alloced_bytes);
#endif
        // check if the table already exists
        if (st->l2_page_tables[l1_index].init) {
            // table exists
            debug_printf("found l2 page table \n");
            l2_pagetable = st->l2_page_tables[l1_index].cap;
        } else {
            // create a new table
            debug_printf("making l2 page table \n");
            CHECK(arml2_alloc(st, &l2_pagetable));

            debug_printf("creating l1 l2 mapping capability \n");
            // write the L1 table entry
            struct capref l2_l1_mapping;
            CHECK(st->slot_alloc->alloc(st->slot_alloc, &l2_l1_mapping));

            debug_printf("mapping l2 page table \n");
            CHECK(vnode_map(st->l1_page_table, l2_pagetable, l1_index,
                            VREGION_FLAGS_READ_WRITE, 0, 1, l2_l1_mapping));

            // add cap to tracking array
            st->l2_page_tables[l1_index].cap = l2_pagetable;
            st->l2_page_tables[l1_index].init = true;
        }
        debug_printf("now allocing slot for frame mapping \n");

        // get the frame from the L2 table
        lvaddr_t l2_index = ARM_L2_OFFSET(curvaddr);
        struct capref l2_frame;
        CHECK(st->slot_alloc->alloc(st->slot_alloc, &l2_frame));
        uint64_t pte_count = (curbytes >> 12) + (curbytes % BASE_PAGE_SIZE != 0 ? 1 : 0);
        debug_printf("frame offset: %llu \n",(uint64_t)curvaddr % BASE_PAGE_SIZE);
        debug_printf("pte_count: %llu \n", pte_count);
        CHECK(vnode_map(l2_pagetable, frame, l2_index, flags, curvaddr % BASE_PAGE_SIZE,pte_count, l2_frame));

        // TODO: make sure that frames larger than a single l2 table is split
        // TODO: store l2_l1_mapping and l2_frame (also a mapping), further,
        // store l2_pagetable that storing is really just needed for unmapping
        // however we might want to levy it later for finding an empty frame
        // range. if we do levy it for that, rewrite frame_alloc and remove the
        // frame_alloc specific suff from st
        debug_printf("l1_index before incr: %p",l1_index);
        l1_index++;
        debug_printf("l1_index after incr: %p \n",l1_index);
    }
    return SYS_ERR_OK;
}

/**
 * \brief unmap a user provided frame, and return the VA of the mapped
 *        frame in `buf`.
 * NOTE: Implementing this function is optional.
 */
errval_t paging_unmap(struct paging_state *st, const void *region)
{
    return SYS_ERR_OK;
}
