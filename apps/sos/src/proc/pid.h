#ifndef _PID_H_
#define _PID_H_
#include "comm/list.h"
#include "coroutine/synch.h"
#include "comm/comm.h"
#include "vm/pagetable.h"
#include "fs/fdtable.h"
#include "vm/address_space.h"
#include "coroutine/coro.h"
#include "comm/list.h"
#include <sos.h>
#define PID_ARRAY_SIZE 6
#define MAX_SUPPORTED_PID (1 << 20) // if allocted pid reach this number (not include this!!), it will then  wrap around.

struct proc;
int alloc_pid(struct proc*);
void free_pid(int pid);
struct proc* pid_to_proc(int pid);
struct proc* index_to_proc(int index);

#endif
