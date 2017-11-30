#ifndef _USR_NETWORK_ICMP_H_
#define _USR_NETWORK_ICMP_H_

// supported protocols
#define ECHO_REPLY 0

struct icmp_header{
    uint8_t typ;
    uint8_t code;
    uint16_t checksum;
} (__attribute__((packed));

void icmp_receive(uint8_t* payload, size_t size);

#endif
