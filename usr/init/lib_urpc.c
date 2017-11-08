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
    remote_spawn,
    receive_memory
};
struct urpc_send_string {
    char string[2000];
};
struct urpc_remote_spawn {
    char name[2000];
};

struct urpc_receive_memory {
    void *base;
    size_t size;
};

struct urpc {
    enum urpc_type type;
    union {
        struct urpc_send_string send_string;
        struct urpc_remote_spawn remote_spawn;
        struct urpc_receive_memory receive_memory;
    } data;
};

//we define a very simple protocol
struct urpc_protocol {
    char master_state;
    char slave_state;
    struct urpc master_data;
    struct urpc slave_data;
};

struct send_queue {
    bool (*func)(__volatile struct urpc* addr,void* data);
    void* data;
    struct send_queue* next;
};

struct send_queue *urpc_send_queue = NULL;
struct send_queue *urpc_send_queue_last = NULL;

static void enqueue(bool (*func)(__volatile struct urpc* addr,void* data), void *data) {
    //todo: locks
    struct send_queue* new = malloc(sizeof(struct send_queue));
    new->func = func;
    new->data = data;
    new->next = NULL;
    if(urpc_send_queue_last == NULL) {
        assert(urpc_send_queue == NULL);
        urpc_send_queue = new;
        urpc_send_queue_last = new;
    } else {
        urpc_send_queue_last->next = new;
        urpc_send_queue_last = new;
    }

}

static void requeue(struct send_queue* sq) {
    //todo: locks
    if(urpc_send_queue_last == NULL) {
        assert(urpc_send_queue == NULL);
        urpc_send_queue = sq;
        urpc_send_queue_last = sq;
    } else {
        urpc_send_queue_last->next = sq;
        urpc_send_queue_last = sq;
    }
}

static bool queue_empty(void) {
    //todo: locks
    return(urpc_send_queue == NULL);
}

static struct send_queue* dequeue(void) {
    //todo: locks
    assert(urpc_send_queue != NULL);
    struct send_queue* fst = urpc_send_queue;
    urpc_send_queue = fst->next;
    if(urpc_send_queue == NULL)
        urpc_send_queue_last = NULL;
    return fst;
}


static void recv_send_string(__volatile struct urpc_send_string* send_string_obj) {
    debug_printf("Obtained the following string from the other core: %s\n",send_string_obj->string);
}

static void recv_remote_spawn(__volatile struct urpc_remote_spawn* remote_spawn_obj) {
    //spawn_load_by_name(remote_spawn_obj->name);
}

static void recv_receive_memory(__volatile struct urpc_receive_memory* receive_memory_obj) {
    //TODO: do the mem receiving stuff
}

static void recv(__volatile struct urpc* data) {
    switch(data->type) {
        case send_string:
            recv_send_string(&data->data.send_string);
            break;
        case remote_spawn:
            recv_remote_spawn(&data->data.remote_spawn);
            break;
        case receive_memory:
            recv_receive_memory(&data->data.receive_memory);
            break;
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
                while (urpc_protocol->master_state != needs_to_be_read && (queue_empty() || urpc_protocol->slave_state != available));
                if(urpc_protocol->master_state == needs_to_be_read) {
                    recv(&urpc_protocol->master_data);
                    urpc_protocol->master_state = available;
                } else {
                    //we can only get into this case if we have something to send and can send right now
                    struct send_queue* sendobj = dequeue();
                    if(!sendobj->func(&urpc_protocol->slave_data,sendobj->data)) {
                        //we are not done yet with this one, so we requeue it
                        requeue(sendobj);
                    }else{
                        //note: this is fine and can't memleak so long as the func that was passed to us via sendobj cleans its own data up after it's done using it
                        free(sendobj);
                    }
                    urpc_protocol->slave_state = needs_to_be_read;
                }
        );
    }
    return 0;
}

static bool send_string_func(__volatile struct urpc* urpcobj, void* data) {
    const char* str = (const char*)data;
    assert(strlen(str) < 2000);
    urpcobj->type = send_string;
    debug_printf("called send_string_func\n");
    memcpy((void*)&urpcobj->data.send_string.string,str,strlen(str)+1);
    return 1;
}

int urpc_slave_init_and_run(void* nothing) {
    assert(sizeof(struct urpc_protocol) <= BASE_PAGE_SIZE);
    const char* str = "Hello from the other side\n";
    __volatile struct urpc_protocol * urpc_protocol = (struct urpc_protocol*)MON_URPC_VBASE;
    with_barrier(urpc_protocol->slave_state = available);
    enqueue(send_string_func,(void*)str);
    enqueue(send_string_func,(void*)"Sometimes the other side feels talkactive\n");
    enqueue(send_string_func,(void*)"and sends way way more than just a single message\n");
    enqueue(send_string_func,(void*)"it might even send many messages indeed!\n");
    for(;;) {
        with_barrier(
                while (urpc_protocol->slave_state != needs_to_be_read && (queue_empty() || urpc_protocol->master_state != available));
                if(urpc_protocol->slave_state == needs_to_be_read) {
                    recv(&urpc_protocol->slave_data);
                    urpc_protocol->slave_state = available;
                } else {
                    //we can only get into this case if we have something to send and can send right now
                    struct send_queue* sendobj = dequeue();
                    if(!sendobj->func(&urpc_protocol->master_data,sendobj->data)) {
                        //we are not done yet with this one, so we requeue it
                        requeue(sendobj);
                    }else{
                        //note: this is fine and can't memleak so long as the func that was passed to us via sendobj cleans its own data up after it's done using it
                        free(sendobj);
                    }
                    urpc_protocol->master_state = needs_to_be_read;
                }
        );
    }
    return 0;
}