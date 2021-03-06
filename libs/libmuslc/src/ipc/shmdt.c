/* @LICENSE(MUSLC_MIT) */

#include <sys/shm.h>
#include "internal/syscall.h"
#include "ipc.h"

int shmdt(const void *addr)
{
#ifdef SYS_shmdt
	return syscall(SYS_shmdt, addr);
#else
	return syscall(SYS_ipc, IPCOP_shmdt, addr);
#endif
}
