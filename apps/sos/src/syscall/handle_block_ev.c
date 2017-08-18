#include "syscall.h"
#include "comm.h"
#include "coro.h"

void reply_success( seL4_Word cap)
{
    seL4_MessageInfo_t reply = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_SetMR(0, 0);
    seL4_Send(cap, reply);
}

void destroy_reply_cap(seL4_Word cap)
{
    cspace_free_slot(cur_cspace, cap);
}

void cb_block_sleep(uint32_t id, void* data)
{
    restart_coro((struct proc*)(data)->p_coro, handle_block_sleep, NULL);
}

void handle_block_sleep(void* argv)
{
    struct proc* proc = current_running_coro()->_proc;
    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "now wake up proc: %d\n", proc->p_pid);
    reply_success(proc->p_reply_cap);
    destroy_reply_cap(proc->p_reply_cap);
    proc->p_reply_cap = 0;
}
