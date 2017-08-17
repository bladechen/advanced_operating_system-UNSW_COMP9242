/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/****************************************************************************
 *
 *      $Id:  $
 *
 *      Description: Simple milestone 0 code.
 *      		     Libc will need sos_write & sos_read implemented.
 *
 *      Author:      Ben Leslie
 *
 ****************************************************************************/

#include <stdarg.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "ttyout.h"

#include <sel4/sel4.h>

#include <sos.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_WHITE    "\x1b[37m"
#define ANSI_COLOR_RESET   "\x1b[0m"

void ttyout_init(void) {
    /* Perform any initialisation you require here */
}

static size_t sos_debug_print(const void *vData, size_t count) {
    size_t i;
    const char *realdata = vData;
    for (i = 0; i < count; i++)
        seL4_DebugPutChar(realdata[i]);
    return count;
}

/* size_t sos_write(void *vData, size_t count) { */
/*     //implement this to use your syscall */
/*     return sos_debug_print(vData, count); */
/* } */

size_t sos_read(void *vData, size_t count) {
    //implement this to use your syscall
    return 0;
}

int tty_debug_print(const char *fmt, ...)
{
    static char print_buf[4096];
	int ret;
    sos_debug_print(ANSI_COLOR_BLUE, sizeof(ANSI_COLOR_BLUE));
	va_list ap;
	va_start(ap, fmt);
    ret = vsnprintf(print_buf, sizeof(print_buf) - 1, fmt, ap);
	va_end(ap);
    ret = sos_debug_print(print_buf, ret);
    sos_debug_print(ANSI_COLOR_RESET, sizeof(ANSI_COLOR_RESET));
	return ret;
}


/// @brief: protocol type(4bytes) + msg length in bytes (4bytes) + msg([msg length] bytes)
///
/// @param:  buf
/// @param:  buflen
///
/// @return: actually ipc sent msg in bytes if success, otherwise negative number return

int req_ipc_print_console(char* buf, size_t buflen)
{
    // seL4_Word msg[seL4_MsgMaxLength]; therefore should multiply by 4 , transfer int to char

    int max_char = seL4_MsgMaxLength * 4 - 8; // maximum bytes sent supported by ipc
    int can_send_buflen = max_char > buflen ? buflen : max_char;

    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 3 + (can_send_buflen) / 4 + (can_send_buflen % 4 == 0 ? 0 : 1));
    seL4_SetTag(tag);
    seL4_SetMR(0, SYSCALL_IPC_PRINT_COLSOLE);
    seL4_SetMR(1, can_send_buflen);
    seL4_SetMR(2, seL4_IPC_Message);

    char* msg_buf  = (char*)(seL4_GetIPCBuffer()->msg + 3);

    memcpy(msg_buf, buf, can_send_buflen);

    seL4_MessageInfo_t rep_msginfo = seL4_Call(SYSCALL_ENDPOINT_SLOT, tag);
    tty_debug_print("[tty] do sending buflen: %d\n", can_send_buflen);
    assert(0 == seL4_MessageInfo_get_label(rep_msginfo));
    return seL4_GetMR(0);
    
}

static int total_send_len = 0;
// set it to false, it would behave like it used to be
static bool large_buffer_transfer = true;

size_t sos_write(void *vData, size_t count)
{
    size_t sent_len = 0;
    char *buf = (char * ) vData;
    tty_debug_print("[tty] begin sos_write len: %d\n", count);

    if (large_buffer_transfer == false) 
    {
        while (sent_len != count)
        {
            int ret =  req_ipc_print_console(buf + sent_len, count - sent_len);
            if (ret < 0 || ret >= 10000000)
            {
                tty_debug_print("[tty] some error happen, give up retry. ret: %d, total sendlen: %d, total count: %d\n", ret, sent_len, count);
                break;

            }
            sent_len += ret;
            tty_debug_print("[tty] already send %d\n", sent_len);
        }
        total_send_len += sent_len;
        tty_debug_print("[tty] sos_write finish, tty totally sent till now: %d\n", total_send_len);
        return sent_len;

    } else 
    {
        tty_debug_print("[tty] send large buffer, in new syscall work flow, do sending...\n");
        // move it to syscall work flow to see if it works well
        return sos_sys_print_to_console(buf, count);
        // seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 10);
        // seL4_SetTag(tag);
        // seL4_SetMR(0, SYSCALL_IPC_PRINT_COLSOLE);
        // seL4_SetMR(1, 0);
        // seL4_SetMR(2, large_Buffer_Transfer_Message);

        // char * shared_buffer = (char *)(0xA0002000);

        // memset((void*)(0xA0002000), 0, (0xA0006000 - 0xA0002000));

        // size_t buflen = count;
        // size_t sent = 0;
        // while (buflen > 0) 
        // {
        //     if (buflen > (0xA0006000 - 0xA0002000))
        //     {
        //         seL4_SetMR(3, 0xA0002000);
        //         seL4_SetMR(4, 0xA0006000);
        //         memcpy(shared_buffer, buf + sent, (0xA0006000 - 0xA0002000));
        //         sent += (0xA0006000 - 0xA0002000);
        //         buflen -= (0xA0006000 - 0xA0002000);
        //         tty_debug_print("[tty] sned large buffer, do sending buflen, if block...\n");
        //         seL4_MessageInfo_t rep_msginfo = seL4_Call(SYSCALL_ENDPOINT_SLOT, tag);
        //         assert(0 == seL4_MessageInfo_get_label(rep_msginfo));
        //     } else
        //     {
        //         seL4_SetMR(3, 0xA0002000);
        //         seL4_SetMR(4, 0xA0002000 + buflen);
        //         memcpy(shared_buffer, buf + sent, buflen);
        //         sent = buflen;
        //         buflen = 0;
        //         tty_debug_print("[tty] sned large buffer, do sending buflen, else block...\n");
        //         seL4_MessageInfo_t rep_msginfo = seL4_Call(SYSCALL_ENDPOINT_SLOT, tag);
        //         tty_debug_print("[tty] send large buffer, do sending buflen...\n");
        //         assert(0 == seL4_MessageInfo_get_label(rep_msginfo));
        //     }
        // }    
        // return sent;    
    }
}

