#include "syscall.h"
#include "handle_syscall.h"
#include "comm/comm.h"
#include "coroutine/coro.h"
#include "clock/clock.h"



static void cb_block_sleep(uint32_t id, void* data)
{
    resume_coro(((struct proc*)(data))->p_coro);
}

void handle_block_sleep(void* argv)
{
    struct proc* proc = current_running_coro()->_proc;
    int sleep_second = (int)(argv);
    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "now sleep  %d proc: %d\n",sleep_second, proc->p_pid);

    register_timer(sleep_second , cb_block_sleep, proc);
    yield_coro();

    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "now wake up proc: %d\n", proc->p_pid);
    if (proc->p_reply_cap != 0)
    {
        reply_success(proc->p_reply_cap);
        destroy_reply_cap(proc->p_reply_cap);
    }
    proc->p_reply_cap = 0;
}
