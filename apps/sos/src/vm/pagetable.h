#ifndef _PAGE_TABLE_H_
#define _PAGE_TABLE_H_
#include "vm.h"
#include "vmem_layout.h"
#include "comm/comm.h"
#include "frametable.h"

#define PAGE_CTRL_MASK (0xfff)
#define PAGE_DIRTY_BIT (1 << 0)
#define PAGE_SWAP_BIT  (1 << 1)
#define PAGE_NOT_FIRST_LOAD (1 << 2)
#define PAGE_VALID_BIT (1 << 3)

#define PAGE_SHARED_BIT (1 << 4)
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

    // uint32_t res_page_cnt;
    // uint32_t swap_page_cnt;
};

struct pagetable* kcreate_pagetable(void);
struct pagetable* create_pagetable(void);
void              destroy_pagetable(struct pagetable* pt );
void              free_page(struct pagetable* pt, vaddr_t vaddr);

static inline void free_pages(struct pagetable* pt, vaddr_t vaddr, int npages)
{
    for (int i = 0; i < npages; ++ i)
    {
        free_page(pt, vaddr +( i << 12));
    }
}

int               alloc_page(struct pagetable* pt,
                             vaddr_t vaddr,
                             seL4_ARM_VMAttributes vm_attr,
                             seL4_CapRights cap_right);


// used for shared vmem, the frame is already allocated
int               page_map(struct pagetable* pt,
                           seL4_CPtr cap,
                           vaddr_t vaddr,
                           seL4_ARM_VMAttributes vm_attr,
                           seL4_CapRights cap_right);


int               set_page_writable(struct pagetable* pt,
                               vaddr_t vaddr,
                               seL4_ARM_VMAttributes
                               vm_attr,
                               seL4_CapRights cap_right);


// called by frame table, while swap out.
uint32_t set_page_swapout(struct pagetable_entry* page,   uint32_t swap_frame);

// app_cap stored in frametable.
seL4_CPtr         fetch_page_cap(struct pagetable* pt, vaddr_t vaddr);

// physical addr refer to sos vaddr, not ut addr
paddr_t           page_phys_addr(struct pagetable* pt, vaddr_t vaddr);


void set_page_already_load(struct pagetable* pt, vaddr_t vaddr);
bool is_page_loaded(struct pagetable* pt, vaddr_t vaddr);
void invalid_page_frame(struct pagetable_entry* page);


void page_statistic(struct pagetable* pt, uint32_t*, uint32_t*);

#endif
