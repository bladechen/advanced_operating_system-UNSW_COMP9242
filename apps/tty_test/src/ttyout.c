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

#include "ttyout.h"

#include <sel4/sel4.h>
#include <sel4/constants.h>

#define SOS_SYSCALL_WRITE_TO_NCCONSOLE 0
#define MIN(a,b) (a > b ? b : a)
#define DIVROUND(a,b) ((a + (b - 1)) / b)

void ttyout_init(void) {
    /* Perform any initialisation you require here */
}

// static size_t sos_debug_print(const void *vData, size_t count) {
//     size_t i;
//     const char *realdata = vData;
//     for (i = 0; i < count; i++)
//         seL4_DebugPutChar(realdata[i]);
//     return count;
// }

size_t _sos_write(void *vData, size_t count) {
    seL4_Word *realdata = (seL4_Word*) vData;
    seL4_IPCBuffer *buf = seL4_GetIPCBuffer(); // the IPCbuffer structure is in seL4 manual chapter 4
    seL4_Word len = 2 + DIVROUND(count, 4); // bytes count to word counts
    seL4_Word real_word_count = MIN(seL4_MsgMaxLength, len); // count how many sel4_Words
    size_t real_byte_count  = MIN(count, (seL4_MsgMaxLength - 2) * 4); // count how many bytes

    /* Set up syscall parameters */
    buf->msg[0] = SOS_SYSCALL_WRITE_TO_NCCONSOLE;
    buf->msg[1] = real_byte_count;

    int i;
    for (i = 2; i < len; i++) {
      buf->msg[i] = realdata[i - 2];
    }

    /* Fire off syscall and wait for reply */
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, real_word_count + 1); // +1 since take tag into consideration
    // invoke on server thread endpoints.
    seL4_Call(SYSCALL_ENDPOINT_SLOT, tag);

    return real_byte_count; 
}

size_t sos_write(void *vData, size_t count) {
    //implement this to use your syscall
    seL4_Word *data;
    seL4_Word woffset;
    size_t bleft;

    seL4_Word *alldata = vData;
    size_t written = 0;

    while (written < count) {
        /* Adjust 'data' pointer and number of remaining bytes */
        woffset = DIVROUND(written, 4);
        data = alldata + woffset;
        bleft = count - written;

        /* Attempt to write remainder */
        written += _sos_write(data, bleft);
    }
 
    return written;
}

size_t sos_read(void *vData, size_t count) {
    //implement this to use your syscall
    return 0;
}

