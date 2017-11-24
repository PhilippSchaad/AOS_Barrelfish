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

#undef DEBUG_LEVEL
  #define DEBUG_LEVEL DETAILED

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

struct urpc_waiting_state{
    bool waiting;
    struct urpc2_data ret;
};
static struct urpc_waiting_state urpc_waiting_calls[255];
struct urpc2_data urpc2_send_and_receive(struct urpc2_data (*func)(void *data), void *payload, char type)
{
    uint32_t index = type;
    debug_printf("urpc2_send_and_receive type %d\n", index);
    debug_printf("lock is %d\n", urpc_waiting_calls[index].waiting);
    // TODO: we rely on the type being equal to the one specified in func. this is potentially dangerous and may lead to unexpected behaviour when
    // mismatched

    //TODO: make this stuff threadsafe
    // check that there is no other ongoing transmission with the same id
    __volatile bool *waiting = &urpc_waiting_calls[index].waiting;
    while (*waiting){
        thread_yield();
    }
    DBG_LINE;

    // add to table of waiting calls
    urpc_waiting_calls[index].waiting = true;
    debug_printf("urpc2_send_and_receive 1\n");
    // do the request
    urpc2_enqueue(func, payload);
    debug_printf("urpc2_send_and_receive 2\n");

    // wait for the answer
    while (*waiting){
        thread_yield();
    }
    debug_printf("sendandreceive index: %u size: %u\n",urpc_waiting_calls[index].ret.type, urpc_waiting_calls[index].ret.size_in_bytes);
    debug_printf("urpc2_send_and_receive 3\n");
    return urpc_waiting_calls[index].ret;
}
#include <lib_rpc.h>
#include <aos/aos_rpc_shared.h>

static void* urpc2_rpc_send_helper1(unsigned char rpc_type, unsigned char rpc_id, struct capref cap, size_t payloadsize, void *payload) {
    unsigned char* transfer = malloc(payloadsize+6);
    memcpy(transfer,&payloadsize,4);
    transfer[4] = rpc_type;
    transfer[5] = rpc_id;
    memcpy(&transfer[6],payload,payloadsize);
    debug_printf("urpc copying: %u bytes - %s\n",payloadsize,(char*)&transfer[6]);
    return transfer;
}

static struct urpc2_data urpc2_rpc_send_helper2(void* transfer) {
    size_t payloadsize = *(int32_t *)transfer;
    void * too_much_memcpy = malloc(payloadsize+2);
    memcpy(too_much_memcpy,&((unsigned char*)transfer)[4], payloadsize+2);
    free(transfer);
    debug_printf("sending %u - %s\n",payloadsize,&(((char*)too_much_memcpy)[2]));
    return init_urpc2_data(rpc_over_urpc, 0 /*TODO_ID*/, payloadsize+2, too_much_memcpy);
}

void urpc2_send_response(struct recv_list *rl, struct capref cap, size_t payloadsize, void *payload) {
    //todo: handle cap being other than NULL_CAP
    assert((rl->type & 1) == 0);
    debug_printf("D call of rpc type %u and id %u\n",(unsigned int) (rl->type&1), (unsigned int)rl->id);
    urpc2_enqueue(urpc2_rpc_send_helper2,urpc2_rpc_send_helper1(rl->type&1,rl->id,cap,payloadsize,payload));
}


//todo: we can make this a lot nicer by refactoring lib_rpc to be transmission backend agnostic
static struct recv_list recv_rpc_over_urpc(struct urpc2_data * data) {
    //we just assume that we can reconstruct it like the aos_rpc_shared thing. Which implies they should share code
    struct recv_list k;
    unsigned char type = ((char*)data->data)[0];
    unsigned char id = ((char*)data->data)[1];
    size_t size = (data->size_in_bytes-2) + ((data->size_in_bytes-2) % 4);
    k.type = type;
    k.id = id; //todo: consider if this is sane, given that it comes from another core
    k.payload = (uintptr_t*)&(((char *)data->data)[2]);
    k.size = size;
    k.index = size;
    k.next = NULL;
    k.chan = NULL; //consider having a response channel that this thread is waiting on?
    //hell, consider resending from here to the other thread as if it were a RPC? Or is that too insane?
    //also we still need to figure out how to transfer caps
    debug_printf("received urpc call of rpc type %u and id %u\n",(unsigned int) type, (unsigned int)id);
    return k;
}

