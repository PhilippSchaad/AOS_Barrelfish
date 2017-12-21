#ifndef NAMESERVER_INTERNAL_H
#define NAMESERVER_INTERNAL_H

#include <nameserver.h>

#define RPC_MESSAGE(message_type)       (message_type << 1)
#define RPC_ACK_MESSAGE(message_type)   ((message_type << 1) + 0x1)
#define NS_RPC_TYPE_HANDSHAKE           0x1
#define NS_RPC_TYPE_REMOVE_SELF         0x2
#define NS_RPC_TYPE_REGISTER_SERVICE    0x3
#define NS_RPC_TYPE_DEREGISTER_SERVICE  0x4
#define NS_RPC_TYPE_LOOKUP              0x5
#define NS_RPC_TYPE_ENUMERATE           0x6
#define NS_RPC_TYPE_ENUMERATE_COMPLEX   0x7
#define NS_RPC_TYPE_DEBUG_DUMP          0x8

char* serialize_nameserver_prop(struct nameserver_properties* ns_p);
errval_t deserialize_nameserver_prop(char* input, struct nameserver_properties** ns_p_out, int* str_consumed);
char* serialize_nameserver_query(struct nameserver_query* ns_q, size_t *serlen);
errval_t deserialize_nameserver_query(char* input, struct nameserver_query** ns_q_out);
//does not serialize or deserialize the caps ofc
char* serialize_nameserver_info(struct nameserver_info* ns_i);
errval_t deserialize_nameserver_info(char* input, struct nameserver_info** ns_i_out);

void set_ns_cap(struct capref cap); //allows shortcircuiting the setup process a bit if we already have the nameserver cap
void ns_debug_dump(void);

#endif //NAMESERVER_INTERNAL_H
