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
#include "vm/vm.h"
#include "proc/proc.h"
#include "proc/pid.h"
#include "syscall.h"
#include "vm/pagetable.h"
#include <sos.h>
#include "fs/file_syscall.h"

// used to replace the long switch case in `handle_syscall`
#define NUMBER_OF_SYSCALL   100

syscall_func syscall_func_arr[NUMBER_OF_SYSCALL] = {
    {.syscall=&sos_syscall_print_to_console},
    {.syscall=&sos_syscall_read},
    {.syscall=&sos_syscall_write},
    {.syscall=&sos_syscall_open},
    {.syscall=&sos_syscall_usleep},
    {.syscall=&sos_syscall_time_stamp},
    {.syscall=&sos_syscall_brk},
    {.syscall=&sos_syscall_close},
    {.syscall=&sos_syscall_stat},
    {.syscall=&sos_syscall_get_dirent},
    {.syscall=&sos_syscall_remove},
    {.syscall=&sos_syscall_create_process},
    {.syscall=&sos_syscall_delete_process},
    {.syscall=&sos_syscall_wait_process},
    {.syscall=&sos_syscall_process_status},
    {.syscall=&sos_syscall_exit_process},
    {.syscall=&sos_syscall_process_my_pid},
    {.syscall=&sos_syscall_mmap},
    {.syscall=&sos_syscall_munmap},
    {.syscall=&sos_syscall_vm_shared}};

extern timestamp_t g_cur_timestamp_us;
/* extern struct serial * serial_handler = NULL; */
extern struct serial_console _serial;

/* for process creation */
extern seL4_CPtr _sos_ipc_ep_cap;
/* extern struct proc* proc_array[PROC_ARRAY_SIZE]; */

/*
*   In M4, assume read from/write to console device
*/

bool path_transfer(char* in, size_t off)
{
    //FIXME
    static char file_name [4096];
    if (off >= APP_PROCESS_IPC_SHARED_BUFFER_SIZE - 20)
    {
        return false;
    }
    /* assert(off <= APP_PROCESS_IPC_SHARED_BUFFER_SIZE); */
    memcpy(file_name, in, off);
    file_name[off] = 0;
    int out_len = 0;
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
    struct ipc_buffer_ctrl_msg* msg = &(proc->p_context.p_ipc_ctrl);
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "begin sos_syscall_read, from pid: %u\n", proc->p_pid);
    /* COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "* fd: %d, readlen: %d %d\n",msg->file_id, msg->offset, APP_PROCESS_IPC_SHARED_BUFFER_SIZE); */
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "* fd: %d, readlen: %d\n",msg->file_id, msg->offset);
    size_t read_len = 0;
    assert(msg->offset <=  APP_PROCESS_IPC_SHARED_BUFFER_SIZE);
    int ret = syscall_read(msg->file_id, (char*)get_ipc_buffer(proc), msg->offset, &read_len);
    struct ipc_buffer_ctrl_msg ctrl;
    if (ret == 0 )
    {
        ctrl.offset = read_len;
        ctrl.ret_val = 0;

        COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "end sos_syscall_read pid: %u, read len: %d\n", proc->p_pid,  ctrl.offset);
    }
    else
    {
        ctrl.offset = 0;
        ctrl.ret_val = read_len;

        ERROR_DEBUG("end sos_syscall_write pid: %u, err: %d\n", proc->p_pid, read_len);
    }
    ipc_reply(&ctrl, &(proc->p_context.p_reply_cap));
}

// Remove files
void sos_syscall_remove(void * argv) {
    struct proc * proc = (struct proc *) argv;
    assert(proc == get_current_proc());
    struct ipc_buffer_ctrl_msg* msg = &(proc->p_context.p_ipc_ctrl);

    struct ipc_buffer_ctrl_msg reply_cmsg;
    char* file_name = (get_ipc_buffer(proc));
    if (path_transfer(file_name, msg->offset) == false)
    {
        reply_cmsg.ret_val = EINVAL;
        ERROR_DEBUG("sos_syscall_remove proc: %u err\n",proc->p_pid);
        ipc_reply(&reply_cmsg, &(proc->p_context.p_reply_cap));
        return;

    }
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "sos_syscall_remove: [%s]\n", file_name);

    int err = 0;
    int ret = syscall_remove(file_name, &err);

    reply_cmsg.offset = 0;
    reply_cmsg.ret_val = ret;

    ipc_reply(&reply_cmsg, &(proc->p_context.p_reply_cap));
}

