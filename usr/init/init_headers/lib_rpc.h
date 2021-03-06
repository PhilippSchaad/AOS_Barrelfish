#ifndef LIB_RPC_H
#define LIB_RPC_H

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/waitset.h>

#include <spawn/spawn.h>

#include <mem_alloc.h>
#include <lib_procman.h>
#include <lib_urpc.h>

struct domain {
    struct lmp_chan chan;
    struct capability identification_cap;
    int id;
    char *name;
    struct domain *next;
};

struct domain_list {
    struct domain *head;
};

struct domain_list *active_domains;

void recv_deal_with_msg(struct recv_list *data);
void init_rpc(void);
bool nameserver_online(void);

#endif /* LIB_RPC_H */
