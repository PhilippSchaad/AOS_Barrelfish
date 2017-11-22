#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <barrelfish_kpi/init.h>
#include <spawn/spawn.h>
#include <mem_alloc.h>
#include <mm/mm.h>
#include <aos/paging.h>
#include <lib_urpc.h>
#include <lib_urpc2.h>
#include "init_headers/lib_urpc2.h"

//we still need to port the proper ID system, so until now it uses this. Just rip it out of aos_rpc_shared
#define TODO_ID 0

// look, the proper way to implement this is via interrupts (see inthandler.c
// and anything mentioning IPI or IRQ in the kernel code)
// but apparently we are supposed to just spawn a thread and spin spin spin on
// it so that is what we'll do for now

// copy from a kernel only header. TODO: Make nicer
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

enum urpc_type { send_string, remote_spawn, init_mem_alloc , register_process};

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

bool already_received_memory = false;
bool urpc_ram_is_initalized(void) { return already_received_memory; }


// we define a very simple protocol
struct urpc_protocol {
    char master_state;
    char slave_state;
    struct urpc master_data;
    struct urpc slave_data;
};

static void
recv_send_string(__volatile struct urpc_send_string *send_string_obj)
{
    debug_printf("CCM: %s", send_string_obj->string);
}

static void
recv_init_mem_alloc(__volatile struct urpc_bootinfo_package *bootinfo_package)
{
    debug_printf("received bootinfo package\n");
    struct bootinfo *n_bi =
        (struct bootinfo *) malloc(sizeof(struct bootinfo));
    memcpy((void *) n_bi, (void *) &bootinfo_package->boot_info,
           sizeof(struct bootinfo));
    memcpy((void *) &n_bi->regions, (void *) &bootinfo_package->regions,
           sizeof(struct mem_region) * n_bi->regions_length);
    dump_bootinfo(n_bi,1);
    initialize_ram_alloc(n_bi);
    debug_printf("what is going wrong...\n");
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
    debug_printf("done with mem recv\n");
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
    case register_process:
        // TODO: this currently only works for two cores...
        procman_register_process((char*) data->data.send_string.string, disp_get_core_id() == 1 ? 0 : 1, NULL);
        break;
    }
}

//todo: this is a temp wrapper and should be changed asap because it contains an entirely unnecessary memcpy
static void recv_wrapper(struct urpc2_data *data) {
    assert(data->size_in_bytes < 2000);
    struct urpc data_wrapper;
    data_wrapper.type = data->type;
    memcpy(&data_wrapper.data,data->data,data->size_in_bytes);
    recv(&data_wrapper);
}

static struct urpc2_data send_string_func(void *data)
{
    char *str = (char *) data;
    return init_urpc2_data(send_string,TODO_ID,strlen(str)+1,str);
}

static struct urpc2_data send_register_process_func(void *data)
{
    char *name = (char *) data;
    return init_urpc2_data(register_process,TODO_ID,strlen(name)+1,name);
}

static struct urpc2_data send_init_mem_alloc_func(void *data)
{
    struct bootinfo *p_bi = (struct bootinfo *) data;
    assert(p_bi->regions_length <= 10);
    struct urpc_bootinfo_package* urpc_bootinfo = malloc(sizeof(struct urpc_bootinfo_package));
    memcpy((void *) &urpc_bootinfo->boot_info, p_bi,
           sizeof(struct bootinfo));
    memcpy((void *) urpc_bootinfo->regions, p_bi->regions,
           sizeof(struct mem_region) * p_bi->regions_length);
    struct capref mmstrings_cap = {
        .cnode = cnode_module, .slot = 0,
    };
    struct frame_identity mmstrings_id;
    CHECK(frame_identify(mmstrings_cap, &mmstrings_id));
    urpc_bootinfo->mmstrings_base = mmstrings_id.base;
    urpc_bootinfo->mmstrings_size = mmstrings_id.bytes;
    return init_urpc2_data(init_mem_alloc,TODO_ID,sizeof(struct urpc_bootinfo_package),urpc_bootinfo);
}

//we do the mem transfer before we go into the real protocol
static void slave_setup(void) {
    __volatile struct urpc_protocol *urpc_protocol =
            (struct urpc_protocol *) MON_URPC_VBASE;
    while(already_received_memory == false) {
        MEMORY_BARRIER;
        if(urpc_protocol->slave_state == needs_to_be_read) {
            assert(urpc_protocol->slave_data.type == init_mem_alloc);
            recv_init_mem_alloc(&urpc_protocol->slave_data.data.urpc_bootinfo);
        }
    }
    //we clear it as a signal to the other side that it can go into the real protocol now
    urpc_protocol->slave_state = available;
    MEMORY_BARRIER;
}

void urpc_slave_init_and_run(void)
{
    urpc2_init_and_run((void*)(MON_URPC_VBASE+(32*64)),(void*)MON_URPC_VBASE,recv_wrapper,slave_setup);
}

void urpc_sendstring(char *str) { urpc2_enqueue(send_string_func, str); }

void urpc_register_process(char *str) {
    urpc2_enqueue(send_register_process_func, str);
}

void urpc_init_mem_alloc(struct bootinfo *p_bi)
{
    urpc2_enqueue(send_init_mem_alloc_func, p_bi);
}

static struct urpc2_data spawnprocess_internal(void *data)
{
    char *str = (char *) data;
    return init_urpc2_data(remote_spawn,TODO_ID,strlen(str)+1,str);
}

void urpc_spawn_process(char *name) {
    DBG(DETAILED, "send request to spawn %s\n", name);
    urpc2_enqueue(spawnprocess_internal, name);
}

static void *slave_page_urpc_vaddr;
//we do the mem transfer before we go into the real protocol
static void master_setup(void) {
    __volatile struct urpc_protocol *urpc_protocol =
            (struct urpc_protocol *) slave_page_urpc_vaddr;
    struct urpc2_data data = send_init_mem_alloc_func(bi);
    MEMORY_BARRIER;
    urpc_protocol->slave_data.type = init_mem_alloc;
    memcpy((void*)&urpc_protocol->slave_data.data.urpc_bootinfo,data.data,data.size_in_bytes);
    urpc_protocol->slave_state = needs_to_be_read;
    //we wait for a signal of the other side that it can go into the real protocol now
    while(urpc_protocol->slave_state != available) MEMORY_BARRIER;
    free(data.data);
}

void urpc_master_init_and_run(void *urpc_vaddr)
{
    //ugly hack
    slave_page_urpc_vaddr = urpc_vaddr;
    urpc2_init_and_run(urpc_vaddr,urpc_vaddr+(32*64),recv_wrapper,master_setup);
}
