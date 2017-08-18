/* Define the syscall processing functions in this file*/

#ifndef SOS_SYSCALL_H
#define SOS_SYSCALL_H

#include <sos.h>
#include "proc/proc.h"

// move the `handle_syscall` from main to this file
void handle_syscall(seL4_Word badge, struct proc * app_process);

// you can get control message via IPC within the function
int sos_syscall_print_to_console(struct proc * proc);
int sos_syscall_open(struct proc * proc);
int sos_syscall_read(struct proc * proc);
int sos_syscall_write(struct proc * proc);
int sos_syscall_usleep(struct proc * proc);
int sos_syscall_time_stamp(struct proc * proc);

#endif // SOS_SYSCALL_H
