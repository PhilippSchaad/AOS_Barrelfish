#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <aos/paging.h>
#include <aos/aos_rpc_shared.h>

#include <mm/mm.h>
#include <spawn/spawn.h>

#include <barrelfish_kpi/asm_inlines_arch.h>
#include <barrelfish_kpi/init.h>

#include <lib_rpc.h>
#include <lib_urpc.h>
#include <lib_urpc2.h>
#include <lib_terminal.h>
#include <mem_alloc.h>

// TODO: We still need to port the proper ID system, so until now it uses this.
// Just rip it out of aos_rpc_shared
#define TODO_ID 0

// Look, the proper way to implement this is via interrupts (see inthandler.c
// and anything mentioning IPI or IRQ in the kernel code).
// But apparently we are supposed to just spawn a thread and spin spin spin on
// it so that is what we'll do for now.

// Copy from a kernel only header. TODO: Make nicer
#define INIT_BOOTINFO_VBASE ((lvaddr_t) 0x200000)
#define INIT_ARGS_VBASE (INIT_BOOTINFO_VBASE + BOOTINFO_SIZE)
#define INIT_DISPATCHER_VBASE (INIT_ARGS_VBASE + ARGS_SIZE)
#define MON_URPC_VBASE (INIT_DISPATCHER_VBASE + DISPATCHER_SIZE)

// XXX: WARNING! USE THIS WITH CAUTION. Preliminary returns will NOT return
// the lock, control flow with 'continue' and 'break' might work differently
// than expected inside of synchronized blocks, and nested synchronized blocks
// can cause massive trouble. Be aware of that when using synchronized here!
#define synchronized(_mut)                                                    \
    for (struct thread_mutex *_mutx = &_mut; _mutx != NULL; _mutx = NULL)     \
        for (thread_mutex_lock(&_mut); _mutx != NULL;                         \
             _mutx = NULL, thread_mutex_unlock(&_mut))

enum urpc_state { non_initalized = 0, needs_to_be_read, available };

struct urpc_bootinfo_package {
    struct bootinfo boot_info;
    struct mem_region regions[10];
    genpaddr_t mmstrings_base;
    gensize_t mmstrings_size;
};

struct urpc_send_string {
    char string[2000];
};

struct urpc_term_send_char {
    char c;
};

struct urpc_remote_spawn {
    char name[2000];
};

struct urpc {
    enum urpc_type type;
    union {
        struct urpc_send_string send_string;
        struct urpc_term_send_char send_char;
        struct urpc_remote_spawn remote_spawn;
        struct urpc_bootinfo_package urpc_bootinfo;
    } data;
};

bool already_received_memory = false;
bool urpc_ram_is_initalized(void) { return already_received_memory; }

// We define a very simple protocol.
struct urpc_protocol {
    char master_state;
    char slave_state;
    struct urpc master_data;
    struct urpc slave_data;
};

/// Performance measurement function.
static struct urpc2_data perf_func(void *data)
{
    int *str = (int *) data;
    debug_printf("size to send: %d on core %u\n", *str,
                 (unsigned int) disp_get_core_id());
    reset_cycle_counter();
    return init_urpc2_data(rpc_perf_measurement, TODO_ID, *str, str);
}

/// Performance measurement call for URPC.
void urpc_perf_measurement(void *payload)
{
    urpc2_enqueue(perf_func, payload);
}

static void
recv_send_string(__volatile struct urpc_send_string *send_string_obj)
{
    debug_printf("CCM: %s", send_string_obj->string);
}

static void
recv_term_send_char(__volatile struct urpc_term_send_char *sendchar_obj)
{
    terminal_feed_buffer(sendchar_obj->c);
}

static void
recv_init_mem_alloc(__volatile struct urpc_bootinfo_package *bootinfo_package)
{
    struct bootinfo *n_bi =
        (struct bootinfo *) malloc(sizeof(struct bootinfo));
    memcpy((void *) n_bi, (void *) &bootinfo_package->boot_info,
           sizeof(struct bootinfo));
    memcpy((void *) &n_bi->regions, (void *) &bootinfo_package->regions,
           sizeof(struct mem_region) * n_bi->regions_length);
    initialize_ram_alloc(n_bi);

    // Set up the modules from the boot info for spawning.
    struct capref mmstrings_cap = {
        .cnode = cnode_module, .slot = 0,
    };
    struct capref l1_cnode = {
        .cnode = cnode_task, .slot = TASKCN_SLOT_ROOTCN,
    };
    CHECK(cnode_create_foreign_l2(l1_cnode, ROOTCN_SLOT_MODULECN,
                                  &cnode_module));
    CHECK(frame_forge(mmstrings_cap, bootinfo_package->mmstrings_base,
                      bootinfo_package->mmstrings_size, disp_get_core_id()));
    for (int i = 0; i < n_bi->regions_length; i++) {
        struct mem_region reg = n_bi->regions[i];
        if (reg.mr_type == RegionType_Module) {
            struct capref module_cap = {
                .cnode = cnode_module, .slot = reg.mrmod_slot,
            };
            CHECK(frame_forge(module_cap, reg.mr_base, reg.mrmod_size,
                              disp_get_core_id()));
        }
    }
    already_received_memory = true;
}

