// TODO we need check all malloc
#include "vm.h"
#include "address_space.h"
#include "frametable.h"
#include "vmem_layout.h"
#include "pagetable.h"
#include "syscall/handle_syscall.h"
#include "proc/proc.h"
#include "coroutine/coro.h"
#include "swaptable.h"
#include "sys/debug.h"

void vm_bootstrap(void)
{
    printf("initialise frametable...\n");
    frametable_init(0, 0);
    init_swapping();
}

void vm_shutdown(void)
{

}

static void vm_fault(void* argv); // coroutine func

// currently still in main coroutine.
void handle_vm_fault(struct proc* proc, vaddr_t restart_pc, vaddr_t fault_addr, int fault_code)
{
    assert(proc != NULL);
    proc->p_context.vm_fault_code = fault_code;
    seL4_CPtr reply_cap = cspace_save_reply_cap(cur_cspace);
    assert(reply_cap != CSPACE_NULL);
    assert(proc->p_context.p_reply_cap == 0);
    proc->p_context.p_reply_cap = reply_cap;
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

    int fault = sel4_fault_code_to_fault_type(cur_proc->p_context.vm_fault_code);
    if (fault == FAULT_FATAL)
    {
        proc_exit(cur_proc);
        return ;

    }

    if (vaddr == 0) // dereference NULL pointer
    {
        ERROR_DEBUG("[SEGMENT FAULT] proc %d dereference NULL\n", cur_proc->p_pid);
        proc_exit(cur_proc);
        return ;
    }
    vaddr &= seL4_PAGE_MASK;
    struct  as_region_metadata *region = as_get_region(cur_proc->p_resource.p_addrspace, vaddr);
    if (region == NULL)
    {
        ERROR_DEBUG( "[SEGMENT FAULT] proc %d can not find region via vaddr 0x%x\n", cur_proc->p_pid, vaddr);
        proc_exit(cur_proc);
        return;
    }
    if (region->type == IPC || region->type == IPC_SHARED_BUFFER)
    {
        ERROR_DEBUG( "ipc region should be mapped 0x%x\n", vaddr);
        assert(0); // should not happend, because we map while start proc.
    }
    int ret = 0;
    if (cur_proc->p_context.vm_fault_code == 2063)// vm fault on write on readonly page.
    {
        if ((region->rwxflag & PF_W) == 0) // but you do not have write right. segmentfault
        {
            ERROR_DEBUG( "[SEGMENT FAULT] proc %d write on readonly region[%d] 0x%x\n", cur_proc->p_pid, region->type, vaddr);
            proc_exit(cur_proc);

            return;
        }
        else // we should make it dirty(writable) :)
        {
            /* assert(0); */
            ret = as_handle_page_fault(cur_proc->p_resource.p_pagetable, region, vaddr, sel4_fault_code_to_fault_type(cur_proc->p_context.vm_fault_code));
        }
    }
    else
    {
        // two case for coming here:
        // 1. the page never mapped in
        // 2. the page swapped out.
        // we simply alloc_page and zero them out.
        //
        if (region->type == STACK || region->type == HEAP || region->type == MMAP)
        {
            ret = as_handle_page_fault(cur_proc->p_resource.p_pagetable, region, vaddr, sel4_fault_code_to_fault_type(cur_proc->p_context.vm_fault_code));
        }
        else // code, date
        {
            assert (region->type == CODE || region->type == DATA );
            ret = as_handle_elfload_fault(cur_proc->p_resource.p_pagetable, region, vaddr, sel4_fault_code_to_fault_type(cur_proc->p_context.vm_fault_code));
        }
    }
    COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, "handle pid[%d] vm fault finish: %d\n", cur_proc->p_pid, ret);

    if (ret == ENOMEM)
    {
        ERROR_DEBUG("[OUT OF MEMORY] kill proc %d\n", cur_proc->p_pid);
        proc_exit(cur_proc);
    }
    else if(ret != 0)
    {
        ERROR_DEBUG("[SEGMENT FAULT] proc %d at 0x%x!\n", cur_proc->p_pid, vaddr);
        proc_exit(cur_proc);
    }
    else
    {
        // we map the fault page into proc address space, restart proc
        seL4_MessageInfo_t reply = seL4_MessageInfo_new(0, 0, 0, 1);
        seL4_SetMR(0, 0);
        seL4_Send(cur_proc->p_context.p_reply_cap, reply);
        destroy_reply_cap(&cur_proc->p_context.p_reply_cap);
    }
}


void dump_vm_state()
{
    dump_frame_status();
    dump_swap_status();
}

extern struct proc kproc;

static size_t shared_mem_bytes = 0;

static struct shared_vmem* shm_head = NULL;

// currently we only support exactly match.
static struct shared_vmem* find_shared_vm(uint32_t addr, uint32_t size, struct shared_vmem** prev)
{

    struct shared_vmem* pv = NULL;
    struct shared_vmem* tmp = NULL;
    for (tmp = shm_head;
         tmp != NULL; tmp = tmp->next)
    {
        if (tmp->app_vaddr == addr && (tmp->npages << 12) == size)
        {
            break;
        }

        pv = tmp;
    }
    if (pv && prev)
    {
        *prev = pv;
    }
    return tmp;
}

