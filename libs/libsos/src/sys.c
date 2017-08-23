#include "sys.h"
#include "assert.h"
#include "sos.h"
void handle_no_implemented_syscall(const char* s)
{
    tty_debug_print("[sosh] system call [%s] not implemented", s);
    /* assert(0); */
}
