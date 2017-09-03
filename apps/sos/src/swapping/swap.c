#include "swapping.h"
#include "vm/frametable.h"
#include "vm/pagetable.h"
#include <sel4/sel4.h>

static struct vnode * pagefile_vn = NULL;

void init_swapping()
{
    pagefile_vn = (struct vnode *)malloc(sizeof(struct vnode));
    int ret = vfs_open("nfs:pagefile", O_RDWR| O_CREAT, O_RDWR| O_CREAT, &test_vn);
    assert(ret == 0);
}


int write_to_pagefile(seL4_Word sos_vaddr, int offset)
{
    assert(IS_PAGE_ALIGNED(sos_vaddr));
    assert(IS_PAGE_ALIGNED(offset));
    struct iovec iov;
    struct uio u;
    uio_kinit(&iov, &u, sos_vaddr, seL4_PAGE_SIZE, offset, UIO_WRITE);
    int ret = VOP_WRITE(pagefile_vn, &u);
    return ret;
}


int read_from_pagefile(seL4_Word sos_vaddr, int offset)
{
    assert(IS_PAGE_ALIGNED(sos_vaddr));
    assert(IS_PAGE_ALIGNED(offset));
    struct iovec iov;
    struct uio u;
    uio_kinit(&iov, &u, sos_vaddr, seL4_PAGE_SIZE, offset, UIO_READ);
    int ret = VOP_READ(pagefile_vn, &u);
    return ret;
}


