#ifndef _USR_NETWORK_DOMAIN_INTERFACE_H_
#define _USR_NETWORK_DOMAIN_INTERFACE_H_

#include <aos/aos.h>
#include <stdlib.h>

// message types that can be sent to the network driver
typedef enum network_control_message_type {
    NETWORK_REGISTER_PORT,
    NETWORK_DEREGISTER_PORT,
} network_control_message_t;

errval_t network_register_port(uint16_t port, uint16_t protocol, domainid_t network_pid, coreid_t network_core);



// the following are structs that faciliate the message passing
struct network_register_deregister_port_message{
    network_control_message_t message_type;
    uint16_t protocol;
    domainid_t pid;
    coreid_t core;
};

#endif
