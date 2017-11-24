#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <lib_urpc2.h>
#include "init_headers/lib_urpc2.h"

// XXX: WARNING! USE THIS WITH CAUTION. Preliminary returns will NOT return
// the lock, control flow with 'continue' and 'break' might work differently
// than expected inside of synchronized blocks, and nested synchronized blocks
// can cause massive trouble. Be aware of that when using synchronized here!
#define synchronized(_mut)                                                    \
    for (struct thread_mutex *_mutx = &_mut; _mutx != NULL; _mutx = NULL)     \
        for (thread_mutex_lock(&_mut); _mutx != NULL;                         \
             _mutx = NULL, thread_mutex_unlock(&_mut))

static struct thread_mutex urpc2_thread_mutex;


//protocol:
//flags is 8 bits as follows:
#define MSG_WAITING 1
#define MSG_BEGIN_BLOCK 2
#define MSG_END_BLOCK 4
#define MSG_HAS_SIZE_INT32 8
#define MSG_HAS_SIZE_INT64 16 //this will have to suffice, as INT128 is annoying to emulate. Otherwise we could do 24 as INT128. But 8 yottabyte should be sufficiently large anyway.
struct ringbuffer_entry {
    char flags;
    char entry[63];
};

enum urpc2_states {
    BUFFER_FULL,
    BUFFER_EMPTY,
    DONE_WITH_TASK,
    WORKING
};

//2*32*64 = 4kb of ringbuffer
//partitioned in two because we generally have two way communication
//todo: consider a better split based on expectation of being the sender or the receiver of large quantities of data. Or wether a split really makes things simpler at all.
#define RINGSIZE 32
int current_send = 0;
int current_receive = 0;

__volatile struct ringbuffer_entry *ringbufferSend;
__volatile struct ringbuffer_entry *ringbufferReceive;

void (*urpc2_recv_handler)(struct urpc2_data*);


struct urpc2_send_queue {
    struct urpc2_data (*func)(void *rawdata);
    struct urpc2_data cachedata;
    void*  rawdata;
    struct urpc2_send_queue *next;
    bool hasDataInit;
};

struct urpc2_send_queue *urpc_urpc2_send_queue = NULL;
struct urpc2_send_queue *urpc_urpc2_send_queue_last = NULL;

static void enqueue(struct urpc2_data (*func)(void *data),
                    void *data)
{
    synchronized(urpc2_thread_mutex)
    {
        struct urpc2_send_queue *new = malloc(sizeof(struct urpc2_send_queue));

        new->func = func;
        new->rawdata = data;
        new->next = NULL;
        new->hasDataInit = false;

        if (urpc_urpc2_send_queue_last == NULL) {
            assert(urpc_urpc2_send_queue == NULL);
            urpc_urpc2_send_queue = new;
            urpc_urpc2_send_queue_last = new;
        } else {
            urpc_urpc2_send_queue_last->next = new;
            urpc_urpc2_send_queue_last = new;
        }
    }
}
/*
static void requeue(struct urpc2_send_queue *sq)
{
    synchronized(urpc2_thread_mutex)
    {
        if (urpc_urpc2_send_queue_last == NULL) {
            assert(urpc_urpc2_send_queue == NULL);
            urpc_urpc2_send_queue = sq;
            urpc_urpc2_send_queue_last = sq;
        } else {
            //todo: change this back to queue for last once the rest of the system can handle that
            sq->next = urpc_urpc2_send_queue;
            urpc_urpc2_send_queue = sq;
        }
    }
}
*/
static bool queue_empty(void)
{
    bool status;
    synchronized(urpc2_thread_mutex) status = urpc_urpc2_send_queue == NULL;
    return status;
}

static struct urpc2_send_queue *dequeue(void)
{
    struct urpc2_send_queue *fst;
    synchronized(urpc2_thread_mutex)
    {
        assert(urpc_urpc2_send_queue != NULL);

        fst = urpc_urpc2_send_queue;
        urpc_urpc2_send_queue = fst->next;

        if (urpc_urpc2_send_queue == NULL)
            urpc_urpc2_send_queue_last = NULL;
    }
    return fst;
}


