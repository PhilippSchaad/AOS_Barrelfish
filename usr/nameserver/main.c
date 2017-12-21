
#include <aos/aos.h>
#include <nameserver.h>
#include <aos/nameserver_internal.h>
#include <aos/aos_rpc.h>
#include <aos/aos_rpc_shared.h>

#include <stdio.h>
#include <stdlib.h>

/*#undef DEBUG_LEVEL
#define DEBUG_LEVEL 100
*/
//We have a series of services that we want the nameserver to handle...
//base system services:
//      memserv
//      procman
//      serial manager
//      argueably, init.
//      we also have the proxy service which can send messages from one core to another and to any particular process
//      thus we want to register all of these with the nameserver (even though they are all currently running as part of the init process itself)
//          -> if I find the time I might split off memserv and procman from init just to have something nicer to demonstrate (also more modular approach is in the spirit of this anyway)
//      we could also be cute and say that the nameserver itself counts.
//
//      also since our system is mostly partitioned across two cores and has very very limited crosscore capabilities still (and none in regards to actual sending caps across cores)
//          and our RPC and URPC systems are sadly not unified in a way that would allow seamless crosscore transmitting
//          and most importantly, memserv is necessarily core specific due to the cap sending limitation
//          we will just run two instances of the nameserver
//              -> If the time is found, they can be made to talk with each other to synchronize their state for those things that should be available core-independently
//                  For this they can make use of the proxy service. Possibly extending it into a full on proper proxy which provides proxied channels across cores.

struct doubly_linked_list_of_nsi {
    struct nameserver_info entry;
    struct doubly_linked_list_of_nsi *next;
    struct doubly_linked_list_of_nsi *prev;
};

struct linked_list_of_nsi {
    struct nameserver_info *entry;
    struct linked_list_of_nsi *next;
};

struct process {
    struct lmp_chan *connection_chan; //used as ID
    struct linked_list_of_nsi* services;
    struct process *next;
};

struct nameserver {
    struct doubly_linked_list_of_nsi *services;
    struct process *processes;
};

struct nameserver ns;

__unused
static void dump_ns(void) {
    struct process *p = ns.processes;
    debug_printf("===== DUMPING NAMESERVER CONTENT =====\n");
    unsigned int i = 0;
    while(p != NULL) {
        debug_printf("    process arbitrarily labelled %u\n",i);
        struct linked_list_of_nsi *llnsi = p->services;
        while(llnsi != NULL) {
            char *ser = serialize_nameserver_info(llnsi->entry);
            debug_printf("        '%s'\n",&ser[8]);
            free(ser);
            llnsi = llnsi->next;
        }
        p = p->next;
        i++;
    }
    debug_printf("===== END OF NAMESERVER CONTENT =====\n");
}
__unused
static bool cap_compare(struct capref *c1, struct capref *c2) {
    if(c1 == NULL || c2 == NULL)
        return c1 == c2;
    if(c1==c2)
        return true;
    if(c1->slot != c2->slot)
        return false;
    if(c1->cnode.cnode != c2->cnode.cnode)
        return false;
    if(c1->cnode.croot != c2->cnode.croot)
        return false;
    if(c1->cnode.level != c2->cnode.level)
        return false;
    return true;
}


static bool validate_entry(struct nameserver_info* entry) {
    if(entry == NULL)
        return false;
    if(entry->name == NULL)
        return false;
    if(entry->type == NULL)
        return false;
//    if(cap_compare(&entry->chan_cap,&NULL_CAP))
//        return false;
    return true;
}

static bool eval_query(struct nameserver_query *nsq, struct nameserver_info *nsi) {
    DBG(VERBOSE,"tag is: %u",(unsigned int) nsq->tag);
    switch(nsq->tag) {
        case nsq_name:
            DBG(VERBOSE,"Current comparison: %s == %s\n",nsi->name,nsq->name);
            return strcmp(nsq->name,nsi->name) == 0;
        case nsq_type:
            DBG(VERBOSE,"Current comparison: %s == %s\n",nsi->type,nsq->type);
            return strcmp(nsq->type,nsi->type) == 0;
    }
    DBG(ERR,"The impossible occured. Beware the nasal demons\n");
    return false;
}

