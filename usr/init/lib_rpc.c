#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <lib_rpc.h>
#include <aos/waitset.h>
#include <aos/aos_rpc_shared.h>

/// Try to find the correct domain identified by cap.
static struct domain *find_domain(struct capref *cap)
{
    struct capability identification_cap;
    CHECK(debug_cap_identify(*cap, &identification_cap));

    assert(active_domains != NULL);
    struct domain *current = active_domains->head;
    while (current != NULL) {
        if (current->identification_cap.u.endpoint.epoffset ==
                identification_cap.u.endpoint.epoffset &&
            current->identification_cap.u.endpoint.listener ==
                identification_cap.u.endpoint.listener) {
            return current;
        }
        current = current->next;
    }

    DBG(DETAILED, "Domain not found\n");

    return NULL;
}

static errval_t new_ram_recv_handler(struct recv_list *data,
                                     struct lmp_chan *chan)
{
    DBG(VERBOSE, "ram request received\n");

    assert(data->size == 2);

    size_t size = (size_t) data->payload[0];
    size_t align = (size_t) data->payload[1];

    struct capref ram_cap;
    CHECK(aos_ram_alloc_aligned(&ram_cap, size, align));

    // Fix size based on BASE_PAGE_SIZE, the way we're retrieving it.
    if (size % BASE_PAGE_SIZE != 0) {
        size += (size_t) BASE_PAGE_SIZE - (size % BASE_PAGE_SIZE);
    }
    DBG(DETAILED, "We got ram with size %zu\n", size);

    CHECK(send_response(data, chan, ram_cap, 1, &size));

    return SYS_ERR_OK;
}

static errval_t new_spawn_recv_handler(struct recv_list *data,
                                       struct lmp_chan *chan)
{
    char *recv_name = (char *) data->payload;

    // separate core and name
    // find the last occurence of the "_" char
    int last_occurence;
    size_t length = strlen(recv_name);
    for (last_occurence=length-1; last_occurence>=0; --last_occurence){
        if(recv_name[last_occurence] == '_'){
            // found it
            break;
        }
    }

    coreid_t core = atoi(recv_name+last_occurence+1);
    recv_name[last_occurence] = '\0';
    length = last_occurence;  // TODO: consider that this could be
                                       // made much faster by doing data->size
                                       // * 4 - padding
    char *name = malloc(length + 1);
    strcpy(name, recv_name);
    name[length] = '\0';

    DBG(DETAILED, "receive spawn request: name: %s, core %d\n", recv_name, core);

    // check if we are on the right core, else send cross core request
    if (disp_get_core_id() != core){
        // have to send cross core request
        DBG(DETAILED, "spawn %s on other core\n", name);
        urpc_spawn_process(name);
        //free(name);

        //TODO: we need to get the ID of the created process.
        // we should create a urpc call for that (or create a response for the spawn call)
        send_response(data, chan, NULL_CAP, 1, (unsigned int *) 42 /*TODO: changeme */ );
        //send_response(data, chan, NULL_CAP, 0, NULL);
        return SYS_ERR_OK;
    }

    struct spawninfo *si =
        (struct spawninfo *) malloc(sizeof(struct spawninfo));
    errval_t err;
    err = spawn_load_by_name(name, si);

    // preregister the process we just spawned
    // This will create the process but the process does not know its PID yet and we don't have a rpc channel.
    domainid_t pid = procman_register_process(name, disp_get_core_id(), si);

    // this is done at a separate call, because some day we would want to split the name server from main.
    //domainid_t ret_id = procman_register_process(name, si, 0);
    //domainid_t ret_id = procman_register_process(name, disp_get_core_id());

    debug_printf("Spawned process %s\n");
    send_response(data, chan, NULL_CAP, 1, &pid);

    return SYS_ERR_OK;
}

static errval_t new_process_get_name_recv_handler(struct recv_list *data,
                                                  struct lmp_chan *chan)
{
    int id = (int) data->payload[0];
    char *name = procman_lookup_name_by_id(id);

