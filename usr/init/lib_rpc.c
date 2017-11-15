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


static int pid;

struct adhoc_process_table {
    int id;
    char *name;
    struct spawninfo *si;
    struct adhoc_process_table *next;
};

static struct adhoc_process_table *adhoc_process_table = NULL;

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
    size_t length = strlen(recv_name); // TODO: consider that this could be
                                       // made much faster by doing data->size
                                       // * 4 - padding
    char *name = malloc(length + 1);
    strcpy(name, recv_name);
    name[length] = '\0';
    struct spawninfo *si =
        (struct spawninfo *) malloc(sizeof(struct spawninfo));
    errval_t err;
    err = spawn_load_by_name(name, si);
    size_t ret_id = si->paging_state.debug_paging_state_index;
    struct adhoc_process_table *apt = (struct adhoc_process_table *) malloc(
        sizeof(struct adhoc_process_table));

    apt->id = ret_id;
    apt->name = name;
    apt->si = si;
    apt->next = adhoc_process_table;
    adhoc_process_table = apt;
    debug_printf("Spawned process %s with id %u\n", name, ret_id);
    send_response(data, chan, NULL_CAP, 1, (unsigned int *) &apt->id);

    return SYS_ERR_OK;
}

static errval_t new_process_get_name_recv_handler(struct recv_list *data,
                                                  struct lmp_chan *chan)
{
    int id = (int) data->payload[0];
    struct adhoc_process_table *apt = adhoc_process_table;
    char *name;
    bool found = false;
    while (apt) {
        if (apt->id == id) {
            name = apt->name;
            found = true;
            break;
        }
        apt = apt->next;
    }
    if (!found)
        name = "No Such Id, Sorry.";
    debug_printf("found process with name %s for id %d\n", name, id);
    send_response(data, chan, NULL_CAP, strlen(name) / 4, name);
    return SYS_ERR_OK;
}

static void recv_deal_with_msg(struct recv_list *data)
{
    // Check the message type and handle it accordingly.
    DBG(VERBOSE, "recv msg...\n");
    struct lmp_chan *chan = data->chan;
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
    case RPC_MESSAGE(RPC_TYPE_PROCESS_KILL_ME):
        // TODO: Does this have to remove something in the adhoc_process_table?..
        DBG(DETAILED, "kill_me_recv_handler\n");
        errval_t err = cap_delete(data->cap);
        if (err_is_fail(err)) {
            DBG(WARN, "We failed to delete the dispatcher in init..\n");
        }
        send_response(data, chan, NULL_CAP, 0, NULL);
        break;
    case RPC_MESSAGE(RPC_TYPE_PROCESS_GET_PIDS):
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
        dom->id = pid++;
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
    debug_printf("recv handshake...\n");
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