void sos_syscall_open(void* argv)
{
    struct proc* proc = (struct proc*) argv;
    assert(proc == get_current_proc());


    char* file_name = (get_ipc_buffer(proc));
    struct ipc_buffer_ctrl_msg ctrl;
    if (path_transfer(file_name, proc->p_context.p_ipc_ctrl.offset) == false)
    {
        ctrl.ret_val = EINVAL;
        ERROR_DEBUG("end sos_syscall_open proc: %u, file: [%s] err: %d\n",proc->p_pid, file_name, ctrl.ret_val);
        ipc_reply(&ctrl, &(proc->p_context.p_reply_cap));
        return;
    }

    if (strcmp(file_name, "console:") == 0)
    {
        if (proc->p_context.p_ipc_ctrl.mode == O_RDONLY ||
            proc->p_context.p_ipc_ctrl.mode == O_RDWR)
        {
            strcpy(file_name, "console_exclusive:");
        }
    }
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "begin sos_syscall_open proc: %u, file: [%s] flags: %d, mode: %d\n",proc->p_pid, file_name, proc->p_context.p_ipc_ctrl.mode, proc->p_context.p_ipc_ctrl.mode);


    int fd = 0;
    int ret = syscall_open(file_name, proc->p_context.p_ipc_ctrl.mode, proc->p_context.p_ipc_ctrl.mode, &fd);
    ctrl.offset = 0;
    if (ret == 0 )
    {
        ctrl.ret_val = 0;
        ctrl.file_id = fd;
        COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "end sos_syscall_open proc: %u, file: [%s] fd: %d\n",proc->p_pid, file_name, fd);

    }
    else
    {
        ERROR_DEBUG("end sos_syscall_open proc: %u, file: [%s] err: %d\n",proc->p_pid, file_name, fd);
        ctrl.ret_val = fd;
    }
    ipc_reply(&ctrl, &(proc->p_context.p_reply_cap));
}

void sos_syscall_close(void* argv)
{
    struct proc* proc = (struct proc*) argv;
    assert(proc == get_current_proc());

    int err = 0;
    syscall_close(proc->p_context.p_ipc_ctrl.file_id, &err);
    struct ipc_buffer_ctrl_msg ctrl;
    ctrl.ret_val = err;
    ctrl.offset = 0;
    if (ctrl.ret_val)
    {
        ERROR_DEBUG("sos_syscall_close proc: %u, fd: %d, err: %d\n", proc->p_pid, proc->p_context.p_ipc_ctrl.file_id, err);
    }
    else
    {
        COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "sos_syscall_close proc: %u, fd: %d success!\n", proc->p_pid, proc->p_context.p_ipc_ctrl.file_id);
    }
    ipc_reply(&ctrl, &(proc->p_context.p_reply_cap));
}

void sos_syscall_time_stamp(void * argv)
{
    dump_vm_state();
    struct proc* proc = (struct proc*) argv;
    assert(proc == get_current_proc());
    timestamp_t now = g_cur_timestamp_us;
    memcpy(get_ipc_buffer(proc), &now, 8);
    struct ipc_buffer_ctrl_msg ctrl;
    ctrl.ret_val = 0;
    ctrl.offset = 0;
    ipc_reply(&ctrl, &(proc->p_context.p_reply_cap));
}

// This function correspond to `sos_write` defined in APP scope in `sos.h`
void sos_syscall_print_to_console(void * argv)
{
    struct proc* proc = (struct proc*) argv;
    assert(proc == get_current_proc());

	// seL4_Word start_app_addr = seL4_GetMR(1);

    seL4_Word start_sos_addr = (seL4_Word)get_ipc_buffer(proc);

    // int offset = seL4_GetMR(2);
    int offset = proc->p_context.p_ipc_ctrl.offset;

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
    ipc_reply(&ctrl, &(proc->p_context.p_reply_cap));
}

