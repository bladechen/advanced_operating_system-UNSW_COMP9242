#include "test.h"
#define verbose 5
#include "sys/debug.h"
#include "comm/comm.h"
#include "vfs/vfs.h"
#include "vfs/vnode.h"
#include "dev/nfs.h"
#include <fcntl.h>
#include "vfs/uio.h"
#include "coroutine/coro.h"

#define BUF_SIZE 1000

void static test_extreme(void * argv) 
{
	int ret;

    struct vnode * test_vn = (struct vnode *)malloc(sizeof(struct vnode));

    ret = vfs_open("nfs:test.txt", O_RDWR| O_CREAT, O_RDWR| O_CREAT, &test_vn);

    assert(ret == 0);

    char buf[BUF_SIZE] = {0};
    for(int i = 0; i < BUF_SIZE; i++)
    {
    	buf[i] = (char)(i%26+65);
    }

    struct iovec iov;
    struct uio u;
    // size_t old = f->f_pos;
    uio_kinit(&iov, &u, buf, BUF_SIZE, 0, UIO_WRITE);
    ret = VOP_WRITE(test_vn, &u);


    

    assert(ret == 0);
}

void test_extreme_file_read_write(void)
{
	struct coroutine*  coro = create_coro(NULL, NULL);

	restart_coro(coro, test_extreme, NULL);
}


