/**
 * \file
 * \brief A library for managing physical memory (i.e., caps)
 */

#include <mm/mm.h>
#include <aos/debug.h>

/// Create a new node.
static struct mmnode* mm_create_node(struct mm *mm, enum nodetype type,
                                     genpaddr_t base, gensize_t size)
{
    // Create the node in memory.
    struct mmnode* node = slab_alloc(&mm->slabs);
    node->base=base;
    node->size=size;
    node->type=type;
    return node;
}

/// Add a new node to the correct position in the mm structure.
static errval_t mm_mmnode_add(struct mm *mm, genpaddr_t base, gensize_t size,
                              struct mmnode **retnode)
{
    // Check that the memory is still free by iterating over the mm list.
    struct mmnode* current_node = mm->head;
    struct mmnode* prev_node = NULL;
    while(current_node != NULL){
        if (base < current_node->base){
            // We want to insert before this node.
            if (current_node->base < base + size){
                // The base is within an already allocated sector of memory.
                return MM_ERR_ALREADY_ALLOCATED;
            } else {
                // We have found the right spot.
                break;
            }

        } else {
            // We have not found our spot yet.
            if (base < current_node->base + current_node->size){
                // The base is within an already allocated sector of memory.
                return MM_ERR_ALREADY_ALLOCATED;
            }
            // Else, fall through and go to the next node
        }

        prev_node = current_node;
        current_node = current_node->next;
    }

    // Create the new node.
    struct mmnode* new_node = mm_create_node(mm, NodeType_Free, base, size);

    if (current_node == NULL){
        // We are appending to the end of the list.
        if (mm->head == NULL){
            // We just created the first node.
            mm->head = new_node;
            new_node->prev = NULL;
            new_node->next = NULL;
        } else {
            // prev_node holds the last node of the list.
            prev_node->next = new_node;
            new_node->next = NULL;
            new_node->prev = prev_node;
        }
    } else {
        // Append before the current_node.
        if(prev_node == NULL){
            // We are inserting at the first position, replacing the list head.
            new_node->next = current_node;
            new_node->prev = NULL;
            current_node->prev = new_node;
            mm->head = new_node;
        } else {
            // Insert between the current_node and previous_node.
            new_node->next = current_node;
            new_node->prev = prev_node;
            prev_node->next = new_node;
            current_node->prev = new_node;
        }
    }

    // Return the node we just created.
    *retnode = new_node;

    return SYS_ERR_OK;
}

/// Remove a node from the mm structure.
static errval_t mm_mmnode_remove(struct mm *mm, struct mmnode **p_node)
{
    // Ease of use pointer.
    struct mmnode *node = *p_node;
    if (node->prev == NULL) {
        // We are removing the head.
        mm->head = node->next;
        if (mm->head)
            mm->head->prev = NULL;
    } else {
        node->prev->next = node->next;
        if (node->next)
            node->next->prev = node->prev;
    }

    // Free the node memory.
    slab_free(&mm->slabs, node);

    return SYS_ERR_OK;
}

/// Merge two mm nodes together.
static errval_t mm_mmnode_merge(struct mm *mm, struct mmnode *first,
                               struct mmnode *second)
{
    // Adjust the size of the first node.
    first->size += second->size;
    // Remove the second node.
    CHECK(mm_mmnode_remove(mm, &second));

    return SYS_ERR_OK;
}

/// Find a free node with at least [size] in the mm structure.
static errval_t mm_mmnode_find(struct mm *mm, size_t size, size_t alignment,
                               struct mmnode **retnode)
{
    assert(retnode != NULL);

    // Start at head of mm list.
    struct mmnode *node = mm->head;