static void
recv_remote_spawn(__volatile struct urpc_remote_spawn *remote_spawn_obj)
{
    // TODO: properly integrate with our process management once that is
    // properly constructed
    struct spawninfo *si =
        (struct spawninfo *) malloc(sizeof(struct spawninfo));
    DBG(DETAILED, "receive request to spawn %s\n", remote_spawn_obj->name);
    spawn_load_by_name((char *) remote_spawn_obj->name, si);
    free(si);
}

struct urpc_waiting_state {
    bool waiting;
    struct urpc2_data ret;
};

static struct urpc_waiting_state urpc_waiting_calls[255];

struct urpc2_data urpc2_send_and_receive(struct urpc2_data (*func)(void *data),
                                         void *payload, char type)
{
    uint32_t index = type;

    // TODO: We rely on the type being equal to the one specified in func. This
    // is potentially dangerous and may lead to unexpected behaviour when
    // mismatched.

    // TODO: Make this stuff threadsafe.
    // Check that there is no other ongoing transmission with the same id.
    __volatile bool *waiting = &urpc_waiting_calls[index].waiting;
    while (*waiting)
        thread_yield();

    DBG_LINE;

    // Add to table of waiting calls.
    urpc_waiting_calls[index].waiting = true;

    // Do the request.
    urpc2_enqueue(func, payload);

    // Wait for the answer.
    while (*waiting)
        thread_yield();

    return urpc_waiting_calls[index].ret;
}

static void *urpc2_rpc_send_helper1(unsigned char rpc_type,
                                    unsigned char rpc_id, struct capref cap,
                                    size_t payloadsize, void *payload)
{
    unsigned char *transfer = malloc(payloadsize + 6);
    memcpy(transfer, &payloadsize, 4);
    transfer[4] = rpc_type;
    transfer[5] = rpc_id;
    memcpy(&transfer[6], payload, payloadsize);
    return transfer;
}

static struct urpc2_data urpc2_rpc_send_helper2(void *transfer)
{
    size_t payloadsize = *(int32_t *) transfer;
    void *too_much_memcpy = malloc(payloadsize + 2);
    memcpy(too_much_memcpy, &((unsigned char *) transfer)[4], payloadsize + 2);
    free(transfer);
    return init_urpc2_data(rpc_over_urpc, 0 /* TODO: TODO_ID */,
                           payloadsize + 2, too_much_memcpy);
}

void urpc2_send_response(struct recv_list *rl, struct capref cap,
                         size_t payloadsize, void *payload)
{
    // TODO: Handle cap being other than NULL_CAP.
    assert((rl->type & 1) == 0);

    urpc2_enqueue(urpc2_rpc_send_helper2,
                  urpc2_rpc_send_helper1(rl->type & 1, rl->id, cap,
                                         payloadsize, payload));
}

// TODO: We can make this a lot nicer by refactoring lib_rpc to be transmission
// backend agnostic.
static struct recv_list recv_rpc_over_urpc(struct urpc2_data *data)
{
    // We just assume that we can reconstruct it like the aos_rpc_shared thing.
    // This implies they should share code.
    struct recv_list recv_list_buff;
    unsigned char type = ((char *) data->data)[0];
    unsigned char id = ((char *) data->data)[1];
    size_t size = (data->size_in_bytes - 2) + ((data->size_in_bytes - 2) % 4);
    recv_list_buff.type = type;

    // TODO: Consider if this is sane, given that it comes from another core.
    recv_list_buff.id = id;

    recv_list_buff.payload = (uintptr_t *) &(((char *) data->data)[2]);
    recv_list_buff.size = size;
    recv_list_buff.index = size;
    recv_list_buff.next = NULL;

    // TODO: consider having a response channel that this thread is waiting on?
    // Hell, consider resending from here to the other thread as if it were a
    // RPC? Or is that too insane?
    // Also we still need to figure out how to transfer caps.
    recv_list_buff.chan = NULL;

    return recv_list_buff;
}

struct recv_list urpc2_rpc_over_urpc(struct recv_list *rl, struct capref cap)
{
    struct urpc2_data ud = urpc2_send_and_receive(
        urpc2_rpc_send_helper2,
        urpc2_rpc_send_helper1(rl->type, rl->id, cap, rl->size * 4,
                               rl->payload),
        rpc_over_urpc);
    return recv_rpc_over_urpc(&ud);
}

