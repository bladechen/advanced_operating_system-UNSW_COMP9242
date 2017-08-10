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
#include <vmem_layout.h>

#define verbose 5
#include <sys/debug.h>
#include <sys/panic.h>

#include "proc.h"

// TODO, now we assume there is only one process, and the badge
// 
#define TEMP_ONE_PROCESS_BADGE (1<<3)   

struct proc* proc_create(char* name, seL4_CPtr fault_ep_cap)
{
    int err;
    struct proc * process = (struct proc *)malloc(sizeof(struct proc));

    process->p_name = name;
    // TODO: set the pid dynamically, now we hard code it as 2
    process->pid = 2; 

    /* 
    *  pagetable will take care of the virtual address root
    *  IPC buffer will be created and defined in address space
    *  Stack will be created and defined in address space
    */

    // Init address space and page table 
    process->p_addrspace = as_create();
    if (process->p_addrspace == NULL) {
        color_print(ANSI_COLOR_RED, "proc_create: get a null p_addrspace\n");
        return NULL;
    }
    process->p_pagetable = create_pagetable();
    if (process->p_addrspace == NULL) {
        color_print(ANSI_COLOR_RED, "proc_create: get a null p_pagetable\n");
        return NULL;
    }

    // Create a simple 1 level CSpace
    process->p_croot = cspace_create(1);
    assert(process->p_croot != NULL);

    // Copy the fault endpoint to the user app to enable IPC 
    process->p_ep_cap = cspace_mint_cap(process->p_croot,
                                        cur_cspace,
                                        fault_ep_cap,
                                        seL4_AllRights,
                                        seL4_CapData_Badge_new(TEMP_ONE_PROCESS_BADGE));
    assert(process->p_ep_cap != CSPACE_NULL);

    // Create a new TCB object
    struct sos_object * tcb_obj = (struct sos_object *)malloc(sizeof(struct sos_object));
    err = init_sos_object(tcb_obj, seL4_TCBObject, seL4_TCBBits);
    conditional_panic(err, "Failed to create TCB");
    process->p_tcb = tcb_obj;

    /* ################## This part not done ####################### */
    // configure TCB
    // TODO get the IPC_BUFFER_CAP
    err = seL4_TCB_Configure(process->tcb_obj->cap, process->p_ep_cap, TTY_PRIORITY,
                             process->p_croot->root_cnode, seL4_NilData,
                             process->p_pagetable->vroot->cap, seL4_NilData, PROCESS_IPC_BUFFER,
                             process->p_addrspace->ipc_buffer_cap);
    conditional_panic(err, "Unable to configure new TCB");
    /*############################################################*/

    
    /*###################### elf_load haven't done #############*/
    // parse the cpio image 
    unsigned long elf_size;
    char * elf_base = cpio_get_file(_cpio_archive, name, &elf_size); // extern char _cpio_archive[];
    conditional_panic(!elf_base, "Unable to locate cpio header");

    // load the elf image 
    err = elf_load(process->p_pagetable->vroot, elf_base);
    conditional_panic(err, "Failed to load elf image");
    /*#########################################################*/
    
    
    // TODO create a new activate function
    /* Start the new process */
    seL4_UserContext context;
    memset(&context, 0, sizeof(context));
    context.pc = elf_getEntryPoint(elf_base);
    context.sp = PROCESS_STACK_TOP;
    seL4_TCB_WriteRegisters(tty_test_process.tcb_cap, 1, 0, 2, &context);
}


// TODO free struct proc

void start_first_process(char* app_name, seL4_CPtr fault_ep) {
    int err;

    seL4_Word stack_addr;
    seL4_CPtr stack_cap;
    seL4_CPtr user_ep_cap;

    /* These required for setting up the TCB */
    seL4_UserContext context;

    /* These required for loading program sections */
    char* elf_base;
    unsigned long elf_size;

    /* Create a simple 1 level CSpace */
    tty_test_process.croot = cspace_create(1);
    assert(tty_test_process.croot != NULL);


    /* Copy the fault endpoint to the user app to enable IPC */
    user_ep_cap = cspace_mint_cap(tty_test_process.croot,
                                  cur_cspace,
                                  fault_ep,
                                  seL4_AllRights,
                                  seL4_CapData_Badge_new(TTY_EP_BADGE));
    /* should be the first slot in the space, hack I know */
    assert(user_ep_cap == 1);
    assert(user_ep_cap == USER_EP_CAP);

    /* Create a new TCB object */
    tty_test_process.tcb_addr = ut_alloc(seL4_TCBBits);
    conditional_panic(!tty_test_process.tcb_addr, "No memory for new TCB");
    err =  cspace_ut_retype_addr(tty_test_process.tcb_addr,
                                 seL4_TCBObject,
                                 seL4_TCBBits,
                                 cur_cspace,
                                 &tty_test_process.tcb_cap);
    conditional_panic(err, "Failed to create TCB");

    /* Configure the TCB */
    err = seL4_TCB_Configure(tty_test_process.tcb_cap, user_ep_cap, TTY_PRIORITY,
                             tty_test_process.croot->root_cnode, seL4_NilData,
                             tty_test_process.vroot->cap, seL4_NilData, PROCESS_IPC_BUFFER,
                             tty_test_process.ipc_buffer_cap);
    conditional_panic(err, "Unable to configure new TCB");

    /* Provide a logical name for the thread -- Helpful for debugging */
#ifdef SEL4_DEBUG_KERNEL
    seL4_DebugNameThread(tty_test_process.tcb_cap, app_name);
#endif

    /* parse the cpio image */
    dprintf(1, "\nStarting \"%s\"...\n", app_name);
    elf_base = cpio_get_file(_cpio_archive, app_name, &elf_size);
    conditional_panic(!elf_base, "Unable to locate cpio header");

    /* load the elf image */
    err = elf_load(tty_test_process.vroot, elf_base);
    conditional_panic(err, "Failed to load elf image");


    /* Create a stack frame */
    stack_addr = ut_alloc(seL4_PageBits);
    conditional_panic(!stack_addr, "No memory for stack");
    err =  cspace_ut_retype_addr(stack_addr,
                                 seL4_ARM_SmallPageObject,
                                 seL4_PageBits,
                                 cur_cspace,
                                 &stack_cap);
    conditional_panic(err, "Unable to allocate page for stack");

    /* Map in the stack frame for the user app */
    err = map_page(stack_cap, tty_test_process.vroot,
                   PROCESS_STACK_TOP - (1 << seL4_PageBits),
                   seL4_AllRights, seL4_ARM_Default_VMAttributes);
    conditional_panic(err, "Unable to map stack IPC buffer for user app");

    /* Map in the IPC buffer for the thread */
    err = map_page(tty_test_process.ipc_buffer_cap, tty_test_process.vroot,
                   PROCESS_IPC_BUFFER,
                   seL4_AllRights, seL4_ARM_Default_VMAttributes);
    conditional_panic(err, "Unable to map IPC buffer for user app");

    /* Start the new process */
    memset(&context, 0, sizeof(context));
    context.pc = elf_getEntryPoint(elf_base);
    context.sp = PROCESS_STACK_TOP;
    seL4_TCB_WriteRegisters(tty_test_process.tcb_cap, 1, 0, 2, &context);
}


