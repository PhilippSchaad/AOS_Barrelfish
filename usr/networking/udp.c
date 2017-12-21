#include "udp.h"
#include <netutil/htons.h>
#include "ip.h"
#include <aos/domain_network_interface.h>

// TODO: check that we can add multiple listeners (ports)

// sorted linked list
struct udp_port{
    uint16_t portnum;
    domainid_t pid;
    coreid_t core;
    struct udp_port *next;
};

static struct udp_port* port_list_head;
static struct thread_mutex mutex;

__attribute__((unused))
static void dump_ports(void){
     struct udp_port* port = port_list_head;
     printf("Dump list of registered ports\n");
     while(port != NULL){
        printf("%d: %d\n", port->portnum, port->pid);
        port = port->next;
     }
}

static errval_t udp_port_insert_ordered(struct udp_port* port){
    thread_mutex_lock(&mutex);
    struct udp_port* current_element = port_list_head;
    if (current_element == NULL){
        // created the first port
        port_list_head = port;
    } else {
        while(current_element->next != NULL && current_element->next->portnum < port->portnum){
            current_element = current_element->next;
        }
        if((current_element->next != NULL && current_element->next->portnum == port->portnum)||(current_element->portnum == port->portnum)){
            // error: port already exists
            thread_mutex_unlock(&mutex);
            return AOS_NET_ERR_UDP_PORT_EXISTS;
        }
        else if (current_element->portnum > port->portnum){
            // add before (should only happen at the start of the list)
            assert(port_list_head == current_element);
            port->next = port_list_head;
            port_list_head = port;
        } else {
            port->next = current_element->next;
            current_element->next = port;
        }
    }

    thread_mutex_unlock(&mutex);
    return SYS_ERR_OK;
}
static errval_t udp_port_remove(uint16_t portnum){
    thread_mutex_lock(&mutex);
    struct udp_port* current_element = port_list_head;
    if (current_element == NULL){
        // error, port does not exits
        thread_mutex_unlock(&mutex);
        return AOS_NET_ERR_UDP_NO_SUCH_PORT;
    } else {
        while(current_element->next != NULL && current_element->next->portnum > portnum){
            current_element = current_element->next;
        }
        if(current_element->next != NULL && current_element->next->portnum == portnum){
            struct udp_port* tmp = current_element->next;
            current_element->next = tmp->next;
            free(tmp);
        } else if (current_element->next == NULL && current_element->portnum == portnum){
            // we are at the beginning of the list and there is only one entry left
            // delete last element from list
            port_list_head = NULL;
            free(current_element);
        } else {
            // error: port does not exist
            thread_mutex_unlock(&mutex);
            return AOS_NET_ERR_UDP_NO_SUCH_PORT;
        }
    }
    thread_mutex_unlock(&mutex);
    return SYS_ERR_OK;
}

static errval_t search_udp_port(uint16_t portnum, struct udp_port **port){
    thread_mutex_lock(&mutex);
    struct udp_port* current_element = port_list_head;
    if (current_element == NULL){
        // error, port does not exits
        thread_mutex_unlock(&mutex);
        return AOS_NET_ERR_UDP_NO_SUCH_PORT;
    } else {
        while(current_element->next != NULL && current_element->next->portnum >= portnum){
            current_element = current_element->next;
        }
        if(current_element != NULL && current_element->portnum == portnum){
            *port = current_element;
        } else {
            // error: port does not exist
            thread_mutex_unlock(&mutex);
            return AOS_NET_ERR_UDP_NO_SUCH_PORT;
        }
    }
    thread_mutex_unlock(&mutex);
    return SYS_ERR_OK;
}

void udp_receive(uint8_t* payload, size_t size, uint32_t src, uint32_t dst){
    errval_t err;
    struct udp_datagram* datagram = (struct udp_datagram*) payload;
    //debug_printf("received UDP packet from port %d to port %d\n", ntohs(datagram->header.source_port), ntohs(datagram->header.dest_port));

    struct udp_port *port = NULL;
    err = search_udp_port(ntohs(datagram->header.dest_port), &port);

    //debug_printf("found port %p\n", port);

    if(err_is_fail(err)){
        printf("%s\n",err_getstring(err));
        return;
    }

    // send message to handling process
    err = network_message_transfer(ntohs(datagram->header.source_port), ntohs(datagram->header.dest_port), src, dst, PROTOCOL_UDP, payload + UDP_HEADER_SIZE, size - UDP_HEADER_SIZE, port->pid, port->core);
    //debug_printf("udp receive finished\n");

    //free(payload);
}
void udp_send(uint16_t source_port, uint16_t dest_port, uint8_t* payload, size_t payload_size, uint32_t dst){
    //debug_printf("Sending new udp packet\n");

    //TODO: check if the port matches the one from the sending domain

    struct udp_datagram* datagram = malloc(sizeof(struct udp_datagram));
    datagram->header.source_port = htons(source_port);
    datagram->header.dest_port = htons(dest_port);
    datagram->header.length = htons(payload_size + UDP_HEADER_SIZE);
    // the checksum is not mandatory in ipv4
    datagram->header.checksum = 0;
    memcpy(datagram->payload, payload, payload_size);
    //free(payload);

    ip_packet_send((uint8_t*) datagram, payload_size+UDP_HEADER_SIZE, dst, PROTOCOL_UDP);
    free(datagram);
}

void udp_register_port(uint16_t portnum, domainid_t pid, coreid_t core){
    printf("Open new UDP port %d\n", portnum);
    struct udp_port* port = malloc(sizeof(struct udp_port));
    port->portnum = portnum;
    port->pid = pid;
    port->core = core;
    port->next = NULL;

    errval_t err;
    err = udp_port_insert_ordered(port);
    if(err_is_fail(err)){
        printf("%s\n",err_getstring(err));
        return;
    }
    printf("registered port %u\n", portnum);
    dump_ports();
}
void udp_deregister_port(uint16_t port){
    errval_t err;
    err = udp_port_remove(port);
    if(err_is_fail(err)){
        printf("%s\n",err_getstring(err));
        return;
    }
}

void udp_init(void){
    thread_mutex_init(&mutex);
}

