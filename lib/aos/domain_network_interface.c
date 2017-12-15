#include <aos/domain_network_interface.h>
#include <aos/aos_rpc.h>

errval_t network_register_port(uint16_t port, uint16_t protocol, uint32_t pid, uint32_t core,  errval_t (*callback)(uint8_t* payload, size_t size, uint32_t src, uint16_t source_port, uint16_t dest_port)){

    uint32_t payload = 88;
    aos_rpc_send_message_to_process(aos_rpc_get_init_channel(), pid, core, &payload, 4);

    /*
    // add a piece of shared memory (size is the size of two packet)
    size_t memsize = 0;
    switch(protocol){
        case PROTOCOL_UDP:
            memsize = sizeof(struct udp_datagram);
            break;
        default:
            printf("Error: Protocol %d not supported.", protocol);
            return AOS_NET_ERR_PROTOCOL_NOT_SUPPORTED;
    }


    // allocate the memory for sending and receiving packets
    uint8_t* packet_send_rcv_mem = malloc(memsize);

    // get the actual address of the memory
    default_paging_state()

    // tell the network process the address of the shared memory
    // TODO: make sure that there is only one network driver
*/
    return SYS_ERR_OK;
}
