#include <aos/aos_rpc_shared.h>

#include <lib_procman.h>

/*
 * In theory this works as follows:
 * when the process is created in init, a new process is created in the table.
 * However, the process has then to register itself. this is the point at which
 * the interface to the new process is fully functional. So if you need to send
 * a
 * message to a process, check if the channel is set in the process_info.
 *
 */

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

    return PROCMAN_ERR_PID_NOT_FOUND;
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

    return SYS_ERR_OK;
}

/// Register a process.
// All information is on core 0. But: capsrefs are stored on the core on which
// the process was started to avoid forging them on the other core.
domainid_t procman_register_process(char *name, coreid_t core_id,
                                    struct spawninfo *si)
{
    DBG(DETAILED, "Registering process %s\n", name);

    assert(pt != NULL);
    assert(pt->head != NULL);

    struct process_info *pi = pt->head;

    while (pi->next != NULL)
        pi = pi->next;

    // Get the PID.
    domainid_t pid;
    // If the core_id is on the other core, forward the request to the other
    // core.
    if (disp_get_core_id() != 0) {
        pid = urpc_register_process(name);
    } else {
        pid = pi->id + 1;
    }

    struct process_info *new_proc = malloc(sizeof(struct process_info));
    if (new_proc == NULL) {
        USER_PANIC("Failed to create Process-Info for new process!\n");
        // TODO: handle?
    }

    new_proc->id = pid;
    new_proc->core = core_id;
    new_proc->name = name;
    new_proc->next = NULL;
    new_proc->prev = pi;
    new_proc->si = si;
    new_proc->init = false;

    pi->next = new_proc;

#if (DEBUG_LEVEL == DETAILED)
    procman_print_proc_list();
#endif

    return pid;
}

void procman_foreign_preregister(domainid_t pid, char *name, coreid_t core_id,
                                 struct spawninfo *si)
{
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

    new_proc->id = pid;
    new_proc->core = core_id;
    new_proc->name = name;
    new_proc->next = NULL;
    new_proc->prev = pi;
    new_proc->si = si;
    new_proc->init = false;

    pi->next = new_proc;
    return;
}

domainid_t procman_finish_process_register(char *name, struct lmp_chan *chan)
{
    DBG(DETAILED, "Finish registration of process  %s\n", name);
    // Find the right process.
    // TODO: what happens if two process with the same name want to register at
    // the same time?

    assert(pt != NULL);
    assert(pt->head != NULL);
    struct process_info *pi = pt->head;
    while (pi != NULL) {
        if (!pi->init && strcmp(name, pi->name) == 0) {
            pi->chan = chan;
            pi->init = true;
            return pi->id;
        }
        pi = pi->next;
    }

    // there was no previous register of the process.
    // This situation might happen, if a process is spawned directly by the
    // init process.
    // This means that the spawn information is not saved. If you want to
    // persist the spawninfo here, you should call procman_register_process
    // from init (right before or after the spawning)
    procman_register_process(name, disp_get_core_id(), NULL);
    return procman_finish_process_register(name, chan);
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
    assert(proc_id != 0);

    pi->prev->next = pi->next;
    if (pi->next)
        pi->next->prev = pi->prev;

    // Destroy leftovers of process.
    // TODO: maybe we could add a function here that does additional cleanup:
    // - free memory
    // - destroy caps
    // - ...
    // cap_destroy(pi->si->l1_cnode);
    // free(pi->si);
    // free(pi->chan);

    // If the process is on the other core, delete it there as well. Keep in
    // mind that all processes are registered on core 0.
    if (disp_get_core_id() != 0 || pi->core != 0) {
        // TODO: urpc call to other core to make the cleanup there
    }

    free(pi);

#if (DEBUG_LEVEL == DETAILED)
    procman_print_proc_list();
#endif

    return SYS_ERR_OK;
}

/// List all processes to stdout.
void procman_print_proc_list(void)
{
    // Check if we are on core 0.
    // If the core_id is on the other core, forward the request to the other
    // core.
    if (disp_get_core_id() != 0) {
        // TODO: urpc call to other core to display process list
        return;
    }

    assert(pt != NULL);
    assert(pt->head != NULL);

    printf("===========================================\n");
    printf("| %-9s | %-20s | %-4s |\n", "ID", "NAME", "CORE");
    printf("===========================================\n");
    struct process_info *pi = pt->head;
    while (pi != NULL) {
        printf("| %-9d | %-20s | %-4d |\n", pi->id, pi->name, pi->core);
        pi = pi->next;
    }
    printf("===========================================\n");
}

/// Kill a process.
errval_t procman_kill_process(domainid_t proc_id)
{
    DBG(DETAILED, "Kill process id %" PRIuDOMAINID "\n", proc_id);

    assert(pt != NULL);
    assert(pt->head != NULL);

    struct process_info *pi = NULL;
    errval_t err = find_process_by_id(proc_id, &pi);
    // If we don't find the process here, it may be on the other core.
    if (err_is_fail(err)) {
        if (disp_get_core_id() != 0) {
            // Call to other core.
            return PROCMAN_ERR_PROCESS_ON_OTHER_CORE;
        } else {
            return PROCMAN_ERR_PID_NOT_FOUND;
        }
    }

    // Check if we are on the right core.
    if (disp_get_core_id() != pi->core) {
        // TODO: urpc call to other core to kill process.
        return PROCMAN_ERR_PROCESS_ON_OTHER_CORE;
    } else {
        if (!pi->init) {
            // Process not ready yet...
            // TODO: maybe we can handle this differently
            // TODO: this currently leads to a race condition on foreign cores
            DBG(DETAILED, "PROCMAN_ERR_PROCESS_NOT_YET_STARTED\n");
            return PROCMAN_ERR_PROCESS_NOT_YET_STARTED;
        }
        // cap_destroy(pi->si->dispatcher);
        // lmp_chan_destroy(pi->chan);
        // cap_destroy(pi->chan->remote_cap);
        // free(pi->chan);
        // TODO: rpc call or destroy vital cap

        // actual killing. this currently works using rpc which is not that
        // nice but I have not been able to
        // kill the process without locking the system.
        CHECK(send(pi->chan, NULL_CAP, RPC_MESSAGE(RPC_TYPE_PROCESS_KILL), 0,
                   NULL, NULL_EVENT_CLOSURE,
                   request_fresh_id(RPC_TYPE_PROCESS_KILL)));
    }

    // Deregister.
    procman_deregister(proc_id);

    return SYS_ERR_OK;
}

/// Find a process name by its process ID.
// Returns NULL if not found.
char *procman_lookup_name_by_id(domainid_t proc_id)
{
    DBG(DETAILED, "Looking up name of process %" PRIuDOMAINID "\n", proc_id);

    assert(pt != NULL);
    assert(pt->head != NULL);

    struct process_info *pi = NULL;
    errval_t err = find_process_by_id(proc_id, &pi);

    // If we are not on core 0 it may happen that we do not find the process.
    if (err == PROCMAN_ERR_PID_NOT_FOUND) {
        if (disp_get_core_id() != 0) {
            // TODO: urpc call to other core.
            return NULL;
        }
        return NULL;
    }

    return pi->name;
}
