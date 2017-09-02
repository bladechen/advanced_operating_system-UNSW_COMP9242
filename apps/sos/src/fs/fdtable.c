
#include "file_syscall.h"
#include <vfs/vnode.h>
#include <proc/proc.h>
#include "coroutine/coro.h"
#include "fdtable.h"
#include "file.h"


static int get_rightmost_zero_bit(unsigned int in)
{
    return ((~in) & (in + 1));
}
static int __get_bit(int nr, volatile void * addr)
{
    assert(nr >= 0);
     int *m = ((int *) addr) + (nr >> 5);
     int ret = (*m &( 1 << (nr & 31)));
     return ret;
}
/*
 * >=0 success, < 0 indicates the errno
 */

/* static int bit_2_index[8] = {}; */
static int find_next_fd(struct fdtable *fdt)
{
    int fd = 0;
    for (size_t i = 0; i < fdt->max_fds / FD_BITS ; i ++)
    {
        int bit = get_rightmost_zero_bit(fdt->open_fds_bits[i]);
        if (bit != 0)
        {
            for (size_t j = 0; j < FD_BITS; j ++)
            {
                if ((1 << j) == bit)
                {
                    return fd + j;
                }

            }
        }
        fd += FD_BITS;
    }
    return -1;
}
static struct file* __fd_check(struct files_struct* fst, int fd)
{


    if (is_valid_fd(fst, fd) && fst->fdt->fd_array[fd] != NULL)
    {
        assert(__get_bit(fd, fst->fdt->open_fds_bits)  !=  0);
        return fst->fdt->fd_array[fd];
    }
    else
    {
        return NULL;
    }
}
static void __set_bit(int nr, volatile void * addr)
{
    assert(nr >= 0);
     int *m = ((int *) addr) + (nr >> 5);
     *m |= 1 << (nr & 31);
}
static void __clear_bit(int nr, volatile void * addr)
{
    assert(nr >= 0);
    // The input should lies between [4, 128], otherwise it won't
    // step into this function.
    // The bit map we're using now is composed of 4 Integer, the reason to right move
    // 5 is to find out the offset.
    /*
    *   |-int0-|-int1-|-int2-|-int3-|, find out which range it falls into
    */
    int *m = ((int *) addr) + (nr >> 5);
    // This step tries to find out the exam number, note 00011111 = 31.
    *m &= ~(1 << (nr & 31));
}
static void __set_open_fd(int fd,  struct fdtable *fdt)
{
    __set_bit(fd, fdt->open_fds_bits);
}

static int __alloc_fd(struct files_struct* files)
{
    assert(files != NULL);
    int fd;

    struct fdtable* fdt = files->fdt;
    fd = find_next_fd(fdt);
    if (fd < 0)
    {

        return -EMFILE;
    }
    __set_open_fd(fd, fdt);

    return fd;
}
static void __put_unused_fd(struct files_struct* files, int fd)
{
    struct fdtable *fdt = files->fdt;
    __clear_bit(fd, fdt->open_fds_bits);
    return;
}

static int __close_fd(struct files_struct* files, int fd)
{
    if (fd < 0)
    {
        return EBADF;
    }
    struct fdtable *fdt;
    struct file *file;

    fdt = files->fdt;
    if (fd >=(int) fdt->max_fds)
    {
        return EBADF;
    }
    file = __fd_check(files, fd);
    if (file == NULL)
    {
        return EBADF;
    }
    /* file = fdt->fd_array[fd]; */
    assert(file != NULL);
    fdt->fd_array[fd] = NULL;
    __put_unused_fd(files, fd);
    return close_kern_file(file);
}
static void __fd_install(struct files_struct* fst, int fd, struct file* fp)
{
    assert(fp != NULL);
    assert(fd >= 0);
    assert(fd < (int)fst->fdt->max_fds);
    assert(fst->fdt->fd_array[fd] == NULL);
    assert(__get_bit(fd, fst->fdt->open_fds_bits) != 0);
    fst->fdt->fd_array[fd] = fp;
    return;
}

/*
 * there is intermediate status which is:
 * open_fd_used has been marked 1,
 * but the file pointer has not been installed in fd array
 *
 */
