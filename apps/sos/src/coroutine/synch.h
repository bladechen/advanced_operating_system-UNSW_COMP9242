#ifndef _SYNCH_H_
#define _SYNCH_H_
#include "coro.h"


//XXX act as stack, maybe very unfair, but i am too lazy to change it...
struct chan_obj
{
    struct chan_obj*  _next;
    struct coroutine* _coro;
};

struct wchan
{
    int _size;
    struct chan_obj* _head;
};

struct semaphore
{
    const char* _sem_name; // debug only
    volatile int _sem_count;
    struct wchan* _sem_wchan;

    int _max_count;
};

struct semaphore* sem_create(const char* name, int count, int maximum);

void sem_destroy(struct semaphore* );
void P(struct semaphore*);
void V(struct semaphore*);

#endif
