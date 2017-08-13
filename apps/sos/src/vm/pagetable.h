#ifndef _PAGE_TABLE_H_
#define _PAGE_TABLE_H_
#include "vm.h"
#include "vmem_layout.h"
#include "comm/comm.h"

struct pagetable_entry
{
    uint32_t entity; // [12,31] is for frame_number, other bits reserved for further Milestones.
};

struct sel4_pagetable
{
    struct sos_object      sel4_pt;
    struct sel4_pagetable* next;
};

struct pagetable
{
    struct sos_object        vroot;
    struct pagetable_entry** page_dir;

    struct sel4_pagetable*   pt_list;
};

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
