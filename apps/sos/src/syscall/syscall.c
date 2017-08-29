// FIXME if syscall argv is invalid, simply return error without doing any syscall
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

#define verbose  -1
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

// .will_block is not used actually
syscall_func syscall_func_arr[NUMBER_OF_SYSCALL] = {
    {.syscall=&sos_syscall_print_to_console, .will_block=false},
    {.syscall=&sos_syscall_read, .will_block=true},
    {.syscall=&sos_syscall_write, .will_block=false},
    {.syscall=&sos_syscall_open, .will_block=false},
    {.syscall=&sos_syscall_usleep, .will_block=true},
    {.syscall=&sos_syscall_time_stamp, .will_block=false},
    {.syscall=&sos_syscall_brk, .will_block=false},
    {.syscall=&sos_syscall_close, .will_block=false},
    {.syscall=&sos_syscall_stat, .will_block=false},
    {.syscall=&sos_syscall_get_dirent, .will_block=false}};

extern timestamp_t g_cur_timestamp_us;
/* extern struct serial * serial_handler = NULL; */
extern struct serial_console _serial;

/*
*   In M4, assume read from/write to console device
*/

bool path_transfer(char* in, size_t off)
{
    //FIXME

    static char file_name [4096];
    assert(off <= APP_PROCESS_IPC_SHARED_BUFFER_SIZE);
    memcpy(file_name, in, off);
    file_name[off] = 0;
    int out_len = 0;
    printf ("%u %s\n", off, in);
    if (strcmp(file_name, "console") == 0)
    {
        file_name[off] = ':';
        out_len = off + 1;
    }
    else
    {
        memcpy(file_name, "nfs:", 4);
        memcpy(file_name + 4, in ,  off );
        out_len = off + 4;
    }
    file_name[out_len] = 0;
    out_len += 1;
    memcpy(in, file_name, out_len);
    return true;

}

void sos_syscall_read(void* argv)
{
    struct proc* proc = (struct proc*) argv;
    assert(proc == get_current_proc());
    struct ipc_buffer_ctrl_msg* msg = &(proc->p_ipc_ctrl);
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "sos_syscall_read, from pid: %d\n", proc->p_pid);
    /* COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "* fd: %d, readlen: %d %d\n",msg->file_id, msg->offset, APP_PROCESS_IPC_SHARED_BUFFER_SIZE); */
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "* fd: %d, readlen: %d %d\n",msg->file_id, msg->offset, APP_PROCESS_IPC_SHARED_BUFFER_SIZE);
    size_t read_len = 0;
    assert(msg->offset <=  APP_PROCESS_IPC_SHARED_BUFFER_SIZE);
    int ret = syscall_read(msg->file_id, (char*)get_ipc_buffer(proc), msg->offset, &read_len);
    struct ipc_buffer_ctrl_msg ctrl;
    if (ret == 0 )
    {
        ctrl.offset = read_len;
        ctrl.ret_val = 0;
    }
    else
    {
        ctrl.offset = 0;
        ctrl.ret_val = read_len;
    }
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "read finish pid: %d, ret: %d, len: %u\n", proc->p_pid, ret, read_len);
    ipc_reply(&ctrl, &(proc->p_reply_cap));
}

void sos_syscall_open(void* argv)
{
    struct proc* proc = (struct proc*) argv;
    assert(proc == get_current_proc());


    char* file_name = (get_ipc_buffer(proc));
    path_transfer(file_name, proc->p_ipc_ctrl.offset);
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "sos_syscall_open: [%s]\n", file_name);

    int fd = 0;
    int ret = syscall_open(file_name, proc->p_ipc_ctrl.mode, proc->p_ipc_ctrl.mode, &fd);
    printf ("syscall_open finish\n");
    struct ipc_buffer_ctrl_msg ctrl;
    ctrl.offset = 0;
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
    /* if (offset < APP_PROCESS_IPC_SHARED_BUFFER_SIZE) */
    /*     ((char*)(start_sos_addr))[offset] = 0; */
    /* else */
    /*     ((char*)(start_sos_addr))[offset - 1]  = 0; */

    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_YELLOW,
        "[sos] serial send len: %d\n",ret);
    assert(ret > 0);
    struct ipc_buffer_ctrl_msg ctrl;
    ctrl.ret_val = 0;
    ctrl.offset = ret;
    ipc_reply(&ctrl, &(proc->p_reply_cap));
}

