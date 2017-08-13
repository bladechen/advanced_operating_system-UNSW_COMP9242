#include "vm.h"
#include "address_space.h"
#include "frametable.h"
#include "vmem_layout.h"
#include "pagetable.h"

#define verbose 5
#include "sys/debug.h"
void vm_bootstrap(void)
{
    // TODO, move frametable swap init here.
}
void vm_shutdown(void)
{

}
int vm_fault(vaddr_t vaddr)
{
    /*  For VM fault triggerd in CODE/DATA address space range,
     *  we try to solve it by allocate new frame, then loading contents from elf files.
     *  For fault address resides in STACK/HEAP address region, only allocate
     *  a new frame for its usage.
     */

    struct proc* cur_proc = get_current_app_proc();
    assert (cur_proc != NULL);
    if (vaddr == 0) // dereference NULL pointer
    {
        ERROR_DEBUG( "dereference NULL\n");
        return EFAULT;
    }
    vaddr &= seL4_PAGE_MASK;
    struct  as_region_metadata *region = as_get_region(cur_proc->p_addrspace, vaddr);
    if (region == NULL)
    {
        ERROR_DEBUG( "can not find region via vaddr 0x%x\n", vaddr);
        return EFAULT;
    }
    if (region->type == IPC)
    {
        ERROR_DEBUG( "ipc region should be mapped 0x%x\n", vaddr);
        assert(0); // should not happend, because we map while start proc.
        return EFAULT;
    }
    // we simply alloc_page and zero them out.
    if (region->type == STACK || region->type == HEAP)
    {
        int ret = as_handle_zerofilled_fault(cur_proc->p_pagetable, region, vaddr);
        if (ret != 0)
        {
            return ret;
        }

    }
    else // code, date
    {
        assert (region->type == CODE || region->type == DATA);
        int ret = as_handle_elfload_fault(cur_proc->p_pagetable, region, vaddr);
        if (ret != 0)
        {
            return ret;
        }
    }
    return 0;
}
