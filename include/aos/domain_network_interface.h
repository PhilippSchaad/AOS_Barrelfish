#ifndef _USR_NETWORK_DOMAIN_INTERFACE_H_
#define _USR_NETWORK_DOMAIN_INTERFACE_H_

#include <aos/aos.h>
#include <stdlib.h>

// Supported protocols
#define PROTOCOL_ICMP 1
#define PROTOCOL_UDP 17

// message types that can be sent to the network driver
typedef enum network_control_message_type {
    NETWORK_REGISTER_PORT,
    NETWORK_DEREGISTER_PORT,
    NETWORK_TRANSFER_MESSAGE,
} network_control_message_t;

errval_t network_register_port(uint16_t port, uint16_t protocol, domainid_t network_pid, coreid_t network_core);
errval_t network_message_transfer(uint16_t from_port, uint16_t to_port, uint32_t from, uint32_t to, uint16_t protocol, uint8_t* payload, size_t size, domainid_t network_pid, coreid_t network_core);



// the following are structs that faciliate the message passing
struct network_register_deregister_port_message{
    network_control_message_t message_type;
    uint16_t protocol;
    domainid_t pid;
    coreid_t core;
    uint16_t port;
};

struct network_message_transfer_message{
    network_control_message_t message_type;
    uint16_t protocol;
    uint16_t port_from;
    uint16_t port_to;
    uint32_t ip_from;
    uint32_t ip_to;
    size_t payload_size;
    uint8_t payload[65535];
};

#endif
