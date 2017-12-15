#include <aos/domain_network_interface.h>
#include <aos/aos_rpc.h>

errval_t network_register_port(uint16_t port, uint16_t protocol, domainid_t network_pid, coreid_t network_core)
{
    struct network_register_deregister_port_message message;
    message.message_type = NETWORK_REGISTER_PORT;
    message.protocol = protocol;
    message.pid = 3; // TODO: get this form somewhere
    message.core = disp_get_core_id();

    // TODO: round up size in rpc call
    aos_rpc_send_message_to_process(aos_rpc_get_init_channel(), network_pid, network_core, &message, sizeof(struct network_register_deregister_port_message));

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
