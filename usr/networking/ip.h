#ifndef _USR_NETWORK_IP_H_
#define _USR_NETWORK_IP_H_

#include <aos/aos.h>
#include <stdlib.h>

// Supported protocols
#define ICMP 1


#define VERSION_MASK = 0xf0
#define IHL_MASK = 0x0f
#define IP_MAX_SIZE 65535

// IPv4
struct ip_header {
    uint8_t version_ihl;
    uint8_t tos;
    uint16_t length;
    uint16_t id;
    uint16_t flags_fragmentoffset;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t header_checksum;
    uint32_t source;
    uint32_t destination;
    uint32_t options[10];
} (__attribute__((packed));

union ip_packet{
    ip_header header;
    uint8_t payload[IP_MAX_SIZE];
}

// TODO define flags


void ip_handle_packet(union ip_packet* packet);

#endif