static enum urpc2_states core_send(struct urpc2_data* data) {
//debug_printf("send type: %u size:%u", data->type, data->size_in_bytes);
    assert(data->size_in_bytes < (((int64_t)1)<<63)); //we cut the highest bit to make sure we never run into issues with overflow or stuff
    enum urpc2_states state = WORKING;
    do {
        MEMORY_BARRIER;
        int index_in_buffer = 0;
        if (ringbufferSend[current_send].flags & MSG_WAITING) {
            state = BUFFER_FULL;
            break;
        }
        ringbufferSend[current_send].flags = 0;
        bool smallRem = false;
        char smallRemCount;
        if (data->index + 63 >= data->size_in_bytes && (data->index > 0 || data->size_in_bytes <= 61)) {
            ringbufferSend[current_send].flags |= MSG_END_BLOCK;
            smallRem = true;
            smallRemCount = data->size_in_bytes - data->index;
        }
        if (data->index == 0) {
            ringbufferSend[current_send].flags |= MSG_BEGIN_BLOCK;
            if ((data->size_in_bytes > 61) & (data->size_in_bytes < (((int64_t )1) << 32))) {
                ringbufferSend[current_send].flags |= MSG_HAS_SIZE_INT32;
                uint32_t temp = (uint32_t) data->size_in_bytes;
                memcpy((void*)&(ringbufferSend[current_send].entry[index_in_buffer]),&temp,4);
                index_in_buffer += 4;
            } else if (data->size_in_bytes >= (((int64_t )1) << 32)) {
                ringbufferSend[current_send].flags |= MSG_HAS_SIZE_INT64;
                memcpy((void*)&(ringbufferSend[current_send].entry[index_in_buffer]),&data->size_in_bytes,8);
                index_in_buffer += 8;
            }
            ringbufferSend[current_send].entry[index_in_buffer] = data->type;
            index_in_buffer++;
            ringbufferSend[current_send].entry[index_in_buffer] = data->id;
            index_in_buffer++;
        }
        size_t length = smallRem ? smallRemCount : 63 - index_in_buffer;
        memcpy((void*)&ringbufferSend[current_send].entry[index_in_buffer], &(((char *) data->data)[data->index]), length);
        data->index += length;
        ringbufferSend[current_send].flags |= MSG_WAITING;
        current_send = (current_send + 1) % RINGSIZE;
        if(data->index >= data->size_in_bytes) {
            state = DONE_WITH_TASK;
            break;
        }
    }while(true);
    MEMORY_BARRIER;
    return state;
}

static bool usd_store_used = false;

static enum urpc2_states core_recv(struct urpc2_data* data) {
    enum urpc2_states state = WORKING;
    int temp_counter = 0;
    do {
        MEMORY_BARRIER;
        int index_in_buffer = 0;
        if (!(ringbufferReceive[current_receive].flags & MSG_WAITING)) {
            state = BUFFER_EMPTY;
            break;
        }
        if(data->index != 0 && ringbufferReceive[current_receive].flags & MSG_BEGIN_BLOCK)
            debug_printf("this too should never happen\n");
        if(data->index == 0) {
            if(!(ringbufferReceive[current_receive].flags & MSG_BEGIN_BLOCK))
                debug_printf("well this should never occur\n");
            if(ringbufferReceive[current_receive].flags & MSG_END_BLOCK) {
                data->size_in_bytes = 61; //best guess as we in that case never actually transmit the size and assume the type has a static size it knows anyway
                //we just immediately handle it here as it's just a single entry
                data->type = ringbufferReceive[current_receive].entry[index_in_buffer];
                index_in_buffer++;
                data->id = ringbufferReceive[current_receive].entry[index_in_buffer];
                index_in_buffer++;
                //copyless for efficency
                data->data = (void*)&ringbufferReceive[current_receive].entry[index_in_buffer];
//debug_printf("receive value:%d\n", *((uint32_t*) data->data));
                urpc2_recv_handler(data);
                ringbufferReceive[current_receive].flags &= ~MSG_WAITING;
                current_receive = (current_receive + 1) % RINGSIZE;
                state = DONE_WITH_TASK;
//debug_printf("receive type: %u size:%u", data->type, data->size_in_bytes);
                break;
            }
            else if(ringbufferReceive[current_receive].flags & MSG_HAS_SIZE_INT32) {
                memcpy(&data->size_in_bytes,(void*)&(ringbufferReceive[current_receive].entry[index_in_buffer]),4);
                index_in_buffer += 4;
            } else if(ringbufferReceive[current_receive].flags & MSG_HAS_SIZE_INT64) {
                memcpy(&data->size_in_bytes,(void*)&(ringbufferReceive[current_receive].entry[index_in_buffer]),8);
                index_in_buffer += 8;
            }else {
                assert(!"IMPOSSIBLE. Getting here means urpc2 is broken"); //todo: there is an opt possbility in the existance of this else branch. It means that we have more states than can be taken on by the flags, so we could make some states implicit
            }
            //I just realized that it can only be 32bit long because malloc constraints, so whatever. TODO: fix
            data->data = malloc(data->size_in_bytes);
            data->type = ringbufferReceive[current_receive].entry[index_in_buffer];
            index_in_buffer++;
            data->id = ringbufferReceive[current_receive].entry[index_in_buffer];
            index_in_buffer++;
        }

        bool smallRem = false;
        char smallRemCount;
        if ((data->index + 63 >= data->size_in_bytes) && (data->index > 0 || data->size_in_bytes <= 61)) {
            smallRem = true;
            smallRemCount = data->size_in_bytes - data->index;
        }

        size_t length = smallRem ? smallRemCount : 63 - index_in_buffer;
        memcpy(&(((char *) data->data)[data->index]),(void*)&ringbufferReceive[current_receive].entry[index_in_buffer], length);
        data->index += length;
        ringbufferReceive[current_receive].flags &= ~MSG_WAITING;
        current_receive = (current_receive + 1) % RINGSIZE;
        temp_counter++;
        if(data->index >= data->size_in_bytes) {
debug_printf("receive type: %u size:%u\n", data->type, data->size_in_bytes);
            state = DONE_WITH_TASK;
            usd_store_used = false;
            urpc2_recv_handler(data);
            free(data->data);
            break;
        }
    }while(true);
//debug_printf("receive value:%d\n", *((uint32_t*) data->data));
    MEMORY_BARRIER;
    return state;
}

