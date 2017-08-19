#include "syscall.h"
#include "handle_syscall.h"
#include "comm/comm.h"
#include "coroutine/coro.h"
#include "clock/clock.h"
#include <serial/serial.h>
#include "vm/pagetable.h"

extern struct serial * serial_handler;
// FIXME, currently hard code to get corresponding process
extern struct proc * test_process;

// used to save the char read from console
static char tmp_char;

static void cb_block_sleep(uint32_t id, void* data)
{
    resume_coro(((struct proc*)(data))->p_coro);
}

static void cb_block_read(struct serial *serial, char c)
{
    tmp_char = c;
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
    ipc_reply(&ctrl, &(proc->p_reply_cap));
}

void handle_block_read(void* argv)
{
    struct proc* proc = current_running_coro()->_proc;

    int nbyte = proc->p_ipc_ctrl.offset;
    // int file = proc->p_ipc_ctrl.file_id;

    seL4_Word start_app_addr = proc->p_ipc_ctrl.start_app_buffer_addr;
    seL4_Word start_sos_addr = page_phys_addr(proc->p_pagetable, start_app_addr);

    int read_len = 0;

    // COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "now read block, proc: %d\n",proc->p_pid);

    int err = serial_register_handler(serial_handler, &cb_block_read);

    assert(err==0);


    while (read_len < nbyte) 
    {
        yield_coro();

        // write 1 byte to shared buffer
        memcpy((char*)start_sos_addr+read_len, &tmp_char, 1);
        read_len++;
    }
    
    // COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "now wake up read block proc: %d\n", proc->p_pid);
    // if (proc->p_reply_cap != 0)
    // {
    //     reply_success(proc->p_reply_cap);
    //     destroy_reply_cap(proc->p_reply_cap);
    // }
    // proc->p_reply_cap = 0;

    ipc_buffer_ctrl_msg * ctrl_msg = &(proc->p_ipc_ctrl);//(ipc_buffer_ctrl_msg *)malloc(sizeof(ipc_buffer_ctrl_msg));
    memset(ctrl_msg, 0, sizeof(ipc_buffer_ctrl_msg));
    // proc->p_ipc_ctrl = (ipc_buffer_ctrl_msg *)(seL4_GetIPCBuffer()->msg);

    ctrl_msg->offset = read_len;

    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 5);
    seL4_SetTag(tag);

    memcpy(seL4_GetIPCBuffer()->msg, ctrl_msg, sizeof(ipc_buffer_ctrl_msg));

    seL4_Send(proc->p_reply_cap, tag);

    destroy_reply_cap(proc->p_reply_cap);
    proc->p_reply_cap = 0;
}