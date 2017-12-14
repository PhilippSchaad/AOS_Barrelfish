#ifndef LIB_URPC_H
#define LIB_URPC_H

#include <lib_procman.h>
#include <lib_rpc.h>
#include <lib_urpc2.h>

enum urpc_type {
    send_string,
    term_send_char,
    remote_spawn,
    init_mem_alloc,
    register_process,
    rpc_over_urpc,
    rpc_perf_measurement
};

// TODO: Maybe we can add the receiver ID to this, if we want to support faster
// (and maybe easier) multiplexing of the receiver (when using with regular
// rpcs).
struct event_closure_with_arg {
    void (*func)(void *savedarg, void *newarg);
    void *savedarg;
};

inline struct event_closure_with_arg
init_event_closure_with_arg(void (*func)(void *, void *), void *savedarg)
{
    struct event_closure_with_arg ret;
    ret.func = func;
    ret.savedarg = savedarg;
    return ret;
}

void urpc_master_init_and_run(void *urpc_vaddr);
void urpc_slave_init_and_run(void);

// Does not delete the string. TODO: version which does?
void urpc_sendstring(char *str);
void urpc_term_sendchar(char *c);
void urpc_spawn_process(char *name);
void urpc_init_mem_alloc(struct bootinfo *p_bi);
bool urpc_ram_is_initalized(void);
// Returns a urpc2_data with the data field owned by this instance and needing
// to be freed.
struct urpc2_data urpc2_send_and_receive(struct urpc2_data (*func)(void *data),
                                         void *payload, char type);

void urpc2_send_response(struct recv_list *rl, struct capref cap,
                         size_t payloadsize, void *payload);

// The recv_list owns data alloced at -2 relative to the where the ->data thing
// points. Needs to be freed.
struct recv_list urpc2_rpc_over_urpc(struct recv_list *rl, struct capref cap);

inline void free_urpc_allocated_ack_recv_list(struct recv_list k)
{
    free(((char *) k.payload) - 2);
}

void urpc_perf_measurement(void *payload);

domainid_t urpc_register_process(char *str);

#endif /* LIB_URPC_H */
