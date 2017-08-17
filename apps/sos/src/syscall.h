/* Define the syscall processing functions in this file*/

#ifndef SOS_SYSCALL_H
#define SOS_SYSCALL_H

void sos_syscall_write(struct proc * proc, seL4_Word reply_cap);

#endif // SOS_SYSCALL_H
