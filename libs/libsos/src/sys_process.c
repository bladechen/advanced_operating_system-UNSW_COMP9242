#include <stdio.h>
#include <stdlib.h>
#include <sel4/sel4.h>
#include "sos.h"
#include "sys.h"
#include <string.h>
#include <assert.h>


pid_t sos_process_create(const char *path)
{
    // handle_no_implemented_syscall("sos_process_create");

	tty_debug_print("[app] sos_process_create with path %s\n", path);
    struct ipc_buffer_ctrl_msg ctrl_msg ;

    ctrl_msg.syscall_number = SOS_SYSCALL_CREATE_PROCESS;

    ctrl_msg.offset = strlen(path);

    if (ctrl_msg.offset >= APP_PROCESS_IPC_SHARED_BUFFER_SIZE)
    {
        ctrl_msg.offset = APP_PROCESS_IPC_SHARED_BUFFER_SIZE;
    }

    ctrl_msg.file_id = -1;
    struct ipc_buffer_ctrl_msg ret;
    assert (0 == ipc_call(&ctrl_msg, path, &ret));
    tty_debug_print("[app] sos_process_create return %d, pid %d\n", ret.ret_val, ret.file_id);

    assert(ret.ret_val>=0);
    return (ret.ret_val == 0) ? ret.file_id: ( -ret.ret_val);
}

int sos_process_delete(pid_t pid)
{
    handle_no_implemented_syscall("sos_process_delete");
    return 0;
}


pid_t sos_process_wait(pid_t pid)
{
    handle_no_implemented_syscall("sos_process_wait");
    return 0;
}

int sos_process_status(sos_process_t *processes, unsigned max)
{
    handle_no_implemented_syscall("sos_process_status");
    return 0;
}
