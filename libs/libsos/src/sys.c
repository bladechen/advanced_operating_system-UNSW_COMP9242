#include "sys.h"
#include "assert.h"
#include "sos.h"
void handle_no_implemented_syscall()
{
    tty_debug_print("[sosh] system call not implemented");
    assert(0);
}