static void urpc_register_process_reply(domainid_t *pid);

static void recv(__volatile struct urpc *data)
{
    domainid_t pid;
    switch (data->type) {
    case send_string:
        recv_send_string(&data->data.send_string);
        break;
    case term_send_char:
        recv_term_send_char(&data->data.send_char);
        break;
    case term_buff_consume:
        terminal_buffer_consume_char();
        break;
    case term_set_line:
        set_feed_mode_line();
        break;
    case term_set_direct:
        set_feed_mode_direct();
        break;
    case remote_spawn:
        recv_remote_spawn(&data->data.remote_spawn);
        break;
    case init_mem_alloc:
        recv_init_mem_alloc(&data->data.urpc_bootinfo);
        break;
    case register_process:
        // TODO: This currently only works for two cores...
        // Check the core and drop message.
        // Core 1 doesn't have to do anything here.
        if (disp_get_core_id() != 0) {
            return;
        }
        pid = procman_register_process((char *) data->data.send_string.string,
                                       disp_get_core_id() == 1 ? 0 : 1, NULL);
        urpc_register_process_reply(&pid);
        break;
    case rpc_over_urpc:
    case rpc_perf_measurement:
        assert(!"This shall be called nevermore\n");
    }
}

// TODO: This is a temp wrapper and should be changed asap because it contains
// an entirely unnecessary memcpy.
static void recv_wrapper(struct urpc2_data *data)
{
    uint32_t index = data->type;

    // Check if we were waiting for this call.
    if (urpc_waiting_calls[index].waiting) {
        urpc_waiting_calls[index].ret = *data;

        // We make a copy of the data, because threads and stacks and frees and
        // stuff. Also I'm tired and TODO: rethink all these mallocs
        urpc_waiting_calls[index].ret.data = malloc(data->size_in_bytes);
        memcpy(urpc_waiting_calls[index].ret.data, data->data,
               data->size_in_bytes);

        urpc_waiting_calls[index].waiting = false;
    } else {
        if (data->type == rpc_over_urpc) {
            struct recv_list ret = recv_rpc_over_urpc(data);
            recv_deal_with_msg(&ret);
            return;
        }
    }
    if (data->type == rpc_perf_measurement) {
        DBG(RELEASE, "received bytes: %u\n",
            (unsigned int) data->size_in_bytes);
        if (disp_get_core_id() == 0) {
            unsigned int k = get_cycle_count();
            int j = ((int *) data->data)[1];
            DBG(RELEASE, "URPC_PERFORMANCE_CYCLES: %u for datasize %d\n", k,
                j);
            if (j < 1024 * 1024 * 1) {
                int *payload = malloc(1024 * 10 + j);
                *payload = j + 10 * 1024;
                DBG(RELEASE, "payload: %d\n", *payload);
                urpc_perf_measurement((void *) payload);
            }
        } else {
            struct anon {
                int size;
                int received_size;
                char str[46];
            };
            struct anon *temp = malloc(54);
            *temp = (struct anon){54, (int) data->size_in_bytes,
                                  "just some ponies and stuff"};
            urpc2_enqueue(perf_func, (void *) temp);
        }
    } else if (data->type != rpc_over_urpc) {
        assert(data->size_in_bytes < 2000);
        struct urpc data_wrapper;
        data_wrapper.type = data->type;
        memcpy(&data_wrapper.data, data->data, data->size_in_bytes);
        recv(&data_wrapper);
    }
}

static struct urpc2_data send_string_func(void *data)
{
    char *str = (char *) data;
    return init_urpc2_data(send_string, TODO_ID, strlen(str) + 1, str);
}

static struct urpc2_data term_sendchar_func(void *data)
{
    char *c = (char *) data;
    return init_urpc2_data(term_send_char, TODO_ID, 1, c);
}

static struct urpc2_data term_consume_func(void *data)
{
    return init_urpc2_data(term_buff_consume, TODO_ID, 0, NULL);
}

static struct urpc2_data term_line_mode_func(void *data)
{
    return init_urpc2_data(term_set_line, TODO_ID, 0, NULL);
}

static struct urpc2_data term_direct_mode_func(void *data)
{
    return init_urpc2_data(term_set_direct, TODO_ID, 0, NULL);
}

static struct urpc2_data send_register_process_func(void *data)
{
    char *name = (char *) data;
    return init_urpc2_data(register_process, TODO_ID, strlen(name) + 1, name);
}

static struct urpc2_data send_register_process_reply_func(void *data)
{
    return init_urpc2_data(register_process, TODO_ID, sizeof(domainid_t),
                           data);
}

static struct urpc2_data send_init_mem_alloc_func(void *data)
{
    struct bootinfo *p_bi = (struct bootinfo *) data;

