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
// 	int ret;
//     sos_debug_print(ANSI_COLOR_BLUE, sizeof(ANSI_COLOR_BLUE));
// 	va_list ap;
// 	va_start(ap, fmt);
//     ret = vsnprintf(print_buf, sizeof(print_buf) - 1, fmt, ap);
// 	va_end(ap);
//     ret = sos_debug_print(print_buf, ret);
//     sos_debug_print(ANSI_COLOR_RESET, sizeof(ANSI_COLOR_RESET));
// 	return ret;
// }


/*
*	These functions are called by APP, mainly used to
*	encapsulate the control messages like the the start
* 	of the buffer and the read offset and send to SOS/wait
* 	response from sos via IPC.
*/

int sos_sys_open(const char *path, fmode_t mode) {
    assert(!"You need to implement this");
    return -1;
}

int sos_sys_read(int file, char *buf, size_t nbyte) {
    assert(!"You need to implement this");
    return -1;
}

int sos_sys_write(int file, const char *buf, size_t nbyte)
{
    return 0;
	// if (nbyte == 0) return 0;
 //    if (buf == NULL) return -1;
 //    if (file < 0) return -1;

 //    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 10);
 //    seL4_SetTag(tag);
 //    seL4_SetMR(0, SOS_SYSCALL_WRITE);
 //    seL4_SetMR(1, large_Buffer_Transfer_Message);
 //    seL4_SetMR(4, nbyte);
 //    seL4_SetMR(5, file);

 //    memset((void*)(APP_PROCESS_IPC_SHARED_BUFFER),
 //    	0,
 //    	APP_PROCESS_IPC_SHARED_BUFFER_SIZE);

 //    char * shared_buffer = (char *)(APP_PROCESS_IPC_SHARED_BUFFER);

 //    size_t buflen = nbyte;
 //    size_t sent = 0;
 //    while (buflen > 0)
 //    {
 //        if (buflen >= APP_PROCESS_IPC_SHARED_BUFFER_SIZE)
 //        {
 //            seL4_SetMR(2, APP_PROCESS_IPC_SHARED_BUFFER);
 //            seL4_SetMR(3, APP_PROCESS_IPC_SHARED_BUFFER_GUARD);
 //            memcpy(shared_buffer, buf + sent, APP_PROCESS_IPC_SHARED_BUFFER_SIZE);
 //            sent += APP_PROCESS_IPC_SHARED_BUFFER_SIZE;
 //            buflen -= APP_PROCESS_IPC_SHARED_BUFFER_SIZE;
 //            // tty_debug_print("[tty] sned large buffer, do sending buflen, if block...\n");
 //            seL4_MessageInfo_t rep_msginfo = seL4_Call(SYSCALL_ENDPOINT_SLOT, tag);
 //            assert(0 == seL4_MessageInfo_get_label(rep_msginfo));
 //        } else
 //        {
 //            seL4_SetMR(2, APP_PROCESS_IPC_SHARED_BUFFER);
 //            seL4_SetMR(3, APP_PROCESS_IPC_SHARED_BUFFER + buflen);
 //            memcpy(shared_buffer, buf + sent, buflen);
 //            sent += buflen;
 //            buflen = 0;
 //            // tty_debug_print("[tty] sned large buffer, do sending buflen, else block...\n");
 //            seL4_MessageInfo_t rep_msginfo = seL4_Call(SYSCALL_ENDPOINT_SLOT, tag);
 //            // tty_debug_print("[tty] send large buffer, do sending buflen...\n");
 //            assert(0 == seL4_MessageInfo_get_label(rep_msginfo));
 //        }
 //    }
 //    return 0;
}

void sos_sys_usleep(int msec) {
    assert(!"You need to implement this");
}

int64_t sos_sys_time_stamp(void) {
    assert(!"You need to implement this");
    return -1;
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

/* int sos_write(const char *buf, size_t nbyte) */
/* { */
/*     handle_no_implemented_syscall(); */
/*     return 0; */
/*  */
/*  */
/* } */
int sos_sys_print_to_console(char * vData, size_t nbyte)
{
	if (vData == NULL) return -1;
	if (vData == 0) return -1;

	seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 5);
    seL4_SetTag(tag);
    seL4_SetMR(0, SOS_SYSCALL_IPC_PRINT_COLSOLE);
    seL4_SetMR(1, APP_PROCESS_IPC_SHARED_BUFFER);

    char * shared_buffer = (char *)(APP_PROCESS_IPC_SHARED_BUFFER);

    memset((void*)(APP_PROCESS_IPC_SHARED_BUFFER),
    	0,
    	APP_PROCESS_IPC_SHARED_BUFFER_SIZE);

    size_t buflen = nbyte;
    size_t sent = 0;
    while (buflen > 0)
    {
        if (buflen >= APP_PROCESS_IPC_SHARED_BUFFER_SIZE)
        {
            memcpy(shared_buffer, vData + sent, APP_PROCESS_IPC_SHARED_BUFFER_SIZE);
            sent += APP_PROCESS_IPC_SHARED_BUFFER_SIZE;
            buflen -= APP_PROCESS_IPC_SHARED_BUFFER_SIZE;
            seL4_SetMR(2, APP_PROCESS_IPC_SHARED_BUFFER_SIZE);
            // tty_debug_print("[tty] sned large buffer, do sending buflen, if block...\n");
            seL4_MessageInfo_t rep_msginfo = seL4_Call(SYSCALL_ENDPOINT_SLOT, tag);
            assert(0 == seL4_MessageInfo_get_label(rep_msginfo));
        } else
        {
            memcpy(shared_buffer, vData + sent, buflen);
            sent = buflen;
            buflen = 0;
            seL4_SetMR(2, sent);
            // tty_debug_print("[tty] sned large buffer, do sending buflen, else block...\n");
            seL4_MessageInfo_t rep_msginfo = seL4_Call(SYSCALL_ENDPOINT_SLOT, tag);
            // tty_debug_print("[tty] send large buffer, do sending buflen...\n");
            assert(0 == seL4_MessageInfo_get_label(rep_msginfo));
        }
    }
    return sent;
}

