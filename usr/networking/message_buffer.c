
#include "message_buffer.h"

void net_msg_buf_init(struct net_msg_buf *buf){
    buf->start = buf->buf;
    buf->end = buf->buf;
}
#undef DEBUG_LEVEL
#define DEBUG_LEVEL DETAILED
errval_t net_msg_buf_write(struct net_msg_buf *buf, uint8_t *src, size_t len){
    errval_t err;
    // check if we reach the end of the buffer
    if(buf->start + len > buf->buf + (NET_BUF_SIZE-1)){
        // should wrap around
        // recurse with new parameters
        size_t size_to_write = 1 + buf->buf + (NET_BUF_SIZE-1) - buf->start;
        err = net_msg_buf_write(buf, src, size_to_write);
        if (err_is_fail(err)){
            return err;
        }
        err = net_msg_buf_write(buf, src + size_to_write, len - size_to_write);
        return err;
    }

    // make sure that there is enought space in the buffer
    if (buf->start < buf->end && buf->start + len > buf->end){
        return AOS_NET_ERR_MESSAGE_BUF_OVERFLOW;
    }

    // finally, write the buffer
    memcpy(buf->start, src, len);

    // book keeping
    buf->start += len;

    // some asserts
    assert(buf->start <= buf->buf + NET_BUF_SIZE);

    // if we reached the end of the buffer, go to the beginning again
    if (buf->start == buf->buf + NET_BUF_SIZE){
        buf->start = buf->buf;
    }
    assert(buf->end != buf->start);

    return SYS_ERR_OK;
}
bool net_msg_buf_read_byte(struct net_msg_buf *buf, uint8_t *retval){
    // check if there is something to read
    if (buf->start == buf->end){
        // there is nothing to read
        return false;
    }
    *retval = *buf->end;

    // book keeping
    if (buf->end == buf->buf + NET_BUF_SIZE){
        buf->end = buf->buf;
    } else {
        buf->end++;
    }
    assert(buf->end <= buf->buf + NET_BUF_SIZE);
    return true;
}
size_t net_msg_buf_length(struct net_msg_buf *buf){
    if(buf->end > buf->start){
        return (buf->start - buf->end) + NET_BUF_SIZE;
    } else {
        return buf->end - buf->start;
    }
}
