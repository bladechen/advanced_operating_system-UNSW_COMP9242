#include "vm.h"
#include "address_space.h"
#include "frametable.h"
#include "vmem_layout.h"
#include "pagetable.h"
#include "syscall/handle_syscall.h"

#include "sys/debug.h"
void vm_bootstrap(void)
{
    // TODO, move frametable swap init here.
}
void vm_shutdown(void)
{

}

static void vm_fault(void* argv); // coroutine func

// currently still in main coroutine.
void handle_vm_fault(struct proc* proc, vaddr_t restart_pc, vaddr_t fault_addr, int fault_code)
{
    proc->vm_fault_code = fault_code;
    seL4_CPtr reply_cap = cspace_save_reply_cap(cur_cspace);
    assert(reply_cap != CSPACE_NULL);
    assert(proc->p_reply_cap == 0);
    proc->p_reply_cap = reply_cap;
    // FIXME if doing multi proc.
    // currently, one proc one coroutine, after successfully executed
    // one corotine, will call reset_coro which set status to COROTINE_INIT
    assert(coro_status(proc->p_coro) == COROUTINE_INIT);
    // switch to coroutine, then main coroutine return immediately.
    restart_coro(proc->p_coro, vm_fault, (void*)(fault_addr));
    return;
}

// if segment fault or OOM, we can not destroy proc in this function, because we are still on its coroutine.
// simply mark it as zombie
static void vm_fault(void* argv)
{
    /*  For VM fault triggerd in CODE/DATA address space range,
     *  we try to solve it by allocate new frame, then loading contents from elf files.
     *  For fault address resides in STACK/HEAP address region, only allocate
     *  a new frame for its usage.
     */

    struct proc* cur_proc = get_current_proc();
    vaddr_t vaddr = (vaddr_t)(argv);

    assert (cur_proc != NULL);
    if (vaddr == 0) // dereference NULL pointer
    {
        ERROR_DEBUG("[SEGMENT FAULT] proc %d dereference NULL\n", cur_proc->p_pid);
        proc_to_be_killed(cur_proc);
        return ;
    }
    vaddr &= seL4_PAGE_MASK;
    struct  as_region_metadata *region = as_get_region(cur_proc->p_addrspace, vaddr);
    if (region == NULL)
    {
        ERROR_DEBUG( "[SEGMENT FAULT] proc %d can not find region via vaddr 0x%x\n", cur_proc->p_pid, vaddr);
        proc_to_be_killed(cur_proc);
        return;
    }
    if (region->type == IPC || region->type == IPC_SHARED_BUFFER)
    {
        ERROR_DEBUG( "ipc region should be mapped 0x%x\n", vaddr);
        assert(0); // should not happend, because we map while start proc.
    }
    int ret = 0;
    if (cur_proc->vm_fault_code == 2063)// vm fault on write on readonly page.
    {
        if ((region->rwxflag & PF_W) == 0) // but you do not have write right. segmentfault
        {
            ERROR_DEBUG( "[SEGMENT FAULT] proc %d write on readonly region[%d] 0x%x\n", cur_proc->p_pid, region->type, vaddr);
            proc_to_be_killed(cur_proc);

            return;
        }
        else // we should make it dirty(writable) :)
        {
            ret = as_handle_page_fault(cur_proc->p_pagetable, region, vaddr, 1);
        }
    }
    else
    {
        // two case for coming here:
        // 1. the page never mapped in
        // 2. the page swapped out.
        // we simply alloc_page and zero them out.
        //
        if (region->type == STACK || region->type == HEAP)
        {
            ret = as_handle_page_fault(cur_proc->p_pagetable, region, vaddr, 0);
        }
        else // code, date
        {
            assert (region->type == CODE || region->type == DATA);
            ret = as_handle_elfload_fault(cur_proc->p_pagetable, region, vaddr);
        }
    }

    if (ret == ENOMEM)
    {
        ERROR_DEBUG("[OUT OF MEMORY] kill proc %d\n", cur_proc->p_pid);
        proc_to_be_killed(cur_proc);
    }
    else if(ret != 0)
    {
        ERROR_DEBUG("[SEGMENT FAULT] proc %d at 0x%x!\n", cur_proc->p_pid, vaddr);
        proc_to_be_killed(cur_proc);
    }
    else
    {
        // we map the fault page into proc address space, restart proc
        seL4_MessageInfo_t reply = seL4_MessageInfo_new(0, 0, 0, 1);
        seL4_SetMR(0, 0);
        seL4_Send(cur_proc->p_reply_cap, reply);
        destroy_reply_cap(&cur_proc->p_reply_cap);
    }
}

