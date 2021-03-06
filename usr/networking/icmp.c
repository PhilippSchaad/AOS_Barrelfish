#include "icmp.h"
#include <netutil/checksum.h>
#include "ip.h"
#include <aos/domain_network_interface.h>

void icmp_send(uint8_t type, uint8_t code, uint8_t* payload, size_t payload_size, uint32_t dst,  uint32_t rest_of_header){
    // create packet
    uint8_t* packet = malloc(payload_size + ICMP_HEADER_SIZE);

    // set the payload
    if(payload && payload_size > 0){
        memcpy(packet + ICMP_HEADER_SIZE, payload, payload_size);
        //free(payload);
    }

    // create the header
    struct icmp_header* header = (struct icmp_header*) packet;
    header->type = type;
    header->code = code;
    header->checksum = 0;
    header->rest_of_header = rest_of_header;
    header->checksum = inet_checksum(packet, payload_size + ICMP_HEADER_SIZE);

    // send
    ip_packet_send(packet, payload_size + ICMP_HEADER_SIZE, dst, PROTOCOL_ICMP);
    free(packet);
}

void icmp_receive(uint8_t* payload, size_t size, uint32_t src){
    // TODO: checksum
    //debug_printf("received new icmp packet\n");
    struct icmp_header* header = (struct icmp_header*) payload;

    switch(header->type){
        case ECHO_REQUEST:
            icmp_send(ECHO_REPLY, 0, payload + ICMP_HEADER_SIZE, size - ICMP_HEADER_SIZE, src, header->rest_of_header);
            break;
        default:
            printf("received an icmp packet that is currently not supported: type: %d \n", header->type);
    }
}