struct recv_list urpc2_rpc_over_urpc(struct recv_list *rl, struct capref cap) {
    debug_printf("sending urpc call of rpc type %u and id %u\n",(unsigned int) rl->type, (unsigned int)rl->id);
    struct urpc2_data ud = urpc2_send_and_receive(urpc2_rpc_send_helper2,urpc2_rpc_send_helper1(rl->type,rl->id,cap,rl->size*4,rl->payload),rpc_over_urpc);
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
    case remote_spawn:
        recv_remote_spawn(&data->data.remote_spawn);
        break;
    case init_mem_alloc:
        recv_init_mem_alloc(&data->data.urpc_bootinfo);
        break;
    case register_process:
        // TODO: this currently only works for two cores...
        // check the core and drop message
        // Core 1 doesn't have to do anything here
        if(disp_get_core_id() != 0){
            debug_printf("however, the value here would be: %d\n", (char*) data->data.send_string.string);
            return;
        }
        debug_printf("here I am... \n");
        pid = procman_register_process((char*) data->data.send_string.string, disp_get_core_id() == 1 ? 0 : 1 , NULL);
        debug_printf("... the pony man %d\n", pid);
        urpc_register_process_reply(&pid);
        break;
    case rpc_over_urpc:
        assert(!"This shall be called nevermore\n");
    }
}

//todo: this is a temp wrapper and should be changed asap because it contains an entirely unnecessary memcpy
static void recv_wrapper(struct urpc2_data *data) {
    debug_printf("xxxx>receive value:%d\n", *((uint32_t*) data->data));
    uint32_t index = data->type;
    // check if we were waiting for this call
    debug_printf("waiting? %d\n", urpc_waiting_calls[index].waiting);
    if(urpc_waiting_calls[index].waiting){
        debug_printf("recv_wrapper 1 index: %u size: %u\n",index, data->size_in_bytes);

        debug_printf("the value here is: %d\n", *((uint32_t*) data->data));
        urpc_waiting_calls[index].ret = *data;
        debug_printf("the value here is: %d\n", *((uint32_t*) urpc_waiting_calls[index].ret.data));
        //we make a copy of the data, because threads and stacks and frees and stuff. Also I'm tired and todo: rethink all these mallocs
        urpc_waiting_calls[index].ret.data = malloc(data->size_in_bytes);
        memcpy(urpc_waiting_calls[index].ret.data,data->data,data->size_in_bytes);
        debug_printf("the value here is: %d\n", *((uint32_t*) urpc_waiting_calls[index].ret.data));
        urpc_waiting_calls[index].waiting = false;
    }else //todo: consider having an rpc_over_urpc_ack thing to make this all nicer
        if(data->type == rpc_over_urpc) {
            debug_printf("recv_wrapper 2\n");
            struct recv_list ret = recv_rpc_over_urpc(data);
            recv_deal_with_msg(&ret);
            return;
        }
    if(data->type != rpc_over_urpc) {
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
    return init_urpc2_data(send_string,TODO_ID,strlen(str)+1,str);
}

static struct urpc2_data send_register_process_func(void *data)
{
    char *name = (char *) data;
    return init_urpc2_data(register_process,TODO_ID,strlen(name)+1,name);
}
static struct urpc2_data send_register_process_reply_func(void *data)
{
    debug_printf("when sending, the value is still %d\n", *((uint32_t*) data));
    return init_urpc2_data(register_process,TODO_ID,sizeof(domainid_t),data);
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

domainid_t urpc_register_process(char *str) {
    DBG(DETAILED, "send process register request for %s \n", str);
    struct urpc2_data ud = urpc2_send_and_receive(send_register_process_func,str, register_process);
    DBG(DETAILED, "got answer to process register  request: %d\n", *((uint32_t*)ud.data));
    return *((uint32_t*)ud.data);
    //urpc2_enqueue(send_register_process_func, str);
}
static void urpc_register_process_reply(domainid_t *pid) {
    DBG(DETAILED, "send process pid %d\n", *pid);
    uint32_t* per_pid = malloc(sizeof(uint32_t));
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
