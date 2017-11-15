#ifndef LIB_PROCMAN_H
#define LIB_PROCMAN_H

#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <spawn/spawn.h>

/// Struct to keep track of a process.
struct process_info {
    domainid_t id;
    coreid_t core;
    char *name;
    struct spawninfo si;
    struct process_info *next;
    struct process_info *prev;
};

/// Struct to keep track of all processes.
struct process_table {
    struct process_info *head;
};

struct process_table *pt;

errval_t procman_init(void);
domainid_t procman_register_process(char *name, struct spawninfo *si,
                                    coreid_t core_id);
errval_t procman_deregister(domainid_t proc_id);
void procman_print_proc_list(void);
errval_t procman_kill_process(domainid_t proc_id);
char *procman_lookup_name_by_id(domainid_t proc_id);

#endif /* LIB_PROCMAN_H */
