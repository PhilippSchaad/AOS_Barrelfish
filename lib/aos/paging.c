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

#include <spawn/spawn.h>

#include <stdio.h>
#include <string.h>

#define PAGEFAULT_STACK_SIZE 16384 // = 16 * 1024 = 16kb

//#define no_page_align_in_frame_alloc

static struct paging_state current;
static struct thread_mutex mutex;

static char pagefault_stack[PAGEFAULT_STACK_SIZE];
// TODO: Make threadsafe (Probably best to acquire a lock o.s.s.?)
static void pagefault_handler(enum exception_type type, int subtype,
                              void *addr, arch_registers_state_t *regs,
                              arch_registers_fpu_state_t *fpuregs)
{
    thread_mutex_lock(&mutex);
    struct paging_state *st = get_current_paging_state();

    lvaddr_t vaddr = (lvaddr_t) addr;

    // Do some checks
    if (vaddr == 0x0) {
        thread_mutex_unlock(&mutex);
        DBG(ERR, "Tried to dereference NULL\n");
        thread_exit(1);
    }
    if (vaddr > 0x80000000) {
        thread_mutex_unlock(&mutex);
        // check if we want to map something to kernel space (2GB)
        DBG(ERR, "Tried to alloc something in kernel space...\n");
        thread_exit(1);
    }

    // TODO: Check if we are in a valid heap-range address.
    // TODO: Also check if we need to refill slabs and do so if yes.

    // Align the address where the fault occurred against base page size.
    vaddr = vaddr - (vaddr % BASE_PAGE_SIZE);

    struct capref frame;
    size_t retsize;

    CHECK(frame_alloc(&frame, BASE_PAGE_SIZE, &retsize));
    CHECK(paging_map_fixed_attr(st, vaddr, frame, retsize,
                                VREGION_FLAGS_READ_WRITE));
    thread_mutex_unlock(&mutex);
}

/**
 * \brief Helper function that allocates a slot and
 *        creates a ARM l2 page table capability
 */
__attribute__((unused)) static errval_t arml2_alloc(struct paging_state *st,
                                                    struct capref *ret)
{
    CHECK(st->slot_alloc->alloc(st->slot_alloc, ret));
    CHECK(vnode_create(*ret, ObjType_VNode_ARM_l2));
    return SYS_ERR_OK;
}

// For debugging only. Keeps track of number of created paging states.
static size_t ps_index = 1;

static errval_t slot_alloc2(struct slot_allocator *ca, struct capref *cap)
{
    return slot_alloc(cap);
}

static errval_t slot_free2(struct slot_allocator *ca, struct capref cap)
{
    return slot_free(cap);
}

static struct slot_allocator k;

errval_t paging_init_state(struct paging_state *st, lvaddr_t start_vaddr,
                           struct capref pdir, struct slot_allocator *ca)
{
    DBG(VERBOSE, "paging_init_state\n");

    // For debugging only. Keeps track of number of created paging states.
    st->debug_paging_state_index = ps_index++;
    DBG(DETAILED, "Setting up paging_state #%u", st->debug_paging_state_index);

    // Slot allocator.
    // st->slot_alloc = ca;
    // bad hacks
    k.space = 0;
    k.nslots = 0;
    k.free = slot_free2;
    k.alloc = slot_alloc2;
    st->slot_alloc = &k;

    // Set the l1 page table.
    st->l1_page_table = pdir;

    // Initialize the L2 pagetables.
    size_t i;
    for (i = 0; i < ARM_L1_MAX_ENTRIES; ++i) {
        st->l2_page_tables[i].init = false;
    }
    // Set the start of the free space.
    st->free_vspace.base_addr = start_vaddr;
    DBG(DETAILED, "At time of init our base addr is at: %p \n",
        st->free_vspace.base_addr);
    st->free_vspace.region_size = 0xFFFFFFFF - start_vaddr;

    st->free_vspace.next = NULL;
    st->spawninfo = NULL;

    // TODO: This is an ugly hack so we don't need individual slab allocs.
    //       Probably improve at some point
    size_t minbytes =
        sizeof(struct paging_frame_node) > sizeof(struct paging_map_node)
            ? sizeof(struct paging_frame_node)
            : sizeof(struct paging_map_node);

    if (sizeof(struct paging_used_node) > minbytes)
        minbytes = sizeof(struct paging_used_node);

