#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <aos/waitset.h>
#include <aos/aos_rpc_shared.h>

#include <lib_rpc.h>
#include <lib_urpc.h>

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

static errval_t ram_recv_handler(struct recv_list *data, struct lmp_chan *chan)
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

    if (chan == NULL) { // HACK: We are in URPC
        // TODO: figure out if we want to deal with this?
        USER_PANIC("todo");
        return -1;
    }

    CHECK(send_response(data, chan, ram_cap, 1, &size));

    return SYS_ERR_OK;
}

static errval_t spawn_recv_handler(struct recv_list *data,
                                   struct lmp_chan *chan)
{
    DBG(VERBOSE, "Spawn recv handler\n");

    char *recv_name = (char *) data->payload;

    // Separate core and name.
    // Find the last occurence of the "_" char.
    int last_occurence;
    size_t length = strlen(recv_name);
    for (last_occurence = length - 1; last_occurence >= 0; --last_occurence) {
        if (recv_name[last_occurence] == '_') {
            // Found it.
            break;
        }
    }

    coreid_t core = atoi(recv_name + last_occurence + 1);
    recv_name[last_occurence] = '\0';
    // TODO: Consider that this could be made much faster by doing data->size
    // * 4 - padding
    length = last_occurence;

    char *name = malloc(length + 1);
    strcpy(name, recv_name);
    name[length] = '\0';

    DBG(DETAILED, "receive spawn request: name: %s, core %d\n", recv_name,
        core);
    DBG(DETAILED, "spawninfo: %s size %u\n", (char *) data->payload,
        data->size);

    // Check if we are on the right core, else send cross core request.
    if (disp_get_core_id() != core) {
        // TODO: Please just transfer the number as a number instead of this
        // string hacking stuff
        recv_name[last_occurence] = '_';

        // Have to send cross core request.
        DBG(DETAILED, "spawn %s on other core\n", name);
        DBG(DETAILED, "spawninfo: %s size %u\n", (char *) data->payload,
            data->size);

        // If we are on core 0 we can (and have to) preset the pid.
        domainid_t pid = 0;
        if (disp_get_core_id() == 0) {
            pid = procman_register_process(name, core, NULL);
        }

        void *sto_data = data->payload;

        size_t bytesize = strlen(recv_name) * 4 + 4;
        void *newpayload = malloc(bytesize);
        memcpy(newpayload, data->payload, bytesize - 4);
        *((uint32_t *) (newpayload + strlen(recv_name) + 1)) = pid;
        data->payload = newpayload;
        data->size = bytesize;

        // We don't actually need any response, we just want to know the
        // response exists.
        free_urpc_allocated_ack_recv_list(urpc2_rpc_over_urpc(data, NULL_CAP));

        free(newpayload);
        data->payload = sto_data;

        // TODO: we need to get the ID of the created process.
        // we should create a urpc call for that (or create a response for the
        // spawn call)
        send_response(data, chan, NULL_CAP, 1,
                      (unsigned int *) 42 /* TODO: changeme */);
        return SYS_ERR_OK;
    }

    struct spawninfo *si =
        (struct spawninfo *) malloc(sizeof(struct spawninfo));
    errval_t err;
    err = spawn_load_by_name(name, si);

    // Preregister the process we just spawned.
    // This will create the process but the process does not know its PID yet
    // and we don't have a rpc channel.
    domainid_t pid;
    if (chan == NULL && disp_get_core_id() != 0) { // XXX HACK: We are in URPC
        pid = *((domainid_t *) (recv_name + strlen(recv_name) + 3));
        procman_foreign_preregister(pid, name, core, si);
    } else {
        pid = procman_register_process(name, core, si);
    }

    // This is done at a separate call, because some day we would want to split
    // the name server from main.

    if (chan == NULL) { // XXX HACK: We are in URPC
        urpc2_send_response(data, NULL_CAP, 1, &pid);
        return SYS_ERR_OK;
    } else {
        send_response(data, chan, NULL_CAP, 1, &pid);
    }
    return SYS_ERR_OK;
}

static errval_t process_get_name_recv_handler(struct recv_list *data,
                                              struct lmp_chan *chan)
{
    int id = (int) data->payload[0];
    char *name = procman_lookup_name_by_id(id);

