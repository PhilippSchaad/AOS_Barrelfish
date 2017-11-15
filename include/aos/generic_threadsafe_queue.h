#ifndef GENERIC_THREADSAFE_QUEUE_H
#define GENERIC_THREADSAFE_QUEUE_H

// XXX: WARNING! USE THIS WITH CAUTION. Preliminary returns will NOT return
// the lock, control flow with 'continue' and 'break' might work differently
// than expected inside of synchronized blocks, and nested synchronized blocks
// can cause massive trouble. Be aware of that when using synchronized here!
#define synchronized(_mut)                                                     \
    for (struct thread_mutex *_mutx = &_mut; _mutx != NULL; _mutx = NULL)     \
        for (thread_mutex_lock_nested(&_mut); _mutx != NULL; _mutx = NULL,           \
                thread_mutex_unlock(&_mut))

struct generic_queue {
    void *data;
    struct generic_queue *next;
};

struct generic_queue_obj {
    struct generic_queue *fst;
    struct generic_queue *last;
    struct thread_mutex thread_mutex;
};

void queue_init(struct generic_queue_obj* obj);
void enqueue(struct generic_queue_obj* obj, void *data);
void requeue(struct generic_queue_obj* obj, struct generic_queue *sq);
bool queue_empty(struct generic_queue_obj* obj);
struct generic_queue *dequeue(struct generic_queue_obj* obj);

#endif //GROUPD_GENERIC_THREADSAFE_QUEUE_H
