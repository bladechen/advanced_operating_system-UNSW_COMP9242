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
#include "comm/list.h"

#define verbose 5
#include <sys/debug.h>
#include <sys/panic.h>

#include "proc.h"
#include "vm/address_space.h"
#include "vm/pagetable.h"
#include "vm/vm.h"
#include "clock/clock.h"

#include "syscall/handle_syscall.h"

struct proc kproc;

// TODO, now we assume there is only one process, and the badge
// is hard coded, may try create badge dynamically
/* #define TEMP_ONE_PROCESS_BADGE (1<<3) */

extern char _cpio_archive[];

static int get_free_pid();
static void set_free_pid();


static void clear_proc(struct proc* proc)
{
    proc->p_name = NULL;
    proc->p_pid = -1;
    proc->p_addrspace = NULL;
    proc->p_pagetable = NULL;
    proc->p_tcb = NULL;
    proc->p_croot = NULL;
    proc->p_ep_cap = 0;
    proc->p_coro = NULL;
    proc->p_reply_cap = 0;
    proc->fs_struct = NULL;
    proc->p_status = PROC_STATUS_ZOMBIE;
    proc->vm_fault_code = -1;
    proc->p_badge = 0;
    proc->p_father_pid =  -1;
    proc->someone_wait = false;
    list_init(&(proc->children_list));
    link_init(&(proc->as_child_next));
}

/*
*   Use proc_id % PROC_ARRAY_SIZE to retrieve the location on proc_array
*   Design, the proc_id may greater than the size of proc_array, so that
*   the reuse of proc_id will have to experience a longer time, which will
*   help to mitigate the problem may happen with proc id reuse
*/
struct proc* proc_array[PROC_ARRAY_SIZE] = {NULL}; // make sure it initialized as NULL

uint32_t proc_id_counter = 0;

uint32_t proc_free_slot_counter = PROC_ARRAY_SIZE;

static int procid_to_procarray_index(uint32_t proc_id)
{
    return proc_id % PROC_ARRAY_SIZE;
}


static void init_kproc(char* kname)
{
    clear_proc(&kproc);
    kproc.p_name = strdup(kname);
    kproc.p_pagetable = (struct pagetable*)(kcreate_pagetable());
    kproc.p_addrspace = NULL; // i don't need the restriction.
    kproc.p_tcb = NULL;
    kproc.p_croot = NULL;
    kproc.p_ep_cap = 0;
    kproc.stime = 0;
    set_kproc_coro(&kproc);
    /* should be proc-id 0, the very first proc*/
    kproc.p_pid = get_free_pid();
    proc_array[procid_to_procarray_index(kproc.p_pid)] = &kproc;
}

void proc_bootstrap()
{
    static char* kname = "sos_kernel";
    bootstrap_coro_env();
    init_kproc(kname);
    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "kernel proc at: %p, coroutine at: %p\n", &kproc, kproc.p_coro)
}

struct proc * get_proc_by_pid(int pid)
{
    if (pid < 0)
    {
        return NULL;
    }
    struct proc * temp = proc_array[pid % PROC_ARRAY_SIZE];
    assert(temp != NULL);
    if (temp->p_pid == pid)
    {
        return temp;
    }
    return NULL;
}

/* void loop_through_region(struct addrspace *as); */

static int get_free_pid()
{
    if (proc_free_slot_counter == 0)
    {
        return -1;
    }
    assert(proc_free_slot_counter > 0 && proc_free_slot_counter <= MAX_PROC_ID);
    int i = PROC_ARRAY_SIZE;

    int proc_id = proc_id_counter;
    while(proc_array[proc_id_counter % PROC_ARRAY_SIZE] != NULL)
    {
        proc_id_counter++;
        proc_id = proc_id_counter;
        i --;
        assert(i > 0);
    }

    proc_free_slot_counter --;
    assert(i > 0);
    assert(proc_array[proc_id % PROC_ARRAY_SIZE] == NULL);
    return proc_id;

    /* if (proc_array[proc_id_counter % PROC_ARRAY_SIZE] == NULL) { */
    /*     return proc_id_counter++; */
    /* } else if (proc_array[proc_id_counter % PROC_ARRAY_SIZE]->p_status == PROC_STATUS_ZOMBIE || */
    /*     proc_array[proc_id_counter % PROC_ARRAY_SIZE]->p_status == PROC_STATUS_DIE) { */
    /*     // no longer in use, then, help to clean that up. */
    /*     proc_destroy(proc_array[proc_id_counter % PROC_ARRAY_SIZE]); */
    /*     proc_array[proc_id_counter % PROC_ARRAY_SIZE] = NULL; */
    /*     return proc_id_counter++; */
    /* } */
    /* else */
    /* { */
    /*     // should not reach here */
    /*     assert(0); */
    /*     return -1; */
    /* } */
}

