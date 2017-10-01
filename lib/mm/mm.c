/**
 * \file
 * \brief A library for managing physical memory (i.e., caps)
 */

#include <mm/mm.h>
#include <aos/debug.h>


/**
 * Initialize the memory manager.
 *
 * \param  mm               The mm struct to initialize.
 * \param  objtype          The cap type this manager should deal with.
 * \param  slab_refill_func Custom function for refilling the slab allocator.
 * \param  slot_alloc_func  Function for allocating capability slots.
 * \param  slot_refill_func Function for refilling (making) new slots.
 * \param  slot_alloc_inst  Pointer to a slot allocator instance (typically passed to the alloc and refill functions).
 */
errval_t mm_init(struct mm *mm, enum objtype objtype,
                 slab_refill_func_t slab_refill_func,
                 slot_alloc_t slot_alloc_func,
                 slot_refill_t slot_refill_func,
                 void *slot_alloc_inst)
{
    debug_printf("libmm: mm_init started\n");
    assert(mm != NULL);

    mm->slot_alloc = slot_alloc_func;
    mm->slot_refill = slot_refill_func;
    mm->objtype = objtype;
    mm->slot_alloc_inst = slot_alloc_inst;

    // there is a default slab refill function that can be used if no function is provided.
    if(slab_refill_func == NULL){
        slab_refill_func = slab_default_refill;
    }
    // create the first slab to hold exactly one mnode
    slab_init(&mm->slabs, sizeof(struct mmnode), slab_refill_func);

    debug_printf("libmm: mm ready\n");
    return SYS_ERR_OK;
}

/**
 * Destroys the memory allocator.
 */
void mm_destroy(struct mm *mm)
{
}

/**
 * Adds a capability to the memory manager.
 *
 * \param  cap  Capability to add
 * \param  base Physical base address of the capability
 * \param  size Size of the capability (in bytes)
 */
errval_t mm_add(struct mm *mm, struct capref cap, genpaddr_t base, size_t size)
{
    debug_printf("libmm: add capability of size %zu at %zx", size, base);

    // TODO: implement
    
    
    
    // TODO: Implement
    return LIB_ERR_NOT_IMPLEMENTED;
}

/**
 * Allocate aligned physical memory.
 *
 * \param       mm        The memory manager.
 * \param       size      How much memory to allocate.
 * \param       alignment The alignment requirement of the base address for your memory.
 * \param[out]  retcap    Capability for the allocated region.
 */
errval_t mm_alloc_aligned(struct mm *mm, size_t size, size_t alignment, struct capref *retcap)
{
    // TODO: Implement
    return LIB_ERR_NOT_IMPLEMENTED;
}

/**
 * Allocate physical memory.
 *
 * \param       mm        The memory manager.
 * \param       size      How much memory to allocate.
 * \param[out]  retcap    Capability for the allocated region.
 */
errval_t mm_alloc(struct mm *mm, size_t size, struct capref *retcap)
{
    // TODO: Implement
    return LIB_ERR_NOT_IMPLEMENTED;
}

/**
 * Free a certain region (for later re-use).
 *
 * \param       mm        The memory manager.
 * \param       cap       The capability to free.
 * \param       base      The physical base address of the region.
 * \param       size      The size of the region.
 */
errval_t mm_free(struct mm *mm, struct capref cap, genpaddr_t base, gensize_t size)
{
    // TODO: Implement
    return LIB_ERR_NOT_IMPLEMENTED;
}


/**
 * Allocate slot.
 *
 */
errval_t mm_alloc_slot();
{
    // TODO: Implement
    // TODO: add description
    // TODO: add entry in declaration in .h
    return LIB_ERR_NOT_IMPLEMENTED;
}

/**
 * Free slot.
 *
 */
errval_t mm_free_slot();
{
    // TODO: Implement
    // TODO: add description
    // TODO: add entry in declaration in .h
    return LIB_ERR_NOT_IMPLEMENTED;
}

/**
 * Allocate slab.
 *
 */
errval_t mm_alloc_slab();
{
    // TODO: Implement
    // TODO: add description
    // TODO: add entry in declaration in .h
    return LIB_ERR_NOT_IMPLEMENTED;
}

/**
 * Free slab.
 *
 */
errval_t mm_free_slab();
{
    // TODO: Implement
    // TODO: add description
    // TODO: add entry in declaration in .h
    return LIB_ERR_NOT_IMPLEMENTED;
}

/**
 * add a mmnode to doubly linked list of mmnodes in mm.
 *
 */
errval_t mm_mmnode_add();
{
    // TODO: Implement
    // TODO: add description
    // TODO: add entry in declaration in .h
    return LIB_ERR_NOT_IMPLEMENTED;
}

/**
 * remove a mmnode from doubly linked list of mmnodes in mm.
 * 
 */
errval_t mm_mmnode_remove();
{
    // TODO: Implement
    // TODO: add description
    // TODO: add entry in declaration in .h
    return LIB_ERR_NOT_IMPLEMENTED;
}



