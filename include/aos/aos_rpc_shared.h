
#ifndef AOS_RPC_SHARED_H
#define AOS_RPC_SHARED_H

#include <aos/aos.h>


//while it is theoretically possible to overwrite the channel handlers for receiving/sending messages,
//this introduces a lot of implementation complexity as we now have to ensure that we still handle every message correctly,
//despite an arbitrary interleaving from different threads sending messages.
//another challenge is that certain logical messages are larger than is transferable with a single RPC call
//and ontop of all of that, we only get about 9 32bit sections per message so we need a scheme with little metadata
//otherwise overhead dominates
//so the way we are going to do this is:
//1 recv, 1 send handler. Both are buffered.
//transfer format: 8 bit type, 8 bit unique ID, 16 bits length, 8*32bit payload
//If we use the lowest bit of the type as send/ack distinguishing, then that leaves us with 128 different messages
//and each message type can have up to 256 messages in queue
//overhead of this scheme is only ~11%, it could be halved (for large messages) if one cared to bitwrangle but I didn't feel like it
//further, we'll construct the illusion of larger data transfers ontop of this all via the length param
//tbh length could be send only once and the then free bits used for transfer
//also we could change length encoding so that messages aren't limited to 256kb max size, but it was easier this way and 256kb needs 8192 syscalls anyway, so maybe rethink what you are doing if you hit that limit


// the message type is structured as follows:
// Bits 7-2    Message type
// Bit  0       ACK if set

#define RPC_MESSAGE(message_type) (message_type << 1)
#define RPC_ACK_MESSAGE(message_type) ((message_type << 1 ) + 0x1)
#define RPC_TYPE_NUMBER             0x2
#define RPC_TYPE_STRING             0x3
#define RPC_TYPE_STRING_DATA        0x4
#define RPC_TYPE_RAM                0x5
#define RPC_TYPE_PUTCHAR            0x6
#define RPC_TYPE_HANDSHAKE          0x7
#define RPC_TYPE_PROCESS_SPAWN      0x8
#define RPC_TYPE_PROCESS_GET_NAME   0x9
#define RPC_TYPE_PROCESS_GET_PIDS   0x10
#define RPC_TYPE_PROCESS_KILL_ME    0x11

struct recv_list {
    unsigned char type;
    unsigned char id;
    struct capref cap;
    size_t index;
    size_t size;
    unsigned int* payload;
    struct recv_list* next;
};

struct recv_chan {
    struct lmp_chan *chan;
    void (*recv_deal_with_msg)(struct recv_list *);
    struct recv_list* rpc_recv_list; //todo: instead of having 1 list which shares type+id, split this off into a list of types which each has a list of ids, thereby drastically speeding up lookup
};

errval_t send(struct lmp_chan * chan, struct capref cap, unsigned char type, size_t payloadsize, int* payload, struct event_closure callback_when_done);
errval_t send_chars(struct lmp_chan * chan, struct capref cap, unsigned char type, size_t payloadsize, char* payload, struct event_closure callback_when_done);

#define NULL_EVENT_CLOSURE (struct event_closure){ NULL, NULL }


//possibly does not belong into shared. Inits mutex, opens the channel to dest and sets recv.
        errval_t init_rpc_client(void (*recv_deal_with_msg)(struct recv_list *), struct lmp_chan* chan, struct capref dest);

//Inits mutex and sets recv and all that other stuff. Probably shouldn't be in shared.
        errval_t init_rpc_server(void (*recv_deal_with_msg)(struct recv_list *), struct lmp_chan* chan);

#endif //AOS_RPC_SHARED_H
