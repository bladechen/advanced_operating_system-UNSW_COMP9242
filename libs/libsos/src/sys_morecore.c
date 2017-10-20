/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <autoconf.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <errno.h>
#include <assert.h>
#include <sos.h>
#include <ttyout.h>

/*
 * Statically allocated morecore area.
 *
 * This is rather terrible, but is the simplest option without a
 * huge amount of infrastructure.
 */
/* #define MORECORE_AREA_BYTE_SIZE 0x100000 */
/* char __morecore_area[MORECORE_AREA_BYTE_SIZE]; */

/* Pointer to free space in the morecore area. */
/* static uintptr_t morecore_base = (uintptr_t) &morecore_area; */
/* static uintptr_t morecore_top = (uintptr_t) &morecore_area[MORECORE_AREA_BYTE_SIZE]; */
/* static uintptr_t morecore_base = 0x20001000; */
static uintptr_t morecore_base = 0xFFFFFFFFU;

/* Actual morecore implementation
   returns 0 if failure, returns newbrk if success.
*/

/*
*   it is brk or sbrk?, does not conform to linux definition
*   int brk(void *addr);
*   void *sbrk(intptr_t increment);
*/
long
sys_brk(va_list ap)
{

    uintptr_t ret;
    uintptr_t newbrk = va_arg(ap, uintptr_t);
    /*  */
    /* #<{(|if the newbrk is 0, return the bottom of the heap|)}># */
    /* if (!newbrk) { */
    /*     ret = morecore_base; */
    /* } else if (newbrk < morecore_top && newbrk > (uintptr_t)morecore_area) { */
    /*     ret = morecore_base = newbrk; */
    /* } else { */
    /*     ret = 0; */
    /* } */
    /*  */
    /* return ret; */
    if (morecore_base == 0xFFFFFFFFU )
    {
        assert(newbrk == 0);
        morecore_base = sos_sys_brk(0);
        if (morecore_base == 0)
        {
            assert (0);
            morecore_base = 0xFFFFFFFFU;
            return 0;
        }

        tty_debug_print("first time calling: 0x%x,  current base: 0x%x\n", newbrk, morecore_base);
        return morecore_base;
    }

    /*if the newbrk is 0 or below heap region, return the bottom of the heap*/
    if (newbrk == 0 || newbrk < (uintptr_t) morecore_base) {
        ret = morecore_base;
    }
    else
    {
        /* All other region checking can be handled by SOS */
        ret = sos_sys_brk((seL4_Word) newbrk);

        if (ret) morecore_base = ret;
    }
    tty_debug_print("after calling sys_brk in newbrk: 0x%x, ret:0x%x, current base: 0x%x\n", newbrk, ret, morecore_base);

    return ret;
}

/* Large mallocs will result in muslc calling mmap, so we do a minimal implementation
   here to support that. We make a bunch of assumptions in the process */
long
sys_mmap2(va_list ap)
{
    tty_debug_print("begin sys_mmap2\n");
    void *addr = va_arg(ap, void*);
    size_t length = va_arg(ap, size_t);
    int prot = va_arg(ap, int);
    int flags = va_arg(ap, int);
    int fd = va_arg(ap, int);
    off_t offset = va_arg(ap, off_t);
    tty_debug_print("sys_mmap2 at: 0x%x, length: %u\n",
                    addr, length);


    sos_mmap_t argv ;
    argv.addr = (uint32_t)(addr);
    argv.length= (length);
    argv.prot = (prot);
    argv.flags = (flags);
    argv.fd = (fd);
    argv.offset= (offset);
    /* if (flags & MAP_ANONYMOUS) { */
    /*     #<{(| Steal from the top |)}># */
    /*     uintptr_t base = morecore_top - length; */
    /*     if (base < morecore_base) { */
    /*         printf ("not mem\n"); */
    /*         return -ENOMEM; */
    /*     } */
    /*     morecore_top = base; */
    /*     return base; */
    /* } */


    struct ipc_buffer_ctrl_msg ctrl_msg ;
    ctrl_msg.syscall_number = SOS_SYSCALL_MMAP;
    ctrl_msg.offset = sizeof(sos_mmap_t);
    /* memcpy((void*)APP_PROCESS_IPC_SHARED_BUFFER, &argv, ctrl_msg.offset); */
    struct ipc_buffer_ctrl_msg ret;
    assert (0 == ipc_call(&ctrl_msg, &argv, &ret));
    assert((ret.ret_val == 0 && ret.offset >= 4) ||
           ret.ret_val != 0);

    uint32_t raddr = *(uint32_t*)(APP_PROCESS_IPC_SHARED_BUFFER);
    return ret.ret_val == 0 ? raddr : -1;
}

long sys_munmap(va_list ap)
{
    void *addr = va_arg(ap, void*);
    size_t length = va_arg(ap, size_t);
    tty_debug_print("sys_munmap at: 0x%x, length: %u\n",
                    addr, length);
    sos_mmap_t argv;
    argv.addr = (uint32_t)(addr);
    argv.length = (length);

    struct ipc_buffer_ctrl_msg ctrl_msg ;
    ctrl_msg.syscall_number = SOS_SYSCALL_MUNMAP;
    ctrl_msg.offset = sizeof(sos_mmap_t);

    memcpy((void*)APP_PROCESS_IPC_SHARED_BUFFER, &argv, ctrl_msg.offset);
    struct ipc_buffer_ctrl_msg ret;
    assert (0 == ipc_call(&ctrl_msg, &argv, &ret));
    return ret.ret_val;
}


long
sys_mremap(va_list ap)
{
    assert(!"not implemented");
    return -ENOMEM;
}
