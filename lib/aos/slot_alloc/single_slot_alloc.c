/**
 * \file
 * \brief Slot allocator for a single cnode
 *
 * Allocators should be created with worst case slab requirement.
 * The worst case requirement is the number of slots in the cnode / 2.
 */

/*
 * Copyright (c) 2010, ETH Zurich.
 * Copyright (c) 2015, Hewlett Packard Enterprise Development LP.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <aos/aos.h>
#include <aos/caddr.h>


static errval_t salloc(struct slot_allocator *ca, struct capref *ret)
{
    struct single_slot_allocator *sca = (struct single_slot_allocator*)ca;
    //DBG(DETAILED,"salloc %u %u %u\n", sca->head->space, sca->head->slot, sca->a.space);

    if (sca->a.space == 0) {
        DBG(DETAILED,"salloc no space\n");
        return LIB_ERR_SLOT_ALLOC_NO_SPACE;
    }
    thread_mutex_lock(&ca->mutex);

    // Slot to return
    ret->cnode = sca->cnode;
    ret->slot  = sca->head->slot;

#if 0
    char buf[256];
    debug_print_capref(buf, 256, *ret);
    debug_printf("%p->salloc: ret = %.*s\n", ca, 256, buf);
#endif
    // Decrement space
    sca->head->space--;
    sca->head->slot++;
    sca->a.space--;

    // Check to free head
    if (sca->head->space == 0) {
        struct cnode_meta *walk = sca->head;
        sca->head = walk->next;
        slab_free(&sca->slab, walk);
    }

    thread_mutex_unlock(&ca->mutex);
    DBG(DETAILED,"salloc done\n");
    return SYS_ERR_OK;
}

static errval_t free_slots(struct single_slot_allocator *sca, cslot_t slot,
                           cslot_t count, struct thread_mutex *mutex)
{
    thread_mutex_lock(mutex);

    errval_t err = SYS_ERR_OK;

    struct cnode_meta *walk = sca->head;
    struct cnode_meta *prev = NULL;

    // Entire cnode was allocated
    if (!sca->head) {
        sca->head = slab_alloc(&sca->slab);
        sca->head->slot = slot;
        sca->head->space = count;
        sca->head->next = NULL;
        goto finish;
    }

    // Freeing one before head
    if (slot + count == sca->head->slot) {
        sca->head->slot = slot;
        sca->head->space += count;
        goto finish;
    }

    // Freeing before head
    if (slot < sca->head->slot) {
        struct cnode_meta *new = slab_alloc(&sca->slab);
        new->slot  = slot;
        new->space = count;
        new->next  = sca->head;
        sca->head  = new;
        goto finish;
    }

    while (walk != NULL) {
        // Freeing at the edge of walk
        if (slot == walk->slot + walk->space) {
            walk->space += count;

            // check if we can merge walk to next
            struct cnode_meta *next = walk->next;
            if (next && next->slot == walk->slot + walk->space) {
                walk->space += next->space;
                walk->next = next->next;
                slab_free(&sca->slab, next);
            }

            goto finish;
        }
        else if (slot < walk->slot + walk->space) {
            err = LIB_ERR_SLOT_UNALLOCATED;
            goto unlock;
        }

        // Freing just before walk->next
        if (walk->next && slot + count == walk->next->slot) {
            walk->next->slot = slot;
            walk->next->space += count;
            goto finish;
        }

        // Freeing after walk and before walk->next
        if (walk->next && slot < walk->next->slot) {
            struct cnode_meta *new = walk->next;
            walk->next = slab_alloc(&sca->slab);
            walk->next->slot = slot;
            walk->next->space = count;
            walk->next->next = new;
            goto finish;
        }
        prev = walk;
        walk = walk->next;
    }

    // Freeing after the list
    prev->next = slab_alloc(&sca->slab);
    prev->next->slot = slot;
    prev->next->space = count;
    prev->next->next = NULL;

 finish:
    sca->a.space += count;

 unlock:
    thread_mutex_unlock(mutex);
    return err;
}

static errval_t sfree(struct slot_allocator *ca, struct capref cap)
{
    struct single_slot_allocator *sca = (struct single_slot_allocator*)ca;
    if (!cnodecmp(cap.cnode, sca->cnode)) {
        return LIB_ERR_SLOT_ALLOC_WRONG_CNODE;
    }

    return free_slots(sca, cap.slot, 1, &ca->mutex);
}

cslot_t single_slot_alloc_freecount(struct single_slot_allocator *this)
{
    cslot_t freecount = 0;
    for (struct cnode_meta *walk = this->head; walk; walk = walk->next) {
        freecount += walk->space;
    }
    return freecount;
}

errval_t single_slot_alloc_resize(struct single_slot_allocator *this,
                                  cslot_t newslotcount)
{
    errval_t err;

    cslot_t grow = newslotcount - this->a.nslots;

    size_t bufgrow = SINGLE_SLOT_ALLOC_BUFLEN(grow);
    // Check if we need to refill slab allocator
    if (slab_freecount(&this->slab) < bufgrow) {
        // Cannot simply use malloc here!
        size_t alloc_size = ROUND_UP(bufgrow, BASE_PAGE_SIZE);

        struct capref bufcap;
        err = frame_alloc(&bufcap, alloc_size, &alloc_size);
        if (err_is_fail(err) || alloc_size < bufgrow) {
            USER_PANIC_ERR(err, "ram_alloc() in %s()", __FUNCTION__);
        }

        void *buf;
        err = paging_map_frame(get_current_paging_state(), &buf, bufgrow,
                               bufcap, NULL, NULL);
        if (err_is_fail(err)) {
            USER_PANIC_ERR(err, "paging_map_frame() in %s()", __FUNCTION__);
        }
        slab_grow(&this->slab, buf, bufgrow);
    }

    // Update free slot metadata
    err = free_slots(this, this->a.nslots, grow, &this->a.mutex);
    if (err_is_fail(err)) {
        return err;
    }

    // Update generic metadata
    this->a.nslots = newslotcount;

    return SYS_ERR_OK;
}

errval_t single_slot_alloc_init_raw(struct single_slot_allocator *ret,
                                    struct capref cap, struct cnoderef cnode,
                                    cslot_t nslots, void *buf, size_t buflen)
{
    /* Generic part */
    ret->a.alloc = salloc;
    ret->a.free  = sfree;
    ret->a.space = nslots;
    ret->a.nslots = nslots;
    thread_mutex_init(&ret->a.mutex);

    /* Specific part */
    ret->cap   = cap;
    ret->cnode = cnode;

    slab_init(&ret->slab, sizeof(struct cnode_meta), NULL);
    if (buflen > 0) {
        // check for callers that do not provide enough buffer space
        #if !defined(NDEBUG)
        //on arm, __builtin_return_address does not take arguments !=0
        #if !defined(__arm__) && !defined(__aarch64__) 
        size_t buflen_proper = SINGLE_SLOT_ALLOC_BUFLEN(nslots);
        if (buflen != buflen_proper) {
            debug_printf("******* FIXME: %s buflen=%zu != buflen_proper=%zu"
                         "call stack: %p %p\n",
                         __FUNCTION__, buflen, buflen_proper,
                         __builtin_return_address(0),
                         __builtin_return_address(1));
        }
        #endif
        #endif
        slab_grow(&ret->slab, buf, buflen);
    }

    ret->head = slab_alloc(&ret->slab);
    assert(ret->head != NULL);
    ret->head->slot = 0;
    ret->head->space = nslots;
    ret->head->next = NULL;

    return SYS_ERR_OK;
}

errval_t single_slot_alloc_init(struct single_slot_allocator *ret,
                                 cslot_t nslots, cslot_t *retslots)
{
    DBG(DETAILED,"single_slot_alloc_init\n");
    errval_t err;

    struct capref cap;
    struct cnoderef cnode;
    err = cnode_create(&cap, &cnode, nslots, &nslots);
    if (err_is_fail(err)) {
        return err_push(err, LIB_ERR_CNODE_CREATE);
    }
    size_t buflen = SINGLE_SLOT_ALLOC_BUFLEN(nslots);
    void *buf = malloc(buflen);
    if (!buf) {
        return LIB_ERR_MALLOC_FAIL;
    }
    //we touch all the space we got, to make sure it's actually backed by things
    memset(buf,0,buflen);

    err = single_slot_alloc_init_raw(ret, cap, cnode, nslots, buf, buflen);
    if (err_is_fail(err)) {
        return err_push(err, LIB_ERR_SINGLE_SLOT_ALLOC_INIT_RAW);
    }

    if (retslots) {
        *retslots = nslots;
    }

    return SYS_ERR_OK;
}