    size_t tempsize = strlen(name);
    uintptr_t *payload2;
    size_t payloadsize2;
    convert_charptr_to_uintptr_with_padding_and_copy(name, tempsize, &payload2,
                                                     &payloadsize2);
    if (chan == NULL) { // XXX HACK: We are in URPC
        urpc2_send_response(data, NULL_CAP, payloadsize2 * 4, payload2);
    } else
        send_response(data, chan, NULL_CAP, payloadsize2, payload2);
    free(payload2);
    return SYS_ERR_OK;
}

static void getchar_recv_handler(struct recv_list *data, struct lmp_chan *chan)
{
    char retchar;
    sys_getchar(&retchar);

    send_response(data, chan, NULL_CAP, 1, (void *) &retchar);
}

static void irq_cap_recv_handler(struct recv_list *data, struct lmp_chan *chan)
{
    struct capref irq_cap;

    CHECK(slot_alloc(&irq_cap));
    CHECK(cap_copy(irq_cap, cap_irq));

    send_response(data, chan, irq_cap, 0, NULL);
}

static void process_register_recv_handler(struct recv_list *data,
                                          struct lmp_chan *chan)
{
    DBG(DETAILED, "Process is registering itself\n");

    // Grab the process name.
    char *proc_name = malloc(sizeof(char) * 4 * data->size);
    strcpy(proc_name, (char *) data->payload);

    coreid_t core_id = disp_get_core_id();
    domainid_t proc_id = procman_finish_process_register(proc_name, chan);

    // Send back core id and pid.
    uint32_t *combinedArg = malloc(8);

    combinedArg[0] = proc_id;
    combinedArg[1] = core_id;

    // Store name and pid in domain.
    struct domain *domain = find_domain(&data->cap);
    assert(domain);
    domain->name = proc_name;
    domain->id = proc_id;

    DBG(DETAILED, "process_register_recv_handler: respond with "
                  "core %d pid %d\n",
        combinedArg[1], combinedArg[0]);

    if (chan == NULL) { // XXX HACK: We are in URPC
        urpc2_send_response(data, NULL_CAP, 2 * 4, (void *) combinedArg);
    } else
        send_response(data, chan, NULL_CAP, 2, (void *) combinedArg);
}