    slab_init(&st->slab_alloc, minbytes, slab_default_refill);

    return SYS_ERR_OK;
}

/**
 * \brief This function initializes the paging for this domain
 * It is called once before main.
 */
errval_t paging_init(void)
{
    DBG(VERBOSE, "paging_init\n");

    CHECK(thread_set_exception_handler(
        pagefault_handler, NULL, (void *) &pagefault_stack,
        (void *) &pagefault_stack + PAGEFAULT_STACK_SIZE, NULL, NULL));

    set_current_paging_state(&current);

    // According to the book, the L1 page table is at the following address.
    struct capref l1_pagetable = {
        .cnode = cnode_page, .slot = 0,
    };

    paging_init_state(&current, PAGING_VADDR_START, l1_pagetable,
                      get_default_slot_allocator());

    thread_mutex_init(&mutex);
    return SYS_ERR_OK;
}

/**
 * \brief Initialize per-thread paging state
 */
void paging_init_onthread(struct thread *t)
{
    t->exception_handler = pagefault_handler;
    t->exception_stack = (void *) &pagefault_stack;
    t->exception_stack_top = (void *) &pagefault_stack + PAGEFAULT_STACK_SIZE;
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
    pr->base_addr = (lvaddr_t) base;
    pr->current_addr = pr->base_addr;
    pr->region_size = size;
    pr->slab_alloc = &st->slab_alloc;
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
    // TODO: allow it to use holes for mapping
    lvaddr_t end_addr = pr->base_addr + pr->region_size;
    ssize_t rem = end_addr - pr->current_addr;
    if (rem > req_size) {
        // ok
        *retbuf = (void *) pr->current_addr;
        *ret_size = req_size;
        pr->current_addr += req_size;
    } else if (rem > 0) {
        *retbuf = (void *) pr->current_addr;
        *ret_size = rem;
        pr->current_addr += rem;
        DBG(WARN, "exhausted paging region, "
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
    DBG(DETAILED, "paging_region_unmap with %llx and %u", base, bytes);
    // XXX: should free up some space in paging region, however need to track
    //      holes for non-trivial case
    assert(pr->base_addr >= base);
    assert(pr->base_addr + pr->region_size >= base + bytes);
    assert(pr->current_addr >= base + bytes);
    if (pr->current_addr - bytes == base) {
        pr->current_addr = base;
    } else {
        // TODO: catch node overlaps via asserts
        if (pr->holes == NULL) {
            struct paging_frame_node *new_node =
                (struct paging_frame_node *) slab_alloc(pr->slab_alloc);
            new_node->base_addr = base;
            new_node->region_size = bytes;
            new_node->next = NULL;
            pr->holes = new_node;
        } else {
            struct paging_frame_node *node = pr->holes;
            bool done = false;
            while (node->next != NULL) {
                if (node->base_addr < base) {
                    if (node->base_addr + node->region_size == base) {
                        node->region_size += bytes;
                        done = true;
                        break;
                    } else {
                        node = node->next;
                    }
                } else {
                    struct paging_frame_node *new_node =
                        (struct paging_frame_node *) slab_alloc(
                            pr->slab_alloc);

                    new_node->base_addr = node->base_addr;
                    new_node->region_size = node->region_size;
                    new_node->next = node->next;
                    node->base_addr = base;
                    node->region_size = bytes;
                    node->next = new_node;
                    done = true;
                    break;
                }
            }
            if (!done) {
                struct paging_frame_node *new_node =
                    (struct paging_frame_node *) slab_alloc(pr->slab_alloc);
                new_node->base_addr = base;
                new_node->region_size = bytes;
                new_node->next = NULL;
                node->next = new_node;
            }
        }
    }

    // Coalesce holes.
    if (pr->holes) {
        struct paging_frame_node *node = pr->holes;
        struct paging_frame_node *prev = NULL;
        while (node->next != NULL) {
            struct paging_frame_node *temp = node->next;
            if (temp->base_addr == node->base_addr + node->region_size) {
                node->region_size += temp->region_size;
                node->next = temp->next;
                slab_free(pr->slab_alloc, temp);
            } else {
                prev = node;
                node = node->next;
            }
        }
        if (node->base_addr + node->region_size == pr->current_addr) {
            pr->current_addr -= node->region_size;
            slab_free(pr->slab_alloc, node);
            if (prev != NULL)
                prev->next = NULL;
            else
                pr->holes = NULL;
        }
    }

    return SYS_ERR_OK;
}

/**
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
    struct paging_frame_node *node = &st->free_vspace;
    while (node != NULL) {
        if (node->region_size >= bytes) {
            DBG(DETAILED, "ps %u paging_alloc base: %p size: %u req: %u \n",
                st->debug_paging_state_index, node->base_addr,
                node->region_size, bytes);
            *buf = (void *) node->base_addr;
            node->base_addr += bytes;

#ifndef no_page_align_in_frame_alloc
            lvaddr_t temp = ROUND_UP(node->base_addr, BASE_PAGE_SIZE);
            DBG(DETAILED, "ps %u alignment change does: %u \n",
                st->debug_paging_state_index,
                (size_t)(temp - node->base_addr));
            node->region_size -= bytes + (temp - node->base_addr);
            node->base_addr = temp;
#else
            node->region_size -= bytes;
#endif

            DBG(DETAILED, "ps %u paging_alloc new base: %p new size: %u\n",
                st->debug_paging_state_index, node->base_addr,
                node->region_size);

            if (node->region_size == 0) {
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

        DBG(DETAILED, "ps %u we requested %u bytes but only had %u available "
                      "anymore on region %p \n",
            st->debug_paging_state_index, bytes, node->region_size,
            node->base_addr);
        node = node->next;
    }
    return LIB_ERR_OUT_OF_VIRTUAL_ADDR;
}

/**
 * \brief map a user provided frame, and return the VA of the mapped
 *        frame in `buf`.
 */
errval_t paging_map_frame_attr(struct paging_state *st, void **buf,
                               size_t bytes, struct capref frame, int flags,
                               void *arg1, void *arg2)
{
    CHECK_MSG(paging_alloc(st, buf, bytes), "to addr %p of size %i\n", *buf,
              bytes);
    return paging_map_fixed_attr(st, (lvaddr_t)(*buf), frame, bytes, flags);
}

errval_t slab_refill_no_pagefault(struct slab_allocator *slabs,
                                  struct capref frame, size_t minbytes)
{
    DBG(DETAILED, "slab_refill_no_pagefault wants to alloc bytes: %u\n",
        minbytes);

    // Refill the two-level slot allocator without causing a page-fault
    void *buf;
    struct paging_state *st = get_current_paging_state();
    buf = NULL;
    size_t frame_size;
    DBG(DETAILED, "allocing at least: %u\n", minbytes);
    CHECK(frame_alloc(&frame, minbytes, &frame_size));
    DBG(DETAILED, "allocing in reality: %u\n", frame_size);

    size_t mapped_bytes = 0;
    size_t bytes = frame_size;
    int flags = VREGION_FLAGS_READ_WRITE;
    CHECK_MSG(paging_alloc(st, &buf, bytes), "to addr %p of size %i\n", buf,
              bytes);
    DBG(DETAILED,
        "now pagefault goes into replication of the fixed mapping\n");
    lvaddr_t vaddr = (lvaddr_t) buf;
    while (bytes > 0) {
        // Get the index of the L2 table in the L1 table.
        lvaddr_t l1_index = ARM_L1_OFFSET(vaddr);

        // Check if the table already exists.
        struct capref l2_pagetable;
        if (st->l2_page_tables[l1_index].init) {
            // Table exists.
            DBG(DETAILED, "found l2 page table \n");
            l2_pagetable = st->l2_page_tables[l1_index].cap;
        } else {
            // Create a L2 pagetable.

            // Create a new table.
            DBG(DETAILED, "making l2 page table \n");
            CHECK(arml2_alloc(st, &l2_pagetable));

            DBG(DETAILED, "creating l1 l2 mapping capability \n");
            // Write the L1 table entry.
            struct capref l2_l1_mapping;
            CHECK(st->slot_alloc->alloc(st->slot_alloc, &l2_l1_mapping));

            DBG(DETAILED, "mapping l2 page table \n");
            CHECK(vnode_map(st->l1_page_table, l2_pagetable, l1_index,
                            VREGION_FLAGS_READ_WRITE, 0, 1, l2_l1_mapping));

            // Add cap to tracking array.
            st->l2_page_tables[l1_index].cap = l2_pagetable;
            st->l2_page_tables[l1_index].init = true;

            // Add to child process if necessary.
            if (st->spawninfo != NULL) {
                DBG(DETAILED, "I should add the new slot to the child\n");
                ((struct spawninfo *) st->spawninfo)
                    ->slot_callback(((struct spawninfo *) st->spawninfo),
                                    l2_pagetable);
                ((struct spawninfo *) st->spawninfo)
                    ->slot_callback(((struct spawninfo *) st->spawninfo),
                                    l2_l1_mapping);
            }
        }
        DBG(DETAILED, "now allocing slot for frame mapping \n");

        // Get the frame from the L2 table.
        lvaddr_t l2_index = ARM_L2_OFFSET(vaddr);

        // How many bytes should we map in that frame?
        size_t mapping_size =
            MIN(bytes, (ARM_L2_MAX_ENTRIES - l2_index) * BASE_PAGE_SIZE);

        // Finally, do the mapping.
        struct capref l2_frame;
        CHECK(st->slot_alloc->alloc(st->slot_alloc, &l2_frame));
        CHECK(vnode_map(l2_pagetable, frame, l2_index, flags, mapped_bytes,
                        mapping_size / BASE_PAGE_SIZE, l2_frame));
        if (st->spawninfo != NULL) {
            ((struct spawninfo *) st->spawninfo)
                ->slot_callback(((struct spawninfo *) st->spawninfo),
                                l2_frame);
        }

        // To some house keeping for the next round:
        mapped_bytes += mapping_size;
        bytes -= mapping_size;
        vaddr += mapping_size;
        l1_index++;
        DBG(DETAILED, "Still need to map: %" PRIuGENSIZE " bytes starting "
                      "from 0x%p\n",
            (gensize_t) bytes, vaddr);
    }

    assert(bytes == 0);
    DBG(DETAILED, "mapped %" PRIuGENSIZE " bytes\n", (gensize_t) mapped_bytes);

    slab_grow(slabs, buf, frame_size);

    return SYS_ERR_OK;
}

/**
 * \brief map a user provided frame at user provided VA.
 */
errval_t paging_map_fixed_attr(struct paging_state *st, lvaddr_t vaddr,
                               struct capref frame, size_t bytes, int flags)
{
    DBG(VERBOSE, "st %u fixed alloc: vaddr: %p, bytes: 0x%x \n",
        st->debug_paging_state_index, vaddr, bytes);

    // Iterate over the L2 page tables and map the memory.
    size_t mapped_bytes = 0;
    if (slab_freecount(&st->slab_alloc) == 0) {
        DBG(DETAILED, "triggering special slab refill\n");
        struct capref slabframe;
        st->slot_alloc->alloc(st->slot_alloc, &slabframe);
        DBG(DETAILED, "first hurdle made\n");
        slab_refill_no_pagefault(&st->slab_alloc, slabframe, BASE_PAGE_SIZE);
        DBG(DETAILED, "second hurdle made\n");
    }

    struct paging_used_node *mappings =
        (struct paging_used_node *) slab_alloc(&st->slab_alloc);
    DBG(DETAILED, "we got past the slab_alloc bit");
    mappings->start_addr = vaddr;
    mappings->size = bytes;
    mappings->next = st->mappings;
    st->mappings = mappings;
    mappings->map_list = NULL;
    while (bytes > 0) {
        // Get the index of the L2 table in the L1 table.
        lvaddr_t l1_index = ARM_L1_OFFSET(vaddr);

        // Check if the table already exists.
        struct capref l2_pagetable;
        if (st->l2_page_tables[l1_index].init) {
            // Table exists.
            DBG(DETAILED, "found l2 page table \n");
            l2_pagetable = st->l2_page_tables[l1_index].cap;
        } else {
            // Create a L2 pagetable.

            // Create a new table.
            DBG(DETAILED, "making l2 page table \n");
            CHECK(arml2_alloc(st, &l2_pagetable));

            DBG(DETAILED, "creating l1 l2 mapping capability \n");
            // Write the L1 table entry.
            struct capref l2_l1_mapping;
            CHECK(st->slot_alloc->alloc(st->slot_alloc, &l2_l1_mapping));

            DBG(DETAILED, "mapping l2 page table \n");
            CHECK(vnode_map(st->l1_page_table, l2_pagetable, l1_index,
                            VREGION_FLAGS_READ_WRITE, 0, 1, l2_l1_mapping));

            // Add cap to tracking array.
            st->l2_page_tables[l1_index].cap = l2_pagetable;
            st->l2_page_tables[l1_index].init = true;

            // Add to child process if necessary.
            if (st->spawninfo != NULL) {
                DBG(DETAILED, "I should add the new slot to the child\n");
                ((struct spawninfo *) st->spawninfo)
                    ->slot_callback(((struct spawninfo *) st->spawninfo),
                                    l2_pagetable);
                ((struct spawninfo *) st->spawninfo)
                    ->slot_callback(((struct spawninfo *) st->spawninfo),
                                    l2_l1_mapping);
            }
        }
        DBG(DETAILED, "now allocing slot for frame mapping \n");

        // Get the frame from the L2 table.
        lvaddr_t l2_index = ARM_L2_OFFSET(vaddr);

        // How many bytes should we map in that frame?
        size_t mapping_size =
            MIN(bytes, (ARM_L2_MAX_ENTRIES - l2_index) * BASE_PAGE_SIZE);

        // Finally, do the mapping.
        struct capref l2_frame;
        CHECK(st->slot_alloc->alloc(st->slot_alloc, &l2_frame));
        CHECK(vnode_map(l2_pagetable, frame, l2_index, flags, mapped_bytes,
                        mapping_size / BASE_PAGE_SIZE, l2_frame));
        if (st->spawninfo != NULL) {
            ((struct spawninfo *) st->spawninfo)
                ->slot_callback(((struct spawninfo *) st->spawninfo),
                                l2_frame);
        }

        // TODO: store l2_l1_mapping and l2_frame (also a mapping), further,
        // store l2_pagetable that storing is really just needed for unmapping
        // however we might want to levy it later for finding an empty frame
        // range. if we do levy it for that, rewrite frame_alloc and remove the
        // frame_alloc specific suff from st
        DBG(DETAILED, "now doing the storing thing \n");
        struct paging_map_node *mapentry =
            (struct paging_map_node *) slab_alloc(&st->slab_alloc);
        mapentry->table = l2_pagetable;
        mapentry->mapping = l2_frame;
        mapentry->next = mappings->map_list;
        mappings->map_list = mapentry;

        // To some house keeping for the next round:
        mapped_bytes += mapping_size;
        bytes -= mapping_size;
        vaddr += mapping_size;
        l1_index++;
        DBG(DETAILED, "Still need to map: %" PRIuGENSIZE " bytes starting "
                      "from 0x%p\n",
            (gensize_t) bytes, vaddr);
    }
    assert(bytes == 0);
    DBG(DETAILED, "mapped %" PRIuGENSIZE " bytes\n", (gensize_t) mapped_bytes);
    return SYS_ERR_OK;
}

static void paging_add_space(struct paging_state *st, lvaddr_t base,
                             size_t size)
{
    // TODO: Coalesce nodes
    // TODO: Sort space
    struct paging_frame_node *node =
        (struct paging_frame_node *) slab_alloc(&st->slab_alloc);
    node->base_addr = base;
    node->region_size = size;
    node->next = st->free_vspace.next;
    st->free_vspace.next = node;
}

/**
 * \brief unmap region starting at address `region`.
 * NOTE: Implementing this function is optional.
 */
errval_t paging_unmap(struct paging_state *st, const void *region)
{
    DBG(VERBOSE, "st %u - unmapping %p\n", st->debug_paging_state_index,
        region);

    struct paging_used_node *node = st->mappings;
    struct paging_used_node *prev = NULL;

    while (true) {
        assert(node != NULL); // TODO: make nicer
        if (node->start_addr == (lvaddr_t) region) {
            break;
        }
        prev = node;
        node = node->next;
    }

    if (prev != NULL)
        prev->next = node->next;
    else
        st->mappings = node->next;

    struct paging_map_node *mapnode = node->map_list;

    while (mapnode != NULL) {
        struct paging_map_node *temp = mapnode;
        mapnode = mapnode->next;
        vnode_unmap(temp->table, temp->mapping);
        slab_free(&st->slab_alloc, temp);
    }

    paging_add_space(st, (lvaddr_t) node->start_addr, node->size);

    slab_free(&st->slab_alloc, node);

    return SYS_ERR_OK;
}