/* <<<<<<< HEAD */
static void set_free_pid(int pid)
{
    if (pid < 0)
        return;
    assert(get_proc_by_pid(pid) != NULL);
    proc_free_slot_counter ++;
    proc_array[procid_to_procarray_index(pid)] = NULL;
/* ======= */
/*     if (proc_array[proc_id_counter % PROC_ARRAY_SIZE] == NULL) { */
/*         return proc_id_counter++; */
/*     } else if (proc_array[proc_id_counter % PROC_ARRAY_SIZE]->p_status == PROC_STATUS_ZOMBIE) { */
/*         // no longer in use, then, help to clean that up. */
/*         proc_destroy(proc_array[proc_id_counter % PROC_ARRAY_SIZE]); */
/*         proc_array[proc_id_counter % PROC_ARRAY_SIZE] = NULL; */
/*         return proc_id_counter++;  */
/*     } else { */
/*         // should not reach here */
/*         assert(0); */
/*         return -1; */
/*     } */
/* >>>>>>> 00d4e02b6d0f30d7c9cca8f9f21256d05f47c975 */
}


// FIXME we need split this function....
struct proc* proc_create(char* name, seL4_CPtr fault_ep_cap)
{
    int proc_id = get_free_pid();

    if (proc_id == -1)
    {
        return NULL;
    }

    struct proc * process = (struct proc *)malloc(sizeof(struct proc));
    if (process == NULL)
    {
        return NULL;
    }
    clear_proc(process);

    int err = 0;
    process->p_name = strdup(name);
    process->p_pid = proc_id;
    process->p_father_pid = get_current_proc()->p_pid;
    // TODO we need test badge reuse!!!
    process->p_badge = proc_id % PROC_ARRAY_SIZE;
    proc_array[procid_to_procarray_index(proc_id)] = process;
    process->stime = time_stamp();

    /*
    *  pagetable will take care of the virtual address root
    *  IPC buffer will be created and defined in address space
    *  Stack will be created and defined in address space
    */

    // Init address space and page table
    process->p_addrspace = as_create();
    if (process->p_addrspace == NULL)
    {
        ERROR_DEBUG("proc_create: get a null p_addrspace\n");
        proc_destroy(process);
        return NULL;
    }
    process->p_pagetable = create_pagetable();
    if (process->p_addrspace == NULL)
    {
        ERROR_DEBUG( "proc_create: get a null p_pagetable\n");
        proc_destroy(process);
        return NULL;
    }

    if( init_fd_table(process))
    {
        ERROR_DEBUG( "init_fd_table error\n");
        proc_destroy(process);
        return NULL;
    }
    process->p_addrspace->proc = process;

    // Create a simple 1 level CSpace
    process->p_croot = cspace_create(1);
    assert(process->p_croot != NULL);

    // the order is first init ipc buffer, then setup fault ep?
    as_define_ipc(process, process->p_addrspace);
    as_define_ipc_shared_buffer(process, process->p_addrspace);
    // Copy the fault endpoint to the user app to enable IPC
    process->p_ep_cap = cspace_mint_cap(process->p_croot,
                                        cur_cspace,
                                        fault_ep_cap,
                                        seL4_AllRights,
                                        seL4_CapData_Badge_new(process->p_badge));
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
    // According to `extern char _cpio_archive[];` in main.c
    // It has been declared in main.c
    char * elf_base = cpio_get_file(_cpio_archive, name, &elf_size);
    if (elf_base == NULL)
    {
        proc_destroy(process);
        return NULL;
    }
    /* conditional_panic(!elf_base, "Unable to locate cpio header"); */
    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, " elf_base: 0x%x, entry point: 0x%x   %s\n", (unsigned int)elf_base, (unsigned int)elf_getEntryPoint(elf_base), name);
    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "name: %s\n", name);

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

    // each user level process has one coroutine at sos side
    process->p_coro = create_coro(NULL, NULL);
    assert(process->p_coro != NULL);
    process->p_coro->_proc = process;
    list_add_tail(&(process->as_child_next), &(get_current_proc()->children_list.head));
    return process;
}

void proc_activate(struct proc * process)
{
    if (process->p_status == PROC_STATUS_RUNNING)
    {
        return;
    }
    process->p_status = PROC_STATUS_RUNNING;
    seL4_UserContext context;
    memset(&context, 0, sizeof(context));
    context.pc = elf_getEntryPoint(process->p_addrspace->elf_base);
    context.sp = APP_PROCESS_STACK_TOP;
    seL4_TCB_WriteRegisters(process->p_tcb->cap, 1, 0, 2, &context);
}



// XXX only for current_proc
// pid is the child pid
struct proc* proc_get_child(int pid)
{
    if (pid < 0)
    {
        return NULL;
    }
    struct proc* ret = proc_array[procid_to_procarray_index(pid)];
    if (ret == NULL || ret->p_father_pid != get_current_proc()->p_pid)
    {
        return NULL;
    }
    /* assert(!list_empty(&ret->as_child_next)); */
    return ret;
}