    debug_printf("found process with name %s for id %d\n", name, id);
    size_t tempsize = strlen(name);
    uintptr_t *payload2;
    size_t payloadsize2;
    convert_charptr_to_uintptr_with_padding_and_copy(name, tempsize, &payload2,
                                                     &payloadsize2);
    send_response(data, chan, NULL_CAP, payloadsize2, payload2);
    free(payload2);
    return SYS_ERR_OK;
}

static void process_register_recv_handler(struct recv_list *data,
                                          struct lmp_chan *chan)
{
    DBG(DETAILED, "seems that someone wants to register himself\n");
    // Grab the process name
    char *proc_name = malloc(sizeof(char) * 4 * data->size);
    strcpy(proc_name, (char *) data->payload);

    coreid_t core_id = disp_get_core_id();
    domainid_t proc_id = procman_finish_process_register(proc_name, chan);

    // send back core id and pid
    uint32_t *combinedArg = malloc(8);

    combinedArg[0] = proc_id;
    combinedArg[1] = core_id;

    // store name and pid in domain
    struct domain *domain = find_domain(&data->cap);
    assert(domain);
    domain->name = proc_name;
    domain->id = proc_id;

    //cap_destroy(data->cap);

    DBG(-1, "process_register_recv_handler: respond with "
                  "core %d pid %d\n", combinedArg[1], combinedArg[0]);

    send_response(data, chan, NULL_CAP, 2, (void*) combinedArg);
}

