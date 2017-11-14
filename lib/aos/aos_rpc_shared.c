#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <aos/aos_rpc_shared.h>
#include <generic_threadsafe_queue.h>
#include "../../usr/init/init_headers/generic_threadsafe_queue.h"

//todo: make properly platform independent and stuff

static errval_t actual_sending(struct lmp_chan * chan, struct capref cap, int first_byte, size_t payloadcount, int* payload) {
    switch(payloadcount) {
        case 0:
            return (lmp_chan_send1(chan, LMP_FLAG_SYNC, cap, first_byte));
        case 1:
            return (lmp_chan_send2(chan, LMP_FLAG_SYNC, cap, first_byte, payload[0]));
        case 2:
            return(lmp_chan_send3(chan, LMP_FLAG_SYNC, cap, first_byte, payload[0], payload[1]));
        case 3:
            return(lmp_chan_send4(chan, LMP_FLAG_SYNC, cap, first_byte, payload[0], payload[1], payload[2]));
        case 4:
            return(lmp_chan_send5(chan, LMP_FLAG_SYNC, cap, first_byte, payload[0], payload[1], payload[2], payload[3]));
        case 5:
            return(lmp_chan_send6(chan, LMP_FLAG_SYNC, cap, first_byte, payload[0], payload[1], payload[2], payload[3], payload[4]));
        case 6:
            return(lmp_chan_send7(chan, LMP_FLAG_SYNC, cap, first_byte, payload[0], payload[1], payload[2], payload[3], payload[4], payload[5]));
        case 7:
            return(lmp_chan_send8(chan, LMP_FLAG_SYNC, cap, first_byte, payload[0], payload[1], payload[2], payload[3], payload[4], payload[5], payload[6]));
        case 8:
            return(lmp_chan_send9(chan, LMP_FLAG_SYNC, cap, first_byte, payload[0], payload[1], payload[2], payload[3], payload[4], payload[5], payload[6], payload[7]));
        default:
            USER_PANIC("invalid argcount in actual_sending\n");
    }
    return SYS_ERR_OK;
}

struct send_queue {
    unsigned char type;
    unsigned char id;
    struct lmp_chan* chan;
    struct capref cap;
    size_t index;
    size_t size;
    unsigned int* payload;
    struct event_closure callback_when_done;
};

static int rpc_type_min_id[256];
static int rpc_type_max_id[256];
struct generic_queue_obj rpc_send_queue;

//todo: proper error handling
static errval_t send_loop(void * args) {
    struct send_queue *sq = (struct send_queue *) args;
    struct capref cap;
    size_t remaining = (sq->size-sq->index);
    int first_byte = sq->type << 24 + sq->id << 16 + sq->size;
    if(index == 0) {
        cap = sq->cap;
    }
    else {
        cap = NULL_CAP;
    }
    errval_t err;
    err = actual_sending(sq->chan,cap,first_byte,remaining > 8 ? 8 : remaining,&sq->payload[index]);
    if (err_is_fail(err)){
        // Reregister if failed.
        CHECK(lmp_chan_register_send(&sq->chan, get_default_waitset(),
                                     MKCLOSURE((void *) send_loop,
                                               args)));
        CHECK(event_dispatch(get_default_waitset()));
    }else{
        if(remaining > 8) {
            //we still have data to send, so we adjust and resend
            sq->index += 8;
            CHECK(lmp_chan_register_send(&sq->chan, get_default_waitset(),
                                         MKCLOSURE((void *) send_loop,
                                                   args)));
            CHECK(event_dispatch(get_default_waitset()));
        }
        else{
            if(sq->callback_when_done.handler != NULL)
                sq->callback_when_done.handler(sq->callback_when_done.arg);
            bool done = true;
            synchronized(rpc_queue) {
                assert(rpc_queue.fst->data == args);
                struct generic_queue_obj *temp = rpc_queue.fst; //dequeue the one we just got done with and delete it
                rpc_queue.fst = rpc_queue.fst->next;
                if(rpc_type_min_id[sq->type] == sq->id) {
                    //todo: make proper
                    rpc_type_min_id[sq->type]++;
                }
                free(rpc_queue.fst->data);
                free(temp);
                if(rpc_queue.fst != NULL) {//more to be ran
                    done = false;

                }else{
                    rpc_queue.last = NULL;
                }
            }
            if(!done) {
                CHECK(lmp_chan_register_send(&((struct send_queue*)rpc_queue.fst->data)->chan, get_default_waitset(),
                                             MKCLOSURE((void *) send_loop,
                                                       rpc_queue.fst->data)));
                CHECK(event_dispatch(get_default_waitset()));
            }
            return SYS_ERR_OK;
        }
    }
    return SYS_ERR_OK;
}

