#ifndef AOS_RPC_SHARED_H
#define AOS_RPC_SHARED_H

#include <aos/aos.h>

// while it is theoretically possible to overwrite the channel handlers for
// receiving/sending messages, this introduces a lot of implementation
// complexity as we now have to ensure that we still handle every message
// correctly, despite an arbitrary interleaving from different threads sending
// messages. Another challenge is that certain logical messages are larger than
// is transferable with a single RPC call and ontop of all of that, we only get
// about 9 32bit sections per message so we need a scheme with little metadata.
// Otherwise overhead dominates so the way we are going to do this is:
//
// 1 recv, 1 send handler. Both are buffered.
// transfer format: 8 bit type, 8 bit unique ID, 16 bits length, 8*32bit
// payload
//
// If we use the lowest bit of the type as send/ack distinguishing, then that
// leaves us with 128 different messages and each message type can have up to
// 256 messages in queue. Overhead of this scheme is only ~11%, it could be
// halved (for large messages) if one cared to bitwrangle but I didn't feel
// like it. Further, we'll construct the illusion of larger data transfers
// ontop of this all via the length param.
//
// tbh length could be send only once and the then free bits used for transfer
// also we could change length encoding so that messages aren't limited to
// 256kb max size, but it was easier this way and 256kb needs 8192 syscalls
// anyway, so maybe rethink what you are doing if you hit that limit

// the message type is structured as follows:
// Bits 7-2    Message type
// Bit  0       ACK if set

#define RPC_MESSAGE(message_type)       (message_type << 1)
#define RPC_ACK_MESSAGE(message_type)   ((message_type << 1) + 0x1)
#define RPC_TYPE_NUMBER                 0x2
#define RPC_TYPE_STRING                 0x3
#define RPC_TYPE_RUN_TESTSUITE          0x4
#define RPC_TYPE_RAM                    0x5
#define RPC_TYPE_PUTCHAR                0x6
#define RPC_TYPE_GETCHAR                0x7
#define RPC_TYPE_HANDSHAKE              0x8
#define RPC_TYPE_PROCESS_SPAWN          0x9
#define RPC_TYPE_PROCESS_GET_NAME       0x10
#define RPC_TYPE_PROCESS_GET_PIDS       0x11
#define RPC_TYPE_PROCESS_KILL_ME        0x12
#define RPC_TYPE_PROCESS_REGISTER       0x13
#define RPC_TYPE_PROCESS_KILL           0x14
#define RPC_TYPE_IRQ_CAP                0x15
#define RPC_TYPE_DEV_CAP                0x16
#define RPC_TYPE_PRINT_PROC_LIST        0x17
#define RPC_TYPE_LED_TOGGLE             0x18
#define RPC_TYPE_PROCESS_AWAIT_COMPL    0x19
#define RPC_TYPE_GET_MEM_SERVER         0x20
#define RPC_TYPE_REGISTER_AS_NAMESERVER 0x21
#define RPC_TYPE_GET_NAME_SERVER        0x22
#define RPC_TYPE_DOMAIN_TO_DOMAIN_COM   0x23

struct recv_list {
    unsigned char type;
    unsigned char id;
    struct capref cap;
    size_t index;
    size_t size;
    struct lmp_chan *chan;
    uintptr_t *payload;
    struct recv_list *next;
};

struct recv_chan {
    struct lmp_chan *chan;
    void (*recv_deal_with_msg)(struct recv_list *);
    struct recv_list *rpc_recv_list; // TODO: instead of having 1 list which
                                     // shares type+id, split this off into a
                                     // list of types which each has a list of
                                     // ids, thereby drastically speeding up
                                     // lookup
};

unsigned char request_fresh_id(unsigned char type);

// id needs to be a currently unused id obtained via request_fresh_id
errval_t send(struct lmp_chan *chan, struct capref cap, unsigned char type,
              size_t payloadsize, uintptr_t *payload,
              struct event_closure callback_when_done, unsigned char id);
errval_t persist_send_cleanup_wrapper(struct lmp_chan *chan, struct capref cap,
                                      unsigned char type, size_t payloadsize,
                                      void *payload,
                                      struct event_closure callback_when_done,
                                      unsigned char id);
errval_t send_response(struct recv_list *rl, struct lmp_chan *chan,
                       struct capref cap, size_t payloadsize, void *payload);
errval_t forward_message(struct recv_list *rl, struct lmp_chan *chan,
                       struct capref cap, size_t payloadsize, void *payload);

#define NULL_EVENT_CLOSURE                                                    \
    (struct event_closure) { NULL, NULL }

// almost definitely not something that belongs into shared
void recv_handling(void *args);

// TODO: possibly does not belong into shared. Inits mutex, opens the channel
// to dest and sets recv.
errval_t init_rpc_client(void (*recv_deal_with_msg)(struct recv_list *),
                         struct lmp_chan *chan, struct capref dest);

// TODO: Inits mutex and sets recv and all that other stuff. Probably shouldn't
// be in shared.
errval_t init_rpc_server(void (*recv_deal_with_msg)(struct recv_list *),
                         struct lmp_chan *chan);

void convert_charptr_to_uintptr_with_padding_and_copy(const char *in,
                                                      size_t charsize,
                                                      uintptr_t **out,
                                                      size_t *outsize);

#endif /* AOS_RPC_SHARED_H */