int do_sys_open(int dfd,const char* filename, int flags, mode_t mode, struct files_struct* fst)
{
    assert(fst != NULL);
    int fd = __alloc_fd(fst);
    if (fd < 0)
    {
        ERROR_DEBUG("allocate fd error, while opening: %s\n", (char*)filename);
        return fd;
    }
    struct file* fp = NULL;
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "do_sys_open, dfd: %d, fileanme: %s, flags: %d, mode: %d\n", dfd, filename, flags, mode);
    int ret  = do_flip_open(&fp, dfd, (char*)filename, flags, mode);
    if ( ret != 0)
    {
        ERROR_DEBUG("do flip open error, while opening: %s, put back fd: %d\n", (char*)filename, fd);
        __put_unused_fd(fst, fd);
        return ret > 0 ? -ret: ret;
    }
    else
    {
        __fd_install(fst, fd, fp);
    }
    return fd;
}
int do_sys_close(int fd)
{
    return __close_fd(get_current_proc()->fs_struct, fd);
}

int do_sys_remove(char* filename)
{
    // TODO: need to check whether the file is open

    int ret = do_flip_remove(filename);
    return ret;
}


/* static int __dup2(struct files_struct* fst, struct file* f, int newfd) */
/* { */
/*     struct fdtable *fdt; */
/*     fdt = fst->fdt; */
/*     struct file* tofree =  fdt->fd_array[newfd]; */
/*     #<{(| */
/*      * imtermediate status */
/*      |)}># */
/*     if (tofree == NULL && __get_bit(newfd, fdt->open_fds_bits ) != 0) */
/*     { */
/*  */
/*         return EBUSY; */
/*     } */
/*     #<{(| assert(tofree == NULL); |)}># */
/*     #<{(| assert(__get_bit(newfd, fdt->open_fds_bits) == 0); |)}># */
/*     inc_ref_file(f); */
/*     fdt->fd_array[newfd] = f; */
/*     __set_open_fd(newfd, fdt); */
/*  */
/*     if (tofree != NULL) */
/*     { */
/*  */
/*         close_kern_file(tofree, &(fst->file_lock)); */
/*     } */
/*     return 0; */
/*  */
/* } */
/* int do_sys_dup2(int oldfd, int newfd) */
/* { */
/*     if (oldfd == newfd) */
/*     { */
/*         return EINVAL; */
/*     } */
/*     struct files_struct* fst = get_current_proc()->fs_struct; */
/*     if (is_valid_fd(fst, oldfd) == 0 || */
/*         is_valid_fd(fst, newfd) == 0) */
/*     { */
/*         return EBADF; */
/*     } */
/*  */
/*     struct file* f = __fd_check(fst, oldfd); */
/*     if (f == NULL) */
/*     { */
/*  */
/*         return EBADF; */
/*     } */
/*  */
/*     return  __dup2(fst, f, newfd); */
/*  */
/* } */
/*  */
/* off_t do_sys_lseek(int fd, off_t pos, int whence) */
/* { */
/*     if (whence != SEEK_SET */
/*         && whence != SEEK_CUR */
/*         && whence != SEEK_END) */
/*     { */
/*         return EINVAL; */
/*     } */
/*     struct files_struct* fst = get_current_proc()->fs_struct; */
/*     if (is_valid_fd(fst,fd) == 0) */
/*     { */
/*         return EBADF; */
/*     } */
/*  */
/*  */
/*     struct file* f = __fd_check(fst, fd); */
/*     if (f == NULL) */
/*     { */
/*  */
/*         return EBADF; */
/*     } */
/*  */
/*     #<{(| a trick played here */
/*      * I have some operation on this file handler, although some other threads may close this file handler via fd, but this file handler would not be released after the operations finished. */
/*      |)}># */
/*     inc_ref_file(f); */
/*  */
/*  */
/*  */
/*     off_t ret = kern_file_seek(f, pos, whence); */
/*  */
/*     if ( ret < 0) */
/*     { */
/*         close_kern_file(f, &(fst->file_lock)); */
/*         return ret; */
/*     } */
/*  */
/*     close_kern_file(f, &(fst->file_lock)); */
/*     return ret; */
/*  */
/* } */
int do_sys_read(int fd, char* buf, size_t buf_len)
{
    struct files_struct* fst = get_current_proc()->fs_struct;
    if (is_valid_fd(fst, fd) == 0)
    {
        ERROR_DEBUG("do_sys_read invalid fd: %d\n",fd);
        return -EBADF;
    }

    struct file* f = __fd_check(fst, fd);
    if (f == NULL)
    {
        ERROR_DEBUG("do_sys_read invalid fd: %d\n",fd);
        return -EBADF;
    }
    inc_ref_file(f);

    size_t read_len = 0;
    int ret = kern_file_read(f, buf, buf_len, &read_len);

    if ( ret != 0)
    {
        ERROR_DEBUG("kern_file_read fd: %d error: %d\n", fd, ret);
        close_kern_file(f);
        return ret > 0 ? -ret:ret;
    }

    close_kern_file(f);
    return read_len;

}
ssize_t do_sys_write(int fd, const void *buf, size_t buf_len)
{
    struct files_struct* fst = get_current_proc()->fs_struct;
    if (is_valid_fd(fst, fd) == 0)
    {
        ERROR_DEBUG("do_sys_write invalid fd: %d\n",fd);
        return -EBADF;
    }


    struct file* f = __fd_check(fst, fd);
    if (f == NULL)
    {
        ERROR_DEBUG("do_sys_write invalid fd: %d\n",fd);
        return -EBADF;
    }
    inc_ref_file(f);

    size_t write_len= 0;
    int ret = kern_file_write(f, buf, buf_len, & write_len);

    if (ret != 0)
    {
        ERROR_DEBUG("kern_file_write fd: %d, error: %d\n", fd, ret);
        close_kern_file(f);
        return ret > 0 ? -ret: ret;
    }
    close_kern_file(f);
    return write_len;

}


