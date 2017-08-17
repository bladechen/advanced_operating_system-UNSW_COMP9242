#include <stdio.h>
#include <stdlib.h>
#include <sel4/sel4.h>
#include "sos.h"
#include "sys.h"


pid_t sos_process_create(const char *path)
{
    handle_no_implemented_syscall();
    return 0;
}

int sos_process_delete(pid_t pid)
{
    handle_no_implemented_syscall();
    return 0;
}


pid_t sos_process_wait(pid_t pid)
{
    handle_no_implemented_syscall();
    return 0;
}

int sos_process_status(sos_process_t *processes, unsigned max)
{
    handle_no_implemented_syscall();
    return 0;
}
