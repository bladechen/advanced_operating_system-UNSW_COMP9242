#ifndef _VM_H_
#define _VM_H_

#include "comm/comm.h"

#define DIVROUND(a,b) (((a) + ((b) - 1)) / (b))
// derive seL4 page size from seL4_PageBits
#define seL4_PAGE_BITS (seL4_PageBits)
#define seL4_PAGE_SIZE          (1 << seL4_PageBits)

#define seL4_FRAME_MASK  (0xFFFFF000U)
#define seL4_PAGE_MASK  (seL4_FRAME_MASK)

#define seL4_MAX_FREE_FRAME_POOL (4000) // 4000 * 4k = 16M

#define LEVEL1_PAGE_ENTRY_COUNT (1024)
#define LEVEL1_PAGE_MASK        (0xFFC00000)
#define LEVEL2_PAGE_ENTRY_COUNT (1024)
#define LEVEL2_PAGE_MASK        (0x003FF000)

#define IS_PAGE_ALIGNED(addr) (!(addr & (~seL4_PAGE_MASK)))




typedef uint32_t vaddr_t;
typedef uint32_t paddr_t;

struct proc;
void vm_bootstrap(void);
void vm_shutdown(void);
// int vm_fault(struct proc* cur_proc, vaddr_t vaddr);
void handle_vm_fault(struct proc* proc, vaddr_t restart_pc, vaddr_t fault_addr, int fault_code);

static inline uint32_t shift_swapnumber(uint32_t swap_number)
{
    // assert(swap_number != 0);

    return (swap_number << 12);
}
static inline uint32_t unshift_swapnumber(uint32_t frame_num)
{
    uint32_t ret = frame_num >> 12;
    // assert(ret != 0);
    return ret;
}

#endif
