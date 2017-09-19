#ifndef _CORO_H_
#define _CORO_H_
#include <stdint.h>
#include "comm/list.h"
#include "comm/comm.h"
#include "setjmp.h"
#include "proc/proc.h"

enum COROUTINE_STATUS
{
    COROUTINE_INIT = 0,
    COROUTINE_READY ,
    COROUTINE_RUNNING ,
    COROUTINE_SUSPEND ,
};

// typedef jmp_buf context;
// typedef unsigned long register_t;
//currently only support x86_64
struct context
{
    jmp_buf _jmp;
    // register_t _rbx;
    // register_t _rsp;
    // register_t _rbp;
    // register_t _r12;
    // register_t _r13;
    // register_t _r14;
    // register_t _r15;
    // register_t _rip;
};

// therefore the total stack size is 16K including the surrounded guards
#define STACK_SIZE (4096 * 3) // 16K
#define STACK_GUARD_SIZE (4096)
#define MAX_COROUTINE_NUM (10)
typedef void (*coroutine_func)(void * argv);
struct coroutine
{
    // bool _exit_flag;
    struct context _ctx;
    void*          _stack_top;
    void*          _stack_addr;
    uint32_t       _stack_sz;
    coroutine_func _entry;
    void*          _argv;
    int            _status;
    struct list_head _link;

    struct proc*   _proc;
};


struct schedule
{
    struct list* _pending_list; // coroutine in ready status

    struct list* _block_list; // SUSPEND, maybe wait for io...
    struct coroutine* _running;

    struct coroutine* _daemon;

    uint32_t _stack_base;
};

struct coroutine* current_running_coro();

static inline struct proc* get_current_proc()
{
    return current_running_coro()->_proc;
}
static inline int coro_status(struct coroutine* coro)
{
    return coro->_status;
}

static inline void coro_stop(struct coroutine* coro)
{
    // XXX some assert?
    assert(coro->_status != COROUTINE_RUNNING);
    assert(coro->_proc != get_current_proc()); // you could not stop your self
    coro->_status = COROUTINE_SUSPEND;
    list_del(&coro->_link);
}

void bootstrap_coro_env();
void set_kproc_coro(struct proc* proc);
void shutdown_coro_env();
// void schedule_coro();
void schedule_loop();

struct coroutine* create_coro(coroutine_func func, void* argv);
void destroy_coro(struct coroutine* coro);

void yield_coro(void);
void resume_coro(struct coroutine* coro);
void make_coro_runnable(struct coroutine* coro);

void restart_coro(struct coroutine* coro,  coroutine_func func, void* argv);




#endif
