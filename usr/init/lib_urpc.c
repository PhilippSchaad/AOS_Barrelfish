#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <barrelfish_kpi/init.h>
#include <lib_urpc.h>

//look, the proper way to implement this is via interrupts (see inthandler.c and anything mentioning IPI or IRQ in the kernel code)
//but apparently we are supposed to just spawn a thread and spin spin spin on it so that is what we'll do for now


//copy from a kernel only header. TODO: Make nicer
#define INIT_BOOTINFO_VBASE     ((lvaddr_t) 0x200000)
#define INIT_ARGS_VBASE         (INIT_BOOTINFO_VBASE + BOOTINFO_SIZE)
#define INIT_DISPATCHER_VBASE   (INIT_ARGS_VBASE + ARGS_SIZE)
#define MON_URPC_VBASE          (INIT_DISPATCHER_VBASE + DISPATCHER_SIZE)

//according to the spec we need to use dbm after aquiring a resource and before we use it
//as well as before releasing the resource
//this means this is a typical case of the bracket pattern
#define MEMORY_BARRIER __asm__("dmb")
//so we just encode the bracket pattern as a macro
#define with_barrier(x) MEMORY_BARRIER; x; MEMORY_BARRIER;

enum urpc_state {
    non_initalized = 0,
    needs_to_be_read,
    available
};

enum urpc_type {
    send_string,
    remote_spawn
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
    } data;
};

//we define a very simple protocol
struct urpc_protocol {
    char master_state;
    char slave_state;
    struct urpc master_data;
    struct urpc slave_data;
};


static void recv_send_string(__volatile struct urpc_send_string* send_string_obj) {
    debug_printf("Obtained the following string from the other core: %s\n",send_string_obj->string);
}

static void recv_remote_spawn(__volatile struct urpc_remote_spawn* remote_spawn_obj) {

}

static void recv(__volatile struct urpc* data) {
    switch(data->type) {
        case send_string:
            recv_send_string(&data->data.send_string);
        case remote_spawn:
            recv_remote_spawn(&data->data.remote_spawn);
    }
}

int urpc_master_init_and_run(void* urpc_vaddr) {
    assert(sizeof(struct urpc_protocol) <= BASE_PAGE_SIZE);
    __volatile struct urpc_protocol * urpc_protocol = (struct urpc_protocol*)urpc_vaddr;
    with_barrier(urpc_protocol->master_state = available);
    for(;;) {
        with_barrier(
                while (urpc_protocol->master_state != needs_to_be_read);
                recv(&urpc_protocol->master_data);
                urpc_protocol->master_state = available;
        );
    }
    return 0;
}

static void send_string_func(__volatile struct urpc_protocol* addr, const char* str) {
    assert(strlen(str) < 2000);
    with_barrier(
       __volatile struct urpc_protocol* urpc_protocol = addr;
            while (urpc_protocol->master_state != available);
            debug_printf("we'll write to addr: %p from addr: %p\n",urpc_protocol->master_data.data.send_string.string,str);
           memcpy((void*)&urpc_protocol->master_data.data.send_string.string,str,strlen(str)+1);
       urpc_protocol->master_state = needs_to_be_read;
    );
}

int urpc_slave_init_and_run(void* nothing) {
    assert(sizeof(struct urpc_protocol) <= BASE_PAGE_SIZE);
    debug_printf("first we access MON_URPC_VBASE\n");
    const char* str = "Hello from the other side\n";
    __volatile struct urpc_protocol * urpc_protocol = (struct urpc_protocol*)MON_URPC_VBASE;
    debug_printf("please? %p\n",urpc_protocol);
    send_string_func(urpc_protocol,str);
    send_string_func(urpc_protocol,"Sometimes the other side feels talkactive\n");
    send_string_func(urpc_protocol,"and sends way way more than just a single message\n");
    send_string_func(urpc_protocol,"it might even send many messages indeed!\n");
    with_barrier(urpc_protocol->slave_state = available);
    for(;;) {
        with_barrier(
                while (urpc_protocol->slave_state != needs_to_be_read);
                recv(&urpc_protocol->slave_data);
                urpc_protocol->slave_state = available;
        );
    }
    return 0;
}