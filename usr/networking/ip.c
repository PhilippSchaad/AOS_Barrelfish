#include "ip.h"
#include <netutil/checksum.h>
#include <netutil/htons.h>
#include "slip.h"
#include "icmp.h"
#include "udp.h"

#define IP_PACKET_CHECK(boolval, failtext) \
    if (boolval){ \
        printf(failtext); \
        return; \
    }
void ip_handle_packet(union ip_packet* packet){
    // TODO: add check if the payload size is really as we expect
    // TODO: do all the checks (version and so on...)


    // get the payload size
    size_t payload_header_length = (packet->header.version_ihl & IHL_MASK) * 4;
    size_t payload_size = ntohs(packet->header.length) - payload_header_length;
    uint8_t *payload = packet->payload + payload_header_length;
    //debug_printf("header length is end %d, initial: %d we take away: %d\n", payload_size,ntohs(packet->header.length), (packet->header.version_ihl & IHL_MASK) * 4);
    uint32_t src = ntohl(packet->header.source);


    // crc check
    IP_PACKET_CHECK(inet_checksum(packet->payload, payload_header_length) != 0, "drop faulty packet.\n");

    // check that we are using ipv4
    IP_PACKET_CHECK((VERSION_MASK & packet->header.version_ihl) != 0x40, "We currently only support IPv4. drop\n");

    // check that the length makes sense
    IP_PACKET_CHECK(ntohs(packet->header.length) < payload_header_length, "packet is too small, drop\n");
    IP_PACKET_CHECK(payload_header_length < IP_HEADER_MIN_SIZE*4, "packet header is too small, drop\n");

    // detect fragmentation (fragment offset of flag set)
    IP_PACKET_CHECK((packet->header.flags_fragmentoffset & 0xbf) != 0, "Fragmentation is not supported, drop")

    // handle according to protocol
    switch(packet->header.protocol){
        case PROTOCOL_ICMP:
            icmp_receive(payload, payload_size, src);
            break;
        case PROTOCOL_UDP:
            udp_receive(payload, payload_size, src);
            break;
        default:
            printf("received not supported protocol. drop. protocol was %d\n", packet->header.protocol);
            // TODO: error: non supported protocol. drop
            free(packet);
    }
}

void ip_packet_send(uint8_t *payload, size_t payload_size, uint32_t dst, uint8_t protocol){
    debug_printf("send new packet\n");
    // create a new ip packet
    union ip_packet *packet = malloc(sizeof(union ip_packet));

    // assemble the header
    // TODO: support options?
    packet->header.version_ihl = (4<<4) + IP_HEADER_MIN_SIZE;
    packet->header.tos = 0;
    packet->header.length = htons(IP_HEADER_MIN_SIZE*4 + payload_size);
    packet->header.id = 0;
    packet->header.flags_fragmentoffset = 0;
    packet->header.ttl = 64;
    packet->header.protocol = protocol;
    packet->header.source = htonl(MY_IP);
    packet->header.destination = htonl(dst);
    packet->header.header_checksum = inet_checksum(packet, IP_HEADER_MIN_SIZE*4);

    debug_printf("ip send packet with version ihl: %u and length: %u\n", packet->header.version_ihl, IP_HEADER_MIN_SIZE*4 + payload_size);

    // add the payload
    memcpy(packet->payload + IP_HEADER_MIN_SIZE*4, payload, payload_size);
    free(payload);
    payload = NULL;

    slip_packet_send(packet);
}