static void recv_deal_with_msg(struct recv_list *data)
{
    // Check the message type and handle it accordingly.
    DBG(VERBOSE, "recv msg...\n");
    struct lmp_chan *chan = data->chan;
    uint32_t success;
    switch (data->type) {
    case RPC_MESSAGE(RPC_TYPE_NUMBER):
        printf("Received number %u via RPC\n", data->payload[0]);
        send_response(data, chan, NULL_CAP, 0, NULL);
        break;
    case RPC_MESSAGE(RPC_TYPE_STRING):
        printf("Terminal: %s\n", (char *) data->payload);
        send_response(data, chan, NULL_CAP, 0, NULL);
        break;
    case RPC_MESSAGE(RPC_TYPE_STRING_DATA):
        debug_printf("RPC_TYPE_STRING_DATA is deprecated\n");
        send_response(data, chan, NULL_CAP, 0, NULL);
        break;
    case RPC_MESSAGE(RPC_TYPE_RAM):
        CHECK(new_ram_recv_handler(data, chan));
        break;
    case RPC_MESSAGE(RPC_TYPE_PUTCHAR):
        DBG(DETAILED, "putchar request received\n");
        sys_print((char *) &data->payload[1], 1);
        send_response(data, chan, NULL_CAP, 0, NULL);
        break;
    case RPC_MESSAGE(RPC_TYPE_HANDSHAKE):
        DBG(ERR, "Non handshake handler got handshake RPC. This should never "
                 "happen\n");
        break;
    case RPC_MESSAGE(RPC_TYPE_PROCESS_GET_NAME):
        // debug_printf("RPC_TYPE_PROCESS_GET_NAME is missing\n");
        CHECK(new_process_get_name_recv_handler(data, chan));
        // CHECK(process_get_name_recv_handler(&cap, &msg));
        break;
    case RPC_MESSAGE(RPC_TYPE_PROCESS_SPAWN):
        CHECK(new_spawn_recv_handler(data, chan));
        break;
    case RPC_MESSAGE(RPC_TYPE_PROCESS_KILL):
        // TODO: check if we are allowed to kill this process...
        success = err_is_ok(procman_kill_process(*((uint32_t*) data->payload)));
        if(success != 0){
            debug_printf("delete successful\n");
        } else {
            debug_printf("delete not so successful\n");
        }
        send_response(data, chan, NULL_CAP, 1, &success);
        break;
    case RPC_MESSAGE(RPC_TYPE_PROCESS_KILL_ME):
        // TODO: Add a mechanism to kill a remote process
        // TODO: Find a way to get the PID which gets deleted and remove it
        //       from the procman
        // TODO: Does this have to remove something in the
        // adhoc_process_table?..
        // TODO: find a way to destroy dispatcher
        // Should we destroy the dispatcher at all? the child can do that itself...
        DBG(DETAILED, "kill_me_recv_handler\n");
        struct domain *domain = find_domain(&data->cap);
        DBG(-1, "I remove %s pid %d\n", domain->name, domain->id);
        procman_deregister(domain->id);
        //errval_t err = cap_delete(data->cap);
        //if (err_is_fail(err)) {
        //    DBG(WARN, "We failed to delete the dispatcher in init..\n");
        //}
        //free(domain);
        //send_response(data, chan, NULL_CAP, 0, NULL);
        break;
    case RPC_MESSAGE(RPC_TYPE_PROCESS_REGISTER):
        process_register_recv_handler(data,chan);
        break;
    case RPC_MESSAGE(RPC_TYPE_PROCESS_GET_PIDS):
        DBG(ERR, "No get PIDs handler implemented yet\n");
    default:
        DBG(WARN, "Unable to handle RPC-receipt, expect badness! type: %u\n",
            (unsigned int) data->type);
        return;
    }
}
static errval_t new_handshake_recv_handler(struct capref *child_cap)
{
    DBG(DETAILED, "init received cap\n");

    // Check if the domain/channel already exists. If so, no need to create one
    struct domain *dom = find_domain(child_cap);
    if (dom == NULL) {
        dom = (struct domain *) malloc(sizeof(struct domain));
        dom->next = active_domains->head;
        active_domains->head = dom;

        struct capability identification_cap;
        CHECK(debug_cap_identify(*child_cap, &identification_cap));
        dom->identification_cap = identification_cap;

        CHECK(lmp_chan_accept(&dom->chan, DEFAULT_LMP_BUF_WORDS, *child_cap));

        DBG(DETAILED, "Created new channel\n");
        // we register recv on new channel
        struct recv_chan *rc = malloc(sizeof(struct recv_chan));
        rc->chan = &dom->chan;
        rc->recv_deal_with_msg = recv_deal_with_msg;
        rc->rpc_recv_list = NULL;
        lmp_chan_alloc_recv_slot(rc->chan);

        CHECK(lmp_chan_register_recv(rc->chan, get_default_waitset(),
                                     MKCLOSURE(recv_handling, rc)));
        DBG(DETAILED, "Set up receive handler for channel\n");
    }

    // Send ACK to the child including new cap to bind to
    send(&dom->chan, dom->chan.local_cap, RPC_ACK_MESSAGE(RPC_TYPE_HANDSHAKE),
         0, NULL, NULL_EVENT_CLOSURE,
         request_fresh_id(RPC_ACK_MESSAGE(RPC_TYPE_HANDSHAKE)));

    DBG(DETAILED, "successfully received cap\n");
    return SYS_ERR_OK;
}

static void recv_handshake_handler(struct recv_list *data)
{
    // Check the message type and handle it accordingly.
    DBG(VERBOSE, "recv handshake...\n");
    switch (data->type) {
    case RPC_MESSAGE(RPC_TYPE_HANDSHAKE):
        new_handshake_recv_handler(&data->cap);
        break;
    default:
        DBG(ERR, "Received non-handshake RPC with handshake handler. This "
                 "means the sender is sending wrong!\n");
        return;
    }
}

struct lmp_chan init_chan;
void init_rpc(void)
{
    // create channel to receive child eps
    init_rpc_server(recv_handshake_handler, &init_chan);

    /*
    CHECK(lmp_chan_accept(&init_chan, DEFAULT_LMP_BUF_WORDS, NULL_CAP));
    CHECK(lmp_chan_alloc_recv_slot(&init_chan));
    CHECK(cap_copy(cap_initep, init_chan.local_cap));
    CHECK(lmp_chan_register_recv(
            &init_chan, get_default_waitset(),
            MKCLOSURE((void *) general_recv_handler, &init_chan)));
            */
}