void sos_syscall_write(void* argv)
{
    struct proc* proc = (struct proc*) argv;
    assert(proc == get_current_proc());
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "begin sos_syscall_write pid: %u\n", proc->p_pid);
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "* fd: %d, writelen: %d\n", proc->p_context.p_ipc_ctrl.file_id, proc->p_context.p_ipc_ctrl.offset);

    size_t write_len = 0;
    struct ipc_buffer_ctrl_msg* msg = &(proc->p_context.p_ipc_ctrl);
    assert(msg->offset <=  APP_PROCESS_IPC_SHARED_BUFFER_SIZE);
    int ret = syscall_write(msg->file_id, (char*)get_ipc_buffer(proc), msg->offset, &write_len);
    struct ipc_buffer_ctrl_msg ctrl;
    if (ret == 0 )
    {
        ctrl.ret_val = 0;
        ctrl.offset = write_len;
        COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "end sos_syscall_write pid: %u, write len: %d\n", proc->p_pid,  ctrl.offset);
    }
    else
    {
        ERROR_DEBUG("end sos_syscall_write pid: %u, err: %d\n", proc->p_pid, write_len);
        ctrl.ret_val = write_len;
        ctrl.offset = 0;
    }
    ipc_reply(&ctrl, &(proc->p_context.p_reply_cap));
}

void sos_syscall_usleep(void * argv)
{
    struct proc* proc = (struct proc*) argv;
    assert(proc == get_current_proc());
    int msecond = *((int*)(get_ipc_buffer(proc)));
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "proc %u, get sleep %dms\n", proc->p_pid, msecond);
    handle_block_sleep((void*)(msecond));
}

void sos_syscall_stat(void* argv)
{
    struct proc* proc = (struct proc*) argv;
    assert(proc == get_current_proc());
    char* file_name = (get_ipc_buffer(proc));
    struct ipc_buffer_ctrl_msg ctrl;
    if (path_transfer(file_name, proc->p_context.p_ipc_ctrl.offset) == false)
    {
        ctrl.ret_val = EINVAL;
        ERROR_DEBUG("end sos_syscall_stat proc: %u, file: [%s] err: %d\n",proc->p_pid, file_name, ctrl.ret_val);
        ipc_reply(&ctrl, &(proc->p_context.p_reply_cap));
        return;
    }
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "begin sos_syscall_stat proc: %u, file: [%s]\n", proc->p_pid, file_name);

    struct stat buf;
    int ret = syscall_stat(file_name, &buf);
    ctrl.ret_val = ret;
    ctrl.offset = 0;
    if (ctrl.ret_val == 0)
    {
        ctrl.offset = sizeof (sos_stat_t);
        sos_stat_t* sos_stat = ( sos_stat_t* )( get_ipc_buffer(proc));
        sos_stat->st_type =  buf.st_type;
        sos_stat->st_fmode = buf.st_mode;
        sos_stat->st_size = buf.st_size;
        sos_stat->st_ctime = (long)(buf.st_ctime);
        sos_stat->st_atime = (long)(buf.st_atime) ;
        /* sos_stat->st_mtime = (long)(buf.st_mtime) ; */
        COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN,"end sos_syscall_stat proc: %u, file: \n", proc->p_pid, file_name);
        COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN,"* type: %d, mode: %d, size: %d, ctime: %ld, atime: %ld\n", sos_stat->st_type, sos_stat->st_fmode, (int)(sos_stat->st_size), sos_stat->st_ctime, sos_stat->st_atime);
    }
    else
    {
        ERROR_DEBUG("end sos_syscall_stat proc: %u, err: %d\n", proc->p_pid, ctrl.ret_val);
    }
    ipc_reply(&ctrl, &(proc->p_context.p_reply_cap));
}

