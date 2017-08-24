#include "comm/comm.h"
int syscall_open(const char* filename, int flags, mode_t mode, int* fd_num);
int syscall_close(int fd_num, int *retval);
// int syscall_dup2(int oldfd, int newfd, int* retval);
// int syscall_lseek(int fd, off_t pos, int whence, off_t* retval) ;
int syscall_write(int fd, const char* buf, size_t nbytes, size_t* retval)   ;
int syscall_read(int fd, char* buf, size_t buflen, size_t * retval) ;
