#ifndef _USR_NETWORK_IP_H_
#define _USR_NETWORK_IP_H_

#include <aos/aos.h>
#include <stdlib.h>

// Supported protocols
#define PROTOCOL_ICMP 1


#define VERSION_MASK 0xf0
#define IHL_MASK 0x0f
#define IP_MAX_SIZE 65535
#define IP_HEADER_MIN_SIZE 0x5
#define MY_IP (10<<24) + (0<<16) + (3<<8) + 1

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
} __attribute__((packed));

union ip_packet{
    struct ip_header header;
    uint8_t payload[IP_MAX_SIZE];
};

// TODO define flags


void ip_handle_packet(union ip_packet* packet);

void ip_packet_send(uint8_t *payload, size_t payload_size, uint32_t dst, uint8_t protocol);

#endif
