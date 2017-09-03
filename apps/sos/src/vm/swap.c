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

bool write_to_pagefile(seL4_Word sos_vaddr, int offset)
{
    assert(pagefile_vn != NULL);
    assert(IS_PAGE_ALIGNED(sos_vaddr));
    assert(IS_PAGE_ALIGNED(offset));
    struct iovec iov;
    struct uio u;
    uio_kinit(&iov, &u, (void*)sos_vaddr, seL4_PAGE_SIZE, offset, UIO_WRITE);
    assert(u.uio_resid == seL4_PAGE_SIZE);
    int ret = VOP_WRITE(pagefile_vn, &u);
    // we must make sure it return success, and it writes 4096bytes to the pagefile
    if (ret != 0 || u.uio_resid != 0)
    {
        return false;
    }
    return true;

}

bool read_from_pagefile(seL4_Word sos_vaddr, int offset)
{
    assert(pagefile_vn != NULL);
    assert(IS_PAGE_ALIGNED(sos_vaddr));
    assert(IS_PAGE_ALIGNED(offset));
    struct iovec iov;
    struct uio u;
    uio_kinit(&iov, &u, (void*)sos_vaddr, seL4_PAGE_SIZE, offset, UIO_READ);
    assert(u.uio_resid == seL4_PAGE_SIZE);
    int ret = VOP_READ(pagefile_vn, &u);
    // we must make sure it return success, and it reads 4096bytes from the page file
    if (ret != 0 || u.uio_resid != 0)
    {
        return false;
    }
    return true;
}

