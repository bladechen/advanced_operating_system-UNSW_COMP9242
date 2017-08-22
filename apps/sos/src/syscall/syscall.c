#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <cspace/cspace.h>
#include <serial/serial.h>
#include <clock/clock.h>
#include "comm/comm.h"
#include "vm/vmem_layout.h"
#include "handle_syscall.h"

#define verbose 5
#include <sys/debug.h>
#include <sys/panic.h>
#include "dev/console.h"
#include "vm/frametable.h"
#include "vm/address_space.h"
#include "proc/proc.h"
#include "syscall.h"
#include "vm/pagetable.h"
#include <sos.h>
#include "fs/file_syscall.h"

// used to replace the long switch case in `handle_syscall`
#define NUMBER_OF_SYSCALL   100

syscall_func syscall_func_arr[NUMBER_OF_SYSCALL] = {
    {.syscall=&sos_syscall_print_to_console, .will_block=false},
    {.syscall=&sos_syscall_read, .will_block=true},
    {.syscall=&sos_syscall_write, .will_block=false},
    {.syscall=&sos_syscall_open, .will_block=false},
    {.syscall=&sos_syscall_usleep, .will_block=true},
    {.syscall=&sos_syscall_time_stamp, .will_block=false},
    {.syscall=&sos_syscall_brk, .will_block=false},
    {.syscall=&sos_syscall_close, .will_block=false}};

extern timestamp_t g_cur_timestamp_us;
/* extern struct serial * serial_handler = NULL; */
extern struct serial_console _serial;

/*
*   In M4, assume read from/write to console device
*/

void sos_syscall_read(void* argv)
{
    struct proc* proc = (struct proc*) argv;
    assert(proc == get_current_proc());
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "sos_syscall_read\n");
    size_t read_len = 0;
    struct ipc_buffer_ctrl_msg* msg = &(proc->p_ipc_ctrl);
    assert(msg->offset <=  APP_PROCESS_IPC_SHARED_BUFFER_SIZE);
    int ret = syscall_read(msg->file_id, (char*)APP_PROCESS_IPC_SHARED_BUFFER, msg->offset, &read_len);
    struct ipc_buffer_ctrl_msg ctrl;
    if (ret == 0 )
    {
        ctrl.ret_val = 0;
        ctrl.offset = read_len;
    }
    else
    {
        ctrl.ret_val = read_len;
    }
    ipc_reply(&ctrl, &(proc->p_reply_cap));
}

void sos_syscall_open(void* argv)
{
    struct proc* proc = (struct proc*) argv;
    assert(proc == get_current_proc());
    static char file_name [1000];
    memcpy(file_name, get_ipc_buffer(proc), proc->p_ipc_ctrl.offset);
    file_name[proc->p_ipc_ctrl.offset] = 0;
    if (strcpy(file_name, "cosonle") == 0)
    {
        file_name[proc->p_ipc_ctrl.offset ] = ':';
        file_name[1 + proc->p_ipc_ctrl.offset] = 0;
    }

    int fd = 0;
    int ret = syscall_open(file_name, proc->p_ipc_ctrl.mode, proc->p_ipc_ctrl.mode, &fd);
    struct ipc_buffer_ctrl_msg ctrl;
    if (ret == 0 )
    {
        ctrl.ret_val = 0;
        ctrl.file_id = fd;
    }
    else
    {
        ctrl.ret_val = fd;
    }
    ipc_reply(&ctrl, &(proc->p_reply_cap));
}

void sos_syscall_close(void* argv)
{
    struct proc* proc = (struct proc*) argv;
    assert(proc == get_current_proc());

    int err = 0;
    syscall_close(proc->p_ipc_ctrl.file_id, &err);
    struct ipc_buffer_ctrl_msg ctrl;
    ctrl.ret_val = err;
    ctrl.offset = 0;
    ipc_reply(&ctrl, &(proc->p_reply_cap));
}

void sos_syscall_time_stamp(void * argv)
{
    struct proc* proc = (struct proc*) argv;
    assert(proc == get_current_proc());
    timestamp_t now = g_cur_timestamp_us;
    memcpy(get_ipc_buffer(proc), &now, 8);
    struct ipc_buffer_ctrl_msg ctrl;
    ctrl.ret_val = 0;
    ctrl.offset = 0;
    ipc_reply(&ctrl, &(proc->p_reply_cap));
}

