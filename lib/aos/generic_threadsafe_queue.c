#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <aos/generic_threadsafe_queue.h>

void queue_init(struct generic_queue_obj *obj)
{
    obj->fst = NULL;
    obj->last = NULL;
    thread_mutex_init(&obj->thread_mutex);
}

void enqueue(struct generic_queue_obj *obj, void *data)
{
    synchronized(obj->thread_mutex)
    {
        struct generic_queue *new = malloc(sizeof(struct generic_queue));

        new->data = data;
        new->next = NULL;

        if (obj->last == NULL) {
            assert(obj->fst == NULL);
            obj->fst = new;
            obj->last = new;
        } else {
            obj->last->next = new;
            obj->last = new;
        }
    }
}

void requeue(struct generic_queue_obj *obj, struct generic_queue *sq)
{
    synchronized(obj->thread_mutex)
    {
        if (obj->last == NULL) {
            assert(obj->fst == NULL);
            obj->fst = sq;
            obj->last = sq;
        } else {
            obj->last->next = sq;
            obj->last = sq;
        }
    }
}

bool queue_empty(struct generic_queue_obj *obj)
{
    bool status = false;
    synchronized(obj->thread_mutex) { status = obj->fst == NULL; }
    return status;
}

struct generic_queue *dequeue(struct generic_queue_obj *obj)
{
    struct generic_queue *fst = NULL;
    synchronized(obj->thread_mutex)
    {
        assert(obj->fst != NULL);

        fst = obj->fst;
        obj->fst = fst->next;

        if (obj->fst == NULL)
            obj->last = NULL;
    }
    return fst;
}
