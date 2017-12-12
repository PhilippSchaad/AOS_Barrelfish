#ifndef _USR_NETWORK_SLIP_H_
#define _USR_NETWORK_SLIP_H_

#include <aos/aos.h>
#include <stdlib.h>
#include "ip.h"
#include "message_buffer.h"

#define SLIP_END        0xc0
#define SLIP_ESC        0xdb
#define SLIP_ESC_END    0xdc
#define SLIP_ESC_ESC    0xdd
#define SLIP_ESC_NUL    0xde

void slip_packet_send(union ip_packet* packet);
void slip_init(struct net_msg_buf *message_buffer);

#endif
