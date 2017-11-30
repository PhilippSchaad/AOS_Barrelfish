#include "slip.h"
#include "ip.h"

union ip_packet* ipp;
uint8_t fill_pointer*;

static void ip_add_byte(uint8_t byte){
    // TODO: error handling: what happens when a too long packet is sent?

    // check if we are starting a new packet
    if(!ipp){
        ipp = malloc(sizeof union ip_packet);
        fill_pointer = ipp;
    }

    // add a byte to the packet
    *(fill_pointer++) = byte;
}

// send packet to the next layer
static void finish_ip_packet(void){
    // TODO: do

    free(ipp);
    ipp = NULL;
}

/**
 * This function is ment to be called as a thread
 */
void slip_receive(struct net_msg_buf* buf) {
    // TODO: change type of buf
    uint8_t byte;
    bool escape_next = false;
    while(true){
        while(!net_msg_buf_read_byte(&message_buffer, &byte)){
            thread_yield();
        }

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
                    debug_printf("slip_decode got SLIP_ESC %x, this should not happen\n");
                    ip_add_byte(r);
                    break;
            } else {
                switch (r) {
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

        // received a new byte, check what we should do with it
        ebug_printf("received %d\n", retval);
    }
}

void slip_send(union ip_packet* packet, size_t size){
    uint8_t* end = packet->payload + size;

    uint8_t slip_end = SLIP_END;
    uint8_t slip_esc = SLIP_ESC;
    uint8_t slip_esc_end = SLIP_ESC_END;
    uint8_t slip_esc_esc = SLIP_ESC_ESC;
    uint8_t slip_esc_nul = SLIP_ESC_NUL;

    for (uint8_t* current_byte = packet->payload; current_byte < end; ++current_byte){
        switch(*current_byte){
            case SLIP_END:
                net_msg_buf_write(buf,slip_esc, 1);
                net_msg_buf_write(buf,slip_esc_end, 1);
                break;
            case SLIP_ESC:
                net_msg_buf_write(buf,slip_esc, 1);
                net_msg_buf_write(buf,slip_esc_esc, 1);
                break;

            case 0x0:
                net_msg_buf_write(buf,slip_esc, 1);
                net_msg_buf_write(buf,slip_esc_nul, 1);
                break;

            default:
                net_msg_buf_write(buf,current_byte, 1);
        }
    }
    // end the message
    net_msg_buf_write(&message_buffer, slip_end, 1);
}




