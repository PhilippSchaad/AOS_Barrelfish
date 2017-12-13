#include "slip.h"
#include "ip.h"
#include <netutil/user_serial.h>
#include <netutil/htons.h>

union ip_packet* ipp;
uint8_t* fill_pointer;

static void ip_add_byte(uint8_t byte){
    // TODO: error handling: what happens when a too long packet is sent?

    // check if we are starting a new packet
    if(!ipp){
        ipp = malloc(sizeof(union ip_packet));
        fill_pointer = ipp->payload;
    }

    // check if the packet gets to large, drop if needed
    if (ipp->payload + IP_MAX_SIZE <= fill_pointer){
        debug_printf("slip: error, message got too large, drop it");
        free(ipp);
        ipp = NULL;
        fill_pointer = NULL;
        return;
    }

    // add a byte to the packet
    *(fill_pointer++) = byte;
}

// send packet to the next layer
static void finish_ip_packet(void){
    debug_printf("received new ip packet\n");
    ip_handle_packet(ipp);

    free(ipp);
    ipp = NULL;
}

/**
 * This function is ment to be called as a thread
 */
static void slip_receive(struct net_msg_buf* buf) {
    // TODO: change type of buf
    uint8_t byte;
    bool escape_next = false;
    while(true){
        while(!net_msg_buf_read_byte(buf, &byte)){
            thread_yield();
        }
        //debug_printf("I read %#04x,\n", byte);

        // should the next byte be escaped?
        if(escape_next){
            escape_next = false;
            switch (byte) {
                case SLIP_ESC_END:
                    ip_add_byte(SLIP_END);
                    break;
                case SLIP_ESC_ESC:
                    ip_add_byte(SLIP_ESC);
                    break;
                case SLIP_ESC_NUL:
                    ip_add_byte(0x0);
                    break;
                default:
                    debug_printf("we expected an escaped character but got %d, this should obviously not happen\n", byte);
                    ip_add_byte(byte);
                    break;
            }
        } else {
            switch (byte) {
                case SLIP_END:
                    finish_ip_packet();
                    break;
                case SLIP_ESC:
                    escape_next = true;
                    break;
                default:
                    ip_add_byte(byte);
                    break;
            }
        }
    }
}

static union ip_packet* packet_to_send = NULL;

void slip_packet_send(union ip_packet* packet){
    packet_to_send = packet;
    while(packet_to_send){
        thread_yield();
    }
}

__attribute__((unused))
// should get called async
static void slip_send(void){
    while(true){
    while(!packet_to_send){
        thread_yield();
    }

    uint8_t* end = packet_to_send->payload + ntohs(packet_to_send->header.length);

    uint8_t slip_end = SLIP_END;
    uint8_t slip_esc = SLIP_ESC;
    uint8_t slip_esc_end = SLIP_ESC_END;
    uint8_t slip_esc_esc = SLIP_ESC_ESC;
    uint8_t slip_esc_nul = SLIP_ESC_NUL;

    for (uint8_t* current_byte = packet_to_send->payload; current_byte < end; ++current_byte){
        switch(*current_byte){
            case SLIP_END:
                serial_write(&slip_esc, 1);
                serial_write(&slip_esc_end, 1);
                break;
            case SLIP_ESC:
                serial_write(&slip_esc, 1);
                serial_write(&slip_esc_esc, 1);
                break;

            case 0x0:
                serial_write(&slip_esc, 1);
                serial_write(&slip_esc_nul, 1);
                break;

            default:
                serial_write(current_byte, 1);
        }
    }
    // end the message
    serial_write(&slip_end, 1);

    free(packet_to_send);
    packet_to_send = NULL;
    }
}

void slip_init(struct net_msg_buf *message_buffer){
    thread_create((thread_func_t) slip_receive, message_buffer);
    MEMORY_BARRIER;
    thread_create((thread_func_t) slip_send, NULL);
}
