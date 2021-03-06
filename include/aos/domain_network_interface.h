#ifndef _USR_NETWORK_DOMAIN_INTERFACE_H_
#define _USR_NETWORK_DOMAIN_INTERFACE_H_

#include <aos/aos.h>
#include <stdlib.h>

// Supported protocols
#define PROTOCOL_ICMP 1
#define PROTOCOL_UDP 17

// TODO: set this dynamically (maybe on startup of the network?)
//#define MY_IP (10<<24) + (0<<16) + (3<<8) + 1

// TODO: change this
#define MAX_PACKET_PAYLOAD 6300

// message types that can be sent to the network driver
typedef enum network_control_message_type {
    NETWORK_REGISTER_PORT,
    NETWORK_DEREGISTER_PORT,
    NETWORK_TRANSFER_MESSAGE,
    NETWORK_PRINT_MESSAGE,
} network_control_message_t;

errval_t network_register_port(uint16_t port, uint16_t protocol, domainid_t network_pid, coreid_t network_core);
errval_t network_deregister_port(uint16_t port, uint16_t protocol, domainid_t network_pid, coreid_t network_core);
errval_t network_message_transfer(uint16_t from_port, uint16_t to_port, uint32_t from, uint32_t to, uint16_t protocol, uint8_t* payload, size_t size, domainid_t network_pid, coreid_t network_core);


// IMPORTANT: all message types need the first part to be the message type
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
    uint8_t payload[MAX_PACKET_PAYLOAD];
};

struct network_print_message{
    network_control_message_t message_type;
    size_t payload_size;
    char payload[200];
};
#endif