void sos_syscall_get_dirent(void* argv)
{
    struct proc* proc = (struct proc*) argv;
    assert(proc == get_current_proc());
    struct ipc_buffer_ctrl_msg* in = &(proc->p_context.p_ipc_ctrl);

    assert(in->offset == 2 * sizeof(int));
    int pos = *(int*)(get_ipc_buffer(proc));
    int file_name_len = *(int*)((size_t)(get_ipc_buffer(proc)) + 4);
    char* name = (char*)(get_ipc_buffer(proc));
    file_name_len = file_name_len > APP_PROCESS_IPC_SHARED_BUFFER_SIZE - 1 ? APP_PROCESS_IPC_SHARED_BUFFER_SIZE - 1: file_name_len;
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "begin sos_syscall_get_dirent, proc: %u, pos: %d, given length %d for storing file name\n", proc->p_pid, pos, file_name_len);

    // currently only support nfs.
    char path[10] ;
    memcpy(path, "nfs:", 4);
    memset(name, 0, APP_PROCESS_IPC_SHARED_BUFFER_SIZE);
    int ret = syscall_get_dirent(path, pos, name, file_name_len);
    struct ipc_buffer_ctrl_msg ctrl;
    if (ret != 0)
    {
        COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_RED, "end sos_syscall_get_dirent, proc: %u, pos: %d, err: %d\n", proc->p_pid, pos, ret);
        ctrl.ret_val = ret;
        ctrl.offset = 0;
    }
    else
    {
        COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "end sos_syscall_get_dirent, proc: %u, file[%s]\n", proc->p_pid, name);
        ctrl.ret_val = 0;
        ctrl.offset = strlen(name);
        assert(ctrl.offset <= APP_PROCESS_IPC_SHARED_BUFFER_SIZE);
    }
    ipc_reply(&ctrl, &(proc->p_context.p_reply_cap));
}

void sos_syscall_brk(void* argv)
{
    struct proc* proc = (struct proc*) argv;
    assert(proc == get_current_proc());
    seL4_Word newbrk = *((int*)(get_ipc_buffer(proc)));

    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "begin sos_syscall_brk proc %u, newbrk: 0x%x\n", proc->p_pid, newbrk);

    struct addrspace *as = proc->p_resource.p_addrspace;
    seL4_Word retbrk = 0;

    struct ipc_buffer_ctrl_msg ctrl;
    int ret = as_get_heap_brk(as, newbrk, &retbrk);
    ctrl.ret_val = ret;
    ctrl.offset = 4;
    if (ret == 0)
    {
        memcpy(get_ipc_buffer(proc), &retbrk, 4);
    }
    COLOR_DEBUG(DB_SYSCALL, ret == 0 ? ANSI_COLOR_GREEN : ANSI_COLOR_RED, "end sos_syscall_brk proc %u, return brk: 0x%x, ret: %d\n", proc->p_pid,retbrk, ret);

    ipc_reply(&ctrl, &(proc->p_context.p_reply_cap));
}

void sos_syscall_mmap(void* argv)
{
    struct proc* proc = (struct proc*) argv;
    assert(proc == get_current_proc());

    struct ipc_buffer_ctrl_msg ctrl;
    ctrl.offset = 0;
    if (proc->p_context.p_ipc_ctrl.offset != sizeof (sos_mmap_t))
    {
        ctrl.ret_val = EINVAL;
        goto end_sos_syscall_mmap;
    }

    struct addrspace *as = proc->p_resource.p_addrspace;
    sos_mmap_t mmap_argv;
    memcpy(&mmap_argv, get_ipc_buffer(proc), proc->p_context.p_ipc_ctrl.offset);
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "begin sos_syscall_mmap proc %u, addr: 0x%x, size: %u\n",
                proc->p_pid, mmap_argv.addr, mmap_argv.length);

    uint32_t ret_addr = 0;
    int ret = as_define_mmap(as, &mmap_argv, &ret_addr);
    ctrl.ret_val = ret;
    ctrl.offset = 4;
    if (ret == 0)
    {
        memcpy(get_ipc_buffer(proc), &ret_addr, 4);
    }
    COLOR_DEBUG(DB_SYSCALL, ret == 0 ? ANSI_COLOR_GREEN : ANSI_COLOR_RED, "end  sos_syscall_map, proc %u, return addr: 0x%x, length: %u ret: %d\n", proc->p_pid, ret_addr, mmap_argv.length, ret);

