#include "syscall.h"
#include "handle_syscall.h"
#include "comm/comm.h"
#include "coroutine/coro.h"
#include "clock/clock.h"
#include <serial/serial.h>
#include "vm/pagetable.h"

extern struct serial * serial_handler;
bool block_wait_read = 0;
// FIXME, currently hard code to get corresponding process
extern struct proc * test_process;

// used to save the char read from console
static char tmp_char;

static void cb_block_sleep(uint32_t id, void* data)
{
    resume_coro(((struct proc*)(data))->p_coro);
}

void cb_block_read(struct serial *serial, char c)
{
    // TODO fixme, using buffer
    tmp_char = c;
    if (block_wait_read)
        resume_coro(test_process->p_coro);
}

void handle_block_sleep(void* argv)
{
    struct proc* proc = current_running_coro()->_proc;
    int sleep_second = (int)(argv);
    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "now sleep  %d proc: %d now time: %llu\n",sleep_second, proc->p_pid,  time_stamp()/1000);

    register_timer(sleep_second , cb_block_sleep, proc);
    yield_coro();

    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "now wake up proc: %d, now time: %llu\n", proc->p_pid, time_stamp()/1000);
    struct ipc_buffer_ctrl_msg ctrl;
    ctrl.ret_val = 0;
    ctrl.offset = 0;
    ipc_reply(&ctrl, &(proc->p_reply_cap));
}

void handle_block_read(void* argv)
{
    struct proc* proc = current_running_coro()->_proc;

    int nbyte = proc->p_ipc_ctrl.offset;
    assert(nbyte <=  APP_PROCESS_IPC_SHARED_BUFFER_SIZE);

    seL4_Word start_app_addr = proc->p_ipc_ctrl.start_app_buffer_addr;
    seL4_Word start_sos_addr = get_ipc_buffer(proc);

    int read_len = 0;

    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "now read block, proc: %d, read len: %d\n",proc->p_pid, nbyte);

    while (read_len < nbyte)
    {
        block_wait_read = 1;
        yield_coro();
        block_wait_read = 0;
        // write 1 byte to shared buffer
        memcpy((char*)start_sos_addr+read_len, &tmp_char, 1);
        read_len++;
        if (tmp_char == '\n')
        {
            break;
        }
    }
    struct ipc_buffer_ctrl_msg ret_ctrl;
    ret_ctrl.ret_val = 0;
    ret_ctrl.offset = read_len;


    ipc_reply(&ret_ctrl, &(proc->p_reply_cap));
    // TODO
}
