#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <barrelfish_kpi/init.h>
#include <spawn/spawn.h>
#include <mem_alloc.h>
#include <mm/mm.h>
#include <aos/paging.h>
#include <lib_urpc.h>

// look, the proper way to implement this is via interrupts (see inthandler.c
// and anything mentioning IPI or IRQ in the kernel code)
// but apparently we are supposed to just spawn a thread and spin spin spin on
// it so that is what we'll do for now

// copy from a kernel only header. TODO: Make nicer
#define INIT_BOOTINFO_VBASE ((lvaddr_t) 0x200000)
#define INIT_ARGS_VBASE (INIT_BOOTINFO_VBASE + BOOTINFO_SIZE)
#define INIT_DISPATCHER_VBASE (INIT_ARGS_VBASE + ARGS_SIZE)
#define MON_URPC_VBASE (INIT_DISPATCHER_VBASE + DISPATCHER_SIZE)

// according to the spec we need to use dbm after aquiring a resource and
// before we use it
// as well as before releasing the resource
// this means this is a typical case of the bracket pattern
#define MEMORY_BARRIER __asm__("dmb")
// so we just encode the bracket pattern as a macro
#define with_barrier(x)                                                       \
    MEMORY_BARRIER;                                                           \
    x;                                                                        \
    MEMORY_BARRIER;
#define with_lock(lock, x)                                                    \
    thread_mutex_lock(&lock);                                                 \
    x;                                                                        \
    thread_mutex_unlock(&lock);

static struct thread_mutex urpc_thread_mutex;

enum urpc_state { non_initalized = 0, needs_to_be_read, available };

enum urpc_type { send_string, remote_spawn, init_mem_alloc };

struct urpc_bootinfo_package {
    struct bootinfo boot_info;
    struct mem_region regions[10];
    genpaddr_t mmstrings_base;
    gensize_t mmstrings_size;
};

struct urpc_send_string {
    char string[2000];
};

struct urpc_remote_spawn {
    char name[2000];
};

struct urpc {
    enum urpc_type type;
    union {
        struct urpc_send_string send_string;
        struct urpc_remote_spawn remote_spawn;
        struct urpc_bootinfo_package urpc_bootinfo;
    } data;
};

// we define a very simple protocol
struct urpc_protocol {
    char master_state;
    char slave_state;
    struct urpc master_data;
    struct urpc slave_data;
};

struct send_queue {
    bool (*func)(__volatile struct urpc *addr, void *data);
    void *data;
    struct send_queue *next;
};

struct send_queue *urpc_send_queue = NULL;
struct send_queue *urpc_send_queue_last = NULL;

bool already_received_memory = false;
bool urpc_ram_is_initalized(void) { return already_received_memory; }

static void enqueue(bool (*func)(__volatile struct urpc *addr, void *data),
                    void *data)
{
    with_lock(
        urpc_thread_mutex,
        struct send_queue *new = malloc(sizeof(struct send_queue));
        new->func = func; new->data = data; new->next = NULL;
        if (urpc_send_queue_last == NULL) {
            assert(urpc_send_queue == NULL);
            urpc_send_queue = new;
            urpc_send_queue_last = new;
        } else {
            urpc_send_queue_last->next = new;
            urpc_send_queue_last = new;
        });
}

static void requeue(struct send_queue *sq)
{
    with_lock(urpc_thread_mutex,
              if (urpc_send_queue_last == NULL) {
                  assert(urpc_send_queue == NULL);
                  urpc_send_queue = sq;
                  urpc_send_queue_last = sq;
              } else {
                  urpc_send_queue_last->next = sq;
                  urpc_send_queue_last = sq;
              });
}

static bool queue_empty(void)
{
    bool status;
    with_lock(urpc_thread_mutex, status = urpc_send_queue == NULL);
    return status;
}

static struct send_queue *dequeue(void)
{
    with_lock(
        urpc_thread_mutex, assert(urpc_send_queue != NULL);
        struct send_queue *fst = urpc_send_queue; urpc_send_queue = fst->next;
        if (urpc_send_queue == NULL) urpc_send_queue_last = NULL;) return fst;
}