end_sos_syscall_mmap:
    ipc_reply(&ctrl, &(proc->p_context.p_reply_cap));
}

void sos_syscall_munmap(void* argv)
{
    struct proc* proc = (struct proc*) argv;
    assert(proc == get_current_proc());

    struct ipc_buffer_ctrl_msg ctrl;
    ctrl.offset = 0;
    if (proc->p_context.p_ipc_ctrl.offset != sizeof (sos_mmap_t))
    {
        ctrl.ret_val = EINVAL;
        goto end_sos_syscall_munmap;
    }

    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "begin sos_syscall_munmap proc %u\n", proc->p_pid);

    struct addrspace *as = proc->p_resource.p_addrspace;
    sos_mmap_t mmap_argv;
    memcpy(&mmap_argv, get_ipc_buffer(proc), proc->p_context.p_ipc_ctrl.offset);

    int ret = as_destroy_mmap(as, &mmap_argv);
    ctrl.ret_val = ret;
    COLOR_DEBUG(DB_SYSCALL, ret == 0 ? ANSI_COLOR_GREEN : ANSI_COLOR_RED, "end sos_syscall_munmap, proc %u, munmap addr: 0x%x, length: %u, ret: %d\n", proc->p_pid, mmap_argv.addr, mmap_argv.length, ret);

end_sos_syscall_munmap:
    ipc_reply(&ctrl, &(proc->p_context.p_reply_cap));
}


void sos_syscall_process_my_pid(void* argv)
{
    struct proc* proc = (struct proc*) argv;
    assert(proc == get_current_proc());

    struct ipc_buffer_ctrl_msg ctrl;
    ctrl.offset = 0;
    ctrl.file_id = get_current_proc()->p_pid;
    ctrl.ret_val = 0;
    ipc_reply(&ctrl, &(proc->p_context.p_reply_cap));
}

void sos_syscall_create_process(void * args)
{
    struct proc* proc = (struct proc*) args;
    assert(proc == get_current_proc());

    char* ipc_buf = (get_ipc_buffer(proc));
    struct ipc_buffer_ctrl_msg ctrl;
    ctrl.offset = 0;
    ctrl.file_id = -1;

    int argc = 30;
    char* argv[30] = {NULL};
    int ret = unserialize_exec_argv(ipc_buf, 4096, &argc, argv);
    if (ret != 0)
    {
        ERROR_DEBUG("proc: %d, unserialize_exec_argv err\n", proc->p_pid);
        ctrl.ret_val = EINVAL;
        goto create_end;
    }

    for (int i = 0; i < argc; i ++)
    {
        printf ("argc[%d]: %s\n", i, argv[i]);
    }

    ret = run_program(argv[0], _sos_ipc_ep_cap, argc, argv);
    if (ret < 0)
    {
        ctrl.ret_val = -ret;
        goto create_end;
    }

    ctrl.ret_val = 0;
    ctrl.file_id = ret;
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "%d create proc:%d -> %s [%s]\n", get_current_proc()->p_pid,
                ret, pid_to_proc(ret)->p_status.name, pid_to_proc(ret)->p_status.argv_str);
create_end:
    if (ctrl.ret_val != 0)
    {
        ERROR_DEBUG("%d create proc failed: %d\n",  get_current_proc()->p_pid, ctrl.ret_val);
    }
    ipc_reply(&ctrl, &(proc->p_context.p_reply_cap));
}


