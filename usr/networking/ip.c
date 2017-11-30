#include "ip.h"

void ip_handle_packet(union ip_packet* packet){
    // TODO: add check if the payload size is really as we expect
    // TODO: do all the checks (version and so on...)

    // get the payload size
    size_t payload_size = packet->header->length - packet->header->version_ihl & IHL_MASK;
    uint8_t *payload = packet->payload + payload_size;

    // handle according to protocol
    switch(packet->header->protocol){
        case ICMP:
            //TODO: link
            break;
        default:
            // TODO: error: non supported protocol. drop
    }
}
