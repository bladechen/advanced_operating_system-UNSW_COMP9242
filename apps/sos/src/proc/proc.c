/*
    Process related operations
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <sel4/types.h>
#include <cspace/cspace.h>
#include <mapping.h>
#include <ut_manager/ut.h>
#include <vm/vmem_layout.h>
#include <elf/elf.h>
#include <cpio/cpio.h>
#include "comm/comm.h"

#define verbose 5
#include <sys/debug.h>
#include <sys/panic.h>

#include "proc.h"
#include "vm/address_space.h"

// TODO, now we assume there is only one process, and the badge
// is hard coded, may try create badge dynamically
#define TEMP_ONE_PROCESS_BADGE (1<<3)

extern char _cpio_archive[];

static void clear_proc(struct proc* proc)
{
    proc->p_name = NULL;
    proc->p_pid = 0;
    proc->p_addrspace = NULL;
    proc->p_pagetable = NULL;
    proc->p_tcb = NULL;
    proc->p_croot = NULL;
    proc->p_ep_cap = 0;

}

static struct proc* _cur_proc = NULL;
struct proc* get_current_app_proc()
{
    return _cur_proc;

}
void set_current_app_proc(struct proc* proc)
{
    _cur_proc = proc;
}


/* void loop_through_region(struct addrspace *as); */

struct proc* proc_create(char* name, seL4_CPtr fault_ep_cap)
{
    int err;
    struct proc * process = (struct proc *)malloc(sizeof(struct proc));
    if (process == NULL)
    {
        return NULL;
    }
    clear_proc(process);

    process->p_name = name;
    // TODO: set the pid dynamically, now we hard code it as 2
    process->p_pid = 2;

    /*
    *  pagetable will take care of the virtual address root
    *  IPC buffer will be created and defined in address space
    *  Stack will be created and defined in address space
    */

    // Init address space and page table
    process->p_addrspace = as_create();
    if (process->p_addrspace == NULL)
    {
        color_print(ANSI_COLOR_RED, "proc_create: get a null p_addrspace\n");
        proc_destroy(process);
        return NULL;
    }
    process->p_pagetable = create_pagetable();
    if (process->p_addrspace == NULL)
    {
        color_print(ANSI_COLOR_RED, "proc_create: get a null p_pagetable\n");
        proc_destroy(process);
        return NULL;
    }
    process->p_addrspace->proc = process;

    // Create a simple 1 level CSpace
    process->p_croot = cspace_create(1);
    assert(process->p_croot != NULL);

    // the order is first init ipc buffer, then setup fault ep?
    as_define_ipc(process->p_addrspace);
    // Copy the fault endpoint to the user app to enable IPC
    process->p_ep_cap = cspace_mint_cap(process->p_croot,
                                        cur_cspace,
                                        fault_ep_cap,
                                        seL4_AllRights,
                                        seL4_CapData_Badge_new(TEMP_ONE_PROCESS_BADGE));
    assert(process->p_ep_cap != CSPACE_NULL);
    assert(process->p_ep_cap == 1);// FIXME

    // Create a new TCB object
    struct sos_object * tcb_obj = (struct sos_object *)malloc(sizeof(struct sos_object));
    clear_sos_object(tcb_obj);
    err = init_sos_object(tcb_obj, seL4_TCBObject, seL4_TCBBits);
    conditional_panic(err, "Failed to create TCB");
    process->p_tcb = tcb_obj;

    // configure TCB
    // hardcode priority as 0
    err = seL4_TCB_Configure(process->p_tcb->cap,
                             process->p_ep_cap,
                             0,
                              process->p_croot->root_cnode,
                              seL4_NilData,
                              process->p_pagetable->vroot.cap,
                              seL4_NilData,
                              APP_PROCESS_IPC_BUFFER,
                              as_get_ipc_cap(process->p_addrspace));
    conditional_panic(err, "Unable to configure new TCB");

    // parse the cpio image
    unsigned long elf_size;
    // ### According to `extern char _cpio_archive[];` in main.c
    // It has been defined in main.c
    char * elf_base = cpio_get_file(_cpio_archive, name, &elf_size);
    color_print(ANSI_COLOR_GREEN, "elf_base: %x\n", elf_base);
    color_print(ANSI_COLOR_GREEN, "elf_base: %x, entry point:%x\n", elf_base, elf_getEntryPoint(elf_base));
    conditional_panic(!elf_base, "Unable to locate cpio header");


    /*** load the elf image info, set up addrspace ***/
    // DATA and CODE region is set up by `vm_elf_load`
    err = vm_elf_load(process->p_addrspace, process->p_pagetable->vroot.cap, elf_base);
    conditional_panic(err, "Failed to load elf image");

    // This pointer here is useless act as a placeholder
    vaddr_t stack_pointer;
    as_define_stack(process->p_addrspace, &stack_pointer);

    as_define_heap(process->p_addrspace);

    // TODO: as_define_mmap(process->p_addrspace);

    loop_through_region(process->p_addrspace);
    return process;
}

void proc_activate(struct proc * process)
{
    seL4_UserContext context;
    memset(&context, 0, sizeof(context));
    context.pc = elf_getEntryPoint(process->p_addrspace->elf_base);
    context.sp = APP_PROCESS_STACK_TOP;
    color_print(ANSI_COLOR_GREEN, "0x%x,proc activate, pc: 0x%x, sp: 0x%x\n", process->p_addrspace->elf_base, context.pc, context.sp);
    seL4_TCB_WriteRegisters(process->p_tcb->cap, 1, 0, 2, &context);
}

// TODO  need to check the correctness.
int proc_destroy(struct proc * process)
{
    /* if (process->p_name != NULL) */
    /* { */
    /*     free(process->p_name); */
    /* } */

    if (process->p_addrspace != NULL)
    {
        as_destroy(process->p_addrspace);
        process->p_addrspace = NULL;
    }

    if (process->p_pagetable != NULL)
    {
        destroy_pagetable(process->p_pagetable);
        process->p_pagetable = NULL;
    }

    if (process->p_tcb != NULL)
    {
        seL4_TCB_Suspend(process->p_tcb->cap);
        free_sos_object(process->p_tcb, seL4_TCBBits, process->p_croot);
        process->p_tcb = NULL;
    }

    // revoke & delete capability
    cspace_revoke_cap(process->p_croot, process->p_ep_cap);
    assert(0 == cspace_delete_cap(process->p_croot, process->p_ep_cap));

    // mentioned in where it is defined, One could also rely on cspace_destroy() to free object,
    // if, and only if, there are no copies of caps to the object outside of the cspace being destroyed.
    // This should be a temporary solution
    cspace_destroy(process->p_croot);

    process->p_croot = NULL;

    free(process);

    return 0;
}