static void
recv_send_string(__volatile struct urpc_send_string *send_string_obj)
{
    debug_printf("CCM: %s", send_string_obj->string);
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
        .cnode = cnode_module,
        .slot = 0,
    };
    struct capref l1_cnode = {
        .cnode = cnode_task,
        .slot = TASKCN_SLOT_ROOTCN,
    };
    CHECK(cnode_create_foreign_l2(l1_cnode, ROOTCN_SLOT_MODULECN, &cnode_module));
    CHECK(frame_forge(mmstrings_cap, bootinfo_package->mmstrings_base,
                bootinfo_package->mmstrings_size, disp_get_core_id()));
    for (int i = 0; i < n_bi->regions_length; i++) {
        struct mem_region reg = n_bi->regions[i];
        if (reg.mr_type == RegionType_Module) {
            struct capref module_cap = {
                .cnode = cnode_module,
                .slot = reg.mrmod_slot,
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
    spawn_load_by_name((char *) remote_spawn_obj->name, si);
    free(si);
}

static void recv(__volatile struct urpc *data)
{
    switch (data->type) {
    case send_string:
        recv_send_string(&data->data.send_string);
        break;
    case remote_spawn:
        recv_remote_spawn(&data->data.remote_spawn);
        break;
    case init_mem_alloc:
        recv_init_mem_alloc(&data->data.urpc_bootinfo);
        break;
    }
}

static int urpc_internal_master(void *urpc_vaddr)
{
    __volatile struct urpc_protocol *urpc_protocol =
        (struct urpc_protocol *) urpc_vaddr;
    with_barrier(urpc_protocol->master_state = available);
    for (;;) {
        MEMORY_BARRIER;
        if (urpc_protocol->master_state != needs_to_be_read &&
                (queue_empty() || urpc_protocol->slave_state != available)) {
            MEMORY_BARRIER;
            thread_yield();
        } else {
            if (urpc_protocol->master_state == needs_to_be_read) {
                recv(&urpc_protocol->master_data);
                urpc_protocol->master_state = available;
            } else {
                // we can only get into this case if we have something to send
                // and can send right now
                struct send_queue *sendobj = dequeue();
                if (!sendobj->func(&urpc_protocol->slave_data,
                                   sendobj->data)) {
                    // we are not done yet with this one, so we requeue it
                    requeue(sendobj);
                } else {
                    // note: this is fine and can't memleak so long as the func
                    // that was passed to us via sendobj cleans its own data up
                    // after it's done using it
                    free(sendobj);
                }
                urpc_protocol->slave_state = needs_to_be_read;
            }
            MEMORY_BARRIER;
        }
    }
    return 0;
}

void urpc_master_init_and_run(void *urpc_vaddr)
{
    assert(sizeof(struct urpc_protocol) <= BASE_PAGE_SIZE);
    thread_mutex_init(&urpc_thread_mutex);
    thread_create(urpc_internal_master, urpc_vaddr);
}

static bool send_string_func(__volatile struct urpc *urpcobj, void *data)
{
    const char *str = (const char *) data;
    assert(strlen(str) < 2000);
    urpcobj->type = send_string;
    memcpy((void *) &urpcobj->data.send_string.string, str, strlen(str) + 1);
    return 1;
}

static bool send_init_mem_alloc_func(__volatile struct urpc *urpcobj,
                                     void *data)
{
    struct bootinfo *p_bi = (struct bootinfo *) data;
    assert(p_bi->regions_length <= 10);
    urpcobj->type = init_mem_alloc;
    memcpy((void *) &urpcobj->data.urpc_bootinfo.boot_info, p_bi,
           sizeof(struct bootinfo));
    memcpy((void *) &urpcobj->data.urpc_bootinfo.regions, p_bi->regions,
           sizeof(struct mem_region) * p_bi->regions_length);
    struct capref mmstrings_cap = {
        .cnode = cnode_module,
        .slot = 0,
    };
    struct frame_identity mmstrings_id;
    CHECK(frame_identify(mmstrings_cap, &mmstrings_id));
    urpcobj->data.urpc_bootinfo.mmstrings_base = mmstrings_id.base;
    urpcobj->data.urpc_bootinfo.mmstrings_size = mmstrings_id.bytes;
    return 1;
}

static int urpc_internal_slave(void *nothing)
{
    __volatile struct urpc_protocol *urpc_protocol =
        (struct urpc_protocol *) MON_URPC_VBASE;
    with_barrier(urpc_protocol->slave_state = available);
    for (;;) {
        MEMORY_BARRIER;
        if (urpc_protocol->slave_state != needs_to_be_read &&
                (queue_empty() || urpc_protocol->master_state != available)) {
            MEMORY_BARRIER;
            thread_yield();
        } else {
            if (urpc_protocol->slave_state == needs_to_be_read) {
                recv(&urpc_protocol->slave_data);
                urpc_protocol->slave_state = available;
            } else {
                // we can only get into this case if we have something
                // to send and can send right now
                struct send_queue *sendobj = dequeue();
                if (!sendobj->func(&urpc_protocol->master_data,
                                   sendobj->data)) {
                    // we are not done yet with this one, so we
                    // requeue it
                    requeue(sendobj);
                } else {
                    // note: this is fine and can't memleak so long as
                    // the func that was passed to us via sendobj
                    // cleans its own data up after it's done using it
                    free(sendobj);
                }
                urpc_protocol->master_state = needs_to_be_read;
            }
            MEMORY_BARRIER;
        }
    }
    return 0;
}

void urpc_slave_init_and_run(void)
{
    assert(sizeof(struct urpc_protocol) <= BASE_PAGE_SIZE);
    thread_mutex_init(&urpc_thread_mutex);
    thread_create(urpc_internal_slave, NULL);
}

void urpc_sendstring(char *str) { enqueue(send_string_func, str); }

void urpc_init_mem_alloc(struct bootinfo *p_bi)
{
    enqueue(send_init_mem_alloc_func, p_bi);
}

static bool spawnprocess_internal(__volatile struct urpc *urpcobj, void *data)
{
    const char *str = (const char *) data;
    assert(strlen(str) < 2000);
    urpcobj->type = remote_spawn;
    memcpy((void *) &urpcobj->data.remote_spawn.name, str, strlen(str) + 1);
    return 1;
}

void urpc_spawn_process(char *name) { enqueue(spawnprocess_internal, name); }
