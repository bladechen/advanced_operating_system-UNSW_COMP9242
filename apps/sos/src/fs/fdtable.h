#ifndef _FD_TABLE_H_
#define _FD_TABLE_H_

#include "proc/proc.h"
#include "coroutine/coro.h"
#include "comm/comm.h"
#include "comm/list.h"
#include "vfs/vnode.h"
#include "file.h"

struct file;
#define MAX_FD_COUNT_PER_PROCESS 128
#define FD_BITS (sizeof(unsigned int) * 8)

struct fdtable
{
    unsigned int max_fds;
    struct file **fd_array;
    // unsigned int *full_fds_bits;
    volatile unsigned int *open_fds_bits;

};

struct files_struct
{
    struct fdtable* fdt;
};

static inline bool
is_valid_fd(struct files_struct*f, int fd)
{
    return !(fd < 0 || fd >= (int)f->fdt->max_fds);
}

int do_sys_open(int dfd,  const char* filename, int flags, mode_t mode, struct files_struct*);
int do_sys_close(int fd);
// int do_sys_dup2(int oldfd, int newfd) ;
// off_t do_sys_lseek(int fd, off_t pos, int whence);

ssize_t do_sys_read(int fd, char* buf, size_t buf_len);

ssize_t do_sys_write(int fd, const void *buf, size_t nbytes);
int init_fd_table(struct proc* cur);
void destroy_fd_table(struct proc* proc);
// int init_stdio(struct files_struct* fst);
#endif
