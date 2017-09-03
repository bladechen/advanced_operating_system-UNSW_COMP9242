#include "swap.h"
#include "vm/frametable.h"
#include "vfs/vfs.h"
#include "vfs/uio.h"
#include "vfs/vnode.h"
#include "vm/pagetable.h"
#include <sel4/sel4.h>

static struct vnode * pagefile_vn = NULL;

void init_swapping()
{
    assert(pagefile_vn == NULL);
    char* path = "nfs:pagefile";
    assert(0 == vfs_open(path, O_RDWR|O_CREAT, O_RDWR|O_CREAT, &pagefile_vn));
    assert(pagefile_vn != NULL);
}

int write_to_pagefile(seL4_Word sos_vaddr, int offset)
{
    assert(pagefile_vn != NULL);
    assert(IS_PAGE_ALIGNED(sos_vaddr));
    assert(IS_PAGE_ALIGNED(offset));
    struct iovec iov;
    struct uio u;
    uio_kinit(&iov, &u, (void*)sos_vaddr, seL4_PAGE_SIZE, offset, UIO_WRITE);
    return VOP_WRITE(pagefile_vn, &u);
}

int read_from_pagefile(seL4_Word sos_vaddr, int offset)
{
    assert(pagefile_vn != NULL);
    assert(IS_PAGE_ALIGNED(sos_vaddr));
    assert(IS_PAGE_ALIGNED(offset));
    struct iovec iov;
    struct uio u;
    uio_kinit(&iov, &u, (void*)sos_vaddr, seL4_PAGE_SIZE, offset, UIO_READ);
    return  VOP_READ(pagefile_vn, &u);
}

