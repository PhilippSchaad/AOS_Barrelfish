#ifndef LIB_PROCMAN_H
#define LIB_PROCMAN_H

#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <spawn/spawn.h>

#include <lib_urpc.h>

/// Struct to keep track of a process.
struct process_info {
    domainid_t id;
    coreid_t core;
    char *name;
    struct spawninfo *si;
    struct process_info *next;
    struct process_info *prev;
    struct lmp_chan *chan;
    bool init;
};

/// Struct to keep track of all processes.
struct process_table {
    struct process_info *head;
};

struct process_table *pt;

errval_t procman_init(void);
domainid_t procman_register_process(char *name, coreid_t core_id,
                                    struct spawninfo *si);
domainid_t procman_finish_process_register(char *name, struct lmp_chan *chan);
errval_t procman_deregister(domainid_t proc_id);
void procman_foreign_preregister(domainid_t pid, char *name, coreid_t core_id,
                                 struct spawninfo *si);
void procman_print_proc_list(void);
errval_t procman_kill_process(domainid_t proc_id);
char *procman_lookup_name_by_id(domainid_t proc_id);
struct lmp_chan *procman_get_channel_by_id(domainid_t proc_id);

#endif /* LIB_PROCMAN_H */
