#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <aos/aos_rpc_shared.h>
#include <aos/generic_threadsafe_queue.h>

// TODO: make properly platform independent and stuff

static errval_t actual_sending(struct lmp_chan *chan, struct capref cap,
                               int first_byte, size_t payloadcount,
                               uintptr_t *payload)
{
    if (!chan) {
        USER_PANIC("no more chan...");
    }
    switch (payloadcount) {
    case 0:
        return lmp_chan_send1(chan, LMP_FLAG_SYNC, cap, first_byte);
    case 1:
        return lmp_chan_send2(chan, LMP_FLAG_SYNC, cap, first_byte,
                              payload[0]);
    case 2:
        return lmp_chan_send3(chan, LMP_FLAG_SYNC, cap, first_byte, payload[0],
                              payload[1]);
    case 3:
        return lmp_chan_send4(chan, LMP_FLAG_SYNC, cap, first_byte, payload[0],
                              payload[1], payload[2]);
    case 4:
        return lmp_chan_send5(chan, LMP_FLAG_SYNC, cap, first_byte, payload[0],
                              payload[1], payload[2], payload[3]);
    case 5:
        return lmp_chan_send6(chan, LMP_FLAG_SYNC, cap, first_byte, payload[0],
                              payload[1], payload[2], payload[3], payload[4]);
    case 6:
        return lmp_chan_send7(chan, LMP_FLAG_SYNC, cap, first_byte, payload[0],
                              payload[1], payload[2], payload[3], payload[4],
                              payload[5]);
    case 7:
        return lmp_chan_send8(chan, LMP_FLAG_SYNC, cap, first_byte, payload[0],
                              payload[1], payload[2], payload[3], payload[4],
                              payload[5], payload[6]);
    case 8:
        return lmp_chan_send9(chan, LMP_FLAG_SYNC, cap, first_byte, payload[0],
                              payload[1], payload[2], payload[3], payload[4],
                              payload[5], payload[6], payload[7]);
    default:
        USER_PANIC("invalid argcount in actual_sending\n");
    }
    return SYS_ERR_OK;
}

struct send_queue {
    unsigned char type;
    unsigned char id;
    struct lmp_chan *chan;
    struct capref cap;
    size_t index;
    size_t size;
    unsigned int *payload;
    struct event_closure callback_when_done;
};

static int rpc_type_min_id[256];
static int rpc_type_max_id[256];
struct generic_queue_obj rpc_send_queue;

// TODO: proper error handling
static errval_t send_loop(void *args)
{
    struct send_queue *sq = (struct send_queue *) args;
    struct capref cap;
    size_t remaining = (sq->size - sq->index);
    int first_byte = (sq->type << 24) + (sq->id << 16) + sq->size;
    if (sq->index == 0) {
        cap = sq->cap;
    } else {
        cap = NULL_CAP;
    }

    errval_t err =
        actual_sending(sq->chan, cap, first_byte,
                       remaining > 8 ? 8 : remaining, &sq->payload[sq->index]);
    if (err_is_fail(err)) {
        // Reregister if failed.
        CHECK(lmp_chan_register_send(sq->chan, get_default_waitset(),
                                     MKCLOSURE((void *) send_loop, args)));
        CHECK(event_dispatch(get_default_waitset()));
    } else {
        if (remaining > 8) {
            // we still have data to send, so we adjust and resend
            sq->index += 8;
            CHECK(lmp_chan_register_send(sq->chan, get_default_waitset(),
                                         MKCLOSURE((void *) send_loop, args)));
            CHECK(event_dispatch(get_default_waitset()));
        } else {
            if (sq->callback_when_done.handler != NULL)
                sq->callback_when_done.handler(sq->callback_when_done.arg);

            bool done = true;
            synchronized(rpc_send_queue.thread_mutex)
            {
                assert(rpc_send_queue.fst->data == args);

                struct generic_queue *temp =
                    rpc_send_queue.fst; // dequeue the one we just got done
                                        // with and delete it
                rpc_send_queue.fst = rpc_send_queue.fst->next;

                if (rpc_type_min_id[sq->type] == sq->id) {
                    // TODO: make proper
                    rpc_type_min_id[sq->type] =
                        (rpc_type_min_id[sq->type] + 1) % 256;
                }

                free(temp->data);
                free(temp);

                if (rpc_send_queue.fst != NULL) // more to be ran
                    done = false;
                else
                    rpc_send_queue.last = NULL;
            }
            if (!done) {
                CHECK(lmp_chan_register_send(
                    ((struct send_queue *) rpc_send_queue.fst->data)->chan,
                    get_default_waitset(),
                    MKCLOSURE((void *) send_loop, rpc_send_queue.fst->data)));
                CHECK(event_dispatch(get_default_waitset()));
            }
            return SYS_ERR_OK;
        }
    }
    return SYS_ERR_OK;
}