__unused
static void kill_dead_services(void) {
    struct process *prev = NULL;
    struct process *cur = ns.processes;
    while(cur != NULL) {
        if (cur->connection_chan->connstate == LMP_DISCONNECTED) {
            debug_printf("Dead process found, removing!\n");
            struct linked_list_of_nsi *prev2 = NULL;
            struct linked_list_of_nsi *cur2 = cur->services;
            while (cur2 != NULL) {
                free_nameserver_info(cur2->entry);
                prev2 = cur2;
                cur2 = cur2->next;
                free(prev2);
            }
            struct process *temp = cur;
            if(prev != NULL) {
                prev->next = cur->next;
                cur = cur->next;
            }else{
                ns.processes = cur->next;
                cur = cur->next;
            }
            free(temp);
        }else{
            prev = cur;
            cur = cur->next;
        }
    }
}


static errval_t find_service(struct lmp_chan *requester_chan, struct nameserver_query* query, bool remove, struct nameserver_info **result) {
    //todo: rich query language, for now name or tag only
    struct process *prev = NULL;
    struct process *cur = ns.processes;
    while(cur != NULL) {
        struct linked_list_of_nsi* prev2 = NULL;
        struct linked_list_of_nsi* cur2 = cur->services;
        while(cur2 != NULL) {
            //todo:restructure such that remove skips other processes immediately
            if(eval_query(query,cur2->entry)) {
                *result = cur2->entry;
                if(remove) {
                    if(cur->connection_chan != requester_chan) {
                        DBG(WARN, "tried to delete someone elses service\n");
                        return 315; //Todo: turn into proper error message that one is trying to delete something one's not allowed to
                    }
                    if(prev2 != NULL) {
                        prev2->next = cur2->next;
                        free(cur2);
                    }else{
                        cur->services = cur->services->next;
                        free(cur2);
                        if(cur->services == NULL) {
                            if(prev != NULL) {
                                prev->next = cur->next;
                                free(cur);
                            }else{
                                ns.processes = ns.processes->next;
                                free(cur);
                            }
                        }
                    }
                }
                DBG(VERBOSE,"found process with name %s\n",(*result)->name);
                return SYS_ERR_OK;
            }
            prev2 = cur2;
            cur2 = cur2->next;
        }
        prev = cur;
        cur = cur->next;
    }
    *result = NULL;
    DBG(VERBOSE,"could not find service\n");
    return 314; //todo: real error msg that we did not find result
    //begin: check to prevent double registration

}

static errval_t add_service(struct lmp_chan *requester_chan, struct nameserver_info *entry) {
    struct process *prev = NULL;
    struct process *cur = ns.processes;
    //begin: check to prevent double registration
    struct linked_list_of_nsi* cur2 = cur->services;
    while(cur != NULL) {
        while (cur2 != NULL) {
            if (strcmp(cur2->entry->name, entry->name) == 0) {
                DBG(WARN, "entry already exists\n");
                return 314; //todo: replace with proper error msg
            }
            cur2 = cur2->next;
        }
        cur = cur->next;
    }
    //end:  check to prevent double registration
    cur = ns.processes;
    while(cur != NULL) {
        if(cur->connection_chan == requester_chan)
            break;
        prev = cur;
        cur = cur->next;
    }
    if(cur == NULL)
    {
        cur = malloc(sizeof(struct process));
        if(prev != NULL)
            cur->next = ns.processes;
        ns.processes = cur;
        cur->connection_chan = requester_chan;
        cur->services = NULL;
    }

    cur2 = malloc(sizeof(struct linked_list_of_nsi));
    cur2->entry = entry;
    cur2->next = cur->services;
    cur->services = cur2;
    DBG(VERBOSE,"Successfully added %s\n",entry->name);
    return SYS_ERR_OK;
}

//return true if successful, false otherwise
static bool ns_remove_self_handler(struct lmp_chan *requester_chan) {
    DBG(VERBOSE,"process wants to remove itself\n");
    struct process *prev = NULL;
    struct process *cur = ns.processes;
    while(cur != NULL) {
        if(cur->connection_chan != requester_chan)
            break;
        prev = cur;
        cur = cur->next;
    }
    if(cur != NULL) {
        struct linked_list_of_nsi* cur2 = cur->services;
        struct linked_list_of_nsi* next2;
        while(cur2 != NULL) {
            next2 = cur2->next;
            free_nameserver_info(cur2->entry);
            free(cur2);
            cur2 = next2;
        }
        if(prev != NULL)
            prev->next = cur->next;
        else
            ns.processes = cur->next;
        free(cur);
        return 1;
    }
    return 0;
}

