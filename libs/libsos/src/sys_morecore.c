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

/*
 * Statically allocated morecore area.
 *
 * This is rather terrible, but is the simplest option without a
 * huge amount of infrastructure.
 */
/* #define MORECORE_AREA_BYTE_SIZE 0x100000 */
/* char __morecore_area[MORECORE_AREA_BYTE_SIZE]; */
static const uint32_t morecore_area = (0x20001000);
#define MORECORE_AREA_BYTE_SIZE (0x21FFE000 - 0x20001000)

/* Pointer to free space in the morecore area. */
/* static uintptr_t morecore_base = (uintptr_t) &morecore_area; */
/* static uintptr_t morecore_top = (uintptr_t) &morecore_area[MORECORE_AREA_BYTE_SIZE]; */
static uintptr_t morecore_base = 0x20001000;
static uintptr_t morecore_top = (uintptr_t) (0x21FFE000);

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

    // /*if the newbrk is 0, return the bottom of the heap*/
    // if (!newbrk) {
    //     ret = morecore_base;
    // } else if (newbrk < morecore_top && newbrk > (uintptr_t)morecore_area) {
    //     ret = morecore_base = newbrk;
    // } else {
    //     ret = 0;
    // }

    // return ret;

    /*if the newbrk is 0 or below heap region, return the bottom of the heap*/
    if (newbrk == 0 || newbrk < (uintptr_t) morecore_base) {
        ret = morecore_base;
    } else {
        /* All other region checking can be handled by SOS */
        ret = sos_sys_brk((seL4_Word) newbrk);
        if (ret) morecore_base = ret;
    }

    return ret;
}

/* Large mallocs will result in muslc calling mmap, so we do a minimal implementation
   here to support that. We make a bunch of assumptions in the process */
long
sys_mmap2(va_list ap)
{
    void *addr = va_arg(ap, void*);
    size_t length = va_arg(ap, size_t);
    int prot = va_arg(ap, int);
    int flags = va_arg(ap, int);
    int fd = va_arg(ap, int);
    off_t offset = va_arg(ap, off_t);
    (void)addr;
    (void)prot;
    (void)fd;
    (void)offset;
    if (flags & MAP_ANONYMOUS) {
        /* Steal from the top */
        uintptr_t base = morecore_top - length;
        if (base < morecore_base) {
            return -ENOMEM;
        }
        morecore_top = base;
        return base;
    }
    assert(!"not implemented");
    return -ENOMEM;
}

long
sys_mremap(va_list ap)
{
    assert(!"not implemented");
    return -ENOMEM;
}
