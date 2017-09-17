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
#include <sys/panic.h>
#include "proc.h"
#include "vm/address_space.h"
#include "vm/pagetable.h"
#include "vm/vm.h"
#include "clock/clock.h"

#include "syscall/handle_syscall.h"

struct proc kproc;

// TODO remove it in m8
extern char _cpio_archive[];



static inline int get_proc_status(struct proc* proc)
{
    return proc->p_status.status;
}

static inline bool proc_status_check(int from_status, int to_status)
{
    if (from_status == PROC_STATUS_INVALID)
    {
        return (to_status == PROC_STATUS_INIT);
    }
    else if (from_status == PROC_STATUS_INIT)
    {
        return (to_status == PROC_STATUS_EXIT || // load elf fails
               to_status == PROC_STATUS_RUNNING);
    }
    else if (from_status == PROC_STATUS_RUNNING)
    {
        return (to_status == PROC_STATUS_EXIT);
    }
    else if (from_status == PROC_STATUS_EXIT)
    {
        return (to_status == PROC_STATUS_ZOMBIE);
    }
    else
    {
        assert(0);
    }
    return false;
}

static inline void set_proc_status(struct proc* proc, int status)
{
    int cur_status = get_proc_status(proc);
    assert(proc_status_check(cur_status, status));
    proc->p_status.status = status;
}

static void clear_proc_resource(struct proc* proc)
{
    proc->p_resource.p_addrspace = NULL;
    proc->p_resource.p_pagetable = NULL;
    proc->p_resource.fs_struct = NULL;
    proc->p_resource.p_tcb = NULL;
    proc->p_resource.p_croot = NULL;
    proc->p_resource.p_ep_cap = 0;
}

static void clear_proc_status(struct proc* proc)
{
    proc->p_status.argv_str = NULL;
    proc->p_status.name = NULL;
    proc->p_status.status = PROC_STATUS_INVALID;
    proc->p_status.stime = (unsigned int)(time_stamp()/1000);

}
static void clear_proc_context(struct proc* proc)
{
    proc->p_context.p_reply_cap = 0;
    proc->p_context.vm_fault_code = 0;
}

static void clear_proc(struct proc* proc)
{
    proc->p_pid = -1;
    proc->p_father_pid =  -1;
    proc->p_coro = NULL;
    proc->someone_wait = false;

    proc->p_waitchild = NULL;
    list_init(&(proc->children_list));
    link_init(&(proc->as_child_next));
    clear_proc_resource(proc);
    clear_proc_status(proc);
    clear_proc_context(proc);
}


static void init_kproc(char* kname)
{
    clear_proc(&kproc);
    kproc.p_status.name = strdup(kname);
    kproc.p_resource.p_pagetable = (struct pagetable*)(kcreate_pagetable());
    set_kproc_coro(&kproc); //daemon coroutine
    kproc.p_pid = alloc_pid(&kproc);
    kproc.p_father_pid = -1; // i have no parent..
    kproc.p_wait_pid = INVALID_WAIT_PID;
    assert(kproc.p_pid == 0);
}

void proc_bootstrap()
{
    char* kname = "sos_kernel";
    bootstrap_coro_env();
    init_kproc(kname);
    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "kernel proc at: %p, coroutine at: %p\n", &kproc, kproc.p_coro);
    kproc.p_status.status = PROC_STATUS_RUNNING;
}


