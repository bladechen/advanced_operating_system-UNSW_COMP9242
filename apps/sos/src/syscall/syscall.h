/* Define the syscall processing functions in this file*/

#ifndef SOS_SYSCALL_H
#define SOS_SYSCALL_H

// TODO: will move the `handle_syscall` from main to this file
// int handle_syscall(seL4_Word badge, int num_args);

// you can get control message via IPC within the function
int sos_syscall_print_to_console(struct proc * proc, seL4_Word reply_cap);
int sos_syscall_open(struct proc * proc, seL4_Word reply_cap);
int sos_syscall_read(struct proc * proc, seL4_Word reply_cap);
int sos_syscall_write(struct proc * proc, seL4_Word reply_cap);
int sos_syscall_usleep(struct proc * proc, seL4_Word reply_cap);
int sos_syscall_time_stamp(struct proc * proc, seL4_Word reply_cap);

#endif // SOS_SYSCALL_H
