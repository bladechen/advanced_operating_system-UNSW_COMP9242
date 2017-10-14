#ifndef _SYNCH_H_
#define _SYNCH_H_
#include "coro.h"
#include "comm/list.h"


//XXX act as stack, maybe very unfair, but i am too lazy to change it...
struct chan_obj
{
    // struct chan_obj*  _next;
    struct list_head _link_obj;
    struct coroutine* _coro;
};

struct wchan
{
    int _size;
    struct list _head;
    // struct chan_obj* _head;
    // struct chan_obj* _tail;
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


// struct lock
// {
//
// };
struct cv
{
    struct wchan* _cv_wchan;
    // XXX maybe we need lock here.
};



struct cv *cv_create(const char *name);
void cv_destroy(struct cv *);

/*
 * Operations:
 *    cv_wait      - Release the supplied lock, go to sleep, and, after
 *                   waking up again, re-acquire the lock.
 *    cv_signal    - Wake up one thread that's sleeping on this CV.
 *    cv_broadcast - Wake up all threads sleeping on this CV.
 *
 * For all three operations, the current thread must hold the lock passed
 * in. Note that under normal circumstances the same lock should be used
 * on all operations with any particular CV.
 *
 * These operations must be atomic. You get to write them.
 */
void cv_wait(struct cv *cv);
void cv_signal(struct cv *cv);
void cv_broadcast(struct cv *cv);


#endif