// TODO: consider using a bitfield instead of this fairly brittle thing
unsigned char request_fresh_id(unsigned char type)
{
    unsigned char id;
    synchronized(rpc_send_queue.thread_mutex)
    {
        unsigned char min_id = rpc_type_min_id[type];
        unsigned char max_id = rpc_type_max_id[type];
        if ((max_id + 1) % 256 == min_id)
            USER_PANIC("we ran out of message slots\n");
        // TODO: turn this into a proper error msg that gets returned and is
        // expected to be handled by the caller.

        rpc_type_max_id[type] = (max_id + 1) % 256;
        id = max_id;
    }
    return id;
}

errval_t send(struct lmp_chan *chan, struct capref cap, unsigned char type,
              size_t payloadsize, uintptr_t *payload,
              struct event_closure callback_when_done, unsigned char id)
{
    // obtain fresh ID for type then enqueue message into sending loop
    assert(payloadsize < 65536); // Needed so we can encode the size in 16 bits

    bool need_to_start = false;
    DBG(-1, "Sending message with: type 0x%x id %u\n", type >> 1, id);
    synchronized(rpc_send_queue.thread_mutex)
    {
        struct send_queue *sq = malloc(sizeof(struct send_queue));

        sq->type = type;
        sq->id = id;
        sq->index = 0;
        sq->size = payloadsize;
        sq->payload = payload;
        sq->cap = cap;
        sq->chan = chan;
        sq->callback_when_done = callback_when_done;

        struct generic_queue *new = malloc(sizeof(struct generic_queue));

        new->data = (void *) sq;
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
    if (need_to_start) {
        CHECK(lmp_chan_register_send(
            ((struct send_queue *) rpc_send_queue.fst->data)->chan,
            get_default_waitset(),
            MKCLOSURE((void *) send_loop, rpc_send_queue.fst->data)));
        CHECK(event_dispatch(get_default_waitset()));
    }
    debug_printf("Response has been sent\n");
    return SYS_ERR_OK;
}

static struct recv_list *rpc_recv_lookup(struct recv_list *head,
                                         unsigned char type, unsigned char id)
{
    while (head != NULL) {
        if (head->type == type && head->id == id)
            return head;
        head = head->next;
    }
    return NULL;
}

static void rpc_recv_list_remove(struct recv_list **rl_head,
                                 struct recv_list *rl)
{
    assert(rl != NULL);

    struct recv_list *prev = NULL;
    struct recv_list *cur = *rl_head;
    while (true) {
        if (rl == cur)
            break;
        prev = cur;
        cur = cur->next;
    }

    if (prev == NULL)
        *rl_head = cur->next;
    else
        prev->next = cur->next;
}

struct send_cleanup_struct {
    void *data;
    struct event_closure callback;
};

static void send_cleanup(void *data)
{
    struct send_cleanup_struct *scs = (struct send_cleanup_struct *) data;
    free(scs->data);
    if (scs->callback.handler != NULL)
        scs->callback.handler(scs->callback.arg);
    free(data);
}

// this function persists the payload during the call and initiates the
// after-call cleanup
errval_t persist_send_cleanup_wrapper(struct lmp_chan *chan, struct capref cap,
                                      unsigned char type, size_t payloadsize,
                                      void *payload,
                                      struct event_closure callback_when_done,
                                      unsigned char id)
{
    size_t trailing = (payloadsize % 4 != 0 ? 4 - (payloadsize % 4) : 0);
    size_t payloadsize2 = (payloadsize + trailing) / 4;
    uintptr_t *payload2 = malloc(payloadsize2);

    memcpy(payload2, payload, payloadsize);

    if (trailing != 0)
        memset(&((char *) payload2)[payloadsize], 0, trailing);

    struct send_cleanup_struct *scs =
        malloc(sizeof(struct send_cleanup_struct));

    scs->data = payload2;
    scs->callback = callback_when_done;

    struct event_closure callback2 = MKCLOSURE(send_cleanup, scs);

    return send(chan, cap, type, payloadsize2, payload2, callback2, id);
}

// conveniance function for creating responses
errval_t send_response(struct recv_list *rl, struct lmp_chan *chan,
                       struct capref cap, size_t payloadsize, void *payload)
{
    // ACKs should not generate ACKs
    assert((rl->type & 0x1) == 0);

    // add the id to the data
    // round to 32 bit
    // ( we are assuming here that the id is smaller than an int)
    size_t payloadsize2 = payloadsize + 1;
    uintptr_t *payload2 = malloc(payloadsize2 * sizeof(uintptr_t));

    if (payloadsize2 > 1)
        memcpy(&payload2[1], payload, payloadsize * sizeof(uintptr_t));

    payload2[0] = (int) rl->id;
    struct send_cleanup_struct *scs =
        malloc(sizeof(struct send_cleanup_struct));

    scs->data = payload2;
    scs->callback = NULL_EVENT_CLOSURE;

    struct event_closure callback = MKCLOSURE(send_cleanup, scs);

