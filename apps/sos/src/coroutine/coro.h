#include <stdint.h>
#include "comm/list.h"
#include "comm/comm.h"
#include "setjmp.h"

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
#define STACK_SIZE (4 * 4096) // 16K
#define MAX_COROUTINE_NUM (10)
typedef void (*coroutine_func)(void * argv);
struct coroutine
{
    struct context _ctx;
    void*          _stack_top;
    void*          _stack_addr;
    uint32_t       _stack_sz;
    coroutine_func _entry;
    void*          _argv;
    int            _status;
    struct list_head _link;
};

struct schedule
{
    struct list* _pending_list; // coroutine in ready status

    struct list* _block_list; // SUSPEND, maybe wait for io...
    struct coroutine* _running;

    struct coroutine* _daemon;
};


void bootstrap_coro_env();
void shutdown_coro_env();
void schedule_coro();
void schedule_loop();

struct coroutine* create_coro(coroutine_func func, void* argv);
void destroy_coro(struct coroutine* coro);

void yield_coro(void);
void resume_coro(struct coroutine* coro);

void restart_coro(struct coroutine* coro,  coroutine_func func, void* argv);
