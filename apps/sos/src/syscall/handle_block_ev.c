#include "syscall.h"
#include "handle_syscall.h"
#include "comm/comm.h"
#include "coroutine/coro.h"
#include "clock/clock.h"
#include "proc/proc.h"
#include "proc/pid.h"
#include <serial/serial.h>
#include "vm/pagetable.h"


// used to save the char read from console
/*  */
static void cb_block_sleep(uint32_t id, void* data)
{
    struct proc* proc = pid_to_proc((int)(data));
    if (proc == NULL ||
        proc->p_status.status != PROC_STATUS_SLEEP)
    {
        ERROR_DEBUG("resume pid: %d failed\n",
                    (int)(data));
        return;
    }
    resume_coro(proc->p_coro);
}

void handle_block_sleep(void* argv)
{
    struct proc* proc = current_running_coro()->_proc;
    int sleep_second = (int)(argv);
    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "now sleep  %d proc: %d now time: %llu\n",sleep_second, proc->p_pid,  time_stamp()/1000);

    register_timer(sleep_second , cb_block_sleep, (void*)proc->p_pid);
    proc->p_status.status = PROC_STATUS_SLEEP;
    yield_coro();
    proc->p_status.status = PROC_STATUS_RUNNING;

    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "now wake up proc: %d, now time: %llu\n", proc->p_pid, time_stamp()/1000);
    struct ipc_buffer_ctrl_msg ctrl;
    ctrl.ret_val = 0;
    ctrl.offset = 0;
    ipc_reply(&ctrl, &(proc->p_context.p_reply_cap));
}

