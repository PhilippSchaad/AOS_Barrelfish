#ifndef _USR_NETWORK_ICMP_H_
#define _USR_NETWORK_ICMP_H_

#include <aos/aos.h>
#include <stdlib.h>

// supported protocols
#define ECHO_REPLY 0
#define ECHO_REQUEST 8

#define ICMP_HEADER_SIZE 8

struct icmp_header{
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint32_t rest_of_header;
} __attribute__((packed));

void icmp_receive(uint8_t* payload, size_t size, uint32_t src);
void icmp_send(uint8_t type, uint8_t code, uint8_t* payload, size_t payload_size, uint32_t dst,  uint32_t rest_of_header);

#endif
