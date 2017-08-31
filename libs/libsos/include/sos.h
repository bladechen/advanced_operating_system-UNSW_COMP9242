/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/* Simple operating system interface */

#ifndef _SOS_H
#define _SOS_H

#include <stdio.h>
#include <stdint.h>
#include <sel4/sel4.h>


/* The shared buffer address */
#define APP_PROCESS_IPC_SHARED_BUFFER       (0xA0002000)
#define APP_PROCESS_IPC_SHARED_BUFFER_GUARD (0xA0003000)
#define APP_PROCESS_IPC_SHARED_BUFFER_SIZE  (0x00001000)

/* The position of corresponding ep in SOS*/
#define SYSCALL_ENDPOINT_SLOT  (1)

/* System calls number for SOS */
#define SOS_SYSCALL_IPC_PRINT_COLSOLE   (0)
#define SOS_SYSCALL_READ                (1)
#define SOS_SYSCALL_WRITE               (2)
#define SOS_SYSCALL_OPEN                (3)
#define SOS_SYSCALL_USLEEP              (4)
#define SOS_SYSCALL_TIME_STAMP          (5)
#define SOS_SYSCALL_BRK                 (6)
#define SOS_SYSCALL_CLOSE               (7)
#define SOS_SYSCALL_STAT                (8)
#define SOS_SYSCALL_GET_DIRENT          (9)
#define SOS_SYSCALL_REMOVE              (10) 

/* Endpoint for talking to SOS */
#define SOS_IPC_EP_CAP     (0x1)
#define TIMER_IPC_EP_CAP   (0x2)

/* Limits */
#define PROCESS_MAX_FILES 16
#define MAX_IO_BUF 0x1000
#define N_NAME 32

/* file modes */
#define FM_EXEC  1
#define FM_WRITE 2
#define FM_READ  4
typedef int fmode_t;

/* stat file types */
#define ST_FILE    1    /* plain file */
#define ST_SPECIAL 2    /* special (console) file */
typedef int st_type_t;

#define IPC_CTRL_MSG_LENGTH (sizeof (ipc_buffer_ctrl_msg))
// This struct is for transfering control message over IPC
// the actual data is put in app-sos shared buffer
typedef struct ipc_buffer_ctrl_msg {

    int   ret_val;
    int         seq_num; //only for check purpose

    int         syscall_number;
    seL4_Word   start_app_buffer_addr;
    int         offset;
    int         file_id;

    int mode;
} ipc_buffer_ctrl_msg;

extern void *memcpy(void* ptr_dst, const void* ptr_src, unsigned int n);

size_t my_serial_send(const void *vData, size_t count, struct ipc_buffer_ctrl_msg* ctrl, int*);
static inline void serialize_ipc_ctrl_msg(const struct ipc_buffer_ctrl_msg* msg)
{
    memcpy(seL4_GetIPCBuffer()->msg, msg, IPC_CTRL_MSG_LENGTH);
}
static inline void unserialize_ipc_ctrl_msg(struct ipc_buffer_ctrl_msg* msg)
{

    memcpy(msg, seL4_GetIPCBuffer()->msg,IPC_CTRL_MSG_LENGTH);

}

int ipc_call(const struct ipc_buffer_ctrl_msg* ctrl,const  void* data,  struct ipc_buffer_ctrl_msg* ret);

int ipc_recv(struct ipc_buffer_ctrl_msg* ctrl, void* data, size_t count, struct ipc_buffer_ctrl_msg* ret);

// we assume reply data buffer would be less than ipc shared buffer


typedef struct {
  st_type_t st_type;    /* file type */
  fmode_t   st_fmode;   /* access mode */
  unsigned  st_size;    /* file size in bytes */
  long long      st_ctime;   /* Unix file creation time (ms) */
  long long      st_atime;   /* Unix file last access (open) time (ms) */
} sos_stat_t;

typedef int pid_t;

typedef struct {
  pid_t     pid;
  unsigned  size;            /* in pages */
  unsigned  stime;           /* start time in msec since booting */
  char      command[N_NAME]; /* Name of exectuable */
} sos_process_t;

// move the m0 print to console system call, transform
// and apply to current work flow, to see if it works.
int sos_sys_print_to_console(char * vData, size_t nbyte);

/* I/O system calls */

int sos_sys_open(const char *path, fmode_t mode);
/* Open file and return file descriptor, -1 if unsuccessful
 * (too many open files, console already open for reading).
 * A new file should be created if 'path' does not already exist.
 * A failed attempt to open the console for reading (because it is already
 * open) will result in a context switch to reduce the cost of busy waiting
 * for the console.
 * "path" is file name, "mode" is one of O_RDONLY, O_WRONLY, O_RDWR.
 */

int sos_sys_close(int file);
/* Closes an open file. Returns 0 if successful, -1 if not (invalid "file").
 */

int sos_sys_remove(const char * path);
/* Used to remove files from nfs, test purpose
*/

int sos_sys_read(int file, char *buf, size_t nbyte);
/* Read from an open file, into "buf", max "nbyte" bytes.
 * Returns the number of bytes read.
 * Will block when reading from console and no input is presently
 * available. Returns -1 on error (invalid file).
 */

int sos_sys_write(int file, const char *buf, size_t nbyte);
/* Write to an open file, from "buf", max "nbyte" bytes.
 * Returns the number of bytes written. <nbyte disk is full.
 * Returns -1 on error (invalid file).
 */

int sos_getdirent(int pos, char *name, size_t nbyte);
/* Reads name of entry "pos" in directory into "name", max "nbyte" bytes.
 * Returns number of bytes returned, zero if "pos" is next free entry,
 * -1 if error (non-existent entry).
 */

int sos_stat(const char *path, sos_stat_t *buf);
/* Returns information about file "path" through "buf".
 * Returns 0 if successful, -1 otherwise (invalid name).
 */

pid_t sos_process_create(const char *path);
/* Create a new process running the executable image "path".
 * Returns ID of new process, -1 if error (non-executable image, nonexisting
 * file).
 */

int sos_process_delete(pid_t pid);
/* Delete process (and close all its file descriptors).
 * Returns 0 if successful, -1 otherwise (invalid process).
 */

pid_t sos_my_id(void);
/* Returns ID of caller's process. */

int sos_process_status(sos_process_t *processes, unsigned max);
/* Returns through "processes" status of active processes (at most "max"),
 * returns number of process descriptors actually returned.
 */

pid_t sos_process_wait(pid_t pid);
/* Wait for process "pid" to exit. If "pid" is -1, wait for any process
 * to exit. Returns the pid of the process which exited.
 */

int64_t sos_sys_time_stamp(void);
/* Returns time in microseconds since booting.
 */

void sos_sys_usleep(int msec);
/* Sleeps for the specified number of milliseconds.
 */

seL4_Word sos_sys_brk(seL4_Word newbrk);

/*************************************************************************/
/*                                   */
/* Optional (bonus) system calls                     */
/*                                   */
/*************************************************************************/

int sos_share_vm(void *adr, size_t size, int writable);
/* Make VM region ["adr","adr"+"size") sharable by other processes.
 * If "writable" is non-zero, other processes may have write access to the
 * shared region. Both, "adr" and "size" must be divisible by the page size.
 *
 * In order for a page to be shared, all participating processes must execute
 * the system call specifying an interval including that page.
 * Once a page is shared, a process may write to it if and only if all
 * _other_ processes have set up the page as shared writable.
 *
 * Returns 0 if successful, -1 otherwise (invalid address or size).
 */

#endif
