/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <stdarg.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sos.h>
#include "sys.h"

#include <sel4/sel4.h>



/*
*   These functions are called by APP, mainly used to
*   encapsulate the control messages like the the start
*   of the buffer and the read offset and send to SOS/wait
*   response from sos via IPC.
*/

int sos_sys_open(const char *path, fmode_t mode) {
    assert(!"You need to implement this");
    return -1;
}

int sos_sys_read(int file, char *buf, size_t nbyte) {
    if (nbyte < 0) return -1;
    if (file < 0) return -1;

    size_t buflen = nbyte;
    size_t read_len = 0;

    while (buflen > 0) 
    {
        if (buflen >= APP_PROCESS_IPC_SHARED_BUFFER_SIZE) 
        {
            seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 5);
            seL4_SetTag(tag);

            ipc_buffer_ctrl_msg * ctrl_msg = (ipc_buffer_ctrl_msg *)(seL4_GetIPCBuffer()->msg);

            ctrl_msg->syscall_number = SOS_SYSCALL_READ;
            ctrl_msg->start_app_buffer_addr = APP_PROCESS_IPC_SHARED_BUFFER;
            ctrl_msg->file_id = file;
            // indicate SOS should write how many bytes to shared buffer
            ctrl_msg->offset = APP_PROCESS_IPC_SHARED_BUFFER_SIZE;

            memset((void*)(APP_PROCESS_IPC_SHARED_BUFFER),
                0,
                APP_PROCESS_IPC_SHARED_BUFFER_SIZE);

            /* Reply from SOS*/
            seL4_MessageInfo_t rep_msginfo = seL4_Call(SYSCALL_ENDPOINT_SLOT, tag);
            assert(0 == seL4_MessageInfo_get_label(rep_msginfo));
            // this is the ctrl msg sent from SOS to APP
            ctrl_msg = (ipc_buffer_ctrl_msg *)(seL4_GetIPCBuffer()->msg);
            assert(ctrl_msg->offset == APP_PROCESS_IPC_SHARED_BUFFER_SIZE);

            char * shared_buffer = (char *)(APP_PROCESS_IPC_SHARED_BUFFER);

            memcpy(buf + read_len, shared_buffer, APP_PROCESS_IPC_SHARED_BUFFER_SIZE);

            read_len += APP_PROCESS_IPC_SHARED_BUFFER_SIZE;
            buflen -= APP_PROCESS_IPC_SHARED_BUFFER_SIZE;
        } else 
        {
            seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 5);
            seL4_SetTag(tag);

            ipc_buffer_ctrl_msg * ctrl_msg = (ipc_buffer_ctrl_msg *)(seL4_GetIPCBuffer()->msg);

            ctrl_msg->syscall_number = SOS_SYSCALL_READ;
            ctrl_msg->start_app_buffer_addr = APP_PROCESS_IPC_SHARED_BUFFER;
            ctrl_msg->file_id = file;
            // indicate SOS should write how many bytes to shared buffer
            ctrl_msg->offset = buflen;

            memset((void*)(APP_PROCESS_IPC_SHARED_BUFFER),
                0,
                APP_PROCESS_IPC_SHARED_BUFFER_SIZE);

            /* Reply from SOS*/
            seL4_MessageInfo_t rep_msginfo = seL4_Call(SYSCALL_ENDPOINT_SLOT, tag);
            assert(0 == seL4_MessageInfo_get_label(rep_msginfo));
            // this is the ctrl msg sent from SOS to APP
            ctrl_msg = (ipc_buffer_ctrl_msg *)(seL4_GetIPCBuffer()->msg);
            assert(ctrl_msg->offset == buflen);

            char * shared_buffer = (char *)(APP_PROCESS_IPC_SHARED_BUFFER);

            memcpy(buf + read_len, shared_buffer, buflen);

            read_len += buflen;
            buflen = 0;
        }
    }

    assert(read_len == nbyte);

    return 0;
}

int sos_sys_write(int file, const char *vData, size_t nbyte)
{
    /* assert(vData != NULL); */
    if (nbyte == 0 || vData == NULL)
    {
        return 0;
    }
    struct ipc_buffer_ctrl_msg  ctrl_msg;
    ctrl_msg.syscall_number = SOS_SYSCALL_WRITE;
    ctrl_msg.start_app_buffer_addr = APP_PROCESS_IPC_SHARED_BUFFER;
    ctrl_msg.file_id = file;
    return my_serial_send(vData, nbyte, & ctrl_msg);

}

void sos_sys_usleep(int msec)
{

    if (msec <= 0) return;

    struct ipc_buffer_ctrl_msg ctrl_msg ;

    ctrl_msg.syscall_number = SOS_SYSCALL_USLEEP;

    ctrl_msg.offset = sizeof(int);

    ctrl_msg.file_id = -1;
    struct ipc_buffer_ctrl_msg ret;
    assert (0 == ipc_call(&ctrl_msg, &msec, &ret));
    return ;


}

