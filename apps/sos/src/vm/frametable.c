// TODO Designs that avoid deleting and retyping a frame object for every frame free and frame alloc request. For example, by using a high- and low-water mark.
// using a list to store free-ed frame. which can be alloc in O(1) without deleting and retyping
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <sel4/types.h>
#include <cspace/cspace.h>
#include <mapping.h>
#include <ut_manager/ut.h>
#include "vm/vmem_layout.h"

#define verbose 5
#include <sys/debug.h>
#include <sys/panic.h>

#include "frametable.h"

static frame_table _frame_table = NULL;
static seL4_Word   _managed_frame_num = 0;
static seL4_Word   _ut_hi = 0;
static seL4_Word   _ut_lo = 0;

static int _free_frame_index = -1; // point to the frame which is free-ed, but not deleted and unmaped

static int _free_frame_count = 0;

/* function declarations */
static int _allocate_frame_table(sos_vaddr_t frame_table_start_vaddr, uint32_t frame_table_size);


static int _build_paddr_to_vaddr_frame(sos_paddr_t paddr, sos_vaddr_t vaddr, seL4_CPtr* cap)
{

    *cap = 0;
    int ret;
    ret = cspace_ut_retype_addr(paddr, seL4_ARM_SmallPageObject, 12,
                                cur_cspace, cap);
    if (ret != 0)
    {
        color_print(ANSI_COLOR_RED, "_build_paddr_to_vaddr_frame:cspace_ut_retype_addr ret: %d\n", ret);
        return ret;
    }

    ret = map_page(*cap, seL4_CapInitThreadPD, vaddr, seL4_AllRights,
                   seL4_ARM_Default_VMAttributes);
    if (ret != 0)
    {
        color_print(ANSI_COLOR_RED, "_build_paddr_to_vaddr_frame:map_page  paddr: %p, vaddr: %p map_page ret: %d\n", paddr, vaddr,  ret);
        return ret;
    }
    return 0;

}

/* function definitions */

/* 1-to-1 mapping for physical address to virtual address
 * virtual address to frame_table array index is also 1-to-1 mapping*/

// return corresponding physical address
static sos_paddr_t frame_translate_vaddr_to_paddr(sos_vaddr_t vaddr) {
    assert(!(vaddr & (seL4_PAGE_SIZE - 1)));
    return (vaddr - WINDOW_START + _ut_lo);
}
// return corresponding virtual address
static sos_vaddr_t frame_translate_paddr_to_vaddr(sos_paddr_t paddr) {
    assert(!(paddr & (seL4_PAGE_SIZE - 1)));
    return (paddr - _ut_lo + WINDOW_START);
}

static int frame_translate_vaddr_to_index(sos_vaddr_t vaddr) {
    if (vaddr < WINDOW_START || vaddr >= (WINDOW_START + _managed_frame_num * seL4_PAGE_SIZE) )
    {
        return -1;
    }
    return (PAGE_SHIFT(vaddr - WINDOW_START));

}

static sos_vaddr_t frame_translate_index_to_vaddr(int index)
{
    if (index < 0 || index >= _managed_frame_num)
    {
        return (sos_vaddr_t)NULL;
    }
    return (WINDOW_START + PAGE_UNSHIFT(index));
}

static void _zero_frame(sos_vaddr_t vaddr)
{
    memset((void*)vaddr, 0, seL4_PAGE_SIZE);
}


static bool _is_valid_vaddr(sos_vaddr_t vaddr)
{
    if (vaddr & (~seL4_FRAME_MASK))
    {
        return false;
    }

    if (vaddr < WINDOW_START || vaddr >= (WINDOW_START + _managed_frame_num * seL4_PAGE_SIZE) )
    {
        return false;
    }
    return true;


}
static frame_table_entry* _get_ft_entry(sos_vaddr_t vaddr)
{
    assert((vaddr & (~seL4_FRAME_MASK)) == 0);
    int index = frame_translate_vaddr_to_index(vaddr);
    assert (index >= 0);
    frame_table_entry* e = _frame_table + index;
    return e;

}

static int _frame_entry_status(frame_table_entry* e)
{
    /* frame_table_entry* e = _get_ft_entry(vaddr); */
    if (e->sos_cap == 0)
    {
        assert(e->app_cap == 0);
        return FRAME_FREE;
    }
    else if (e->app_cap == 0)
    {
        return FRAME_SOS;
    }
    else if (e->app_cap != 0 && e->sos_cap != 0)
    {
        return FRAME_APP;
    }
    assert(0);
    return 0;
}

/* static int frame_translate_paddr_to_index(sos_vaddr_t vaddr) { */
/*     if (vaddr <  _ut_lo || vaddr >= _ut_hi ) ) */
/*     { */
/*         return -1; */
/*     } */
/*     return PAGE_SHIFT(paddr - _ut_lo ); */
/* } */


