#include "pagetable.h"
#include "frametable.h"
#include "comm.h"


static bool _valid_page_addr(uint32_t addr)
{
    return (addr & seL4_PAGE_MASK) != 0 ? true:false;
}
struct pagetable* create_pagetable(void)
{
    struct pagetable* pt = malloc(sizeof (struct pagetable));
    if (pt == NULL)
    {
        color_print(ANSI_COLOR_RED, "malloc pagetable return NULL\n");
        return NULL;
    }

    pt->pt_list = NULL;
    pt->page_dir = NULL;
    init_sos_object(&(pt->vroot));

    pt->page_dir = frame_alloc(NULL);
    if (pt->page_dir == NULL)
    {
        color_print(ANSI_COLOR_RED, "frame_alloc page_dir return NULL\n");
        destroy_pagetable(pt);
        return NULL;
    }

    int ret = init_sos_object(&(pt->vroot), seL4_ARM_PageDirectoryObject, seL4_PageDirBits);
    if (ret != 0)
    {
        destroy_pagetable(pt);
        return NULL;
    }

    return pt;

}

void destroy_pagetable(struct pagetable* pt )
{
    if (pt == NULL)
    {
        return;
    }
    if (pt->page_dir != NULL)
    {
        for (int i = 0; i < LEVEL1_PAGE_ENTRY_COUNT; ++ i)
        {
            if (pt->page_dir[i] != NULL)
            {
                 struct pagetable_entry* l2 = (pt->page_dir + i);
                for (int j = 0; j < LEVEL2_PAGE_ENTRY_COUNT; ++ j)
                {
                    if (l1->l2_page[j].entity != 0)
                    {
                        free_page(pt, seL4_FRAME_NUMBER_MASK & (l1->l2_page[j].entity));
                    }
                }
                frame_free(pt->page_dir[i]);
                pt->page_dir[i] = NULL;
            }
        }
        frame_free(pt->page_dir);
        pt->page_dir = NULL;
    }
    if (pt->pt_list != NULL)
    {
        for (struct sel4_pagetable* it = pt->pt_list;
             it != NULL;
             it = it->next)
        {
            my_unmap_page_table(&(it->sel4_pt));
        }

    }
    free_sos_object(&(pt->vroot), seL4_PageDirBits, NULL);

}

static uint32_t _get_pagetable_entry(struct pagetable* pt, vaddr_t vaddr)
{
    assert(pt != NULL);
    assert((vaddr & (~seL4_PAGE_MASK)) == 0);
    int l1_index = (vaddr & LEVEL1_PAGE_MASK) >> 22;
    int l2_index = (vaddr & LEVEL2_PAGE_MASK) >> 12;
    if (pt->page_dir == NULL)
    {
        return 0;
    }
    else if (pt->page_dir[l1_index] == NULL)
    {
        return 0;

    }
    return pt->page_dir[l1_index][l2_index].entity;

}

static int _insert_pagetable_entry(struct pagetable* pt, vaddr_t vaddr, paddr_t paddr)
{
    assert(pt != NULL);
    assert(_valid_page_addr(vaddr) && _valid_page_addr(paddr) );
    int l1_index = (vaddr & LEVEL1_PAGE_MASK) >> 22;
    int l2_index = (vaddr & LEVEL2_PAGE_MASK) >> 12;
    if (pt->page_dir == NULL)
    {
        return -1;
    }
    if (pt->page_dir[l1_index] == NULL)
    {
        pt->page_dir[l1_index] = frame_alloc(NULL);
        if (pt->page_dir[l1_index]  == NULL)
        {
            return -2;
        }
    }
    assert(_valid_page_addr(pt->page_dir[l1_index][l2_index].entity) == 0);
    pt->page_dir[l1_index][l2_index].entity = paddr;
    return 0;
}

void free_page(struct pagetable* pt, vaddr_t vaddr)
{
    assert(pt != NULL);
    vaddr &= seL4_PAGE_MASK;
    uint32_t entity = _get_pagetable_entry(pt, vaddr);
    assert(entity != 0 && (seL4_PAGE_MASK & entity ) != 0);
    paddr_t paddr = (entity & seL4_PAGE_MASK);
    deattach_page_frame(paddr);
    /* assert(0 ==  seL4_ARM_Page_Unmap(my_cap)); */
    /* assert(0 ==  cspace_delete_cap(cur_cspace, my_cap)); */
}

int alloc_page(struct pagetable* pt,
               vaddr_t vaddr,
               seL4_ARM_VMAttributes vm_attr,
               seL4_CapRights cap_right)
{
    vaddr &= seL4_PAGE_MASK;

    assert(pt != NULL);
    uint32_t entity = _get_pagetable_entry(pt, vaddr);
    assert(entity == 0);

    paddr_t paddr = frame_alloc(NULL);
    if (paddr == NULL)
    {
        color_print(ANSI_COLOR_RED, "frame_alloc return NULL\n");
        return PAGETABLE_OOM;
    }
    int ret = _insert_pagetable_entry(pt, vaddr, paddr);
    if (ret != 0)
    {
        return PAGETABLE_OOM;
    }
    return 0;
}
