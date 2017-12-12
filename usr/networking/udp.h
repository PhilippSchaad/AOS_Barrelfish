#ifndef _USR_NETWORK_ICMP_H_
#define _USR_NETWORK_ICMP_H_

#include <aos/aos.h>
#include <stdlib.h>

struct udp_header{
    uint16_t source_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed));

void udp_receive(uint8_t* payload, size_t size, uint32_t src);
void udp_send(uint16_t source_port, uint16_t dest_port, uint8_t* payload, size_t payload_size, uint32_t dst);
void udp_register_port(uint16_t port, errval_t (*handler)(uint8_t* payload, size_t size, uint32_t src, uint16_t source_port, uint16_t dest_port));

#endif
