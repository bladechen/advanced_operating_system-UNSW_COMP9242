#include "pagetable.h"
#include "frametable.h"
#include "comm.h"

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
    /* pt->vroot.addr = ut_alloc(seL4_PageDirBits); */
    /* if (pt->vroot.addr == 0) */
    /* { */
    /*     color_print("ut_alloc return 0\n"); */
    /*     destroy_pagetable(pt); */
    /*     return NULL; */
    /* } */
    /* int ret = cspace_ut_retype_addr(pt->vroot.addr, */
    /*                                 seL4_ARM_PageDirectoryObject, */
    /*                                 seL4_PageDirBits, */
    /*                                 cur_cspace, */
    /*                                 &(pt->vroot.cap)); */
    /* if (ret != 0) */
    /* { */
    /*     color_print("cspace_ut_retype_addr ret: %d\n", ret); */
    /*     destroy_pagetable(pt); */
    /*     return NULL; */
    /* } */


    return pt;

}

void destroy_pagetable(struct pagetable* pt )
{
    if (pt == NULL)
    {
        return;
    }
    if (pt->l1_page_dir != NULL)
    {
        for (int i = 0; i < LEVEL1_PAGE_ENTRY_COUNT; ++ i)
        {
            if (pt->l1_page_dir[i] != NULL)
            {
                 struct pagetable_entry* l2 = (pt->l1_page_dir + i);
                for (int j = 0; j < LEVEL2_PAGE_ENTRY_COUNT; ++ j)
                {
                    if (l1->l2_page[j].entity != 0)
                    {
                        free_page(pt, seL4_FRAME_NUMBER_MASK & (l1->l2_page[j].entity));


                    }
                }
                frame_free(pt->l1_page_dir[i]);
                pt->l1_page_dir[i] = NULL;
            }
        }
        frame_free(pt->l1_page_dir);

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
    free_sos_object(&(pt->vroot), seL4_PageDirBits);

}
void free_page(struct pagetable* pt, vaddr_t vaddr)
{

}
int alloc_page(struct pagetable* pt,
               vaddr_t vaddr,
               seL4_ARM_VMAttributes vm_attr,
               seL4_CapRights cap_right)
{
    return 0;
}


