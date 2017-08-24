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

int sos_sys_open(const char *path, fmode_t mode)
{

    tty_debug_print("[app] sos_sys_open: %s, with mode %d\n", path, mode);
    struct ipc_buffer_ctrl_msg ctrl_msg ;

    ctrl_msg.syscall_number = SOS_SYSCALL_OPEN;
    ctrl_msg.mode = mode;

    ctrl_msg.offset = strlen(path);
    if (ctrl_msg.offset >= APP_PROCESS_IPC_SHARED_BUFFER_SIZE)
    {
        ctrl_msg.offset = APP_PROCESS_IPC_SHARED_BUFFER_SIZE;
    }

    ctrl_msg.file_id = -1;
    struct ipc_buffer_ctrl_msg ret;
    assert (0 == ipc_call(&ctrl_msg, path, &ret));
    tty_debug_print("[app] sos_sys_open return: val %d, fd %d\n",ret.ret_val,  ret.file_id);

    /* assert(!"You need to implement this"); */
    return (ret.ret_val == 0) ? ret.file_id: ( -ret.ret_val);
}

int sos_sys_read(int file, char *buf, size_t nbyte)
{
    if (buf == NULL ||  nbyte == 0)
    {
        return 0;
    }

    size_t read_len = 0;

    struct ipc_buffer_ctrl_msg  ctrl_msg ;
    ctrl_msg.syscall_number = SOS_SYSCALL_READ;
    ctrl_msg.file_id = file;
    ctrl_msg.start_app_buffer_addr = APP_PROCESS_IPC_SHARED_BUFFER;

    char* shared_buffer = (char *)(APP_PROCESS_IPC_SHARED_BUFFER);
    while (read_len < nbyte)
    {
        ctrl_msg.offset = (nbyte - read_len) > APP_PROCESS_IPC_SHARED_BUFFER_SIZE ? APP_PROCESS_IPC_SHARED_BUFFER_SIZE: nbyte - read_len;
        struct ipc_buffer_ctrl_msg ret;
        assert(0 == ipc_call(&ctrl_msg, NULL, &ret));
        /* tty_debug_print("[sosh] read ret: %d, len: %d, total: %d\n", ret.ret_val, ret.offset, ret.offset + read_len); */
        if (ret.ret_val == 0)
        {
            size_t off = ret.offset;
            if (off < ctrl_msg.offset) // new line comes. return
            {
                memcpy(buf + read_len, shared_buffer, off);
                return read_len + off;
            }
            else
            {
                memcpy(buf + read_len,  shared_buffer, off);
                read_len += off;
            }
        }
        else
        {
            if (read_len == 0)
            {
                assert(-ret.ret_val < 0);
                return -ret.ret_val;
            }
            else
                return read_len;
        }
    }
    return read_len;

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
    int err = 0;
    size_t len = my_serial_send( vData, nbyte, & ctrl_msg, &err);
    if (err == 0)
    {
        return len;
    }
    else
    {
        assert(-err < 0);
        return -err;
    }
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

int64_t sos_sys_time_stamp(void)
{

    struct ipc_buffer_ctrl_msg  ctrl_msg ;
    ctrl_msg.syscall_number = SOS_SYSCALL_TIME_STAMP;
    ctrl_msg.start_app_buffer_addr = APP_PROCESS_IPC_SHARED_BUFFER;

    ctrl_msg.offset = 0;
    struct ipc_buffer_ctrl_msg ret;
    ipc_call(&ctrl_msg, NULL, &ret);
    assert(ret.ret_val == 0);
    return *((uint64_t*)APP_PROCESS_IPC_SHARED_BUFFER);
}

int sos_sys_close(int file)
{
    tty_debug_print("[app] sos_sys_close: fd, %d\n", file);
    struct ipc_buffer_ctrl_msg ctrl_msg ;

    ctrl_msg.syscall_number = SOS_SYSCALL_CLOSE;

    ctrl_msg.file_id = file;
    struct ipc_buffer_ctrl_msg ret;
    ctrl_msg.offset = 0;
    assert (0 == ipc_call(&ctrl_msg, NULL, &ret));
    tty_debug_print("[app] sos_sys_close return: %d\n",  ret.ret_val);

    return ret.ret_val;
}

int sos_stat(const char *path, sos_stat_t *buf)
{
    handle_no_implemented_syscall("sos_stat");
    return 0;
}


int sos_getdirent(int pos, char *name, size_t nbyte)
{
    handle_no_implemented_syscall("sos_getdirent");
    return 0;
}

int ipc_call(const struct ipc_buffer_ctrl_msg* ctrl,const  void* data, struct ipc_buffer_ctrl_msg* ret)
{
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, IPC_CTRL_MSG_LENGTH);
    seL4_SetTag(tag);

    serialize_ipc_ctrl_msg(ctrl);
    char* shared_buffer = (char *)(APP_PROCESS_IPC_SHARED_BUFFER);

    assert(ctrl->offset <= APP_PROCESS_IPC_SHARED_BUFFER_SIZE);
    if (data != NULL) memcpy(shared_buffer, data, ctrl->offset);

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
size_t my_serial_send(const void *vData, size_t count, struct ipc_buffer_ctrl_msg* ctrl, int* err)
{
    size_t sent_len = 0;
    /* tty_debug_print("[app] begin my_serial_send len: %d\n", count); */
    *err = 0;
    while (sent_len != count)
    {
        int can_send_buflen = (count - sent_len) > APP_PROCESS_IPC_SHARED_BUFFER_SIZE? APP_PROCESS_IPC_SHARED_BUFFER_SIZE: count - sent_len;
        ctrl->offset = can_send_buflen;
        struct ipc_buffer_ctrl_msg ret;
        assert(0 == ipc_call(ctrl,  vData + sent_len, &ret));
        if (ret.ret_val != 0)
        {
            tty_debug_print("[app] my_serial_send error!!!!!ret: %d len: %d\n", ret.ret_val, sent_len);
            if (sent_len == 0)
            {
                *err = ret.ret_val;
            }

            return sent_len;
        }
        /* assert(ret.ret_val > 0); */
        sent_len += ret.offset;
    }
    assert(count == sent_len);
    /* tty_debug_print("[app] end my_serial_send len: %d\n", count); */
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

    int err = 0;
    size_t len = my_serial_send( vData, nbyte, & ctrl_msg, &err);
    if (err == 0)
    {
        return len;
    }
    else
    {
        assert(err > 0);
        return -err;
    }
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

