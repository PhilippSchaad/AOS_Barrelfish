
#include "message_buffer.h"

void net_msg_buf_init(struct net_msg_buf *buf){
    buf->start = buf->buf;
    buf->end = buf->buf;
}

static inline void write_byte(struct net_msg_buf *buf, uint8_t *src){
    // write buffer
    *buf->start = *src;

    // book keeping
    buf->start += 1;

    // wrap around if needed
    if(buf->buf + NET_BUF_SIZE <= buf->start){
        buf->start = buf->buf;
    }

    // check if full
    assert(buf->start != buf->end);

}


#undef DEBUG_LEVEL
#define DEBUG_LEVEL DETAILED
errval_t net_msg_buf_write(struct net_msg_buf *buf, uint8_t *src, size_t len){
    for (int i=0; i<len; ++i){
        write_byte(buf, src+i);
    }
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
