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


// static size_t sos_debug_print(const void *vData, size_t count) {
//     size_t i;
//     const char *realdata = vData;
//     for (i = 0; i < count; i++)
//         seL4_DebugPutChar(realdata[i]);
//     return count;
// }

// static int tty_debug_print(const char *fmt, ...)
// {
//     static char print_buf[4096];
//  int ret;
//     sos_debug_print(ANSI_COLOR_BLUE, sizeof(ANSI_COLOR_BLUE));
//  va_list ap;
//  va_start(ap, fmt);
//     ret = vsnprintf(print_buf, sizeof(print_buf) - 1, fmt, ap);
//  va_end(ap);
//     ret = sos_debug_print(print_buf, ret);
//     sos_debug_print(ANSI_COLOR_RESET, sizeof(ANSI_COLOR_RESET));
//  return ret;
// }


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
    if (vData == NULL) return -1;
    if (vData == 0) return -1;
    if (nbyte < 0) return -1;
    if (file < 0) return -1;

    size_t buflen = nbyte;
    size_t sent = 0;
    while (buflen > 0)
    {
        seL4_DebugPutChar('A');
        seL4_DebugPutChar('\n');
        if (buflen >= APP_PROCESS_IPC_SHARED_BUFFER_SIZE)
        {
            seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 5);
            seL4_SetTag(tag);

            ipc_buffer_ctrl_msg * ctrl_msg = (ipc_buffer_ctrl_msg *)(seL4_GetIPCBuffer()->msg);

            ctrl_msg->syscall_number = SOS_SYSCALL_WRITE;
            ctrl_msg->start_app_buffer_addr = APP_PROCESS_IPC_SHARED_BUFFER;
            ctrl_msg->file_id = file;

            char * shared_buffer = (char *)(APP_PROCESS_IPC_SHARED_BUFFER);

            memset((void*)(APP_PROCESS_IPC_SHARED_BUFFER),
                0,
                APP_PROCESS_IPC_SHARED_BUFFER_SIZE);

            memcpy(shared_buffer, vData + sent, APP_PROCESS_IPC_SHARED_BUFFER_SIZE);
            sent += APP_PROCESS_IPC_SHARED_BUFFER_SIZE;
            buflen -= APP_PROCESS_IPC_SHARED_BUFFER_SIZE;
            ctrl_msg->offset = APP_PROCESS_IPC_SHARED_BUFFER_SIZE;

            // memcpy(seL4_GetIPCBuffer()->msg, ctrl_msg, sizeof(ipc_buffer_ctrl_msg));

            seL4_MessageInfo_t rep_msginfo = seL4_Call(SYSCALL_ENDPOINT_SLOT, tag);
            assert(0 == seL4_MessageInfo_get_label(rep_msginfo));
        } else
        {
            seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 5);
            seL4_SetTag(tag);

            ipc_buffer_ctrl_msg * ctrl_msg = (ipc_buffer_ctrl_msg *)(seL4_GetIPCBuffer()->msg);

            ctrl_msg->syscall_number = SOS_SYSCALL_WRITE;
            ctrl_msg->start_app_buffer_addr = APP_PROCESS_IPC_SHARED_BUFFER;
            ctrl_msg->file_id = file;

            char * shared_buffer = (char *)(APP_PROCESS_IPC_SHARED_BUFFER);

            memset((void*)(APP_PROCESS_IPC_SHARED_BUFFER),
                0,
                APP_PROCESS_IPC_SHARED_BUFFER_SIZE);

            memcpy(shared_buffer, vData + sent, buflen);
            sent = buflen;
            buflen = 0;
            // seL4_SetMR(2, sent);
            ctrl_msg->offset = sent;
            // tty_debug_print("[tty] sned large buffer, do sending buflen, else block...\n");
            memcpy(seL4_GetIPCBuffer()->msg, ctrl_msg, sizeof(ipc_buffer_ctrl_msg));

            seL4_MessageInfo_t rep_msginfo = seL4_Call(SYSCALL_ENDPOINT_SLOT, tag);
            // tty_debug_print("[tty] send large buffer, do sending buflen...\n");
            assert(0 == seL4_MessageInfo_get_label(rep_msginfo));
        }
    }
    return sent;  
}

