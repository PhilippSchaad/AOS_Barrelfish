#include "ip.h"
#include <netutil/checksum.h>
#include <netutil/htons.h>
#include "slip.h"
#include "icmp.h"

void ip_handle_packet(union ip_packet* packet){
    // TODO: add check if the payload size is really as we expect
    // TODO: do all the checks (version and so on...)

    // get the payload size
    size_t payload_header_length = (packet->header.version_ihl & IHL_MASK) * 4;
    size_t payload_size = ntohs(packet->header.length) - payload_header_length;
    uint8_t *payload = packet->payload + payload_header_length;
    debug_printf("header length is end %d, initial: %d we take away: %d\n", payload_size,ntohs(packet->header.length), (packet->header.version_ihl & IHL_MASK) * 4);
    uint32_t src = ntohl(packet->header.source);

    // handle according to protocol
    switch(packet->header.protocol){
        case PROTOCOL_ICMP:
            icmp_receive(payload, payload_size, src);
            break;
        default:
            debug_printf("received not supported protocol. drop. protocol was %d\n", packet->header.protocol);
            // TODO: error: non supported protocol. drop
    }
}

void ip_packet_send(uint8_t *payload, size_t payload_size, uint32_t dst, uint8_t protocol){
    debug_printf("send new packet\n");
    // create a new ip packet
    union ip_packet *packet = malloc(sizeof(union ip_packet));

    // assemble the header
    // TODO: support options?
    packet->header.version_ihl = htons((4<<4) + IP_HEADER_MIN_SIZE);
    packet->header.tos = 0;
    packet->header.length = htons(IP_HEADER_MIN_SIZE + payload_size);
    packet->header.id = 0;
    packet->header.flags_fragmentoffset = 0;
    packet->header.ttl = htons(64);
    packet->header.protocol = htons(protocol);
    packet->header.source = htonl(MY_IP);
    packet->header.destination = htonl(dst);
    packet->header.header_checksum = inet_checksum(packet, IP_HEADER_MIN_SIZE);

    // add the payload
    memcpy(packet->payload + IP_HEADER_MIN_SIZE, payload, payload_size);
    free(payload);
    payload = NULL;

    slip_packet_send(packet);
}
