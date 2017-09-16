#include <stdio.h>
#include <stdlib.h>
#include <sel4/sel4.h>
#include "sos.h"
#include "sys.h"
#include <string.h>
#include <assert.h>

// TODO check all the proc syscall!!

pid_t sos_process_exec(int argc, char** argv)
{
    tty_debug_print("[app] sos_process_create with path %s\n", argv[0]);
    struct ipc_buffer_ctrl_msg ctrl_msg ;

    ctrl_msg.syscall_number = SOS_SYSCALL_CREATE_PROCESS;

    char tmp_buf[4096];
    ctrl_msg.offset = serialize_exec_argv(tmp_buf, 4096, argc, argv);

    if (ctrl_msg.offset <= 0)
    {
        tty_debug_print("[app] exec too long argv\n");
        return -1;
    }

    // currently, use file_id field to transfer proc_id
    ctrl_msg.file_id = -1;
    struct ipc_buffer_ctrl_msg ret;
    assert (0 == ipc_call(&ctrl_msg, tmp_buf, &ret));
    tty_debug_print("[app] sos_process_create return %d, pid %d\n", ret.ret_val, ret.file_id);

    return (ret.ret_val == 0) ? ret.file_id: ( -ret.ret_val);

}

pid_t sos_process_create(const char *path)
{
    int argc = 1;
    char *argv[2];
    argv[0] = (char*) path;
    argv[1] = NULL;
    return sos_process_exec(argc, argv);
}

int sos_process_delete(pid_t pid)
{
    tty_debug_print("[app] sos_process_delete with pid %d\n", pid);
    struct ipc_buffer_ctrl_msg ctrl_msg ;

    ctrl_msg.syscall_number = SOS_SYSCALL_PROCESS_DELETE;

    // currently, use file_id field to transfer proc_id
    ctrl_msg.file_id = pid;
    ctrl_msg.offset = 0;
    struct ipc_buffer_ctrl_msg ret;
    assert (0 == ipc_call(&ctrl_msg, NULL, &ret));
    tty_debug_print("[app] sos_process_delete return %d\n", ret.ret_val);

    assert(ret.ret_val == 0);
    // return (ret.ret_val == 0) ? ret.file_id: ( -ret.ret_val);
    return 0;
}


pid_t sos_process_wait(pid_t pid)
{
    tty_debug_print("[app] sos_process_wait with pid %d\n", pid);
    struct ipc_buffer_ctrl_msg ctrl_msg ;
    ctrl_msg.syscall_number = SOS_SYSCALL_PROCESS_WAIT;

    // currently, use file_id field to transfer proc_id
    ctrl_msg.file_id = pid;
    struct ipc_buffer_ctrl_msg ret;
    ctrl_msg.offset = 0;
    assert (0 == ipc_call(&ctrl_msg, NULL, &ret));
    // FIXME implement myid
    tty_debug_print("[app] sos_process_wait return %d\n", ret.ret_val);
    assert(ret.ret_val == 0);
    return 0;
}

int sos_process_status(sos_process_t *processes, unsigned max)
{
    // handle_no_implemented_syscall("sos_process_status");
    tty_debug_print("[app] sos_process_status \n");
    struct ipc_buffer_ctrl_msg ctrl_msg ;

    ctrl_msg.syscall_number = SOS_SYSCALL_PROCESS_STATUS;

    // pass max to SOS
    ctrl_msg.offset = sizeof(int);
    ctrl_msg.start_app_buffer_addr = APP_PROCESS_IPC_SHARED_BUFFER;
    *(int *)(APP_PROCESS_IPC_SHARED_BUFFER) = max;

    struct ipc_buffer_ctrl_msg ret;
    assert (0 == ipc_call(&ctrl_msg, NULL, &ret));
    tty_debug_print("[app] sos_process_status return %d\n", ret.ret_val);

    assert(ret.ret_val == 0);

    int ps_amount = *(int *)(APP_PROCESS_IPC_SHARED_BUFFER);

    seL4_Word processes_start_addr = APP_PROCESS_IPC_SHARED_BUFFER + sizeof(int);
    memcpy(processes, processes_start_addr, ps_amount * sizeof(sos_process_t));

    return ps_amount;
}


void sos_process_exit()
{
    tty_debug_print("[app] sos_process_exit\n");

    /* should use seL4_Send, since we do not expect any response*/
    struct ipc_buffer_ctrl_msg ctrl_msg ;
    ctrl_msg.offset = 0;

    ctrl_msg.syscall_number = SOS_SYSCALL_PROCESS_EXIT;

    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, IPC_CTRL_MSG_LENGTH);
    seL4_SetTag(tag);

    serialize_ipc_ctrl_msg(&ctrl_msg);
    seL4_Send(SYSCALL_ENDPOINT_SLOT, tag);
}
