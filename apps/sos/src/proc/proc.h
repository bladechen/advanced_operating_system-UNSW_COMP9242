#ifndef _PROC_H_
#define _PROC_H_

#include "comm/list.h"
#include "coroutine/synch.h"
#include "comm/comm.h"
#include "vm/pagetable.h"
#include "fs/fdtable.h"
#include "vm/address_space.h"
#include "coroutine/coro.h"
#include "comm/list.h"
#include "pid.h"
#include <sos.h>


enum PROC_STATUS
{

    PROC_STATUS_INIT = 0,
    PROC_STATUS_RUNNING = 1,
    PROC_STATUS_EXIT   = 2, // we should mark proc exit, then destroy in the main coroutine loop
    PROC_STATUS_ZOMBIE = 3, // all the resource of the proc has been destroyed except for the proc itself

    PROC_STATUS_INVALID = 100
};

struct proc_context
{
    seL4_CPtr           p_reply_cap;
    ipc_buffer_ctrl_msg p_ipc_ctrl;
    int vm_fault_code; // i am lazy...
};

struct proc_resource
{
    struct addrspace*  p_addrspace;
    struct pagetable*  p_pagetable;
    struct files_struct*  fs_struct;

    // sel4 kernel pagetable moved to p_pagetable
    // ipc cap moved into p_addrspace

    struct sos_object*  p_tcb;
    cspace_t*           p_croot;
    seL4_CPtr           p_ep_cap;
};

struct proc_status
{
    char*   name; // proc name, current need by cpio to load elf.
    char status; //

    unsigned int stime; // start time in second

};

// struct proc_exec_argv
// {
//     int argc;
//     char** argv;
//     char* elf_base; // replace it futher more
// };

struct proc
{
    struct proc_resource p_resource;
    struct proc_status   p_status;
    struct proc_context  p_context;


    struct coroutine* p_coro;

    int p_pid; // badge is same as p_pid

    int p_father_pid;
    struct list children_list; // for managing the child proc
    struct list_head as_child_next; // used as child link node for the parent proc
    bool someone_wait;
    struct semaphore* p_waitchild;
};


void proc_bootstrap();
/* create all the resource of proc, then activate it. i.e make it running */
struct proc* proc_create(char* name, seL4_CPtr fault_ep_cap);

/* make the proc running */
void proc_destroy(struct proc* proc); // XXX we may no need proc_exit



int proc_start(struct proc* proc, int argc, char** argv);


bool proc_load_elf(struct proc * process, char* file_name);

void proc_exit(struct proc* proc);

/* resume the proc TODO later in M8 */

void recycle_process();


bool proc_wakeup_father(struct proc* child);

void proc_attach_father(struct proc* child, struct proc* father);

static inline void proc_deattch(struct proc* proc)
{
    assert(proc->p_father_pid != -1);
    list_del(&proc->as_child_next);
    proc->p_father_pid = -1;
}

/* Fetch the address space of the current process. */
struct addrspace *proc_getas(void);

/* Change the address space of the current process, and return the old one. */
struct addrspace *proc_setas(struct addrspace *);


// struct proc* proc_get_child(int pid);

// inline static void proc_to_be_killed(struct proc* proc)
// {
//     proc->p_status = PROC_STATUS_ZOMBIE;
// }

void* get_ipc_buffer(struct proc* proc);

static inline struct pagetable* proc_pagetable(struct proc* proc)
{
    return proc->p_resource.p_pagetable;
}

#endif