/* Initialize frame table */
void frametable_init(void) {
    int err;
    uint32_t frame_table_size;
    uint32_t frames_to_be_managed;
    sos_vaddr_t frame_table_start_vaddr;

    // Die on double init
    conditional_panic((_frame_table != NULL), "Frame table has already been initialised");

    // initialize ut_lo and ut_hi
    ut_find_memory(&_ut_lo, &_ut_hi);
    // low level ut mem ensure that it is 4k aligned
    assert(!(_ut_lo & (seL4_PAGE_SIZE -1 )));
    assert(!(_ut_hi & (seL4_PAGE_SIZE -1 )));

    /* dynamically calculate the size of frame_table */
    frames_to_be_managed = DIVROUND((_ut_hi - _ut_lo), seL4_PAGE_SIZE);
    _managed_frame_num = frames_to_be_managed;
    frame_table_size = frames_to_be_managed * sizeof(frame_table_entry);

    frame_table_size = DIVROUND(frame_table_size, seL4_PAGE_SIZE) * seL4_PAGE_SIZE;
    // do this instead of directly WINDOW_START is to keep the address aligned
    // otherwise it may execeed the WINDOW_START when doing allocation

    frame_table_start_vaddr = WINDOW_START - frame_table_size;

    err = _allocate_frame_table(frame_table_start_vaddr, frame_table_size);
    conditional_panic(err, "Failed to allocate frame table");

    assert(frame_table_start_vaddr > DMA_VEND);

    _frame_table = (frame_table) frame_table_start_vaddr;
    for (int i = 0; i < _managed_frame_num; i ++)
    {
        _frame_table[i].sos_cap = 0;
        _frame_table[i].next_free = -1;
    }

    _free_frame_index = -1;
    _free_frame_count =  0;
    dprintf(0, "After initialization: ut_lo:%x ut_high:%x\n", _ut_lo, _ut_hi);
    dprintf(0, "virtual address of frame_table: %x\n", _frame_table);
}

/* allocate frame_table */
static int _allocate_frame_table(sos_vaddr_t frame_table_start_vaddr, uint32_t frame_table_size) {
    int i;
    sos_vaddr_t cur_vaddr = frame_table_start_vaddr;
    assert(!(frame_table_size & (seL4_PAGE_SIZE - 1)));
    seL4_Word num_frames = frame_table_size >> seL4_PageBits;

    for(i = 0; i < num_frames; i++) {
        sos_paddr_t paddr = ut_alloc(seL4_PageBits);
        // dprintf(0, "allocated frame paddr:%x\n", paddr);
        if (paddr) {
            // TODO maybe we need record the frame itself cap
            seL4_Word temp_cap;
            int ret = _build_paddr_to_vaddr_frame(paddr, cur_vaddr, &temp_cap);
            if (ret != 0) {
                return -1;
            }
            cur_vaddr += seL4_PAGE_SIZE;
        } else {
            return -2;
        }
    }

    dprintf(0, "allocated %d pages in range\n", i);

    return 0;
}

/* allocate a frame */
sos_vaddr_t frame_alloc(sos_vaddr_t * vaddr_ptr)
{

    conditional_panic(_frame_table == NULL, "why _frame_table not init while calling frame_alloc.");
    // some free frame already available, no need to ut_alloc
    if (_free_frame_index != -1)
    {
        sos_vaddr_t vaddr = 0;

        _free_frame_count --;
        int index = _free_frame_index;
        _free_frame_index = _frame_table[index].next_free;
        _frame_table[index].next_free = -1;
        assert(_frame_table[index].sos_cap != 0);
        vaddr = frame_translate_index_to_vaddr(index);
        if (vaddr_ptr != NULL) *vaddr_ptr = vaddr;
        _zero_frame(vaddr);
        return vaddr;
    }
    /* static int true_alloc = 0; */
    /* true_alloc ++; */
    /* color_print(ANSI_COLOR_RED, "true alloc %d\n", true_alloc); */
    sos_paddr_t paddr = ut_alloc(seL4_PageBits);
    if (paddr == (sos_paddr_t)NULL) {
        *vaddr_ptr = (sos_vaddr_t) NULL;
        return (sos_vaddr_t) NULL;
    } else {
        assert(paddr >= _ut_lo && paddr < _ut_hi);
        sos_vaddr_t vaddr = frame_translate_paddr_to_vaddr(paddr);

        seL4_Word temp_cap;
        assert (0 == _build_paddr_to_vaddr_frame(paddr, vaddr, &temp_cap));

        uint32_t index = frame_translate_vaddr_to_index(vaddr);

        assert((_frame_table + index)->sos_cap == 0); // something error with ut_alloc ?
        (_frame_table + index)->sos_cap = temp_cap;
        /* (_frame_table + index)->vaddr = vaddr; */


        if (vaddr_ptr != NULL) *vaddr_ptr = vaddr;
        _zero_frame(vaddr);
        return vaddr;
    }
}