void recv_deal_with_msg(struct recv_list *data)
{
    // Check the message type and handle it accordingly.
    DBG(DETAILED, "recv msg...\n");
    struct lmp_chan *chan = data->chan;
    uint32_t success;
    errval_t err;
    switch (data->type) {
    case RPC_MESSAGE(RPC_TYPE_NUMBER):
        if (chan == NULL) { // XXX HACK: We are in URPC
            printf("Received number %u via URPC\n", data->payload[0]);
            urpc2_send_response(data, NULL_CAP, 0, NULL);
            break;
        }
        printf("Received number %u via RPC\n", data->payload[0]);
        send_response(data, chan, NULL_CAP, 0, NULL);
        break;
    case RPC_MESSAGE(RPC_TYPE_STRING):
        if (chan == NULL) { // XXX HACK: We are in URPC
            printf((char *) data->payload);
            fflush(stdout);
            urpc2_send_response(data, NULL_CAP, 0, NULL);
            break;
        }
        printf((char *) data->payload);
        fflush(stdout);
        send_response(data, chan, NULL_CAP, 0, NULL);
        break;
    case RPC_MESSAGE(RPC_TYPE_STRING_DATA):
        debug_printf("RPC_TYPE_STRING_DATA is deprecated\n");
        if (chan == NULL) { // XXX HACK: We are in URPC
            urpc2_send_response(data, NULL_CAP, 0, NULL);
            break;
        }
        send_response(data, chan, NULL_CAP, 0, NULL);
        break;
    case RPC_MESSAGE(RPC_TYPE_RAM):
        assert(chan != NULL &&
               "we can not handle cap transfer across cores via urpc yet");
        CHECK(ram_recv_handler(data, chan));
        break;
    case RPC_MESSAGE(RPC_TYPE_PUTCHAR):
        if (chan == NULL) { // XXX HACK: We are in URPC
            DBG(DETAILED, "putchar request received via URPC\n");
        } else
            DBG(DETAILED, "putchar request received\n");
        sys_print((char *) data->payload, 1);
        if (chan == NULL) { // XXX HACK: We are in URPC
            urpc2_send_response(data, NULL_CAP, 0, NULL);
            break;
        }
        send_response(data, chan, NULL_CAP, 0, NULL);
        break;
    case RPC_MESSAGE(RPC_TYPE_GETCHAR):
        getchar_recv_handler(data, chan);
        break;
    case RPC_MESSAGE(RPC_TYPE_IRQ_CAP):
        irq_cap_recv_handler(data, chan);
        break;
    case RPC_MESSAGE(RPC_TYPE_HANDSHAKE):
        DBG(ERR, "Non handshake handler got handshake RPC. This should never "
                 "happen\n");
        break;
    case RPC_MESSAGE(RPC_TYPE_PROCESS_GET_NAME):
        CHECK(process_get_name_recv_handler(data, chan));
        break;
    case RPC_MESSAGE(RPC_TYPE_PROCESS_SPAWN):
        CHECK(spawn_recv_handler(data, chan));
        break;
    case RPC_MESSAGE(RPC_TYPE_PROCESS_KILL):
        // TODO: Check if we are allowed to kill this process...
        err = procman_kill_process(*((uint32_t *) data->payload));
        if (err == PROCMAN_ERR_PROCESS_ON_OTHER_CORE) {
            // Forward to other core.
            urpc2_rpc_over_urpc(data, NULL_CAP);
            success = *((uint32_t *) data->payload);
        } else {
            success = err_is_ok(err);
        }
        send_response(data, chan, NULL_CAP, 1, &success);
        break;
    case RPC_MESSAGE(RPC_TYPE_PROCESS_KILL_ME):
        // TODO: Should we destroy the dispatcher at all? the child can do that
        // itself...
        DBG(DETAILED, "kill_me_recv_handler\n");
        assert((chan != NULL || disp_get_core_id() == 0) &&
               "why is it possible to ask another core than your own to kill "
               "you?");
        if (chan == NULL) { // XXX HACK: We are in URPC
            DBG(DETAILED, "I remove pid %d\n",
                *((domainid_t *) data->payload));
            procman_deregister(*((domainid_t *) data->payload));
        } else {
            struct domain *domain = find_domain(&data->cap);
            DBG(DETAILED, "I remove %s pid %d\n", domain->name, domain->id);
            procman_deregister(domain->id);

            // If we are on core 1, we have to inform core 0 that the process
            // was killed.
            if (disp_get_core_id() != 0) {
                data->payload = (void *) &(domain->id);
                data->size = 4;
                urpc2_rpc_over_urpc(data, NULL_CAP);
            }
        }
        break;
    case RPC_MESSAGE(RPC_TYPE_PROCESS_REGISTER):
        process_register_recv_handler(data, chan);
        break;
    case RPC_MESSAGE(RPC_TYPE_PROCESS_GET_PIDS):
        DBG(ERR, "No get PIDs handler implemented yet\n");
    default:
        DBG(WARN, "Unable to handle RPC-receipt, expect badness! type: %u\n",
            (unsigned int) data->type);
        if (chan == NULL) // XXX HACK: We are in URPC
            DBG(WARN, "The RPC-receit that was unable to be handled was "
                      "actually a URPC thing!\n");
        return;
    }
}
static errval_t handshake_recv_handler(struct capref *child_cap)
{
    DBG(DETAILED, "init received cap\n");

    // Check if the domain/channel already exists.
    // If so, no need to create one.
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
        // We register recv on new channel.
        struct recv_chan *rc = malloc(sizeof(struct recv_chan));
        rc->chan = &dom->chan;
        rc->recv_deal_with_msg = recv_deal_with_msg;
        rc->rpc_recv_list = NULL;
        bool use_prealloc_slot_buff = false;
        lmp_chan_alloc_recv_slot(rc->chan, use_prealloc_slot_buff);

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
        handshake_recv_handler(&data->cap);
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
    // Create channel to receive child eps.
    init_rpc_server(recv_handshake_handler, &init_chan);
}