static uint32_t find_avaliable_sos_mem_area(uint32_t size)
{
    if (size >= SHARED_SOS_VM_END - SHARED_SOS_VM_START)
    {
        return 0;
    }
    uint32_t proposed_addr = 0;
    int count = 10;
    bool find = 1;
    while (count--)
    {
        find = 1;
        proposed_addr = (rand() * rand()) & 0x0ffff000;
        proposed_addr += SHARED_SOS_VM_START;
        if (proposed_addr + size > SHARED_SOS_VM_END)
        {
            ++ count;
            continue;
        }
        /* assert(proposed_addr <= SHARED_SOS_VM_END) */
        struct shared_vmem* tmp = NULL;
        for (tmp = shm_head;
             tmp != NULL; tmp = tmp->next)
        {

            if (overlap_mem_area(proposed_addr, size, tmp->sos_vaddr, tmp->npages << 12) ||
                overlap_mem_area(tmp->sos_vaddr, tmp->npages << 12, proposed_addr, size))
            {
                find = 0;
                break;
            }
        }
        if (find)
        {
            break;
        }
    }
    if (find)
    {
        COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, "find avaliable shm: 0x%x, length: %u\n", proposed_addr, size);
        return proposed_addr;

    }
    return 0;
}

static bool alloc_sos_vmem(vaddr_t vaddr, int pages)
{
    if (shared_mem_bytes + (pages << 12) >= MAX_SHARED_KMEM_BYTES)
    {
        ERROR_DEBUG("can't alloc more than %d bytes for shared application vmem\n", MAX_SHARED_KMEM_BYTES);
        return false;
    }
    struct pagetable* pt = proc_pagetable(&kproc);
    assert(pt != NULL);
    for (int i = 0; i < pages; ++ i)
    {
        int ret = alloc_page(pt, vaddr + (i << 12), seL4_ARM_Default_VMAttributes|seL4_ARM_Default_VMAttributes, seL4_CanRead | seL4_CanWrite);
        if (ret != 0)
        {
            ERROR_DEBUG("alloc_sos_vmem failed: %d\n", ret);
            free_pages(pt, vaddr, i);
            return false;
        }

    }
    shared_mem_bytes += pages << 12;
    return true;
}

static void insert_shared_vm(struct shared_vmem* shm)
{
    shm->next = NULL;
    if (shm_head == NULL)
    {
        shm_head = shm;
    }
    else
    {
        shm->next = shm_head->next;
        shm_head->next = shm;
    }
}

void remove_vm_share(uint32_t addr, uint32_t size)
{
    struct shared_vmem* prev = NULL;
    struct shared_vmem* cur = find_shared_vm(addr, size, &prev);
    if (cur == NULL)
    {
        ERROR_DEBUG("remove remove_vm_share failed\n");
        return;
    }
    COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, "find vm app_addr: 0x%x, ref: %d\n", cur->app_vaddr, cur->ref_count);
    if (cur->ref_count == 1)
    {
        // the last app ref to it now, free all the resources
        free_pages(proc_pagetable(&kproc), cur->sos_vaddr, size >> 12);
        shared_mem_bytes -= cur->npages << 12;
        if (prev == NULL)
        {
            if (shm_head->next == NULL
                   && shm_head == cur)
            {
                free(shm_head);
                shm_head = NULL;
            }
            else
            {
                assert(shm_head == cur);
                shm_head = cur->next;
                free(cur);
            }
        }
        else
        {
            prev->next = cur->next;
            free(cur);
        }
    }
    else
    {
        cur->ref_count -- ;
    }
}

// the mem use kernel mem, should be non-swapable
int vm_share(struct addrspace* as,
             uint32_t addr, uint32_t size,
             bool writable)

{
    // TODO maybe we scale up to support page & 0xFFF != 0
    // currently only support addr and size both page aligned
    if (!IS_PAGE_ALIGNED(size) || !IS_PAGE_ALIGNED(addr))
    {
        return EINVAL;
    }
    if (addr + size <= addr)
    {
        return EINVAL;
    }

    struct shared_vmem* shm = find_shared_vm(addr, size, NULL);
    if (shm)
    {
        COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, "find existing sos mem for shared: 0x%x at sos: 0x%x\n", addr, shm->sos_vaddr);
        assert(shm->ref_count >= 1 &&
               shm->app_vaddr > 0);
        uint32_t tmp_addr =  addr;
        bool ok = find_available_mem_area (as, &tmp_addr, size);
        if (!ok)
        {
            ERROR_DEBUG("overlap with other area\n");
            return ENOMEM;
        }
        assert(tmp_addr == addr);
        int ret = as_define_shared_vm(as, shm->sos_vaddr, shm->app_vaddr, shm->npages, writable);
        if (ret != 0)
        {
            ERROR_DEBUG("as_define_shared_vm failed: %d\n", ret);
            return ret;
        }
        ++ shm->ref_count ;
        return 0;
    }
    else
    {
        shm = malloc(sizeof (struct shared_vmem));
        if (shm == NULL)
        {
            ERROR_DEBUG("malloc meta data for shared mem failed\n");
            return ENOMEM;
        }
        uint32_t tmp_addr = addr;
        bool ok = find_available_mem_area (as, &tmp_addr, size);
        if (!ok)
        {
            free(shm);
            ERROR_DEBUG("overlap with other area\n");
            return ENOMEM;
        }
        assert(tmp_addr == addr);
        shm->sos_vaddr = find_avaliable_sos_mem_area(size);
        shm->npages = size >> 12;
        shm->app_vaddr =  addr;
        if (shm->sos_vaddr == 0)
        {
            free(shm);
            ERROR_DEBUG("can't find sos mem area\n");
            return ENOMEM;
        }
        ok = alloc_sos_vmem(shm->sos_vaddr, shm->npages);
        if (!ok)
        {
            free(shm);
            ERROR_DEBUG("no enough kmem for vm share\n");
            return ENOMEM;
        }
        int ret = as_define_shared_vm(as, shm->sos_vaddr, shm->app_vaddr, shm->npages, writable);
        if (ret != 0)
        {
            ERROR_DEBUG("as_define_shared_vm failed: %d\n", ret);
            free(shm);
            return ret;
        }
        shm->ref_count = 1;
        insert_shared_vm(shm);
        return 0;
    }
}