static void _free_proc_resource(struct proc* process)
{
    /* assert(process->p_status.status == PROC_STATUS_INVALID ||process->p_status.status == PROC_STATUS_INIT  || process->p_status.status  == PROC_STATUS_EXIT); */
    /*  */
    assert(process->p_wait_pid == INVALID_WAIT_PID);
    destroy_reply_cap(&process->p_context.p_reply_cap);
    if (process->p_status.argv_str != NULL)
    {
        free(process->p_status.argv_str);
        process->p_status.argv_str = NULL;
    }
    if (process->p_resource.fs_struct != NULL)
    {
        destroy_fd_table(process->p_resource.fs_struct);
        process->p_resource.fs_struct = NULL;
    }

    if (process->p_status.name != NULL )
    {
        free(process->p_status.name);
        process->p_status.name = NULL;
    }

    if (process->p_resource.p_addrspace != NULL)
    {
        as_destroy(process->p_resource.p_addrspace);
        process->p_resource.p_addrspace = NULL;
    }

    // FIXME no need double free:
    if (process->p_resource.p_pagetable != NULL)
    {
        destroy_pagetable(process->p_resource.p_pagetable);
        process->p_resource.p_pagetable = NULL;
    }

    if (process->p_resource.p_tcb != NULL)
    {
        free_sos_object(process->p_resource.p_tcb, seL4_TCBBits);
        process->p_resource.p_tcb = NULL;
    }

    /* // revoke & delete capability */
    if (process->p_resource.p_croot != NULL && process->p_resource.p_ep_cap != 0)
    {
        assert(0 == cspace_revoke_cap(process->p_resource.p_croot, process->p_resource.p_ep_cap));
        assert(0 == cspace_delete_cap(process->p_resource.p_croot, process->p_resource.p_ep_cap));
        process->p_resource.p_ep_cap = 0;
    }

    // mentioned in where it is defined, One could also rely on cspace_destroy() to free object,
    // if, and only if, there are no copies of caps to the object outside of the cspace being destroyed.
    //XXX This should be a temporary solution
    if (process->p_resource.p_croot != NULL)
    {
        cspace_destroy(process->p_resource.p_croot);
        process->p_resource.p_croot = NULL;
    }

    if (process->p_coro != NULL)
    {
        destroy_coro(process->p_coro);
        process->p_coro = NULL;
    }
}

static void _free_proc_pid(struct proc* process)
{
    assert(process->someone_wait == 0 && process->p_waitchild == NULL);
    if (process->p_pid != -1)
    {
        free_pid(process->p_pid);
        process->p_pid = -1;
    }
}
// free the resource the process hold, except for the process itself and its pid
static void _free_proc(struct proc * process)
{

    /* assert(process->p_status.status == PROC_STATUS_INVALID || */
    /*        process->p_status.status == PROC_STATUS_INIT  || process->p_status.status == PROC_STATUS_EXIT); */
    // must be father to clear child even it is attach to kproc!
    /* if (get_current_proc() != &kproc) */
    assert(process->p_father_pid == -1 );// || process->p_father_pid == get_current_proc()->p_pid);
    assert(process != get_current_proc());
    assert(process->p_father_pid == -1);
    assert(is_list_empty(&process->children_list));
    assert(process->p_wait_pid == INVALID_WAIT_PID);
    assert(!is_linked(&process->as_child_next));
    assert(process->someone_wait == false);
    assert(process->p_waitchild == NULL);
    _free_proc_resource(process);
    int tmp = process->p_pid;
    _free_proc_pid(process);
    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "destroy process %d 0x%x ok!\n",tmp, process);
    free(process);

    return;
}

