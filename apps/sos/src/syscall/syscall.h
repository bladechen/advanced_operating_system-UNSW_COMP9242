/* Define the syscall processing functions in this file*/

#ifndef SOS_SYSCALL_H
#define SOS_SYSCALL_H

#include <sos.h>

// move the `handle_syscall` from main to this file
void handle_syscall(seL4_Word badge, struct proc * app_process);

// you can get control message via IPC within the function
int sos_syscall_print_to_console(struct proc * proc, seL4_Word reply_cap, ipc_buffer_ctrl_msg * ctrl_msg);
int sos_syscall_open(struct proc * proc, seL4_Word reply_cap, ipc_buffer_ctrl_msg * ctrl_msg);
int sos_syscall_read(struct proc * proc, seL4_Word reply_cap, ipc_buffer_ctrl_msg * ctrl_msg);
int sos_syscall_write(struct proc * proc, seL4_Word reply_cap, ipc_buffer_ctrl_msg * ctrl_msg);
int sos_syscall_usleep(struct proc * proc, seL4_Word reply_cap, ipc_buffer_ctrl_msg * ctrl_msg);
int sos_syscall_time_stamp(struct proc * proc, seL4_Word reply_cap, ipc_buffer_ctrl_msg * ctrl_msg);

#endif // SOS_SYSCALL_H