errval_t send(struct lmp_chan * chan, struct capref cap, unsigned char type, size_t payloadsize, int* payload, struct event_closure callback_when_done) {
    //obtain fresh ID for type
    //then enqueue message into sending loop
    assert(payloadsize < 65536); //otherwise we can't encode the size in 16 bits
    bool need_to_start = false;
    synchronized(rpc_send_queue.thread_mutex) {
        unsigned char min_id = rpc_type_min_id[type];
        unsigned char max_id = rpc_type_max_id[type];
        if(max_id+1 % 256 == min_id)
            USER_PANIC("we ran out of message slots\n"); //todo: turn this into a proper error msg that gets returned and is expected to be handled by the caller
        rpc_type_max_id[type] = max_id+1 % 256;
        struct send_queue* sq = malloc(sizeof(struct send_queue));
        sq->type = type;
        sq->id = max_id;
        sq->index = 0;
        sq->size = payloadsize;
        sq->payload = payload;
        sq->cap = cap;
        sq->chan = chan;
        sq->callback_when_done = callback_when_done;
        struct generic_queue *new = malloc(sizeof(struct generic_queue));

        new->data = (void*)sq;
        new->next = NULL;

        if (rpc_send_queue.last == NULL) {
            assert(rpc_send_queue.fst == NULL);
            need_to_start = true;
            rpc_send_queue.fst = new;
            rpc_send_queue.last = new;
        } else {
            rpc_send_queue.last->next = new;
            rpc_send_queue.last = new;
        }
    }
    if(need_to_start) {
        CHECK(lmp_chan_register_send(&((struct send_queue*)rpc_send_queue.fst->data)->chan, get_default_waitset(),
                                     MKCLOSURE((void *) send_loop,
                                               rpc_queue.fst->data)));
        CHECK(event_dispatch(get_default_waitset()));
    }
    return SYS_ERR_OK;
}

struct send_chars_cleanup_struct {
    void *data;
    struct event_closure callback;
};
static void* send_chars_cleanup(void * data){
    struct send_chars_cleanup_struct* sccs = (struct send_chars_cleanup_struct*)data;
    free(sccs->data);
    if(sccs->callback.handler != NULL)
        sccs->callback.handler(sccs->callback.arg);
    free(data);
}

errval_t send_chars(struct lmp_chan * chan, struct capref cap, unsigned char type, size_t payloadsize, char* payload, struct event_closure callback_when_done) {
    size_t payloadsize2 = (payloadsize + (payloadsize % 4 != 0 ? 4 - (payloadsize %4) : 0)) /4;
    int* payload2 = malloc(payloadsize2);
    memcpy(payload2,payload,payloadsize);
    struct send_chars_cleanup_struct* sccs = malloc(sizeof(struct send_chars_cleanup_struct));
    sccs->data = payload2;
    sccs->callback = callback_when_done;
    struct event_closure callback2 = MKCLOSURE(send_chars_cleanup,sccs);
    send(chan,cap,type,payloadsize2,payload2,callback2);
}



static struct recv_list *rpc_recv_lookup(struct recv_list *head, unsigned char type, unsigned char id) {
    while(head!= NULL) {
        if(head->type == type && head->id == id)
            return head;
        head = head->next;
    }
    return NULL;
}

