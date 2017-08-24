#include "sys.h"
#include "assert.h"
#include "sos.h"
void handle_no_implemented_syscall(const char* s)
{
    (void)s;
    printf("system call not implemented\n");
    /* assert(0); */
}