static void __destroy_fdt(struct fdtable* fdt, struct files_struct* fst)
{
    for (size_t i = 0; i < fdt->max_fds; i ++)
    {
        if (__get_bit(i, fdt->open_fds_bits) )
        {
            assert(fdt->fd_array[i] != NULL);

            close_kern_file(fdt->fd_array[i]);
            fdt->fd_array[i] = NULL;
            __clear_bit(i, fdt->open_fds_bits);
        }

    }
    free(fdt->fd_array);
    free((void*)fdt->open_fds_bits);
    return;
}
static int __init_fdt(struct fdtable* fdt)
{
    assert(fdt != NULL);
    fdt->max_fds = MAX_FD_COUNT_PER_PROCESS;
    fdt->fd_array = malloc(MAX_FD_COUNT_PER_PROCESS * sizeof(struct file*));
    if (fdt->fd_array == NULL)
    {
        return -1;
    }

    memset((fdt->fd_array), 0, MAX_FD_COUNT_PER_PROCESS * sizeof(struct file*));
    fdt->open_fds_bits = malloc(MAX_FD_COUNT_PER_PROCESS/FD_BITS * (sizeof(unsigned int)));
    if (fdt->open_fds_bits == NULL)
    {
        free(fdt->fd_array);
        return -1;
    }
    for (size_t i = 0; i < MAX_FD_COUNT_PER_PROCESS/FD_BITS; i ++)
    {
        fdt->open_fds_bits[i] = 0;
        fdt->fd_array[i] = NULL;
    }
    // reserve 0/1/2.
    fdt->fd_array[0] = (void*)0x01;
    fdt->fd_array[1] = (void*)0x01;
    fdt->fd_array[2] = (void*)0x01;
    fdt->open_fds_bits[0] = 0x7;
    /* fdt->open_fds_bits[0] =  */

    return 0;
}
/* int init_stdio(struct files_struct* fst) */
/* { */
/*     #<{(| (void)fst; |)}># */
/*     int ret = do_sys_open(0, NULL, 0, 0, fst); */
/*     if ( ret < 0) */
/*     { */
/*         return ret; */
/*     } */
/*  */
/*     ret = do_sys_open(1, NULL, 0, 0, fst); */
/*     if ( ret < 0) */
/*     { */
/*         return ret; */
/*     } */
/*  */
/*     ret = do_sys_open(2, NULL, 0, 0, fst); */
/*     if ( ret < 0) */
/*     { */
/*         return ret; */
/*     } */
/*  */
/*     return 0; */
/* } */

int init_fd_table(struct proc* cur)
{

    assert(cur != NULL);
    cur->fs_struct = malloc(sizeof(*(cur->fs_struct)));
    if (cur->fs_struct == NULL)
    {
        return ENOMEM;
    }

    cur->fs_struct->fdt = malloc(sizeof((*cur->fs_struct->fdt)));
    if (cur->fs_struct->fdt == NULL)
    {

        free(cur->fs_struct);
        cur->fs_struct = NULL;
        return ENOMEM;
    }
    int ret = __init_fdt(cur->fs_struct->fdt);
    if (ret != 0)
    {
        free(cur->fs_struct->fdt);

        free(cur->fs_struct);
        cur->fs_struct = NULL;
        return ENOMEM;
    }
    return 0;
}

void destroy_fd_table(struct proc* proc)
{
    assert(proc != NULL);

    struct fdtable* fdt = proc->fs_struct->fdt;
    __destroy_fdt(fdt, proc->fs_struct);

    free(proc->fs_struct->fdt);
    free(proc->fs_struct);
    return;
}




/* int do_sys_stat(const char* path, struct stat *stat_buf) */
/* { */
/*     return 0; */
/* } */