void sos_syscall_write(void* argv)
{
    struct proc* proc = (struct proc*) argv;
    assert(proc == get_current_proc());
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "sos_syscall_write, from pid: %d\n", proc->p_pid);
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "* fd: %d, writelen: %d %d\n", proc->p_ipc_ctrl.file_id, proc->p_ipc_ctrl.offset, APP_PROCESS_IPC_SHARED_BUFFER_SIZE);

    size_t write_len = 0;
    struct ipc_buffer_ctrl_msg* msg = &(proc->p_ipc_ctrl);
    assert(msg->offset <=  APP_PROCESS_IPC_SHARED_BUFFER_SIZE);
    int ret = syscall_write(msg->file_id, (char*)get_ipc_buffer(proc), msg->offset, &write_len);
    struct ipc_buffer_ctrl_msg ctrl;
    if (ret == 0 )
    {
        ctrl.ret_val = 0;
        ctrl.offset = write_len;
    }
    else
    {
        ctrl.ret_val = write_len;
        ctrl.offset = 0;
    }
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "sos_syscall_write, return: %d, offset: %d\n",  ctrl.ret_val,  ctrl.offset);
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

void sos_syscall_stat(void* argv)
{
    struct proc* proc = (struct proc*) argv;
    assert(proc == get_current_proc());
    // FIXME
    char* file_name = (get_ipc_buffer(proc));
    path_transfer(file_name, proc->p_ipc_ctrl.offset);
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "sos_syscall_stat: [%s]\n", file_name);

    struct stat buf;
    int ret = syscall_stat(file_name, &buf);
    struct ipc_buffer_ctrl_msg ctrl;
    ctrl.ret_val = ret;
    if (ctrl.ret_val == 0)
    {
        ctrl.offset = sizeof (sos_stat_t);
        sos_stat_t* sos_stat = ( sos_stat_t* )( get_ipc_buffer(proc));
        sos_stat->st_type =  buf.st_type;
        sos_stat->st_fmode = buf.st_mode;
        sos_stat->st_size = buf.st_size;
        sos_stat->st_ctime = (long long)(buf.st_ctime)* 1000000LL + buf.st_ctimensec;
        sos_stat->st_atime = (long long)(buf.st_atime) * 1000000LL + buf.st_atimensec;
    }
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "sos_syscall_stat, return: %d\n",  ctrl.ret_val);
    ipc_reply(&ctrl, &(proc->p_reply_cap));
}

void sos_syscall_get_dirent(void* argv)
{
    struct proc* proc = (struct proc*) argv;
    assert(proc == get_current_proc());
    struct ipc_buffer_ctrl_msg* in = &(proc->p_ipc_ctrl);

    assert(in->offset == 2 * sizeof(int));
    int pos = *(int*)(get_ipc_buffer(proc));
    int file_name_len = *(int*)((size_t)(get_ipc_buffer(proc)) + 4);
    char* name = (char*)(get_ipc_buffer(proc));
    file_name_len = file_name_len > APP_PROCESS_IPC_SHARED_BUFFER_SIZE ? APP_PROCESS_IPC_SHARED_BUFFER_SIZE: file_name_len;
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "sos_syscall_get_dirent, pos: %d, given length %d for storing file name\n", pos, file_name_len);

    // currently only support nfs.
    char path[10] ;
    memcpy(path, "nfs:", 4);
    int ret = syscall_get_dirent(path, pos, name, file_name_len);
    struct ipc_buffer_ctrl_msg ctrl;
    if (ret != 0)
    {
        ctrl.ret_val = ret;
        ctrl.offset = 0;
    }
    else
    {
        ctrl.ret_val = 0;
        ctrl.offset = strlen(name);
        assert(ctrl.offset <= APP_PROCESS_IPC_SHARED_BUFFER_SIZE);
    }
    ipc_reply(&ctrl, &(proc->p_reply_cap));
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
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "syscall_number: %d, offset: %d, start_addr: 0x%x\n",
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

}