/* static void wait_for_childproc(struct processes) */
// TODO handle pid = -1
void sos_syscall_wait_process(void * argv)
{
    struct proc* proc = (struct proc*) argv;
    assert(proc == get_current_proc());

    struct ipc_buffer_ctrl_msg ctrl;
    ctrl.offset = 0;
    pid_t pid = proc->p_context.p_ipc_ctrl.file_id;
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, " proc: %d wait for %d\n",proc->p_pid, pid);
    if (pid == -1)
    {
        if (is_list_empty(&proc->children_list)) // nothing to wait.
        {
            ERROR_DEBUG("nothing to wait\n");
            ctrl.ret_val = ECHILD;
            goto wait_end;
        }
        proc->p_wait_pid = WAIT_ALL_PID;
        proc->p_waitchild = sem_create("waiting child", 0, -1);
        if (proc->p_waitchild == NULL)
        {
            ERROR_DEBUG("no enough mem creating sem\n");
            ctrl.ret_val = ENOMEM;
            goto wait_end;
        }
        P(proc->p_waitchild);
        COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, " proc: %d now  wakes up by %d\n", proc->p_pid, proc->p_wait_pid);
        struct proc* wait_proc = pid_to_proc(proc->p_wait_pid); // child process will set p_wait_pid to tell.
        assert(wait_proc != NULL);
        assert(!wait_proc->someone_wait);
        assert(wait_proc->p_father_pid == proc->p_pid);
        assert(wait_proc->p_status.status == PROC_STATUS_ZOMBIE);
        proc->p_wait_pid = INVALID_WAIT_PID;
        sem_destroy(proc->p_waitchild);
        proc->p_waitchild = NULL;
        proc_deattch(wait_proc);
        ctrl.file_id = wait_proc->p_pid;
        proc_destroy(wait_proc);
        ctrl.ret_val = 0;
        goto wait_end;
    }

    // waiting for specific child
    struct proc* wait_proc = pid_to_proc(pid);
    if (wait_proc == NULL ||  wait_proc->p_father_pid != proc->p_pid)
    {
        ERROR_DEBUG("not find the child proc\n");
        ctrl.ret_val = ECHILD;
        goto wait_end;
    }
    assert(!list_empty(&wait_proc->as_child_next));
    assert(!is_list_empty(&proc->children_list));
    if (wait_proc->p_status.status == PROC_STATUS_ZOMBIE || wait_proc->p_status.status == PROC_STATUS_EXIT)
    {
        ERROR_DEBUG("waiting for proc at zombie or exit could not happend\n");
        assert(0);
    }

    else if(wait_proc->p_status.status == PROC_STATUS_RUNNING || wait_proc->p_status.status == PROC_STATUS_SLEEP)
    {
        proc->p_waitchild = sem_create("waiting child", 0, -1);
        if (proc->p_waitchild == NULL)
        {
            ERROR_DEBUG("no enough mem creating sem\n");
            ctrl.ret_val = ENOMEM;
            goto wait_end;
        }
        proc->p_wait_pid = pid;
        wait_proc->someone_wait = true;
        COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, " proc: %u now  blocks waiting for %u\n",proc->p_pid, pid);

        P(proc->p_waitchild);

        wait_proc->someone_wait = false;
        sem_destroy(proc->p_waitchild);
        proc->p_waitchild = NULL;
        proc->p_wait_pid = INVALID_WAIT_PID;
        assert(wait_proc->p_status.status == PROC_STATUS_ZOMBIE);
        proc_deattch(wait_proc);
        ctrl.file_id = wait_proc->p_pid;
        proc_destroy(wait_proc);
        ctrl.ret_val = 0;
    }
    else
    {
        ERROR_DEBUG("pid %d wait for pid %d, but it is waiting: %d\n", proc->p_pid, wait_proc->p_pid, proc->p_status.status);
        /* assert(0); */

        ctrl.ret_val = EINVAL;

    }
wait_end:
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, " proc: %u wait for %u finish: %d\n",proc->p_pid, pid, ctrl.ret_val);
    ipc_reply(&ctrl, &(proc->p_context.p_reply_cap));
    return;
}