void frame_free(sos_vaddr_t vaddr)
{

    conditional_panic((_frame_table == NULL), "Frame table uninitialised");

    if (vaddr == (sos_vaddr_t)NULL || (vaddr & (seL4_PAGE_SIZE - 1)))
    {
        color_print(ANSI_COLOR_RED, "invalid vaddr to free: 0x%x\n", vaddr);
        return;
    }

    sos_paddr_t paddr = frame_translate_vaddr_to_paddr(vaddr);
    int index = frame_translate_vaddr_to_index(vaddr);
    if (index < 0 || _frame_table[index].sos_cap == 0)
    {
        color_print(ANSI_COLOR_RED, "try free vaddr: 0x%x error\n", vaddr);
        return;
    }

    frame_table_entry* fte = _frame_table + index;

    if (_free_frame_count >= seL4_MAX_FREE_FRAME_POOL)
    {
        assert(_free_frame_count == seL4_MAX_FREE_FRAME_POOL);

        // unmap from our window
        int err = seL4_ARM_Page_Unmap(fte->sos_cap);
        conditional_panic(err, "Failed to unmap page from SOS window\n");

        // Delete SmallPageObject cap from SOS cspace
        err = cspace_delete_cap(cur_cspace, fte->sos_cap);
        conditional_panic(err, "Failed to delete SmallPageObject cap from SOS cspace\n");

        // return the frame to ut
        ut_free(paddr, seL4_PageBits);

        // clear the table entry
        fte->sos_cap = 0;
        /* color_print(ANSI_COLOR_RED, "true free\n"); */
    }
    else
    {
        /* color_print(ANSI_COLOR_GREEN, "fake free\n"); */
        assert(fte->next_free == -1);
        fte->next_free = _free_frame_index;
        _free_frame_index = index;
        _free_frame_count ++;
    }
}


int set_frame_app_cap(sos_vaddr_t vaddr, seL4_CPtr cap)
{
    if (_is_valid_vaddr(vaddr) == false)
    {
        return FRAME_TABLE_PARA_ERROR;
    }
    frame_table_entry* e = _get_ft_entry(vaddr);
    int status  = _frame_entry_status(e);
    if (status == FRAME_SOS)
    {
        assert(cap != 0);
        e->app_cap = cap;
        return 0;

    }
    else if(status == FRAME_FREE)
    {
        assert(0);

    }
    else if(status == FRAME_APP)
    {
        assert(cap == 0);
        e->app_cap = cap;
        return 0;
    }
    return -1;
}


uint32_t get_frame_app_cap(sos_vaddr_t vaddr)
{
    if (_is_valid_vaddr(vaddr) == false)
    {
        return 0;
    }

    frame_table_entry* e = _get_ft_entry(vaddr);
    return e->app_cap;
}

uint32_t get_frame_sos_cap(sos_vaddr_t vaddr)
{
    if (_is_valid_vaddr(vaddr) == false)
    {
        return 0;
    }

    frame_table_entry* e = _get_ft_entry(vaddr);
    return e->sos_cap;
}



/* int attach_page_frame(sos_vaddr_t vaddr, */
/*                       seL4_ARM_VMAttributes vm_attr, */
/*                       seL4_CapRights cap_right, */
/*                       seL4_CPtr pd) */
/* { */
/*     if (_is_valid_vaddr(vaddr) == false) */
/*     { */
/*         return FRAME_TABLE_PARA_ERROR; */
/*     } */
/*     int status  = _frame_entry_status(vaddr); */
/*     if (status != FRAME_SOS) */
/*     { */
/*         color_print("_frame_entry_status error: %d\n", status); */
/*         return FRAME_TABLE_INVALID_STATUS; */
/*     } */
/*     frame_table_entry* e = _get_ft_entry(vaddr); */
/*  */
/*     e->app_cap = cspace_copy_cap(cur_cspace, cur_cspace, e->sos_cap, seL4_AllRights); */
/*      if (e->app_cap == 0) */
/*      { */
/*          color_print("cspace_copy_cap "); */
/*          return FRAME_TABLE_SEL4_ERROR; */
/*      } */
/*  */
/*      int ret = seL4_ARM_Page_Map(e->app_cap , pd, vaddr, cap_right, vm_attr); */
/*      if(ret == seL4_FailedLookup){ */
/*          #<{(| Assume the error was because we have no page table |)}># */
/*  */
/*          #<{(| color_print(ANSI_COLOR_GREEN, "seL4_FailedLookup\n"); |)}># */
/*          err = _map_page_table(tty_test_process.vroot, fault_addr); */
/*          if(!err){ */
/*  */
/*              err = seL4_ARM_Page_Map(cap , tty_test_process.vroot, fault_addr, seL4_CanWrite|seL4_CanRead, seL4_ARM_Default_VMAttributes); */
/*              assert(err == 0); */
/*          } */
/*      } */
/*      #<{(| assert(cap > 0); |)}># */
/*  */
/* } */