static struct urpc2_data usd_store;
struct urpc2_send_queue *sendobj;
static int urpc2_internal(void *data)
{
    //first we spin on setup, this is so we can transfer ram
    void (*setup_func)(void) = (void(*)(void)) data;
    setup_func();
    while (true) {
        MEMORY_BARRIER;
        if (!(ringbufferReceive[current_receive].flags & MSG_WAITING)&&
            ((sendobj == NULL &&queue_empty()) || (ringbufferSend[current_send].flags & MSG_WAITING))) {
            MEMORY_BARRIER;
            thread_yield();
        } else {
            if (ringbufferReceive[current_receive].flags & MSG_WAITING) {
//debug_printf("received something \n");
                if(usd_store_used != true) {
                    debug_printf("hi\n");
                    struct urpc2_data usd;
                    usd.index = 0;
                    if(core_recv(&usd) != DONE_WITH_TASK) {
                        debug_printf("storing\n");
                        usd_store = usd;
                        usd_store_used = true;
                    }
                }else{
                    if(core_recv(&usd_store) == DONE_WITH_TASK) {
                        debug_printf("bye\n");
                        usd_store_used = false;
                    }
                }

            } else {
//debug_printf("send something \n");
                // we can only get into this case if we have something to send
                // and can send right now
                if(sendobj == NULL) sendobj = dequeue();
                if(!sendobj->hasDataInit) {
                    sendobj->cachedata = sendobj->func(sendobj->rawdata);
                    sendobj->hasDataInit = true;
                }
                if (core_send(&sendobj->cachedata) != DONE_WITH_TASK) {
                    // we are not done yet with this one, so we requeue it
//                    requeue(sendobj);
                } else {
                    // note: this is fine and can't memleak so long as the func
                    // that was passed to us via sendobj cleans its own data up
                    // after it's done using it
                    free(sendobj);
                    sendobj = NULL;
                }
            }
            MEMORY_BARRIER;
        }
    }
    return 0;
}

void urpc2_init_and_run(void* sendbuffer, void* receivebuffer, void (*recv_handler)(struct urpc2_data*), void (*setup_func)(void)) {
    assert(sizeof(struct ringbuffer_entry) == 64);
    thread_mutex_init(&urpc2_thread_mutex);
    ringbufferSend = sendbuffer;
    ringbufferReceive = receivebuffer;
    urpc2_recv_handler = recv_handler;
    thread_create(urpc2_internal, setup_func);
}
void urpc2_enqueue(struct urpc2_data (*func)(void *data), void *data) {
    enqueue(func,data);
}

struct urpc2_data init_urpc2_data(char type, char id, size_t size_in_bytes, void* data) {
    struct urpc2_data ret;
    ret.index = 0;
    ret.data = data;
    ret.type = type;
    ret.id = id;
    ret.size_in_bytes = size_in_bytes;
    return ret;
}
