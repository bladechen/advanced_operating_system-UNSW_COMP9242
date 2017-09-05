#ifndef _PROC_H_
#define _PROC_H_

#include "comm/comm.h"
#include "vm/pagetable.h"
#include "fs/fdtable.h"
#include "vm/address_space.h"
#include "coroutine/coro.h"
#include <sos.h>



enum PROC_STATUS
{

    PROC_STATUS_RUNNING = 0,
    PROC_STATUS_ZOMBIE = 1,
    PROC_STATUS_DIE    = 2,

};

struct proc
{
    char*              p_name; // proc name, current need by cpio to load elf.
    uint32_t p_pid; // hard code make it to 2, TODO in M8 need manage pid
    struct addrspace*  p_addrspace;
    struct pagetable*  p_pagetable;
    struct files_struct*  fs_struct;

    // sel4 kernel pagetable moved to p_pagetable
    // ipc cap moved into p_addrspace

    struct sos_object*  p_tcb;

    cspace_t*           p_croot;

    seL4_CPtr           p_ep_cap;

    struct coroutine*   p_coro;

    seL4_CPtr           p_reply_cap;

    ipc_buffer_ctrl_msg p_ipc_ctrl;

    char p_status; //

    int vm_fault_code; // i am lazy...
};


void proc_bootstrap();
/* create all the resource of proc, then activate it. i.e make it running */
struct proc* proc_create(char* name, seL4_CPtr fault_ep_cap);

/* make the proc running */
int proc_destroy(struct proc* proc); // XXX we may no need proc_exit

void proc_activate(struct proc* proc);

/* suspend the proc TODO later in M8 */
int proc_suspend(struct proc* proc);

/* resume the proc TODO later in M8 */
int proc_resume(struct proc* proc);

void recycle_process();

/* Fetch the address space of the current process. */
struct addrspace *proc_getas(void);

/* Change the address space of the current process, and return the old one. */
struct addrspace *proc_setas(struct addrspace *);


inline static void proc_to_be_killed(struct proc* proc)
{
    proc->p_status = PROC_STATUS_ZOMBIE;
}

// struct proc* get_current_app_proc();
// void set_current_app_proc(struct proc* proc);


#endif

