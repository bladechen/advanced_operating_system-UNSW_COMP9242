/* Define the syscall processing functions in this file*/

#ifndef SOS_SYSCALL_H
#define SOS_SYSCALL_H

#include <sos.h>
#include <stdbool.h>

typedef struct syscall_func {
	int (*syscall)(struct proc *);
	bool will_block;
} syscall_func;

/* Syscall numbers defined in `sos.h`*/
// #define SOS_SYSCALL_IPC_PRINT_COLSOLE   (0)
// #define SOS_SYSCALL_READ                (1)
// #define SOS_SYSCALL_WRITE               (2)
// #define SOS_SYSCALL_OPEN                (3)
// #define SOS_SYSCALL_USLEEP              (4)
// #define SOS_SYSCALL_TIME_STAMP          (5)

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
