#ifndef _USR_NETWORK_UDP_H_
#define _USR_NETWORK_UDP_H_

#include <aos/aos.h>
#include <stdlib.h>

#define UDP_HEADER_SIZE 8
#define UDP_MAX_PAYLOAD_SIZE 1024
#define UDP_MAX_VALID_PORT 65535

struct udp_header{
    uint16_t source_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed));

struct udp_datagram{
    struct udp_header header;
    uint8_t payload[UDP_MAX_PAYLOAD_SIZE];
};

void udp_receive(uint8_t* payload, size_t size, uint32_t src);
void udp_send(uint16_t source_port, uint16_t dest_port, uint8_t* payload, size_t payload_size, uint32_t dst);
void udp_register_port(uint16_t port, errval_t (*handler)(uint8_t* payload, size_t size, uint32_t src, uint16_t source_port, uint16_t dest_port));
//TODO: we do not verify that the sender of this function really owns the port it tries to deregister.
void udp_deregister_port(uint16_t port);
void udp_init(void);

#endif