// only alloc proc without doing pid binding or load elf.
int _init_proc(struct proc* process, char* name, seL4_CPtr fault_ep_cap)
{
    assert(process != NULL);
    assert(process->p_pid > 0); // 0 is for kproc!
    int ret = 0;
    process->p_status.name = strdup(name);
    process->p_wait_pid = INVALID_WAIT_PID;

    // begin init p_resource
    process->p_resource.p_pagetable = create_pagetable();
    if (process->p_resource.p_pagetable == NULL)
    {
        ERROR_DEBUG( "proc_create: get a null p_pagetable\n");
        return -1;
    }

    process->p_resource.p_addrspace = as_create(process->p_resource.p_pagetable);
    if (process->p_resource.p_addrspace == NULL)
    {
        ERROR_DEBUG("proc_create: get a null p_addrspace\n");
        return -1;
    }

    // the order is first init ipc buffer, then setup fault ep?
    ret = as_define_ipc(process->p_resource.p_addrspace);
    if (ret != 0)
    {
        ERROR_DEBUG("as_define_ipc err: %d\n", ret);
        return -1;
    }
    ret = as_define_ipc_shared_buffer(process->p_resource.p_addrspace);
    if (ret != 0)
    {
        ERROR_DEBUG("as_define_ipc_shared_buffer err: %d\n", ret);
        return -1;
    }

    vaddr_t stack_pointer;
    ret = as_define_stack(process->p_resource.p_addrspace, &stack_pointer);
    if (ret != 0)
    {
        ERROR_DEBUG("as_define_stack err: %d\n", ret);
        return -1;
    }

    ret = as_define_heap(process->p_resource.p_addrspace);
    if (ret != 0)
    {
        ERROR_DEBUG("as_define_heap err: %d\n", ret);
        return -1;
    }

    // TODO: as_define_mmap(process->p_addrspace);

    ret = init_fd_table((&process->p_resource.fs_struct));
    if(ret != 0)
    {
        ERROR_DEBUG( "init_fd_table error\n");
        return -1;
    }

    // Create a simple 1 level CSpace
    process->p_resource.p_croot = cspace_create(1);
    if (process->p_resource.p_croot == NULL)
    {
        ERROR_DEBUG("cspace_create error\n");
        return -1;
    }

    // Copy the fault endpoint to the user app to enable IPC
    process->p_resource.p_ep_cap = cspace_mint_cap(process->p_resource.p_croot,
                                        cur_cspace,
                                        fault_ep_cap,
                                        seL4_AllRights,
                                        seL4_CapData_Badge_new(process->p_pid));
    assert(process->p_resource.p_ep_cap != CSPACE_NULL);
    assert(process->p_resource.p_ep_cap == 1);

    struct sos_object * tcb_obj = (struct sos_object *)malloc(sizeof(struct sos_object));
    if (tcb_obj == NULL)
    {
        ERROR_DEBUG("cspace_create error\n");
        return -1;
    }
    clear_sos_object(tcb_obj);
    ret = init_sos_object(tcb_obj, seL4_TCBObject, seL4_TCBBits);
    if (ret != 0)
    {
        ERROR_DEBUG("Failed to create TCB: %d\n", ret);
        return -1;
    }
    process->p_resource.p_tcb = tcb_obj;

    // configure TCB
    // hardcode priority as 0
    ret = seL4_TCB_Configure(process->p_resource.p_tcb->cap,
                             process->p_resource.p_ep_cap,
                             0,
                              process->p_resource.p_croot->root_cnode,
                              seL4_NilData,
                              process->p_resource.p_pagetable->vroot.cap,
                              seL4_NilData,
                              APP_PROCESS_IPC_BUFFER,
                              as_get_ipc_cap(process->p_resource.p_addrspace));
    if (ret != 0)
    {
        ERROR_DEBUG("seL4_TCB_Configure failed: %d\n", ret);
        return -1;
    }

    // init misc
    process->p_coro = create_coro(NULL, NULL);
    if (process->p_coro == NULL)
    {
        ERROR_DEBUG("create_coro failed: %d\n", ret);
        return -1;
    }
    process->p_coro->_proc = process;

    loop_through_region(process->p_resource.p_addrspace);
    set_proc_status(process, PROC_STATUS_INIT);
    return 0;
}

struct proc* proc_create(char* name, seL4_CPtr fault_ep_cap)
{
    struct proc * process = (struct proc *)malloc(sizeof(struct proc));
    if (process == NULL)
    {
        return NULL;
    }
    clear_proc(process);
    int pid = alloc_pid(process);
    if (pid == -1)
    {
        _free_proc(process);
        return NULL;
    }
    process->p_pid = pid;
    int ret = _init_proc(process, name, fault_ep_cap);
    if (ret != 0)
    {
        ERROR_DEBUG("try to init proc '%s' with pid: %d error\n", name, pid);
        _free_proc(process);
        return NULL;
    }
    return process;
}

