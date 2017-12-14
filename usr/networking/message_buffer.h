/**
 * \file
 * \brief create a message buffer (fifo)
 */

#ifndef _USR_NETWORKING_UTIL_H_
#define _USR_NETWORKING_UTIL_H_

#include <aos/aos.h>
#include <stdlib.h>

#define NET_BUF_SIZE 200

struct net_msg_buf {
    uint8_t buf[NET_BUF_SIZE];
    uint8_t* start;
    uint8_t* end;
};

void net_msg_buf_init(struct net_msg_buf *buf);
errval_t net_msg_buf_write(struct net_msg_buf *buf, uint8_t *src, size_t len);
bool net_msg_buf_read_byte(struct net_msg_buf *buf, uint8_t *retval);
size_t net_msg_buf_length(struct net_msg_buf *buf);

#endif
