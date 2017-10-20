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

void static test_randomly_rw(void * argv) 
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
    uio_kinit(&iov, &u, buf, BUF_SIZE, 0, UIO_WRITE);
    ret = VOP_WRITE(test_vn, &u);
    assert(ret == 0);

    int read_buf_size = 26*sizeof(char);
    char * read_buf = (char *)malloc(read_buf_size);

    /* read from offsets 26, should be the second loop of alphabet characters */
    uio_kinit(&iov, &u, read_buf, read_buf_size, 26, UIO_READ);
    ret = VOP_READ(test_vn, &u);
    assert(ret == 0);

    char temp[26];
    memcpy(temp, buf, 26);

    // printf("in test_extreme, read_buf: %s, temp: %s\n", read_buf, temp);

    assert(strcmp(read_buf, temp) == 0);    


    int write_buf_size = read_buf_size;
    char * write_buf = (char *)malloc(write_buf_size);
    for(int i = 0; i < 26; i++)
    {
        write_buf[i] = (char)(i%26+65);
    }

    /* write to the end of file */
    uio_kinit(&iov, &u, write_buf, 26, BUF_SIZE, UIO_WRITE);
    ret = VOP_WRITE(test_vn, &u);
    assert(ret == 0);

    memset(read_buf, 0, 26);

    /* read from 1000(BUF_SIZE), to see if it is the corresponding 26 alphabets */
    uio_kinit(&iov, &u, read_buf, read_buf_size, BUF_SIZE, UIO_READ);
    ret = VOP_READ(test_vn, &u);
    assert(ret == 0);    

    assert(strcmp(read_buf, temp) == 0);
}

void test_randomly_file_read_write(void)
{
    struct coroutine*  coro = create_coro(NULL, NULL);

    restart_coro(coro, test_randomly_rw, NULL);
}