int64_t sos_sys_time_stamp(void) {

    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, IPC_CTRL_MSG_LENGTH);
    seL4_SetTag(tag);

    ipc_buffer_ctrl_msg * ctrl_msg = (ipc_buffer_ctrl_msg *) seL4_GetIPCBuffer()->msg;

    ctrl_msg->syscall_number = SOS_SYSCALL_TIME_STAMP;
    ctrl_msg->start_app_buffer_addr = APP_PROCESS_IPC_SHARED_BUFFER;


    ctrl_msg->offset = 0;


    seL4_MessageInfo_t rep_msginfo = seL4_Call(SYSCALL_ENDPOINT_SLOT, tag);
    assert(0 == seL4_MessageInfo_get_label(rep_msginfo));

    return *((uint64_t*)APP_PROCESS_IPC_SHARED_BUFFER);
}

int sos_sys_close(int file)
{
    assert(!"You need to implement this");
    return 0;
}

int sos_stat(const char *path, sos_stat_t *buf)
{
    handle_no_implemented_syscall();
    return 0;
}


int sos_getdirent(int pos, char *name, size_t nbyte)
{
    handle_no_implemented_syscall();
    return 0;
}

int ipc_call(struct ipc_buffer_ctrl_msg* ctrl,const  void* data, struct ipc_buffer_ctrl_msg* ret)
{
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, IPC_CTRL_MSG_LENGTH);
    seL4_SetTag(tag);

    serialize_ipc_ctrl_msg(ctrl);
    char* shared_buffer = (char *)(APP_PROCESS_IPC_SHARED_BUFFER);

    assert(ctrl->offset <= APP_PROCESS_IPC_SHARED_BUFFER_SIZE);
    memcpy(shared_buffer, data, ctrl->offset);

    seL4_MessageInfo_t rep_msginfo = seL4_Call(SYSCALL_ENDPOINT_SLOT, tag);
    assert(0 == seL4_MessageInfo_get_label(rep_msginfo));
    unserialize_ipc_ctrl_msg(ret);
    return 0;
}


/*
*   This function will be called in sys_stdio.c, when `printf` try to
*   print to STDOUT/STDERR.
*   This function is different from `sos_sys_write()`
*   It will write to console through serial. This also fulfill
*   purpose of `sos_sys_print_to_console` for m0. And in SOS syscall.c
*   the corresponding function is still named `sos_sys_print_to_console`.
*/
size_t my_serial_send(const void *vData, size_t count, struct ipc_buffer_ctrl_msg* ctrl)
{
    size_t sent_len = 0;
    tty_debug_print("[app] begin my_serial_send len: %d\n", count);
    while (sent_len != count)
    {
        int can_send_buflen = (count - sent_len) > APP_PROCESS_IPC_SHARED_BUFFER_SIZE? APP_PROCESS_IPC_SHARED_BUFFER_SIZE: count - sent_len;
        ctrl->offset = can_send_buflen;
        struct ipc_buffer_ctrl_msg ret;
        assert(0 == ipc_call(ctrl,  vData + sent_len, &ret));
        assert(ret.ret_val > 0);
        sent_len += ret.ret_val;
    }
    assert(count == sent_len);
    return sent_len;
}

int sos_write(const char *vData, size_t nbyte)
{
    /* assert(vData != NULL); */
    if (nbyte == 0 || vData == NULL)
    {
        return 0;
    }
    struct ipc_buffer_ctrl_msg  ctrl_msg;
    ctrl_msg.syscall_number = SOS_SYSCALL_IPC_PRINT_COLSOLE;
    ctrl_msg.start_app_buffer_addr = APP_PROCESS_IPC_SHARED_BUFFER;

    return my_serial_send( vData, nbyte, & ctrl_msg);
}


seL4_Word sos_sys_brk(seL4_Word newbrk)
{

    struct ipc_buffer_ctrl_msg  ctrl_msg ;

    ctrl_msg.syscall_number = SOS_SYSCALL_BRK;
    ctrl_msg.start_app_buffer_addr = APP_PROCESS_IPC_SHARED_BUFFER;

    ctrl_msg.offset = sizeof(seL4_Word);


    struct ipc_buffer_ctrl_msg ret;
    assert (0 == ipc_call(&ctrl_msg, &newbrk, &ret));
    tty_debug_print("ipc_call brk return %d,new brk: 0x%x\n", ret.ret_val, *((seL4_Word *)APP_PROCESS_IPC_SHARED_BUFFER));
    if (ret.ret_val == 0)
    {
        return *((seL4_Word *)APP_PROCESS_IPC_SHARED_BUFFER);
    }
    else
    {
        return 0;
    }
}