static void ns_register_service_handler(struct recv_list *data) {
    struct nameserver_info *ns_info;
    DBG(DETAILED,"adding service...\n");
    DBG(VERBOSE,"received: %s\n",(char*)&data->payload[2]);
    CHECK(deserialize_nameserver_info((char*)data->payload,&ns_info));
    DBG(VERBOSE,"deserialized!\n");
    ns_info->chan_cap = data->cap;
    if(!validate_entry(ns_info)) {
        uint32_t res = -1;
        send_response(data,data->chan,NULL_CAP,1,&res);
        return;
    }
    DBG(VERBOSE,"validated!\n");
    //todo: real error handling
    errval_t err = add_service(data->chan,ns_info);
    if(err != 314)
        DBG(VERBOSE,"added!\n");
    send_response(data,data->chan,NULL_CAP,0,NULL);
}

static void ns_deregister_handler(struct recv_list *data) {
    struct nameserver_query nsq;
    nsq.tag = nsq_name;
    nsq.name = (char*)data->payload;
    struct nameserver_info *nsi;
    find_service(data->chan,&nsq,true,&nsi);
    uintptr_t k;
    if(nsi != NULL) {
        DBG(VERBOSE,"sending response with success\n");
        k = 1;
    }else{
        DBG(VERBOSE,"sending response without success\n");
        k = 0;
    }
    send_response(data, data->chan, nsi->chan_cap,1,&k);
}

static void ns_lookup_handler(struct recv_list *data) {
    struct nameserver_query *nsq;
    DBG(VERBOSE,"received: %s\n",(char*)data->payload);
    CHECK(deserialize_nameserver_query((char*)data->payload,&nsq));
    struct nameserver_info *nsi;
    find_service(data->chan,nsq,false,&nsi);
    if(nsi != NULL) {
        char *ser = serialize_nameserver_info(nsi);
        uintptr_t *out;
        size_t outsize;
        DBG(VERBOSE,"sending response with success\n");
        convert_charptr_to_uintptr_with_padding_and_copy(ser,strlen(&ser[8])+9,&out,&outsize);
        DBG(VERBOSE,"size %u, msg '%s'\n",outsize,(char*)(&out[2]));
        free(ser);
        send_response(data, data->chan, nsi->chan_cap,outsize,out);
        DBG(VERBOSE,"done\n");
        free(out);
    }else{
        DBG(VERBOSE,"sending response without success\n");
        send_response(data, data->chan, NULL_CAP,0,NULL);
    }
}

static void ns_lookup_enumerate(struct recv_list *data) {
    struct nameserver_query *nsq;
    DBG(VERBOSE,"received: %s\n",(char*)data->payload);
    CHECK(deserialize_nameserver_query((char*)data->payload,&nsq));
    struct process *cur = ns.processes;
    int enum_count = 0;
    int strlength = 0;
    while(cur != NULL) {
        struct linked_list_of_nsi *cur2 = cur->services;
        while (cur2 != NULL) {
            if (eval_query(nsq, cur2->entry)) {
                enum_count++;
                strlength += strlen(cur2->entry->name);
            }
            cur2 = cur2->next;
        }
        cur = cur->next;
    }
    if(enum_count == 0) {
        send_response(data,data->chan,NULL_CAP,0,NULL);
        return;
    }
    int k = 0;
    DBG(VERBOSE,"strlength %d, enum_count %d\n",strlength,enum_count);
    char* str = malloc(strlength+enum_count);
    char* curstr = str;
    cur = ns.processes;
    while(cur != NULL) {
        struct linked_list_of_nsi *cur2 = cur->services;
        while (cur2 != NULL) {
            if (eval_query(nsq, cur2->entry)) {
                strcpy(curstr,cur2->entry->name);
                curstr += strlen(cur2->entry->name);
                k++;
                if(k < enum_count) {
                    *curstr = ',';
                    curstr++;
                }else
                    *curstr = '\0';
            }
            cur2 = cur2->next;
        }
        cur = cur->next;
    }
    uintptr_t *out;
    size_t outsize;
    convert_charptr_to_uintptr_with_padding_and_copy(str,strlength+enum_count,&out,&outsize);
    DBG(VERBOSE,"size %u, msg '%s'\n",outsize,(char*)(out));
    free(str);
    send_response(data, data->chan, NULL_CAP,outsize,out);
    DBG(VERBOSE,"done\n");
    free(out);
}

