#include "udp.h"
#include <netutil/htons.h>

// sorted linked list
struct udp_port{
    uint16_t portnum;
    errval_t (*handler)(uint8_t* payload, size_t size, uint32_t src, uint16_t source_port, uint16_t dest_port);
    struct udp_port *next;
};

static struct udp_port* port_list_head;
static struct thread_mutex mutex;

static errval_t udp_port_insert_ordered(struct udp_port* port){
    thread_mutex_lock(&mutex);
    struct udp_port* current_element = port_list_head;
    if (current_element == NULL){
        // created the first port
        port_list_head = port;
    } else {
        while(current_element->next != NULL && current_element->next->portnum > port->portnum){
            current_element = current_element->next;
        }
        if(current_element->next != NULL && current_element->next->portnum == port->portnum){
            // error: port already exists
            thread_mutex_unlock(&mutex);
            return AOS_NET_ERR_UDP_PORT_EXISTS;
        }
        port->next = current_element->next;
        current_element->next = port;
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

void udp_receive(uint8_t* payload, size_t size, uint32_t src){
    struct udp_datagram* datagram = (struct udp_datagram*) payload;
    debug_printf("received UDP packet to from port %d to port %d\n", ntohs(datagram->header.source_port), ntohs(datagram->header.dest_port));

    struct udp_port *port = NULL;
    CHECK(search_udp_port(ntohs(datagram->header.dest_port), &port));
    port->handler(datagram->payload, size - UDP_HEADER_SIZE, src, datagram->header.source_port, datagram->header.dest_port);

    free(datagram);
}
void udp_send(uint16_t source_port, uint16_t dest_port, uint8_t* payload, size_t payload_size, uint32_t dst){

}

void udp_register_port(uint16_t portnum, errval_t (*handler)(uint8_t* payload, size_t size, uint32_t src, uint16_t source_port, uint16_t dest_port)){
    struct udp_port* port = malloc(sizeof(struct udp_port));
    port->portnum = portnum;
    port->handler = handler;
    port->next = NULL;
    CHECK(udp_port_insert_ordered(port));
    printf("registered port %u\n", portnum);
}

void udp_deregister_port(uint16_t port){
    CHECK(udp_port_remove(port));
}

void udp_init(void){
    thread_mutex_init(&mutex);
}

