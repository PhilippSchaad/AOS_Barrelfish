/**
 * \file
 * \brief A library for managing physical memory (i.e., caps)
 */

#include <mm/mm.h>
#include <aos/debug.h>

//#############################
// private function definitions
//#############################
errval_t mm_alloc_slot(void);
errval_t mm_free_slot(void);
errval_t mm_alloc_slab(void);
errval_t mm_free_slab(void);
errval_t mm_mmnode_add(struct mm *mm, genpaddr_t base, uint8_t size, struct mmnode **node);
struct mmnode* mm_create_node(struct mm *mm, enum nodetype type, genpaddr_t base, gensize_t size);
errval_t mm_mmnode_remove(struct mm *mm, struct mmnode **node);
//#############################



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

    // create the head of the doubly linked list
    // TODO: do

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
    debug_printf("libmm: add capability of size %zu at %zx \n", size, base);

    // create the node
    struct mmnode *node = NULL;

    // finish the node and add it to the list
    errval_t err = mm_mmnode_add(mm, base, size, &node);

    // add the capability to the node
    if (err_is_ok(err)) {

        // create the capability struct
        struct capinfo capability = {
            .cap = cap,
            .base = base,
            .size = size
        };

        assert(node != NULL);
        node->cap = capability;
    }
    return err;
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
errval_t mm_alloc_slot(void)
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
errval_t mm_free_slot(void)
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
errval_t mm_alloc_slab(void)
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
errval_t mm_free_slab(void)
{
    // TODO: Implement
    // TODO: add description
    // TODO: add entry in declaration in .h
    return LIB_ERR_NOT_IMPLEMENTED;
}

/**
 * Add a mmnode to doubly linked list of mmnodes in mm.
 *
 * \param       mm      The mm structure to insert into
 * \param       cap     Capability to add
 * \param       base    Physical base address of the capability
 * \param       size    Size of the capability (in bytes)
 * \param[out]  node    Pointe to the newly added node
 */
errval_t mm_mmnode_add(struct mm *mm, genpaddr_t base, uint8_t size, struct mmnode **node)
{
    // this function should create a node in the memory (slab) and should add the node to the linked list (at the right position)
    // TODO: define alignment

    // check that the memory is still free
    struct mmnode* current_node = mm->head;
    struct mmnode* prev_node = NULL;
    while(current_node != NULL){
        if (base < current_node->base){
            // the address is smaller
            if (current_node->base < base + size){
                // the base is within an already allocated sector of memory. raise an error
                // TODO: Raise error
                return 1;
            } else {
                // we have found the right spot
                break;
            }
        } else {
            // base is larger than the current node (or equal)
            if (base < current_node->base + current_node->size){
                // the base is within an already allocated sector of memory. raise an error
                // TODO: Raise error
                return 1;
            }
            // else go to the next node
        }

        prev_node = current_node;
        current_node = current_node->next;
    }

    struct mmnode* actual_node = mm_create_node(mm, NodeType_Allocated, base, size);

    if (current_node == NULL){
        // append to the end of the list
        if (mm->head == NULL){
            // we just created the first node
            mm->head = actual_node;
            actual_node->prev = NULL;
            actual_node->next = NULL;
        } else {
            // prev_node holds the last node of the list
            prev_node->next = actual_node;
            actual_node->next = NULL;
            actual_node->prev = prev_node;
        }
    } else {
        // append before the current_node
        actual_node->next = current_node;
        actual_node->prev = current_node->prev;
        current_node->prev->next = actual_node;
        current_node->prev = actual_node;
    }

    // set node to the one we just created
    *node = actual_node;
        return 0;
        // TODO: check for freed memory
}

struct mmnode* mm_create_node(struct mm *mm, enum nodetype type, genpaddr_t base, gensize_t size){
    // create the node in memory
    struct mmnode* node = slab_alloc(&mm->slabs);
    node->base=base;
    node->size=size;
    node->type=type;
    return node;
}

/**
 * remove a mmnode from doubly linked list of mmnodes in mm.
 *
 * \param mm    The mm structure to remove from
 * \param node  The mmnode to remove
 */
errval_t mm_mmnode_remove(struct mm *mm, struct mmnode **p_node)
{
    // TODO: add entry in declaration in .h

    // Ease of use pointer
    struct mmnode *node = *p_node;
    if (node->prev == NULL) {
        // We are removing the head
        mm->head = node->next;
        if (mm->head)
            mm->head->prev = NULL;
    } else {
        node->prev->next = node->next;
        if (node->next)
            node->next->prev = node->prev;
    }

    // Free the node memory
    slab_free(&mm->slabs, node);

    return SYS_ERR_OK;
}
