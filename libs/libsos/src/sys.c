#include "sys.h"
#include "assert.h"
#include "sos.h"
void handle_no_implemented_syscall(const char* s)
{
    printf("system call not implemented\n", s);
    /* assert(0); */
}