    return send(chan, cap, rl->type + 1, payloadsize2, payload2, callback,
                request_fresh_id(rl->type + 1));
}

static int refill_nono = 0;

// TODO: error handling
void recv_handling(void *args)
{
    debug_printf("recv_handling\n");
    refill_nono++;
    struct recv_chan *rc = (struct recv_chan *) args;
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;
    struct capref cap;

    lmp_chan_recv(rc->chan, &msg, &cap);
    //    if(cap != NULL_CAP) // TODO: figure out if this if check would be a
    //    good idea or not when properly implemented
    //    -> you mean, to ensure that ram RPCs send a cap, for example? not all
    //    rpcs need to send caps.
    //    we should consider freeing the slot again when not used anymore.
    // if we receive an NULL_CAP, delete it again.
    if (capref_is_null(cap)) {
        cap_destroy(cap);
    }

    bool use_prealloc_slot_buff = true;
    lmp_chan_alloc_recv_slot(rc->chan, use_prealloc_slot_buff);
    CHECK(lmp_chan_register_recv(rc->chan, get_default_waitset(),
                           MKCLOSURE(recv_handling, args)));
    slot_alloc_refill_preallocated_slots_conditional(refill_nono);
    debug_printf("after lmp_chan_reg_recv\n");

    assert(msg.buf.msglen > 0);

    unsigned char type = msg.words[0] >> 24;
    unsigned char id = (msg.words[0] >> 16) & 0xFF;
    size_t size = msg.words[0] & 0xFFFF;
    DBG(-1, "Received message with: type 0x%x id %u\n", type >> 1, id);

    if (size < 9) // fast path for small messages
    {
        struct recv_list rl;
        rl.payload = &msg.words[1];
        rl.size = size;
        rl.id = id;
        rl.type = type;
        rl.cap = cap;
        rl.index = size;
        rl.next = NULL;
        rl.chan = rc->chan;
        rc->recv_deal_with_msg(&rl);
    } else {
        struct recv_list *rl = rpc_recv_lookup(rc->rpc_recv_list, type, id);
        if (rl == NULL) {
            rl = malloc(sizeof(struct recv_list));
            rl->payload = malloc(size * 4);
            rl->size = size;
            rl->id = id;
            rl->type = type;
            rl->cap = cap;
            rl->index = 0;
            rl->chan = rc->chan;
            rl->next = rc->rpc_recv_list;
            rc->rpc_recv_list = rl;
        }
        size_t rem = rl->size - rl->index;
        size_t count = (rem > 8 ? 8 : rem);
        memcpy(&rl->payload[rl->index], &msg.words[1], count * 4);
        rl->index += count;
        assert(rl->index <= rl->size);
        if (rl->index == rl->size) {
            // done transferring this msg, we can now do whatever we should do
            // with this
            rpc_recv_list_remove(&rc->rpc_recv_list, rl);
            rc->recv_deal_with_msg(rl);
            free(rl->payload);
            free(rl);
        }
    }
    refill_nono--;
}

errval_t init_rpc_client(void (*recv_deal_with_msg)(struct recv_list *),
                         struct lmp_chan *chan, struct capref dest)
{
    thread_mutex_init(&rpc_send_queue.thread_mutex);
    // Allocate lmp channel structure.
    // Create local endpoint.
    // Set remote endpoint to dest's endpoint.
    CHECK(lmp_chan_accept(chan, DEFAULT_LMP_BUF_WORDS, dest));
    bool use_prealloc_slot_buff = false;
    lmp_chan_alloc_recv_slot(chan, use_prealloc_slot_buff);
    struct recv_chan *rc = malloc(sizeof(struct recv_chan));
    rc->chan = chan;
    rc->recv_deal_with_msg = recv_deal_with_msg;
    rc->rpc_recv_list = NULL;

    lmp_chan_register_recv(rc->chan, get_default_waitset(),
                           MKCLOSURE(recv_handling, rc));
    debug_printf("hi\n");
    return SYS_ERR_OK;
}

errval_t init_rpc_server(void (*recv_deal_with_msg)(struct recv_list *),
                         struct lmp_chan *chan)
{
    thread_mutex_init(&rpc_send_queue.thread_mutex);

    CHECK(lmp_chan_accept(chan, DEFAULT_LMP_BUF_WORDS, NULL_CAP));
    bool use_prealloc_slot_buff = false;
    CHECK(lmp_chan_alloc_recv_slot(chan, use_prealloc_slot_buff));
    CHECK(cap_copy(cap_initep, chan->local_cap));
    struct recv_chan *rc = malloc(sizeof(struct recv_chan));
    rc->chan = chan;
    rc->recv_deal_with_msg = recv_deal_with_msg;
    rc->rpc_recv_list = NULL;

    lmp_chan_register_recv(rc->chan, get_default_waitset(),
                           MKCLOSURE(recv_handling, rc));
    return SYS_ERR_OK;
}

void convert_charptr_to_uintptr_with_padding_and_copy(const char *in,
                                                      size_t charsize,
                                                      uintptr_t **out,
                                                      size_t *outsize)
{
    size_t trailing = (charsize % 4 != 0 ? 4 - (charsize % 4) : 0);
    *outsize = (charsize + trailing) / 4;
    *out = malloc((*outsize) * sizeof(uintptr_t));

    memcpy(*out, in, charsize);

    if (trailing != 0)
        memset(&((char *) *out)[charsize], 0, trailing);
}