bool proc_load_elf(struct proc * process, char* file_name)
{
    assert(get_proc_status(process) == PROC_STATUS_INIT);
    unsigned long elf_size;
    char * elf_base = cpio_get_file(_cpio_archive, file_name, &elf_size);
    if (elf_base == NULL)
    {
        ERROR_DEBUG("cpio_get_file NULL\n");
        return false;
    }
    /* conditional_panic(!elf_base, "Unable to locate cpio header"); */
    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, " elf_base: 0x%x, entry point: 0x%x   %s\n", (unsigned int)elf_base, (unsigned int)elf_getEntryPoint(elf_base), file_name);

    /*** load the elf image info, set up addrspace ***/
    // DATA and CODE region is set up by `vm_elf_load`
    //  in parent coroutine!
    int err = vm_elf_load(process->p_resource.p_addrspace, process->p_resource.p_pagetable->vroot.cap, elf_base);
    if (err != 0)
    {
        return false;
    }
    return true;
}


static int _init_proc_argv_on_stack(struct proc* proc, int argc, char** argv, uint32_t* argv_addr, uint32_t* stack_top)
{
    *argv_addr = 0;
    if (argc == 0)
    {
        *stack_top = APP_PROCESS_STACK_TOP;
        return 0;
    }
    int total_argv_len = 0;
    for (int i = 0; i < argc; ++ i)
    {
        total_argv_len += strlen(argv[0]);
    }
    // XXX to make problem easy, we only support less than 1 pages argv.
    if (total_argv_len >= 4000)
    {
        ERROR_DEBUG("sos can't support the argv len larger than 4000\n");
        return -1;
    }
    struct  as_region_metadata *region = as_get_region(proc->p_resource.p_addrspace, APP_PROCESS_STACK_TOP - seL4_PAGE_SIZE);
    assert(region != NULL);
    int ret = as_handle_page_fault(proc->p_resource.p_pagetable, region, APP_PROCESS_STACK_TOP - seL4_PAGE_SIZE, FAULT_WRITE);
    if (ret != 0)
    {
        ERROR_DEBUG("as_handle_page_fault failed: %d\n", ret);
        return -1;
    }
    uint32_t tmp = page_phys_addr(proc->p_resource.p_pagetable, APP_PROCESS_STACK_TOP - seL4_PAGE_SIZE);
    assert(tmp != 0);

    *stack_top = APP_PROCESS_STACK_TOP - seL4_PAGE_SIZE;
    *argv_addr = *stack_top;

    char* phy_addr = (char*)(tmp + 4 * argc);
    uint32_t virt_addr = *argv_addr ;
    virt_addr += 4 * argc;
    for (int i = 0; i < argc; ++ i)
    {
        memcpy((char*)(tmp) + 4 * i, &virt_addr, 4); // **argv
        memcpy(phy_addr, argv[i], strlen(argv[i])); // argv[0....]
        phy_addr[strlen(argv[i])] = 0;
        phy_addr += strlen(argv[i]) + 1;
        printf ("argv[%d] %s inapp viraddr: 0x%08x len: %d\n", i, argv[i], virt_addr, strlen(argv[i]));
        virt_addr += strlen(argv[i]) + 1;
    }
    *phy_addr = 0;
    return 0;
}

int proc_start(struct proc* proc, int argc, char** argv)
{
    int err = 0;
    if (get_proc_status(proc) == PROC_STATUS_RUNNING)
    {
        return 0;
    }
    assert(get_proc_status(proc) ==  PROC_STATUS_INIT);
    int len = 0;
    for (int i = 0; i < argc; i ++)
    {
        len += strlen(argv[i]);
    }
    assert(proc->p_status.argv_str == NULL);
    if (len != 0)
    {
        proc->p_status.argv_str = malloc(len + argc);
        int idx = 0;
        for (int i = 0; i < argc; i ++)
        {
            memcpy(proc->p_status.argv_str + idx, argv[i], strlen(argv[i]));
            idx += strlen(argv[i]);
            if (i != argc -1)
                proc->p_status.argv_str[idx ++] = ' ';
        }
        proc->p_status.argv_str[idx] = 0;
        assert(idx == len + argc - 1);
    }
    else
    {
        proc->p_status.argv_str = strdup(proc->p_status.name);
    }
    printf ("proc argv [%s]\n", proc->p_status.argv_str);

    seL4_UserContext context;
    memset(&context, 0, sizeof(context));
    uint32_t argv_stack_addr = 0;
    uint32_t stack_top = 0;
    err = _init_proc_argv_on_stack(proc, argc, argv, &argv_stack_addr, &stack_top);
    if (err != 0)
    {
        ERROR_DEBUG("_init_proc_argv_on_stack error %d\n", err);
        return err;
    }
    context.r5 = argc;
    context.r6 = argv_stack_addr;
    context.pc = elf_getEntryPoint(proc->p_resource.p_addrspace->elf_base);
    context.sp = stack_top;
    set_proc_status(proc, PROC_STATUS_RUNNING);
    seL4_TCB_WriteRegisters(proc->p_resource.p_tcb->cap, 1, 0, 16, &context);
    return 0;
}