static void ns_active_chan_handler(struct recv_list *data) {
    DBG(VERBOSE,"post handshake msg\n");
    //kill_dead_services();
    //dump_ns();
    bool res;
    switch(data->type) {
        case RPC_MESSAGE(NS_RPC_TYPE_REMOVE_SELF):
            res = !ns_remove_self_handler(data->chan);
            send_response(data,data->chan,NULL_CAP,1,&res);
            break;
        case RPC_MESSAGE(NS_RPC_TYPE_REGISTER_SERVICE):
            ns_register_service_handler(data);
            break;
        case RPC_MESSAGE(NS_RPC_TYPE_DEREGISTER_SERVICE):
            ns_deregister_handler(data);
            break;
        case RPC_MESSAGE(NS_RPC_TYPE_LOOKUP):
            ns_lookup_handler(data);
            break;
        case RPC_MESSAGE(NS_RPC_TYPE_ENUMERATE):
            ns_lookup_enumerate(data);
            break;
        case RPC_MESSAGE(NS_RPC_TYPE_ENUMERATE_COMPLEX):
            break;
        case RPC_MESSAGE(NS_RPC_TYPE_DEBUG_DUMP):
            dump_ns();
            send_response(data,data->chan,NULL_CAP,0,NULL);
            break;
        default:
            break;
    }
    //dump_ns();
}

static errval_t ns_recv_handshake(struct capref *recv_cap) {
    DBG(DETAILED, "ns received cap\n");

        struct recv_chan *rc = malloc(sizeof(struct recv_chan));
        rc->chan = malloc(sizeof(struct lmp_chan));
        rc->recv_deal_with_msg = ns_active_chan_handler;
        rc->rpc_recv_list = NULL;
        CHECK(lmp_chan_accept(rc->chan, DEFAULT_LMP_BUF_WORDS, *recv_cap));
        DBG(DETAILED, "Created new channel\n");
        // We register recv on new channel.
        lmp_chan_alloc_recv_slot(rc->chan);
        CHECK(lmp_chan_register_recv(rc->chan, get_default_waitset(),
                                     MKCLOSURE(recv_handling, rc)));
        DBG(DETAILED, "Set up receive handler for channel\n");

    // Send ACK to the child including new cap to bind to
    send(rc->chan, rc->chan->local_cap, RPC_ACK_MESSAGE(NS_RPC_TYPE_HANDSHAKE),
         0, NULL, NULL_EVENT_CLOSURE,0);

    DBG(DETAILED, "successfully received cap\n");
    return SYS_ERR_OK;

}

static void ns_recv_handshake_handler(struct recv_list *data)
{
    // Check the message type and handle it accordingly.
    DBG(VERBOSE, "recv handshake...\n");
    switch (data->type) {
        case RPC_MESSAGE(NS_RPC_TYPE_HANDSHAKE):
            ns_recv_handshake(&data->cap);
            break;
        default:
            DBG(ERR, "Received non-handshake Nameserver RPC with handshake handler. This "
                    "means the sender is sending wrong!\n");
            return;
    }
}

int main(int argc, char *argv[])
{
    debug_printf("Nameserver online\n");
    //bootstrapping:
    //We have a channel with init, now the next thing we need to do is tell init that we are the nameserver and how to reach us, so it can distribute that to everyone else on their request
    //afterwards we spin to win
    struct lmp_chan chan_with_init;
    init_rpc_server(ns_recv_handshake_handler,&chan_with_init);
    aos_rpc_register_as_nameserver(get_init_rpc(),chan_with_init.local_cap);

    debug_printf("Nameserver registered\n");

    // Hang around
    struct waitset *default_ws = get_default_waitset();
    errval_t err;
    while (true) {
        err = event_dispatch(default_ws);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }
}