static void recycle_child(struct proc* proc)
{
    assert(list_empty(&proc->as_child_next));
    if (proc->p_status == PROC_STATUS_RUNNING) //place under kproc
    {
        COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "place pid: %d under kproc\n", proc->p_pid);

        list_add_tail(&(proc->as_child_next), &(kproc.children_list.head));
        proc->p_father_pid = kproc.p_pid;
    }
    else if(proc->p_status == PROC_STATUS_ZOMBIE) // destroy it
    {
        COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "clear zombie pid: %d \n", proc->p_pid);
        /* proc_handle_children_process(proc); // XXX recursive.... */
        // clear zombie
        proc_destroy(proc);

    }
    else
    {
        // something not handled?
        assert(0);
    }
    return;
}

static void proc_handle_children_process(struct proc * process)
{
    assert(process != NULL);
	struct list_head *cur = NULL;
	struct list_head* tmp = NULL;
	list_for_each_safe(cur, tmp, &(process->children_list.head))
	{
        struct proc* child = list_entry(cur, struct proc, as_child_next);
		assert(child != NULL);
		link_detach(child, as_child_next);
		assert(child->p_father_pid == process->p_pid);
        recycle_child(child);
	}

	assert(is_list_empty(&(process->children_list)));

    return;
}


void proc_wakeup_father(struct proc* child)
{
    assert(child != NULL);
    assert(child->p_status == PROC_STATUS_RUNNING);
    if (child->someone_wait)
    {
        struct proc* father = get_proc_by_pid(child->p_father_pid);
        // father killed by someone...
        if (father == NULL)
        {
            ERROR_DEBUG("proc_wakeup_father failed\n");
            return;
        }
        assert(father->p_waitchild != NULL);
        V(father->p_waitchild);
    }

}

int proc_destroy(struct proc * process)
{
    // TODO maybe we need handle kproc destroy specially
    assert(process->p_status == PROC_STATUS_ZOMBIE);
    // must be father to clear child even it is attach to kproc!
    if (get_current_proc() != &kproc)
    {
        assert(process->p_father_pid == get_current_proc()->p_pid);
        assert(process != get_current_proc());
    }
    assert(is_list_empty(&process->children_list));
    assert(list_empty(&process->as_child_next));
    assert(process->someone_wait == false);
    destroy_reply_cap(&process->p_reply_cap);

    set_free_pid(process->p_pid);
    // TODO free fs, free pid in M7
    /* destroy_fd_table(process); TODO */

    if (process->p_name != NULL )
    {
        free(process->p_name);
        process->p_name = NULL;
    }

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
        free_sos_object(process->p_tcb, seL4_TCBBits);
        process->p_tcb = NULL;
    }

    /* // revoke & delete capability */
    assert(0 == cspace_revoke_cap(process->p_croot, process->p_ep_cap));
    assert(0 == cspace_delete_cap(process->p_croot, process->p_ep_cap));

    // mentioned in where it is defined, One could also rely on cspace_destroy() to free object,
    // if, and only if, there are no copies of caps to the object outside of the cspace being destroyed.
    // This should be a temporary solution
    cspace_destroy(process->p_croot);

    process->p_croot = NULL;

    destroy_coro(process->p_coro);
    free(process);
    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "destroy process 0x%x ok!\n", process);
    return 0;
}

void proc_exit(struct proc* proc)
{
    assert(proc->p_status == PROC_STATUS_RUNNING);
    proc->p_status = PROC_STATUS_ZOMBIE;
    seL4_TCB_Suspend(proc->p_tcb->cap);
    proc_handle_children_process(proc);

    assert(!list_empty(&proc->as_child_next));

    struct proc* father = get_proc_by_pid(proc->p_father_pid);
    assert(father != NULL); // except for kproc, but it should not exit
    assert(father->p_status == PROC_STATUS_RUNNING); // if father not running it should under init, and init is also running:)
    /* if (proc != get_current_proc()) */
    /*     coro_stop(proc->p_coro);  //make sure app coro not schedule again */

    // TODO
}

// FIXME in M7
void recycle_process()
{
    struct list_head *cur = NULL;
	struct list_head* tmp = NULL;
	list_for_each_safe(cur, tmp, &(kproc.children_list.head))
	{
        struct proc* child = list_entry(cur, struct proc, as_child_next);
		assert(child != NULL);
        if (child->p_status == PROC_STATUS_ZOMBIE)
        {
            link_detach(child, as_child_next);
            assert(child->p_father_pid == kproc.p_pid);
            assert(list_empty(&child->p_coro->_link)) ;
            assert(coro_status(child->p_coro) == COROUTINE_INIT);
            assert(list_empty(&child->children_list.head));
            proc_destroy(child);
            dump_vm_state();
        }
	}

}


