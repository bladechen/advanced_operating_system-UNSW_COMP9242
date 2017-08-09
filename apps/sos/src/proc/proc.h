#ifndef _PROC_H_
#define _PROC_H_

#include "comm.h"
#include "pagetable.h"
#include "address_space.h"
// struct addrspace;
// struct page

struct proc
{
    char*              p_name;
    pid_t              p_pid; // hard code make it to 2, TODO in M8 need manage pid
    // char*              p_elfbase; // for further fault handler load code/data section into page/frame table
    struct addrspace*  p_addrspace;
    struct pagetable*  p_pagetable;


    struct sos_object  p_tcb;
    // struct sos_object  p_vroot; // moved to pagetable

    // seL4_Word ipc_buffer_addr; moved into p_addrspace
    // seL4_CPtr ipc_buffer_cap;

    cspace_t*           p_croot;

    seL4_CPtr           p_ep_cap;

};

/* create all the resource of proc, then activate it. i.e make it running */
struct proc* proc_create(char* name, seL4_CPtr fault_ep_cap);

/* make the proc running */
// int proc_activate(struct proc* proc);

/* suspend the proc TODO later in M8 */
int proc_suspend(struct proc* proc);

/* resume the proc TODO later in M8 */
int proc_resume(struct proc* proc);

void proc_destroy(struct proc* proc); // XXX we may no need proc_exit


/* Fetch the address space of the current process. */
struct addrspace *proc_getas(void);

/* Change the address space of the current process, and return the old one. */
struct addrspace *proc_setas(struct addrspace *);

#endif