    assert(p_bi->regions_length <= 10);

    struct urpc_bootinfo_package *urpc_bootinfo =
        malloc(sizeof(struct urpc_bootinfo_package));

    memcpy((void *) &urpc_bootinfo->boot_info, p_bi, sizeof(struct bootinfo));
    memcpy((void *) urpc_bootinfo->regions, p_bi->regions,
           sizeof(struct mem_region) * p_bi->regions_length);

    struct capref mmstrings_cap = {
        .cnode = cnode_module, .slot = 0,
    };

    struct frame_identity mmstrings_id;

    CHECK(frame_identify(mmstrings_cap, &mmstrings_id));

    urpc_bootinfo->mmstrings_base = mmstrings_id.base;
    urpc_bootinfo->mmstrings_size = mmstrings_id.bytes;

    return init_urpc2_data(init_mem_alloc, TODO_ID,
                           sizeof(struct urpc_bootinfo_package),
                           urpc_bootinfo);
}

// We do the mem transfer before we go into the real protocol.
static void slave_setup(void)
{
    __volatile struct urpc_protocol *urpc_protocol =
        (struct urpc_protocol *) MON_URPC_VBASE;

    while (already_received_memory == false) {
        MEMORY_BARRIER;
        if (urpc_protocol->slave_state == needs_to_be_read) {
            assert(urpc_protocol->slave_data.type == init_mem_alloc);
            recv_init_mem_alloc(&urpc_protocol->slave_data.data.urpc_bootinfo);
        }
    }

    // We clear it as a signal to the other side that it can go into the real
    // protocol now.
    urpc_protocol->slave_state = available;

    MEMORY_BARRIER;
}

void urpc_slave_init_and_run(void)
{
    urpc2_init_and_run((void *) (MON_URPC_VBASE + (32 * 64)),
                       (void *) MON_URPC_VBASE, recv_wrapper, slave_setup);
}

void urpc_sendstring(char *str) { urpc2_enqueue(send_string_func, str); }

void urpc_term_sendchar(char *c)
{
    urpc2_enqueue(term_sendchar_func, c);
}

void urpc_term_consume(void)
{
    urpc2_enqueue(term_consume_func, NULL);
}

void urpc_term_set_line_mode(void)
{
    urpc2_enqueue(term_line_mode_func, NULL);
}

void urpc_term_set_direct_mode(void)
{
    urpc2_enqueue(term_direct_mode_func, NULL);
}

domainid_t urpc_register_process(char *str)
{
    DBG(DETAILED, "send process register request for %s \n", str);
    struct urpc2_data ud = urpc2_send_and_receive(send_register_process_func,
                                                  str, register_process);
    DBG(DETAILED, "got answer to process register  request: %d\n",
        *((uint32_t *) ud.data));
    return *((uint32_t *) ud.data);
}

static void urpc_register_process_reply(domainid_t *pid)
{
    DBG(DETAILED, "send process pid %d\n", *pid);
    uint32_t *per_pid = malloc(sizeof(uint32_t));
    *per_pid = *pid;
    urpc2_enqueue(send_register_process_reply_func, per_pid);
}

void urpc_init_mem_alloc(struct bootinfo *p_bi)
{
    urpc2_enqueue(send_init_mem_alloc_func, p_bi);
}

static struct urpc2_data spawnprocess_internal(void *data)
{
    char *str = (char *) data;
    return init_urpc2_data(remote_spawn, TODO_ID, strlen(str) + 1, str);
}

void urpc_spawn_process(char *name)
{
    DBG(DETAILED, "send request to spawn %s\n", name);
    urpc2_enqueue(spawnprocess_internal, name);
}

static void *slave_page_urpc_vaddr;

// We do the mem transfer before we go into the real protocol.
static void master_setup(void)
{
    __volatile struct urpc_protocol *urpc_protocol =
        (struct urpc_protocol *) slave_page_urpc_vaddr;

    struct urpc2_data data = send_init_mem_alloc_func(bi);

    MEMORY_BARRIER;

    urpc_protocol->slave_data.type = init_mem_alloc;
    memcpy((void *) &urpc_protocol->slave_data.data.urpc_bootinfo, data.data,
           data.size_in_bytes);
    urpc_protocol->slave_state = needs_to_be_read;

    // We wait for a signal of the other side that it can go into the real
    // protocol now.
    while (urpc_protocol->slave_state != available)
        MEMORY_BARRIER;

    free(data.data);
}

void urpc_master_init_and_run(void *urpc_vaddr)
{
    // XXX: Ugly hack.
    slave_page_urpc_vaddr = urpc_vaddr;
    urpc2_init_and_run(urpc_vaddr, urpc_vaddr + (32 * 64), recv_wrapper,
                       master_setup);
}
