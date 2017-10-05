/**
 * \file
 * \brief A library for managing physical memory (i.e., caps)
 */

#include <mm/mm.h>
#include <aos/debug.h>

//#############################
// private function definitions
//#############################
errval_t mm_mmnode_add(struct mm *mm, genpaddr_t base, gensize_t size, struct mmnode **node);
struct mmnode* mm_create_node(struct mm *mm, enum nodetype type, genpaddr_t base, gensize_t size);
errval_t mm_mmnode_remove(struct mm *mm, struct mmnode **node);
errval_t mm_mmnode_find(struct mm *mm, size_t size, struct mmnode **retnode);
void mm_print_manager(struct mm *mm);
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
    mm->head = NULL;
    mm->refilling_slabs = false;

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
    // Iterate over all mm nodes and destroy their capabilities.
    struct mmnode *node = mm->head;
    struct mmnode *next_node = mm->head;
    while (node != NULL) {
        cap_destroy(node->cap.cap);
        next_node = node->next;
        mm_mmnode_remove(mm, &node);
        node = next_node;
    }
}

/**
 * Adds a capability to the memory manager.
 *
 * \param  cap  Capability to add
 * \param  base Physical base address of the capability
 * \param  size Size of the capability (in bytes)
 */
errval_t mm_add(struct mm *mm, struct capref cap, genpaddr_t base, gensize_t size)
{
    debug_printf("libmm: add capability of size %"PRIu64" MB at %zx \n", size / 1048576, base);

    // create the node
    struct mmnode *node = NULL;

    // finish the node and add it to the list
    errval_t err = mm_mmnode_add(mm, base, size, &node);

    // add the capability to the node
    if (err_is_ok(err)) {
        assert(node != NULL);
        
        // create the capability
        node->cap.cap = cap;
        node->cap.base = base;
        node->cap.size = size;
        
        mm->initial_base = base;
    } else {
        debug_printf("mm_add: %s", err_getstring(err));
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
    if (size % alignment != 0) {
        size += (size_t) alignment - (size % alignment);
    }
    assert(retcap != NULL);
    debug_printf("Allocate %u bytes\n", size);
    
    errval_t err;
    struct mmnode *node = NULL;

    // Find a free node in the list.
    err = mm_mmnode_find(mm, size, &node);
    if (err_is_fail(err)) {
        return err;
    }
    
    assert(node != NULL);
    assert(node->type == NodeType_Free);

    // Split the node
    struct mmnode *new_node = NULL;
    if (node->size > (gensize_t)size){
        // store old size
        genpaddr_t orig_node_base = node->base;

        // set size of existing node
        node->size -= size;
        node->base += size;

        // create the new node
        err = mm_mmnode_add(mm, orig_node_base, size, &new_node);

        // add the capability to the node
        if (err_is_fail(err)) {
            node->size += size;
            node->base -= size;
            return err;
        }
        
        // create the new capability
        err = mm->slot_alloc(mm->slot_alloc_inst, 1, &(new_node->cap.cap));
        if (err_is_fail(err)) {
            debug_printf("mm_alloc: could not create a slot");
            node->size += size;
            node->base -= size;
            return err;
        }
        new_node->cap.base = orig_node_base;
        new_node->cap.size = size;
        node->cap.size -= size;
        node->cap.base += size;
        
        err = cap_retype(new_node->cap.cap, node->cap.cap, new_node->base - mm->initial_base, mm->objtype, (gensize_t) size, 1);
        if (err_is_fail(err)) {
            debug_printf("mm_alloc: could not retype cap %s \n", err_getstring(err));
            node->size += size;
            node->base -= size;
            return err;
        }
        assert(new_node != NULL);
        
    } else {
        // TODO: I don't think this works atm
        slab_free(&(mm->slabs), new_node);
        new_node = node;
    }
    
    new_node->type = NodeType_Allocated;
    *retcap = new_node->cap.cap;
    
    debug_printf("Allocated %u bytes\n", size);
    return SYS_ERR_OK;
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
    return mm_alloc_aligned(mm, size, BASE_PAGE_SIZE, retcap);
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
    struct mmnode *node = mm->head;
    while (node != NULL) {
        if (node->base == base && node->size == size) {
            node->type = NodeType_Free;
            cap_destroy(node->cap.cap);
            debug_printf("Freed\n");
            return SYS_ERR_OK;
        }
        node = node->next;
    }

    debug_printf("Failed to free\n");
    return MM_ERR_MM_FREE;
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
errval_t mm_mmnode_add(struct mm *mm, genpaddr_t base, gensize_t size, struct mmnode **node)
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

    struct mmnode* new_node = mm_create_node(mm, NodeType_Free, base, size);

    if (current_node == NULL){
        // append to the end of the list
        if (mm->head == NULL){
            // we just created the first node
            mm->head = new_node;
            new_node->prev = NULL;
            new_node->next = NULL;
        } else {
            // prev_node holds the last node of the list
            prev_node->next = new_node;
            new_node->next = NULL;
            new_node->prev = prev_node;
        }
    } else {
        // append before the current_node
        if(prev_node == NULL){
            // new head
            new_node->next = current_node;
            new_node->prev = NULL;
            current_node->prev = new_node;
            mm->head = new_node;
        } else {
            new_node->next = current_node;
            new_node->prev = prev_node;
            prev_node->next = new_node;
            current_node->prev = new_node;
        }
    }

    // set node to the one we just created
    *node = new_node;
    return SYS_ERR_OK;
}

struct mmnode* mm_create_node(struct mm *mm, enum nodetype type, genpaddr_t base, gensize_t size){
    // check if we have enough slabs left. If we fill the last slab, we cannot create new slabs because they need slabs themselves.
    if(slab_freecount(&mm->slabs) < 2 && ! mm->refilling_slabs){
        // indicate that we are refilling the slabs
        mm->refilling_slabs = true;
        errval_t err;
        err = mm->slabs.refill_func(&mm->slabs);
        //done
        mm->refilling_slabs = false;
        if(err_is_fail(err)){
            return NULL;
        }
    }
    
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

/**
 * Find a free mmnode with at least [size] in the mm doubly linked list.
 *
 * \param       mm      The mm struct to search in
 * \param       size    The size to try and fit
 * \param[out]  retnode The fitting node
 */
errval_t mm_mmnode_find(struct mm *mm, size_t size, struct mmnode **retnode)
{
    assert(retnode != NULL);

    // Start at head of mm list.
    struct mmnode *node = mm->head;

    while (node != NULL) {
        if (node->type == NodeType_Free &&
                ((gensize_t) size <= node->size)) {
            // Free node found where size fits.
            *retnode = node;
            return SYS_ERR_OK;
        }
        // No match, continue iterating.
        node = node->next;
    }

    // No matching node found.
    return MM_ERR_NOT_FOUND;
}

// debug print
void mm_print_manager(struct mm *mm){
    debug_printf("Dumping Memory Manager nodes:\n");
    struct mmnode* node = mm->head;
    int i = 0;

    if (node == NULL)
        printf("    MM list empty!\n");
    while(node != NULL){
        printf("    Node %d: start: %zx, size: %"PRIu64" KB - Cap says: base: %zx size: %"PRIu64" KB - ",
                i, node->base, node->size / 1024 , node->cap.base, node->cap.size / 1024);
        if (node->type == NodeType_Free)
            printf("Node free\n");
        if (node->type == NodeType_Allocated)
            printf("Node allocated\n");
        node = node->next;
        ++i;
    }
}