void sos_syscall_delete_process(void * argv)
{
    struct proc* proc = (struct proc*) argv;

    struct ipc_buffer_ctrl_msg ctrl;
    ctrl.offset = 0;
    ctrl.ret_val = 0;
    ctrl.file_id = 0;
    int pid = proc->p_context.p_ipc_ctrl.file_id;
    struct proc * proc_to_be_deleted = pid_to_proc(pid);
    if (!proc_to_be_deleted || pid == 0 || pid == TIME_DRIVER_PID) // you can not kill kproc(sos), and the time driver
    {
        ERROR_DEBUG("kill pid: %d ESRCH\n", pid);
        ctrl.ret_val = ESRCH;
        ipc_reply(&ctrl, &(proc->p_context.p_reply_cap));
        return;
    }

    // like linux D status, you can't kill it while blocked on nfs or something else
    if (coro_status(proc_to_be_deleted->p_coro) == COROUTINE_SUSPEND && proc_to_be_deleted->p_status.status != PROC_STATUS_SLEEP)
    {

        ERROR_DEBUG("kill pid: %d in D status, failed\n", pid);
        ctrl.ret_val = EPERM;
        ipc_reply(&ctrl, &(proc->p_context.p_reply_cap));
        return;
    }
    // but for sleep, it can be killed.
    if (proc_to_be_deleted->p_status.status == PROC_STATUS_SLEEP)
    {

        ERROR_DEBUG("kill pid: %d in S status\n", pid);
        assert(coro_status(proc_to_be_deleted->p_coro) == COROUTINE_SUSPEND);
        proc_to_be_deleted->p_status.status = PROC_STATUS_RUNNING; // proc_exit only can deal with PROC_STATUS_RUNNING
    }
    proc_exit(proc_to_be_deleted);

    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "sos_syscall_delete_process pid: %d success\n", proc_to_be_deleted->p_pid);

    // TODO we need write something on nc, it kills it self.
    if (proc_to_be_deleted != get_current_proc())
        ipc_reply(&ctrl, &(proc->p_context.p_reply_cap));
}


void sos_syscall_process_status(void * argv)
{
    struct proc* proc = (struct proc*) argv;
    void* ipc_buf = (get_ipc_buffer(proc));
    struct ipc_buffer_ctrl_msg ctrl;
    ctrl.offset = 0;
    if (proc->p_context.p_ipc_ctrl.offset != 4)
    {
        ctrl.ret_val = EINVAL;
        goto end_sos_syscall_process_status;
    }
    int ps_amount = *(int *)ipc_buf;
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "begin sos_syscall_process_status proc %u\n", proc->p_pid);
    /*  */
    /* // Temporary array to hold return values */
    sos_process_t * processes = (sos_process_t *)(get_ipc_buffer(proc) + 4) ;
    int left_length = seL4_PAGE_SIZE - 4;
    int i = 1;
    int j = 0;
    while (i < PID_ARRAY_SIZE && j < ps_amount)
    {
        struct proc* tmp = index_to_proc(i);
        if (tmp == NULL)
        {
            i++;
            continue;
        }
        if (j * sizeof (sos_process_t) >= left_length)
        {
            ctrl.ret_val = EMSGSIZE;
            goto end_sos_syscall_process_status;
        }

        processes[j].ppid = tmp->p_father_pid;
        processes[j].pid = tmp->p_pid;
        processes[j].status = proc_status_display(tmp);
        proc_mem(tmp, &processes[j].size, &processes[j].swap_size);
        processes[j].stime = tmp->p_status.stime;
        // "name [argv0 argv1...]"
        memset(processes[j].command, 0, N_NAME);
        char* s= malloc(strlen(tmp->p_status.name) + 1 + 2 + 1 + strlen(tmp->p_status.argv_str));
        if (s == NULL)
        {
            ctrl.ret_val = ENOMEM;
            goto end_sos_syscall_process_status;
        }
        memcpy(s, tmp->p_status.name, strlen(tmp->p_status.name));
        int idx = strlen(tmp->p_status.name);
        s[idx ++] = ' ';
        s[idx ++] = '[';
        memcpy(s + idx, tmp->p_status.argv_str, strlen(tmp->p_status.argv_str));
        idx += strlen(tmp->p_status.argv_str);
        s[idx ++] = ']';
        s[idx ++] = 0;
        printf ("%s\n", s);
        strncpy(processes[j].command, s, N_NAME - 1);
        free(s);
        processes[j].command[N_NAME - 1] = 0;

        printf ("%d %d %c %d %d %d %s\n", processes[j].pid, processes[j].ppid,
                processes[j].status,
                processes[j].stime,
                processes[j].size,
                processes[j].swap_size,
                processes[j].command);
        i++;
        j++;
    }

    // write processes array and ps_amount to share buffer, then reply
    memcpy((int *)ipc_buf, &j, sizeof(int));

    ctrl.ret_val = 0;
    ctrl.offset = sizeof(int) + j * sizeof(sos_process_t);

