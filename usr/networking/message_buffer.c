
#include "message_buffer.h"

static struct thread_mutex mutex;

void net_msg_buf_init(struct net_msg_buf *buf){
    buf->start = buf->buf;
    buf->end = buf->buf;
    thread_mutex_init(&mutex);
}

static inline void write_byte(struct net_msg_buf *buf, uint8_t *src){
    // check if full
    if(buf->start+1 == buf->end){
        // drop byte
        return;
    }

    // write buffer
    *buf->start = *src;

    // book keeping
    buf->start += 1;

    // wrap around if needed
    if(buf->buf + NET_BUF_SIZE <= buf->start){
        buf->start = buf->buf;
    }
}

#undef DEBUG_LEVEL
#define DEBUG_LEVEL DETAILED
errval_t net_msg_buf_write(struct net_msg_buf *buf, uint8_t *src, size_t len){
    thread_mutex_lock(&mutex);
    for (int i=0; i<len; ++i){
        write_byte(buf, src+i);
    }
    thread_mutex_unlock(&mutex);
    //debug_printf("%s, |%d|%d|%d| %d (%d bytes written)\n", __func__ , buf->start - buf->buf, buf->end - buf->buf, NET_BUF_SIZE, *src, len);
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
    thread_mutex_lock(&mutex);
    if (buf->end+1 == buf->buf + NET_BUF_SIZE){
        buf->end = buf->buf;
    } else {
        buf->end++;
    }
    assert(buf->end <= buf->buf + NET_BUF_SIZE);
    thread_mutex_unlock(&mutex);

    //debug_printf("%s, |%d|%d|%d| %d\n", __func__, buf->start - buf->buf, buf->end - buf->buf, NET_BUF_SIZE, *retval);
    return true;
}
size_t net_msg_buf_length(struct net_msg_buf *buf){
    if(buf->end > buf->start){
        return (buf->start - buf->end) + NET_BUF_SIZE;
    } else {
        return buf->end - buf->start;
    }
}
