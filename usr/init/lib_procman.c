#include <lib_procman.h>

/// Find a process in the proclist with a given ID.
static errval_t find_process_by_id(domainid_t proc_id,
                                   struct process_info **ret_pi)
{
    DBG(DETAILED, "Trying to lookup process with ID %" PRIuDOMAINID "\n",
        proc_id);

    assert(pt != NULL);
    assert(pt->head != NULL);

    struct process_info *pi = pt->head;

    while (pi != NULL) {
        if (pi->id == proc_id) {
            *ret_pi = pi;
            return SYS_ERR_OK;
        }
        pi = pi->next;
    }

    return 1; // TODO: Create error for process ID not found.
}

/// Initialize the procman.
errval_t procman_init(void)
{
    DBG(DETAILED, "Initializing the process manager\n");
    
    assert(pt == NULL);

    pt = malloc(sizeof(struct process_table));
    if (pt == NULL) {
        USER_PANIC("Failed to create Process-table!\n");
        // TODO: handle?
    }

    // Add the init process to the list.
    pt->head = malloc(sizeof(struct process_info));
    if (pt->head == NULL) {
        USER_PANIC("Failed to create Process-table entry for init!\n");
        // TODO: handle?
    }
    pt->head->id = 0;
    pt->head->core = 0;
    pt->head->next = NULL;
    pt->head->prev = NULL;
    pt->head->name = "init";
    // TODO: Add spawn-info

    return SYS_ERR_OK;
}

/*
/// Register a process.
domainid_t procman_register_process(char *name, struct spawninfo *si,
                                    coreid_t core_id)
{
    DBG(DETAILED, "Registering process %s\n", name);

    assert(pt != NULL);
    assert(pt->head != NULL);

    struct process_info *pi = pt->head;

    while (pi->next != NULL)
        pi = pi->next;

    struct process_info *new_proc = malloc(sizeof(struct process_info));
    if (new_proc == NULL) {
        USER_PANIC("Failed to create Process-Info for new process!\n");
        // TODO: handle?
    }

    new_proc->id = pi->id + 1;
    new_proc->core = core_id;
    new_proc->name = name;
    new_proc->si = *si;
    new_proc->next = NULL;
    new_proc->prev = pi;

#if DEBUG_LEVEL == DETAILED
    procman_print_proc_list();
#endif

    pi->next = new_proc;

    return new_proc->id;
}
*/

/// Register a process.
domainid_t procman_register_process(char *name, coreid_t core_id)
{
    DBG(DETAILED, "Registering process %s\n", name);

    assert(pt != NULL);
    assert(pt->head != NULL);

    struct process_info *pi = pt->head;

    while (pi->next != NULL)
        pi = pi->next;

    struct process_info *new_proc = malloc(sizeof(struct process_info));
    if (new_proc == NULL) {
        USER_PANIC("Failed to create Process-Info for new process!\n");
        // TODO: handle?
    }

    new_proc->id = pi->id + 1;
    new_proc->core = core_id;
    new_proc->name = name;
    new_proc->next = NULL;
    new_proc->prev = pi;

    pi->next = new_proc;

#if DEBUG_LEVEL == DETAILED
    procman_print_proc_list();
#endif

    return new_proc->id;
}

/// Deregister a process.
errval_t procman_deregister(domainid_t proc_id)
{
    DBG(DETAILED, "Deregistering process id %" PRIuDOMAINID "\n", proc_id);

    assert(pt != NULL);
    assert(pt->head != NULL);

    struct process_info *pi = NULL;
    CHECK(find_process_by_id(proc_id, &pi));

    assert(pi->prev != NULL); // That would mean deregisterint init.

    pi->prev->next = pi->next;
    if (pi->next)
        pi->next->prev = pi->prev;

    free(pi);

    return SYS_ERR_OK;
}

/// List all processes to stdout.
void procman_print_proc_list(void)
{
    assert(pt != NULL);
    assert(pt->head != NULL);

    debug_printf("Dumping process list:\n");
    debug_printf("=====================================\n");
    debug_printf("| %9s | %9s | %9s |\n", "ID", "NAME", "CORE");
    debug_printf("=====================================\n");
    struct process_info *pi = pt->head;
    while (pi != NULL) {
        debug_printf("| %9d | %9s | %9d |\n", pi->id, pi->name, pi->core);
        pi = pi->next;
    }
    debug_printf("=====================================\n");
}

/// Kill a process.
errval_t procman_kill_process(domainid_t proc_id)
{
    // TODO: Implement
    return LIB_ERR_NOT_IMPLEMENTED;
}

/// Find a process name by its process ID.
char *procman_lookup_name_by_id(domainid_t proc_id)
{
    DBG(DETAILED, "Looking up name of process %" PRIuDOMAINID "\n", proc_id);

    assert(pt != NULL);
    assert(pt->head != NULL);

    struct process_info *pi = NULL;
    CHECK(find_process_by_id(proc_id, &pi));

    return pi->name;
}