end_sos_syscall_process_status:
    ipc_reply(&ctrl, &(proc->p_context.p_reply_cap));
}

// try to kill itself
void sos_syscall_exit_process(void* argv)
{
    struct proc* proc = (struct proc*) argv;
    assert(get_current_proc() == proc);
    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "sos_syscall_exit_process: %d\n", proc->p_pid);
    proc_exit(proc);
}


void sos_syscall_vm_shared(void* argv)
{
    struct proc* proc = (struct proc*) argv;
    assert(get_current_proc() == proc);
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "sos_syscall_vm_shared: %d, addr: 0x%x, length: %u, writable: %d\n", proc->p_pid, proc->p_context.p_ipc_ctrl.start_app_buffer_addr, proc->p_context.p_ipc_ctrl.file_id, proc->p_context.p_ipc_ctrl.seq_num);
    struct ipc_buffer_ctrl_msg ctrl;
    ctrl.offset = 0;
    struct addrspace *as = proc->p_resource.p_addrspace;

    ctrl.ret_val = vm_share(as, proc->p_context.p_ipc_ctrl.start_app_buffer_addr,
                            proc->p_context.p_ipc_ctrl.file_id, proc->p_context.p_ipc_ctrl.seq_num);
    COLOR_DEBUG(DB_SYSCALL,  ctrl.ret_val == 0 ? ANSI_COLOR_GREEN : ANSI_COLOR_RED, "end sos_syscall_vm_shared, proc %u, return addr: 0x%x, length: %u ret: %d\n", proc->p_pid, proc->p_context.p_ipc_ctrl.start_app_buffer_addr,  proc->p_context.p_ipc_ctrl.file_id, ctrl.ret_val);

    ipc_reply(&ctrl, &(proc->p_context.p_reply_cap));
}

void handle_syscall(seL4_Word badge, struct proc * app_process)
{
    assert(app_process != NULL);
    seL4_Word syscall_number;
    seL4_CPtr reply_cap;

    struct ipc_buffer_ctrl_msg * ctrl_msg = &(app_process->p_context.p_ipc_ctrl);
    memcpy(ctrl_msg, seL4_GetIPCBuffer()->msg, sizeof(ipc_buffer_ctrl_msg));

    syscall_number = ctrl_msg->syscall_number;
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "proc: %u, syscall_number: %d, msg_len: %d\n",
        app_process->p_pid, syscall_number, ctrl_msg->offset);

    assert(coro_status(app_process->p_coro) == COROUTINE_INIT);
    /* Save the caller */
    assert(app_process->p_context.p_reply_cap == 0);
    if (syscall_number != SOS_SYSCALL_PROCESS_EXIT)
    {
        reply_cap = cspace_save_reply_cap(cur_cspace);
        assert(reply_cap != CSPACE_NULL);

        // in case the app process block, the reply_cap and message get flushed
        // we put these into `proc struct`
        app_process->p_context.p_reply_cap = reply_cap;
    }

    if (syscall_number < 0 || syscall_number > NUMBER_OF_SYSCALL) {
        printf("%s:%d (%s) Unknown syscall %d\n",
                       __FILE__, __LINE__, __func__, syscall_number);
        assert("unknown syscall number!\n");
    }

    /* Invoke corresponding syscall */
    restart_coro(app_process->p_coro, syscall_func_arr[syscall_number].syscall, app_process);
}