// This function correspond to `sos_write` defined in APP scope in `sos.h`
void sos_syscall_print_to_console(void * argv)
{
    struct proc* proc = (struct proc*) argv;
    assert(proc == get_current_proc());

	// seL4_Word start_app_addr = seL4_GetMR(1);

    seL4_Word start_sos_addr = (seL4_Word)get_ipc_buffer(proc);

    // int offset = seL4_GetMR(2);
    int offset = proc->p_ipc_ctrl.offset;

    // bypass fs/vfs check, because it may not open "console:""
    int ret = serial_send(_serial._serial_handler, (char *)start_sos_addr, offset);

    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_YELLOW,
        "[sos] serial send len: %d \n",ret);
    struct ipc_buffer_ctrl_msg ctrl;
    ctrl.offset = 0;
    ctrl.ret_val = 0;
    ctrl.offset = ret;
    ipc_reply(&ctrl, &(proc->p_reply_cap));
}

void sos_syscall_write(void* argv)
{
    struct proc* proc = (struct proc*) argv;
    assert(proc == get_current_proc());
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "sos_syscall_write\n");
    size_t write_len = 0;
    struct ipc_buffer_ctrl_msg* msg = &(proc->p_ipc_ctrl);
    assert(msg->offset <=  APP_PROCESS_IPC_SHARED_BUFFER_SIZE);
    int ret = syscall_write(msg->file_id, (char*)APP_PROCESS_IPC_SHARED_BUFFER, msg->offset, &write_len);
    struct ipc_buffer_ctrl_msg ctrl;
    if (ret == 0 )
    {
        ctrl.ret_val = 0;
        ctrl.offset = write_len;
    }
    else
    {
        ctrl.ret_val = write_len;
    }
    ipc_reply(&ctrl, &(proc->p_reply_cap));
}


void sos_syscall_usleep(void * argv)
{
    struct proc* proc = (struct proc*) argv;
    assert(proc == get_current_proc());
    int msecond = *((int*)(get_ipc_buffer(proc)));
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "proc %d, get sleep %d\n", proc->p_pid, msecond);
    handle_block_sleep((void*)(msecond));
}

void sos_syscall_brk(void* argv)
{
    struct proc* proc = (struct proc*) argv;
    assert(proc == get_current_proc());
    seL4_Word newbrk = *((int*)(get_ipc_buffer(proc)));

    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "proc %d, newbrk: 0x%x\n", proc->p_pid, newbrk);
    /* frame_alloc(NULL); */
    /* COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "enter as_get_heap_brk\n"); */


    struct addrspace *as = proc->p_addrspace;
    seL4_Word retbrk = 0;

    struct ipc_buffer_ctrl_msg ctrl;
    int ret = as_get_heap_brk(as, newbrk, &retbrk);
    ctrl.ret_val = ret;
    ctrl.offset = 4;
    if (ret == 0)
    {
        memcpy(get_ipc_buffer(proc), &retbrk, 4);
    }

    ipc_reply(&ctrl, &(proc->p_reply_cap));
}

void handle_syscall(seL4_Word badge, struct proc * app_process)
{
    seL4_Word syscall_number;
    seL4_CPtr reply_cap;

    struct ipc_buffer_ctrl_msg * ctrl_msg = &(app_process->p_ipc_ctrl);
    memcpy(ctrl_msg, seL4_GetIPCBuffer()->msg, sizeof(ipc_buffer_ctrl_msg));

    syscall_number = ctrl_msg->syscall_number;
    dprintf(0, "syscall_number: %d, offset: %d, start_addr: 0x%x\n",
        syscall_number, ctrl_msg->offset, ctrl_msg->start_app_buffer_addr);

    assert(coro_status(app_process->p_coro) == COROUTINE_INIT);
    /* Save the caller */
    reply_cap = cspace_save_reply_cap(cur_cspace);
    assert(reply_cap != CSPACE_NULL);

    // in case the app process block, the reply_cap and message get flushed
    // we put these into `proc struct`
    app_process->p_reply_cap = reply_cap;

    if (syscall_number < 0 || syscall_number > NUMBER_OF_SYSCALL) {
        printf("%s:%d (%s) Unknown syscall %d\n",
                       __FILE__, __LINE__, __func__, syscall_number);
        assert("unknown syscall number!\n");
    }

    /* Invoke corresponding syscall */
    restart_coro(app_process->p_coro, syscall_func_arr[syscall_number].syscall, app_process);
    /* (*syscall_func_arr[syscall_number].syscall)(app_process); */

    // If the syscall won't block we will process the reply_cap revoke here
    /* if (syscall_func_arr[syscall_number].will_block == false) { */
    /*     #<{(| destroy_reply_cap(&(app_process->p_reply_cap)); |)}># */
    /* } */

}