static void rpc_recv_list_remove(struct recv_list **rl_head, struct recv_list* rl) {
    assert(rl != NULL);
    struct recv_list *prev =NULL;
    struct recv_list *cur = *rl_head;
    while(true) {
        if(rl == cur)
            break;
        prev = cur;
        cur = cur->next;
    }
    if(prev == NULL) {
        *rl_head = cur->next;
    }else {
        prev->next = cur->next;
    }
}


//todo: error handling
static void recv_handling(void* args) {

    struct recv_chan *rc = (struct recv_chan *) args;
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;
    struct capref cap;

    lmp_chan_recv(rc->chan, &msg, &cap);
//    if(cap != NULL_CAP) //todo: figure out if this if check would be a good idea or not when properly implemented
    lmp_chan_alloc_recv_slot(rc->chan);
    lmp_chan_register_recv(
            rc->chan, get_default_waitset(),
            MKCLOSURE(recv_handling, args));

    assert(msg.buf.msglen > 0);
//    DBG(VERBOSE, "Handling RPC receive event (type %d)\n", msg.words[0]);
    unsigned char type = msg.words[0] >> 24;
    unsigned char id = (msg.words[0] >> 16) & 0xFF;
    size_t size = msg.words[0] & 0xFFFF;
    if(size < 9) //fast path for small messages
    {
        struct recv_list rl;
        rl.payload = &msg.words[1];
        rl.size = size;
        rl.id = id;
        rl.type = type;
        rl.cap = cap;
        rl.index = size;
        rl.next = NULL;
        rc->recv_deal_with_msg(&rl);
    }else {
        struct recv_list *rl = rpc_recv_lookup(rc->rpc_recv_list, type, id);
        if (rl == NULL) {
            rl = malloc(sizeof(struct recv_list));
            rl->payload = malloc(size*4);
            rl->size = size;
            rl->id = id;
            rl->type = type;
            rl->cap = cap;
            rl->index = 0;
            rl->next = rc->rpc_recv_list;
            rc->rpc_recv_list = rl;
        }
        size_t rem = rl->size - rl->index;
        size_t count = (rem > 8 ? 8 : rem);
        memset(rl->payload[rl->index],&msg.words[1],count*4);
        rl->index+=count;
        assert(rl->index <= rl->payload);
        if(rl->index == rl->payload) {
            //done transferring this msg, we can now do whatever we should do with this
            rpc_recv_list_remove(rc,rl);
            rc->recv_deal_with_msg(rl);
            free(rl);
        }
    }
}

errval_t init_rpc_client(void (*recv_deal_with_msg)(struct recv_list *), struct lmp_chan* chan, struct capref dest) {
    thread_mutex_init(&rpc_send_queue.thread_mutex);
    // Allocate lmp channel structure.
    // Create local endpoint.
    // Set remote endpoint to dest's endpoint.
    CHECK(lmp_chan_accept(chan, DEFAULT_LMP_BUF_WORDS, dest));
    struct recv_chan *rc = malloc(sizeof(struct recv_chan));
    rc->chan = chan;
    rc->recv_deal_with_msg = recv_deal_with_msg;
    rc->rpc_recv_list = NULL;

    lmp_chan_register_recv(
            rc->chan, get_default_waitset(),
            MKCLOSURE(recv_handling, rc));
    return SYS_ERR_OK;
}

errval_t init_rpc_server(void (*recv_deal_with_msg)(struct recv_list *), struct lmp_chan* chan) {
    thread_mutex_init(&rpc_send_queue.thread_mutex);

    CHECK(lmp_chan_accept(chan, DEFAULT_LMP_BUF_WORDS, NULL_CAP));
    CHECK(lmp_chan_alloc_recv_slot(chan));
    CHECK(cap_copy(cap_initep, chan->local_cap));
    struct recv_chan *rc = malloc(sizeof(struct recv_chan));
    rc->chan = chan;
    rc->recv_deal_with_msg = recv_deal_with_msg;
    rc->rpc_recv_list = NULL;

    lmp_chan_register_recv(
            rc->chan, get_default_waitset(),
            MKCLOSURE(recv_handling, rc));
    return SYS_ERR_OK;
}