static void _recycle_child(struct proc* proc)
{
    assert(!is_linked(&proc->as_child_next));
    if (get_proc_status(proc) == PROC_STATUS_RUNNING || get_proc_status(proc) == PROC_STATUS_SLEEP) //place under kproc
    {
        COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "place pid: %d under kproc\n", proc->p_pid);

        list_add_tail(&(proc->as_child_next), &(kproc.children_list.head));
        proc->someone_wait = false;
        proc->p_father_pid = kproc.p_pid;
    }
    else if(get_proc_status(proc) == PROC_STATUS_ZOMBIE) // destroy it
    {
        COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "free zombie pid: %d \n", proc->p_pid);
        proc_destroy(proc);
    }
    else
    {
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
		assert(child->p_father_pid == process->p_pid);
        proc_deattch(child);
        _recycle_child(child);
	}

	assert(is_list_empty(&(process->children_list)));

    return;
}


static bool proc_wakeup_father(struct proc* child)
{
    assert(child != NULL);
    assert(get_proc_status(child) == PROC_STATUS_ZOMBIE);
    struct proc* father = pid_to_proc(child->p_father_pid);
    assert(father != NULL);
    if (child->p_father_pid == kproc.p_pid) // sosh shell die...
    {
        ERROR_DEBUG("already under kproc, kproc will collect : %d\n", child->p_pid);
        return false;
    }
    // father exactly wait this process, wait(child->p_pid)
    if (child->someone_wait)
    {
        if (get_proc_status(father) == PROC_STATUS_RUNNING)
        {
            assert(father->p_wait_pid == child->p_pid);
            COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "proc %d wake up: %d\n", child->p_pid, father->p_pid);
            assert(father->p_waitchild != NULL);
            V(father->p_waitchild);
            return true;
        }
        else
        {
            assert(get_proc_status(father) != PROC_STATUS_SLEEP);
            ERROR_DEBUG("wake up father failed pid: %d is in status: %d\n", father->p_pid, get_proc_status(father));
            return false;
        }
    }
    else
    {
        if (father->p_wait_pid == WAIT_ALL_PID )//&& get_proc_status(father) == PROC_STATUS_RUNNING)
        {
            assert(get_proc_status(father) == PROC_STATUS_RUNNING);
            COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "proc %d (waitall) wake up: %d\n", child->p_pid, father->p_pid);
            father->p_wait_pid = child->p_pid; //tell father who you are waiting for.
            assert(father->p_waitchild != NULL);
            V(father->p_waitchild);
            return true;
        }
        else
        {
            return false;
        }
    }
}


void proc_attach_father(struct proc* child, struct proc* father)
{
    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "pid: %d attach to pid: %d as a child\n", child->p_pid, father->p_pid);
    assert(!is_linked(&(child->as_child_next)));
    assert(child->p_father_pid == -1);;
    list_del(&(child->as_child_next));
    list_add_tail(&(child->as_child_next), &(father->children_list.head));
    child->p_father_pid = father->p_pid;
}