    while (node != NULL) {
        size_t dif = node->base % alignment;
        if(dif > 0)
            dif = alignment - dif;
        if (node->type == NodeType_Free &&
                ((gensize_t) (size+dif) <= node->size)) {
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

/// Allocate slots and refill the slot allocator if necessary.
static errval_t mm_slot_alloc(struct mm *mm, uint64_t slots,
                              struct capref* cap)
{
    struct slot_prealloc* sa = (struct slot_prealloc*) mm->slot_alloc_inst;

    // Check both entries of the metadata array for free slots
    // we need slots to allocate new ones so make sure the slot count is above
    // a certain threshold. Also make sure that we are not currently refilling
    // as this would lead to a infinite recursion.
    // In the worst case we also need to refill the slabs

    if (sa->meta[0].free < (uint64_t) 4 && sa->meta[1].free < (uint64_t) 4 &&
            !sa->is_refilling) {
        // No free slots left, try to create new ones.
        sa->is_refilling = true;
        CHECK(mm->slot_refill(mm->slot_alloc_inst));
        sa->is_refilling = false;
    }
    CHECK(mm->slot_alloc(mm->slot_alloc_inst, slots, cap));
    return SYS_ERR_OK;
}

/// Debug printer for the mm structure.
__attribute__((unused))
static void mm_print_manager(struct mm *mm){
    debug_printf("Dumping Memory Manager nodes:\n");
    struct mmnode* node = mm->head;
    int i = 0;

    if (node == NULL)
        printf("    MM list empty!\n");
    while(node != NULL){
        struct frame_identity fi;
        frame_identify(node->cap.cap, &fi);
        printf("    Node %d: start: %zx, size: %"PRIu64" KB - Cap says: " \
               "base: %zx size: %"PRIu64" KB - ",
                i, node->base, node->size / 1024 , fi.base, fi.bytes / 1024);
        if (node->type == NodeType_Free)
            printf("Node free\n");
        if (node->type == NodeType_Allocated)
            printf("Node allocated\n");
        node = node->next;
        ++i;
    }
}

/**
 * \brief Initialize the memory manager.
 *
 * \param  mm               The mm struct to initialize.
 * \param  objtype          The cap type this manager should deal with.
 * \param  slab_refill_func Custom function for refilling the slab allocator.
 * \param  slot_alloc_func  Function for allocating capability slots.
 * \param  slot_refill_func Function for refilling (making) new slots.
 * \param  slot_alloc_inst  Pointer to a slot allocator instance.
 * \returns                 Error return code.
 */
errval_t mm_init(struct mm *mm, enum objtype objtype,
                 slab_refill_func_t slab_refill_func,
                 slot_alloc_t slot_alloc_func,
                 slot_refill_t slot_refill_func,
                 void *slot_alloc_inst)
{
    debug_printf("libmm: Initializing...\n");
    assert(mm != NULL);

    mm->slot_alloc = slot_alloc_func;
    mm->slot_refill = slot_refill_func;
    mm->objtype = objtype;
    mm->slot_alloc_inst = slot_alloc_inst;
    mm->head = NULL;
    mm->refilling_slabs = false;

    // There is a default slab refill function that can be used if no
    // custom function is provided.
    if(slab_refill_func == NULL){
        slab_refill_func = slab_default_refill;
    }
    // Create the first slab to hold exactly one mmnode.
    slab_init(&mm->slabs, sizeof(struct mmnode), slab_refill_func);

    debug_printf("libmm: Initialized\n");
    return SYS_ERR_OK;
}

/**
 * Destroys a/the memory allocator.
 *
 * \param   mm  The memory allocator to destroy.
 */
void mm_destroy(struct mm *mm)
{
    // Iterate over all mm nodes and destroy their capabilities.
    struct mmnode *node = mm->head;
    struct mmnode *next_node = mm->head;
    while (node != NULL) {
        cap_revoke(node->cap.cap);
        cap_destroy(node->cap.cap);
        next_node = node->next;
        mm_mmnode_remove(mm, &node);
        node = next_node;
    }
    // destroy our ram cap
    cap_revoke(mm->ram_cap);
    cap_destroy(mm->ram_cap);
}

/**
 * \brief Adds a capability to the memory manager.
 *
 * \param  cap  Capability to add.
 * \param  base Physical base address of the capability.
 * \param  size Size of the capability (in bytes).
 * \returns     Error return code.
 */
errval_t mm_add(struct mm *mm, struct capref cap, genpaddr_t base,
                gensize_t size)
{
    debug_printf("libmm: Adding a capability of size %"PRIu64" MB at %zx \n",
                 size / 1048576, base);

    // Create the node.
    struct mmnode *node = NULL;

    // Finish the node and add it to the list.
    CHECK(mm_mmnode_add(mm, base, size, &node));

    assert(node != NULL);

    // Create the capability.
    node->cap.base = base;
    node->cap.size = size;

    // Store the initial head's base address.
    mm->initial_base = base;

    // Add the capability to the node.
    CHECK(mm_slot_alloc(mm, 1, &(node->cap.cap)));

    // Store reference to the capability.
    mm->ram_cap = cap;
    CHECK(cap_retype(node->cap.cap, mm->ram_cap, 0, mm->objtype,
                     (gensize_t) size, 1));

    return SYS_ERR_OK;
}

/**
 * Allocate aligned physical memory.
 *
 * \param       mm        The memory manager.
 * \param       size      How much memory to allocate.
 * \param       alignment The base address alignment requirement.
 * \param[out]  retcap    Capability for the allocated region.
 */
errval_t mm_alloc_aligned(struct mm *mm, size_t size, size_t alignment,
                          struct capref *retcap)
{
    assert(retcap != NULL);

    // TODO: some space is lost here (page tables)
    // Adjust alignment to page size, making sure that the addresses are
    // aligned and not just the size.
    if (alignment % BASE_PAGE_SIZE != 0) {
        alignment += (size_t) BASE_PAGE_SIZE - (alignment % BASE_PAGE_SIZE);
    }

    // Adjust size to base alignment.
    if (size % BASE_PAGE_SIZE != 0) {
        size += (size_t) BASE_PAGE_SIZE - (size % BASE_PAGE_SIZE);
    }

    struct mmnode *node = NULL;

    // Check if we have enough slabs left first. If we fill the last slab,
    // we cannot create new slabs because they need slabs themselves.
    // We do this here to not disrupt the addition of the actual node
    if(slab_freecount(&mm->slabs) < 4 && ! mm->refilling_slabs){
        // Indicate that we are refilling the slabs.
        mm->refilling_slabs = true;
        CHECK(mm->slabs.refill_func(&mm->slabs));
        // Indicate that we are done.
        mm->refilling_slabs = false;
    }

    // Find a free node in the list.
    CHECK(mm_mmnode_find(mm, size, alignment, &node));

    assert(node != NULL);
    assert(node->type == NodeType_Free);
    size_t dif = node->base % alignment;
    if(dif > 0)
        dif = alignment - dif;

    // Split the node to the correct size.
    struct mmnode *new_node = NULL;
    if (node->size > (gensize_t)(size+dif)){
        //todo: fix this ugly ugly hack properly. This hack currently has us lose space to fullfill alignment restrictions
        //      ideally we'd just allocate a new node and store the remaining space in there
        node->base += dif;
        node->size -= dif;

        // Store old size.
        genpaddr_t orig_node_base = node->base;

        // Set size of existing node.
        node->size -= size;
        node->base += (genpaddr_t) size;

        // Create the new node.
        CHECK(mm_mmnode_add(mm, orig_node_base, size, &new_node));

        // Add the capability to the node.
        new_node->cap.base = orig_node_base;
        new_node->cap.size = size;
        node->cap.size -= size;
        node->cap.base += size;

        assert(new_node != NULL);
    } else {
        //todo: fix this ugly ugly hack properly. This hack currently has us lose space to fullfill alignment restrictions
        //      ideally we'd just allocate a new node and store the remaining space in there
        node->base += dif;
        node->size -= dif;
        // Size of the node exactly as needed.
        slab_free(&(mm->slabs), new_node);
        new_node = node;
    }

    // TODO: cleanup

    // Create the capability.
    CHECK(mm_slot_alloc(mm, 1, &(new_node->cap.cap)));

    CHECK(cap_retype(new_node->cap.cap, mm->ram_cap,
                     new_node->base - mm->initial_base, mm->objtype,
                     (gensize_t) size, 1));

    new_node->type = NodeType_Allocated;
    *retcap = new_node->cap.cap;
    return SYS_ERR_OK;
}

/**
 * \brief Allocate physical memory.
 *
 * \param       mm        The memory manager.
 * \param       size      How much memory to allocate.
 * \param[out]  retcap    Capability for the allocated region.
 * \returns               Error return code.
 */
errval_t mm_alloc(struct mm *mm, size_t size, struct capref *retcap)
{
    return mm_alloc_aligned(mm, size, BASE_PAGE_SIZE, retcap);
}

/**
 * \brief Free a certain region (for later re-use).
 *
 * \param       mm        The memory manager.
 * \param       cap       The capability to free.
 * \param       base      The physical base address of the region.
 * \param       size      The size of the region.
 * \returns               Error return code.
 */
errval_t mm_free(struct mm *mm, struct capref cap, genpaddr_t base,
                 gensize_t size)
{
    // Iterate over the list to find the correct node to free.
    struct mmnode *node = mm->head;
    while (node != NULL) {
        // Try matching based on base address and size.
        if (node->base == base && node->size - size <= BASE_PAGE_SIZE) {
            // Free the node.
            node->type = NodeType_Free;

            // XXX: What's happening here exactly, why are both calls
            // throwing errors and why are we ok with it?..

            // Revoke children and delete the capability.
            cap_revoke(node->cap.cap);
            //CHECK(cap_revoke(node->cap.cap));
            // destroy = delete + free slot.
            cap_destroy(node->cap.cap);
            //CHECK(cap_destroy(node->cap.cap));

            // Try to merge with node before or after or both.
            if (node->next != NULL && node->next->type == NodeType_Free) {
                // Merge with next.
                CHECK(mm_mmnode_merge(mm,node,node->next));
            }
            if (node->prev != NULL && node->prev->type == NodeType_Free) {
                // Merge with prev.
                CHECK(mm_mmnode_merge(mm,node->prev,node));
            }
            return SYS_ERR_OK;
        }
        node = node->next;
    }

    debug_printf("Failed to free, node not found\n");
    return MM_ERR_MM_FREE;
}