void sos_sys_usleep(int msec) {

    printf("soshhhh, In sos_sys_usleep function\n");

    if (msec <= 0) return;

    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 5);
    seL4_SetTag(tag);

    ipc_buffer_ctrl_msg * ctrl_msg = (ipc_buffer_ctrl_msg *) seL4_GetIPCBuffer()->msg;

    ctrl_msg->syscall_number = SOS_SYSCALL_USLEEP;
    ctrl_msg->start_app_buffer_addr = APP_PROCESS_IPC_SHARED_BUFFER;

    // char * shared_buffer = (char *)(APP_PROCESS_IPC_SHARED_BUFFER);

    memset((void*)(APP_PROCESS_IPC_SHARED_BUFFER),
        0,
        APP_PROCESS_IPC_SHARED_BUFFER_SIZE);

    *(int *)APP_PROCESS_IPC_SHARED_BUFFER = msec;

    ctrl_msg->offset = sizeof(int);

    ctrl_msg->file_id = -1;

    // memcpy(seL4_GetIPCBuffer()->msg, ctrl_msg, sizeof(ipc_buffer_ctrl_msg)); 

    seL4_MessageInfo_t rep_msginfo = seL4_Call(SYSCALL_ENDPOINT_SLOT, tag);
    assert(0 == seL4_MessageInfo_get_label(rep_msginfo));
}

int64_t sos_sys_time_stamp(void) {

    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 5);
    seL4_SetTag(tag);

    /* ipc_buffer_ctrl_msg * ctrl_msg = (ipc_buffer_ctrl_msg *)malloc(sizeof(ipc_buffer_ctrl_msg)); */
    ipc_buffer_ctrl_msg * ctrl_msg = (ipc_buffer_ctrl_msg *) seL4_GetIPCBuffer()->msg;

    ctrl_msg->syscall_number = SOS_SYSCALL_TIME_STAMP;
    ctrl_msg->start_app_buffer_addr = APP_PROCESS_IPC_SHARED_BUFFER;

    // char * shared_buffer = (char *)(APP_PROCESS_IPC_SHARED_BUFFER);

    memset((void*)(APP_PROCESS_IPC_SHARED_BUFFER),
        0,
        APP_PROCESS_IPC_SHARED_BUFFER_SIZE);

    /* *(int *)APP_PROCESS_IPC_SHARED_BUFFER = msec; */

    ctrl_msg->offset = 0;

    /* ctrl_msg->file_id = -1; */

    /* memcpy(seL4_GetIPCBuffer()->msg, ctrl_msg, sizeof(ipc_buffer_ctrl_msg)); */

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

