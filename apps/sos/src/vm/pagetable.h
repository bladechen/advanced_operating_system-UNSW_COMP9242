#ifndef _PAGE_TABLE_H_
#define _PAGE_TABLE_H_
#include "vm.h"
#include "vmem_layout.h"
#include "comm/comm.h"
#include "frametable.h"

#define PAGE_CTRL_MASK (0xfff)
#define PAGE_DIRTY_BIT (1 << 0)
#define PAGE_SWAP_BIT  (1 << 1)
// define more here if needed


struct pagetable_entry
{
    uint32_t entity; // [12,31] is for frame_number or the swap id, other bits reserved for further Milestones.
};

struct sel4_pagetable
{
    struct sos_object      sel4_pt;
    struct sel4_pagetable* next;
};

typedef sos_vaddr_t (*alloc_frame_func)();
typedef void (*free_frame_func)(sos_vaddr_t vaddr);
struct pagetable
{
    struct sos_object        vroot;
    struct pagetable_entry** page_dir;

    struct sel4_pagetable*   pt_list;
    alloc_frame_func         alloc_func;
    free_frame_func          free_func;
};

struct pagetable* kcreate_pagetable(void);
struct pagetable* create_pagetable(void);
void              destroy_pagetable(struct pagetable* pt );
void              free_page(struct pagetable* pt, vaddr_t vaddr);

int               alloc_page(struct pagetable* pt,
                             vaddr_t vaddr,
                             seL4_ARM_VMAttributes vm_attr,
                             seL4_CapRights cap_right);

// app_cap stored in frametable.
seL4_CPtr         fetch_page_cap(struct pagetable* pt, vaddr_t vaddr);

// physical addr refer to sos vaddr, not ut addr
paddr_t           page_phys_addr(struct pagetable* pt, vaddr_t vaddr);

#endif
