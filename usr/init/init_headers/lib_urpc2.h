#ifndef LIB_URPC2_H
#define LIB_URPC2_H
//in theory the way this stuff was written below means that if size_in_bytes is within 63 of the max value, it all breaks. So todo: catch that case
struct urpc2_data {
    char type;
    char id;
    uint64_t size_in_bytes;
    uint64_t index;
    void* data;
};
void urpc2_init_and_run(void* sendbuffer, void* receivebuffer, void (*recv_handler)(struct urpc2_data*));
void urpc2_enqueue(struct urpc2_data (*func)(void *data), void *data);
struct urpc2_data init_urpc2_data(char type, char id, size_t size_in_bytes, void* data);

#endif //LIB_URPC2_H
