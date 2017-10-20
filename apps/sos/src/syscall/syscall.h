/* Define the syscall processing functions in this file*/

#ifndef SOS_SYSCALL_H
#define SOS_SYSCALL_H

#include <sos.h>
#include "proc/proc.h"
#include <stdbool.h>

typedef struct syscall_func {
	void (*syscall)(void* );
} syscall_func;


void handle_syscall(seL4_Word badge, struct proc * app_process);

void sos_syscall_print_to_console(void* argv);
void sos_syscall_open(void* argv);
void sos_syscall_read(void* argv);
void sos_syscall_write(void* argv);
void sos_syscall_usleep(void* argv);
void sos_syscall_time_stamp(void* argv);
void sos_syscall_brk(void* argv);
void sos_syscall_close(void* argv);
void sos_syscall_stat(void* argv);
void sos_syscall_get_dirent(void* argv);
void sos_syscall_remove(void* argv);
void sos_syscall_create_process(void* argv);
void sos_syscall_delete_process(void* argv);
void sos_syscall_wait_process(void* argv);
void sos_syscall_process_status(void* argv);
void sos_syscall_exit_process(void* argv);
void sos_syscall_process_my_pid(void* argv);
void sos_syscall_mmap(void* argv);
void sos_syscall_munmap(void* argv);
void sos_syscall_vm_shared(void* argv);

#endif // SOS_SYSCALL_H