void proc_destroy(struct proc * process)
{
    /* if (process) */
    assert(process != &kproc);
    int status = get_proc_status(process);
    if (status == PROC_STATUS_INIT || status == PROC_STATUS_INVALID)
    {
        assert(get_current_proc() != process);
        _free_proc(process);
    }
    else if (status == PROC_STATUS_RUNNING)
    {
        proc_exit(process);
    }
    else if(status == PROC_STATUS_EXIT)
    {
        assert(get_current_proc() == &kproc);
        proc_handle_children_process(process);
        _free_proc_resource(process);
        set_proc_status(process, PROC_STATUS_ZOMBIE);
        // if its father not waiting for this process, we destroyed it immediately!
        // XXX which is a little different from POSIX waitpid
        // because sosh do not explicit collect its spawned background process.
        assert(process->p_waitchild == NULL && process->p_wait_pid == INVALID_WAIT_PID);
        bool wake_success = proc_wakeup_father(process);
        if (!wake_success)
        {
            ERROR_DEBUG("no one wait to collect pid: %d, just destroy now\n", process->p_pid);
            process->someone_wait = false;
            proc_deattch(process);
            _free_proc(process);
            dump_vm_state();
        }
    }
    else if(status == PROC_STATUS_ZOMBIE)
    {
        assert(process->p_father_pid == -1 || get_current_proc() == &kproc ||get_current_proc()->p_pid == process->p_father_pid);
        /* _free_proc_pid(); */
        _free_proc(process);
    }
    else
    {
        assert(0);
    }
    return;
}

void proc_exit(struct proc* proc)
{
    assert(get_proc_status(proc) == PROC_STATUS_RUNNING);
    /* assert(proc->p_father_pid  != -1); */
    set_proc_status(proc, PROC_STATUS_EXIT);
    seL4_TCB_Suspend(proc->p_resource.p_tcb->cap);

    struct proc* father = pid_to_proc(proc->p_father_pid);
    assert(father != NULL); // except for kproc, but it should not exit
    assert(get_proc_status(father) == PROC_STATUS_RUNNING || get_proc_status(father) == PROC_STATUS_SLEEP);
    if (proc->p_waitchild != NULL)
    {
        sem_destroy(proc->p_waitchild);
        proc->p_waitchild = NULL;
        proc->p_wait_pid = INVALID_WAIT_PID;
    }
    /* assert(!list_empty(&proc->as_child_next)); */
}

void recycle_process()
{
    struct list_head *cur = NULL;
	struct list_head* tmp = NULL;
	list_for_each_safe(cur, tmp, &(kproc.children_list.head))
	{
        struct proc* child = list_entry(cur, struct proc, as_child_next);
		assert(child != NULL);
        if (get_proc_status(child) == PROC_STATUS_ZOMBIE)
        {
            link_detach(child, as_child_next);
            assert(child->p_father_pid == kproc.p_pid);
            assert(!is_linked(&child->p_coro->_link)) ;
            assert(coro_status(child->p_coro) == COROUTINE_INIT);
            assert(!is_linked(&child->children_list.head));
            proc_destroy(child);
            dump_vm_state();
        }
	}
    for (int i = 0; i < PID_ARRAY_SIZE; ++ i)
    {
        struct proc* p = index_to_proc(i);
        if (p != NULL && get_proc_status(p) == PROC_STATUS_EXIT)
        {
            assert(p->p_father_pid != -1);
            // INIT is kill or exit, SUSPEND is killed while sleeping
            assert(p->p_coro->_status == COROUTINE_INIT || p->p_coro->_status == COROUTINE_SUSPEND);
            proc_destroy(p);
            assert(p->p_status.status == PROC_STATUS_ZOMBIE);
        }
    }
}

void* get_ipc_buffer(struct proc* proc)
{
    // XXX must be 4k, otherwise it will not continuous!!!!
    return (void*)( page_phys_addr(proc->p_resource.p_pagetable, APP_PROCESS_IPC_SHARED_BUFFER));

}

char proc_status_display(struct proc* proc)
{
    int status = get_proc_status(proc);
    if (status == PROC_STATUS_INIT)
    {
        return 'I';
    }
    else if (status == PROC_STATUS_RUNNING)
    {
        if (proc->p_coro->_status == COROUTINE_SUSPEND)
            return 'D';
        else
            return 'R';
    }
    else if (status == PROC_STATUS_SLEEP)
    {
        return 'S';
    }
    else if(status == PROC_STATUS_EXIT)
    {
        return 'E';
    }
    else if(status == PROC_STATUS_ZOMBIE)
    {
        return 'Z';
    }
    else if(status == PROC_STATUS_INVALID)
    {
        return 'V';
    }
    assert(0);
    return 'X';
}