/*
*   This function will be called in sys_stdio.c, when `printf` try to
*   print to STDOUT/STDERR.
*   This function is different from `sos_sys_write()`
*   It will write to console through serial. This also fulfill
*   purpose of `sos_sys_print_to_console` for m0. And in SOS syscall.c
*   the corresponding function is still named `sos_sys_print_to_console`.
*/
int sos_write(const char *vData, size_t nbyte)
{
    if (vData == NULL) return -1;
    if (vData == 0) return -1;

    size_t buflen = nbyte;
    size_t sent = 0;
    while (buflen > 0)
    {
        seL4_DebugPutChar('W');
        seL4_DebugPutChar('\n');
        if (buflen >= APP_PROCESS_IPC_SHARED_BUFFER_SIZE)
        {
            seL4_DebugPutChar('F');
            seL4_DebugPutChar('\n');
            seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 5);
            seL4_SetTag(tag);

            ipc_buffer_ctrl_msg * ctrl_msg = (ipc_buffer_ctrl_msg *)(seL4_GetIPCBuffer()->msg);

            ctrl_msg->syscall_number = SOS_SYSCALL_IPC_PRINT_COLSOLE;
            ctrl_msg->start_app_buffer_addr = APP_PROCESS_IPC_SHARED_BUFFER;

            char * shared_buffer = (char *)(APP_PROCESS_IPC_SHARED_BUFFER);

            memset((void*)(APP_PROCESS_IPC_SHARED_BUFFER),
                0,
                APP_PROCESS_IPC_SHARED_BUFFER_SIZE);


            memcpy(shared_buffer, vData + sent, APP_PROCESS_IPC_SHARED_BUFFER_SIZE);
            sent += APP_PROCESS_IPC_SHARED_BUFFER_SIZE;
            buflen -= APP_PROCESS_IPC_SHARED_BUFFER_SIZE;
            // seL4_SetMR(2, APP_PROCESS_IPC_SHARED_BUFFER_SIZE);
            ctrl_msg->offset = APP_PROCESS_IPC_SHARED_BUFFER_SIZE;
            // tty_debug_print("[tty] sned large buffer, do sending buflen, if block...\n");

            /* memcpy(seL4_GetIPCBuffer()->msg, ctrl_msg, sizeof(ipc_buffer_ctrl_msg)); */

            seL4_MessageInfo_t rep_msginfo = seL4_Call(SYSCALL_ENDPOINT_SLOT, tag);
            assert(0 == seL4_MessageInfo_get_label(rep_msginfo));
        } else
        {
            seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 5);
            seL4_SetTag(tag);

            ipc_buffer_ctrl_msg * ctrl_msg = (ipc_buffer_ctrl_msg *)(seL4_GetIPCBuffer()->msg);

            ctrl_msg->syscall_number = SOS_SYSCALL_IPC_PRINT_COLSOLE;
            ctrl_msg->start_app_buffer_addr = APP_PROCESS_IPC_SHARED_BUFFER;

            char * shared_buffer = (char *)(APP_PROCESS_IPC_SHARED_BUFFER);

            memset((void*)(APP_PROCESS_IPC_SHARED_BUFFER),
                0,
                APP_PROCESS_IPC_SHARED_BUFFER_SIZE);

            memcpy(shared_buffer, vData + sent, buflen);
            sent = buflen;
            buflen = 0;
            // seL4_SetMR(2, sent);
            ctrl_msg->offset = sent;
            // tty_debug_print("[tty] sned large buffer, do sending buflen, else block...\n");

            /* memcpy(seL4_GetIPCBuffer()->msg, ctrl_msg, sizeof(ipc_buffer_ctrl_msg)); */

            seL4_MessageInfo_t rep_msginfo = seL4_Call(SYSCALL_ENDPOINT_SLOT, tag);
            // tty_debug_print("[tty] send large buffer, do sending buflen...\n");
            assert(0 == seL4_MessageInfo_get_label(rep_msginfo));
        }
    }
    return sent;
}


seL4_Word sos_sys_brk(seL4_Word newbrk)
{
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 5);
    seL4_SetTag(tag);

    ipc_buffer_ctrl_msg * ctrl_msg = (ipc_buffer_ctrl_msg *) seL4_GetIPCBuffer()->msg;

    ctrl_msg->syscall_number = SOS_SYSCALL_BRK;
    ctrl_msg->start_app_buffer_addr = APP_PROCESS_IPC_SHARED_BUFFER;

    memset((void*)(APP_PROCESS_IPC_SHARED_BUFFER),
        0,
        APP_PROCESS_IPC_SHARED_BUFFER_SIZE);

    *(seL4_Word *)APP_PROCESS_IPC_SHARED_BUFFER = newbrk;

    ctrl_msg->offset = sizeof(seL4_Word);

    ctrl_msg->file_id = -1;

    seL4_MessageInfo_t rep_msginfo = seL4_Call(SYSCALL_ENDPOINT_SLOT, tag);

    // receive message from SOS
    if(0 == seL4_MessageInfo_get_label(rep_msginfo)) 
    {
        return newbrk;
    } else
    {
        return NULL; // return error message or return NULL?
    }
}

