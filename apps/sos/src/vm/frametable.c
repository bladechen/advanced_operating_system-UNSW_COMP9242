#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <sel4/types.h>
#include <cspace/cspace.h>
#include <mapping.h>
#include <ut_manager/ut.h>
#include "vm/vmem_layout.h"

#include <sys/debug.h>
#include <sys/panic.h>

#include "frametable.h"
#include "pagetable.h"
#include "swaptable.h"
#include "vm.h"

#define MAX_CAP_ID 1000000 // delete FIXME !!!
static frame_table _frame_table = NULL;
// total managed_frame memory is _managed_frame_num * PAGE_SIZE
static seL4_Word   _managed_frame_num = 0;
static seL4_Word   _ut_hi = 0;
static seL4_Word   _ut_lo = 0;

// point to the frame which is free-ed, but not deleted and unmaped
/* static int _sos_free_index = -1; */

struct frame_table_head
{
    int first;
    int last;

    size_t start_vaddr;
    size_t end_vaddr;

    int free_pages;
    int total_pages;


    int tick_index; // for second chance replacement algorithm
    int tick_start;
    int tick_end;
};

static struct frame_table_head _app_free_index = {-1, -1, 0, 0};

static struct frame_table_head _sos_free_index = {-1, -1, 0, 0};


// used for the pool limitation
/* static int _free_frame_count = 0; */

static bool _is_empty_uframe();

static void _pin_frame(struct frame_table_entry* e)
{
    e->ctrl |= FRAME_PIN_BIT;
}

static void _unpin_frame(struct frame_table_entry* e)
{
    e->ctrl &= ~FRAME_PIN_BIT;
}

static void _clock_set_frame(struct frame_table_entry* e)
{
    e->ctrl |= FRAME_CLOCK_TICK_BIT;

}

static sos_vaddr_t frame_translate_index_to_vaddr(int index);
static void _dump_frame_table_entry(struct frame_table_entry* e)
{
    COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, "frame id: %d[%d:%d]\n", e->myself, e->prev, e->next);
    COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, "* addr: 0x%08x status: %d, ctrl: %d\n", frame_translate_index_to_vaddr(e->myself), e->status, e->ctrl);

}

static struct frame_table_entry* _find_evict_uframe()
{

    assert(_is_empty_uframe());
    struct frame_table_entry* ret = NULL;
    int previous = _app_free_index.tick_index;
    bool next_run = false;
    while (true)
    {
        struct frame_table_entry* tmp = &_frame_table[_app_free_index.tick_index];
        _dump_frame_table_entry(tmp);
        assert(tmp->status == FRAME_APP);

        if (!(tmp->ctrl & FRAME_PIN_BIT) &&(tmp->ctrl & FRAME_CLOCK_TICK_BIT) == 0)
        {
            ret = tmp;
            break;
        }
        tmp->ctrl &= (~FRAME_CLOCK_TICK_BIT);

        _app_free_index.tick_index ++;
        if (_app_free_index.tick_index == _app_free_index.tick_end)
        {
            _app_free_index.tick_index = _app_free_index.tick_start;
        }

        if (_app_free_index.tick_index == previous && next_run == false)
        {
            next_run = true;
            continue;
        }
        if (_app_free_index.tick_index == previous)
        {
            ERROR_DEBUG("no evict frame!!!!\n");
            break;
        }
    }
    return ret;
}

static int _allocate_frame_table(sos_vaddr_t frame_table_start_vaddr, uint32_t frame_table_size);


// for mapping utype mem to sos vm only, not for app vm
static int _build_paddr_to_vaddr_frame(sos_paddr_t paddr, sos_vaddr_t vaddr, seL4_CPtr* cap)
{

    *cap = 0;
    /* printf ("_build_paddr_to_vaddr_frame: 0x%x to 0x%x\n", paddr , vaddr); */
    int ret;
    ret = cspace_ut_retype_addr(paddr,
                                seL4_ARM_SmallPageObject,
                                seL4_PAGE_BITS,
                                cur_cspace,
                                cap);
    if (ret != 0)
    {
        ERROR_DEBUG("_build_paddr_to_vaddr_frame:cspace_ut_retype_addr ret: %d\n", ret);
        return ESEL4API;
    }

    ret = map_page(*cap,
                   seL4_CapInitThreadPD,
                   vaddr,
                   seL4_AllRights,
                   seL4_ARM_Default_VMAttributes);
    if (ret != 0)
    {
        ERROR_DEBUG("_build_paddr_to_vaddr_frame:map_page  paddr: %p, vaddr: %p map_page ret: %d\n", paddr, vaddr, ret);
        return ESEL4API;
    }
    assert(*cap > 0);
    return 0;

}


/*
 * 1-to-1 mapping for physical address to virtual address
 * virtual address to frame_table array index is also 1-to-1 mapping
 * */

static inline bool _is_valid_vaddr(sos_vaddr_t vaddr)
{
    if (IS_PAGE_ALIGNED(vaddr) == false)
    {
        return false;
    }
    if (vaddr < WINDOW_START || vaddr >= (WINDOW_START + _managed_frame_num * seL4_PAGE_SIZE) )
    {
        return false;
    }
    return true;
}

static inline bool _is_valid_paddr(sos_paddr_t paddr)
{
    if (IS_PAGE_ALIGNED(paddr) == false)
    {
        return false;
    }
    if (paddr <  _ut_lo || paddr >= _ut_hi )
    {
        return false;
    }
    return true;
}
// return untyped address
/* static sos_paddr_t frame_translate_vaddr_to_paddr(sos_vaddr_t vaddr) */
/* { */
/*     assert(_is_valid_vaddr(vaddr)); */
/*     return (vaddr - WINDOW_START + _ut_lo); */
/* } */
// return sos mapped virtual address
static sos_vaddr_t frame_translate_paddr_to_vaddr(sos_paddr_t paddr)
{
    assert(_is_valid_paddr(paddr));
    return (paddr - _ut_lo + WINDOW_START);
}

static int frame_translate_vaddr_to_index(sos_vaddr_t vaddr)
{
    assert(_is_valid_vaddr(vaddr));
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

static frame_table_entry* _get_ft_entry(sos_vaddr_t vaddr)
{
    assert(_is_valid_vaddr(vaddr));
    int index = frame_translate_vaddr_to_index(vaddr);
    assert (index >= 0);
    frame_table_entry* e = _frame_table + index;
    return e;

}

static bool _is_empty_uframe()
{
    if (_app_free_index.last == -1)
    {
        assert(_app_free_index.first == -1);
        assert(_app_free_index.free_pages == 0);
        return true;
    }
    return false;
}

static bool _is_empty_kframe()
{
    if (_sos_free_index.last == -1)
    {
        assert(_sos_free_index.first == -1);
        assert(_sos_free_index.free_pages == 0);
        return true;
    }
    return false;
}

static bool _valid_uvaddr(sos_vaddr_t vaddr)
{
    return (vaddr >= _app_free_index.start_vaddr && vaddr <= _app_free_index.end_vaddr);

}

static bool _valid_kvaddr(sos_vaddr_t vaddr)
{
    return (vaddr >= _sos_free_index.start_vaddr && vaddr <= _sos_free_index.end_vaddr);
}

static int _frame_entry_status(frame_table_entry* e)
{
    /* printf ("next %d, prev %d, status: %d\n", e->next, e->prev, e->status); */
    if (e->status == FRAME_UNINIT)
    {
        return FRAME_UNINIT;
    }
    if (e->status == FRAME_FREE_SOS || e->status == FRAME_FREE_APP)
    {
        assert(e->remap_cap == 0 && e->frame_cap != 0);
        /* assert(!_is_empty_kframe()); */
        /* assert(e->next != -1 || e->prev != -1); */
    }
    /* else if (e->status == FRAME_APP) */
    /* { */
    /*     #<{(| assert(e->frame_cap != 0 && e->remap_cap); |)}># */
    /* } */
    /* else if(e->) */
    return e->status;
}

static bool _alloc_ut_page(sos_vaddr_t* vaddr, seL4_Word* cap)
{
    sos_paddr_t paddr = ut_alloc(seL4_PageBits);
    if (paddr == (sos_paddr_t)NULL)
    {
        return false;
    }
    else
    {
        assert(_is_valid_paddr(paddr));
        *vaddr = frame_translate_paddr_to_vaddr(paddr);

        assert (0 == _build_paddr_to_vaddr_frame(paddr, *vaddr, cap));
        assert(*cap > 0);

        return true;
    }
}


static int _pre_alloc_frames(size_t pages, struct frame_table_head* _free, enum frame_entry_status status)
{

    assert(pages >= 1);
    sos_vaddr_t vaddr;
    seL4_Word cap;
    assert(_alloc_ut_page(&vaddr, &cap));
    int index = frame_translate_vaddr_to_index(vaddr);
    _free->first = index;
    int prev_index = index;
    _frame_table[index].prev = -1;
    _frame_table[index].status = status;
    _frame_table[index].frame_cap = cap;
    _free->start_vaddr = vaddr;
    _free->end_vaddr = vaddr;


    for (size_t i = 1; i < pages; ++ i)
    {
        if (_alloc_ut_page(&vaddr, &cap))
        {
            if (vaddr <= _free->start_vaddr)
            {
                _free->start_vaddr = vaddr;
            }
            if (vaddr >= _free->end_vaddr)
            {
                _free->end_vaddr = vaddr;
            }
            index = frame_translate_vaddr_to_index(vaddr);
            _frame_table[index].frame_cap = cap;
            _frame_table[index].prev = prev_index;
            _frame_table[prev_index].next = index;
            _frame_table[index].status = status;
            prev_index = index;
        }
        else
        {
            return -1;
        }
    }
    _free->last = index;
    _frame_table[index].next = -1;
    _free->free_pages = pages;
    _free->total_pages = pages;
    return 0;
}


void frametable_init(size_t umem, size_t kmem)
{
    if (umem == 0)
    {
        umem = DEFAULT_UMEM_BYTES;
    }
    if (kmem == 0)
    {
        kmem = DEFAULT_KMEM_BYTES;
    }
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
    assert(_managed_frame_num * seL4_PAGE_SIZE < WINDOW_END);
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
        _frame_table[i].frame_cap = 0;
        _frame_table[i].remap_cap = 0;
        _frame_table[i].next= -1;
        _frame_table[i].prev= -1;
        _frame_table[i].status = FRAME_UNINIT;
        _frame_table[i].myself = i;
        _frame_table[i].owner = NULL;
    }
    /* _app_free_index = -1; */
    /* _sos_free_index = -1; */


    size_t upages = DIVROUND(umem, seL4_PAGE_SIZE);

    if (_pre_alloc_frames(upages, &_app_free_index, FRAME_FREE_APP) != 0)
    {
        ERROR_DEBUG("reserve %u bytes %d pages for user mem failed\n", umem, upages);
        assert(0);
    }

    _app_free_index.tick_start = _app_free_index.first;
    _app_free_index.tick_index = _app_free_index.tick_start;
    _app_free_index.tick_end = _app_free_index.last;

    printf ("start: %d, end: %d\n", _app_free_index.tick_start, _app_free_index.tick_end);

    size_t kpages = DIVROUND(kmem, seL4_PAGE_SIZE);
    if (_pre_alloc_frames(kpages, &_sos_free_index, FRAME_FREE_SOS) != 0)
    {
        ERROR_DEBUG("reserve %u bytes %d pages for  kmem failed\n", kmem, kpages);
        assert(0);
    }

    /* _free_frame_count =  0; */
    COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, "Frame table init finish: ut_lo:0x%x ut_high:0x%x\n", _ut_lo, _ut_hi);
    COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, "* start vaddr of frame_table: 0x%x\n", _frame_table);
    COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, "* manage vaddr from 0x%x to 0x%x\n", WINDOW_START, WINDOW_START + seL4_PAGE_SIZE * _managed_frame_num);

    COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, "* umem start at [%d:0x%08x] to [%d:0x%08x]\n", _app_free_index.first, _app_free_index.start_vaddr, _app_free_index.last, _app_free_index.end_vaddr);
    COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, "* kmem start at [%d:0x%08x] to [%d:0x%08x]\n", _sos_free_index.first, _sos_free_index.start_vaddr, _sos_free_index.last, _sos_free_index.end_vaddr);
}

/* allocate frame_table */
static int _allocate_frame_table(sos_vaddr_t frame_table_start_vaddr, uint32_t frame_table_size)
{
    int i;
    sos_vaddr_t cur_vaddr = frame_table_start_vaddr;
    assert(!(frame_table_size & (seL4_PAGE_SIZE - 1)));
    seL4_Word num_frames = frame_table_size >> seL4_PageBits;

    for(i = 0; i < num_frames; i++)
    {
        sos_paddr_t paddr = ut_alloc(seL4_PageBits);
        if (paddr)
        {
            // TODO maybe we need record the frame itself cap
            seL4_Word temp_cap;
            int ret = _build_paddr_to_vaddr_frame(paddr, cur_vaddr, &temp_cap);
            if (ret != 0)
            {
                return ESEL4API;
            }
            cur_vaddr += seL4_PAGE_SIZE;
        }
        else
        {
            return ESEL4API;
        }
    }

    COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, "frame table occupied for %d pages\n", i);
    return 0;
}


static void _remove_from_frame_head(struct frame_table_entry* e, struct frame_table_head* head)
{
    // entry in the middle, not affect head
    if (e->next != -1 && e->prev != -1)
    {
        _frame_table[e->next].prev = e->prev;
        _frame_table[e->prev].next = e->next;

    }
    // the last available entry
    else if (head->first == e->myself && head->last == e->myself && e->next == -1 && e->prev == -1)
    {
        head->first = head->last = -1;
    }
    // the first entry pointed by head->first
    else if(head->first == e->myself && e->next != -1 && e->prev == -1)
    {
        head->first = e->next;
        _frame_table[e->next].prev = -1;
    }
    else if (head->last == e->myself && e->prev != -1 && e->next == -1)
    {
        head->last = e->prev;
        _frame_table[e->prev].next = -1;
    }
    else
    {
        ERROR_DEBUG("strange frame status removed entry id %d [<-%d:%d->], head first: %d, last: %d\n",
                    e->myself, e->prev, e->next, head->first, head->last);
        assert(0);
    }

    e->next = -1;
    e->prev = -1;

}

// get frame from head
static struct frame_table_entry* _grab_free_frame(struct frame_table_head* head)
{
    // nothing left
    if (head->first == -1 && head->last== -1)
    {
        assert(head->free_pages == 0);
        return NULL;
    }
    assert(head->first != -1 && head->last != -1);
    struct frame_table_entry* e = _frame_table + head->first;
    _remove_from_frame_head(e, head);
    head->free_pages --;
    return e;
}

//put frame at the tail!!!
static void  _put_back_frame(struct frame_table_entry* e, struct  frame_table_head* head)
{
    assert(head->free_pages < head->total_pages);
    assert(e->next == -1 && e->prev == -1);
    e->prev = head->last;
    if (head->last != -1)
    {
        _frame_table[head->last].next = e->myself;
    }
    head->last = e->myself;
    if (head->first == -1) // empty head
    {
        head->first = e->myself;
    }
    head->free_pages ++;
}

sos_vaddr_t kframe_alloc()
{
    conditional_panic(_frame_table == NULL, "why _frame_table not init while calling frame_alloc.");

    struct frame_table_entry* e = _grab_free_frame(&_sos_free_index);
    if (e == NULL)
    {
        assert(_is_empty_kframe());
        return (sos_vaddr_t)(NULL);
    }
    assert(FRAME_FREE_SOS == _frame_entry_status(e));
    e->status = FRAME_SOS;
    sos_vaddr_t vaddr = frame_translate_index_to_vaddr(e->myself);
    assert(_valid_kvaddr(vaddr));
    _zero_frame(vaddr);
    return vaddr;
}


static void _clear_uframe(struct frame_table_entry* e)
{
    assert(e->remap_cap == 0);
    e->status =  FRAME_FREE_APP;
    e->ctrl = 0;
    e->owner = NULL;
    e->swap_frame_version = 0;
    e->swap_frame_number = 0;
    /* e-> */
}
/* allocate a frame */
sos_vaddr_t uframe_alloc()
{
    conditional_panic(_frame_table == NULL, "why _frame_table not init while calling frame_alloc.");

    struct frame_table_entry* e = _grab_free_frame(&_app_free_index);
    if (e == NULL)
    {
        // FIXME
        /* assert(0); */
        assert(_is_empty_uframe());
        // now try to evict a frame
        e = _find_evict_uframe();
        if (e == NULL)
        {
            return (sos_vaddr_t)(NULL); // OOM
        }

        assert(e->status == FRAME_APP);
        assert(e->remap_cap != 0);
        assert(e->owner != NULL);
        _pin_frame(e); //need pin it, make sure other one not evict or do something with this frame
        uint32_t swap_frame = 0;
        int ret = do_swapout_frame(frame_translate_index_to_vaddr(e->myself), e->swap_frame_number,  (e->ctrl & FRAME_DIRTY_BIT) ? e->swap_frame_version : 0, &swap_frame);
        // TODO print something here
        // TODO see frame swap same
        if (ret != 0)
        {
            ERROR_DEBUG("do_swapout_frame 0x%x, error: %d\n", frame_translate_index_to_vaddr(e->myself), ret);;
            _unpin_frame(e);
            return (sos_vaddr_t)(NULL);
        }

        uint32_t old_page = set_page_swapout((struct pagetable_entry*)(e->owner), swap_frame);
        assert((old_page & seL4_PAGE_MASK) == frame_translate_index_to_vaddr(e->myself));

        _unpin_frame(e);
        _clear_uframe(e);
        // now this frame is fresh new!!!
    }
    assert(FRAME_FREE_APP == _frame_entry_status(e));
    e->status = FRAME_APP;
    _clock_set_frame(e);
    sos_vaddr_t vaddr = frame_translate_index_to_vaddr(e->myself);
    assert(_valid_uvaddr(vaddr));
    _zero_frame(vaddr);
    _dump_frame_table_entry(e);
    return vaddr;
}

void kframe_free(sos_vaddr_t vaddr)
{
    conditional_panic((_frame_table == NULL), "Frame table uninitialised");

    if (vaddr == (sos_vaddr_t)NULL || _is_valid_vaddr(vaddr) == 0 || _valid_kvaddr(vaddr) == false)
    {
        ERROR_DEBUG("invalid kvaddr to free: 0x%x\n", vaddr);
        return;
    }

    int index = frame_translate_vaddr_to_index(vaddr);
    if (index < 0 || _frame_entry_status(_frame_table + index) != FRAME_SOS)
    {
        ERROR_DEBUG("invalid user frame status 0x%x\n", vaddr);
        return;
    }

    frame_table_entry* fte = _frame_table + index;
    assert(fte->remap_cap == 0); // upper layer should make sure release the cap, then free it
    fte->status = FRAME_FREE_SOS;
    _put_back_frame(fte, &_sos_free_index);
}


void uframe_free(sos_vaddr_t vaddr)
{
    conditional_panic((_frame_table == NULL), "Frame table uninitialised");

    if (vaddr == (sos_vaddr_t)NULL || _is_valid_vaddr(vaddr) == 0 ||  _valid_uvaddr(vaddr) == false)
    {
        ERROR_DEBUG("invalid user vaddr to free: 0x%x\n", vaddr);
        return;
    }

    int index = frame_translate_vaddr_to_index(vaddr);
    if (index < 0 || _frame_entry_status(_frame_table + index) != FRAME_APP)
    {
        ERROR_DEBUG("invalid user frame status 0x%x\n", vaddr);
        return;
    }

    frame_table_entry* fte = _frame_table + index;
    fte->status = FRAME_FREE_APP;
    fte->owner = NULL;
    fte->ctrl = 0;
    assert(fte->remap_cap == 0); // upper layer should make sure release the cap, then free it
    _put_back_frame(fte, &_app_free_index);
}

// the caller should firstly frame_alloc, then try to remap the frame to app cap
// the first argv vaddr is as paddr in pagetable!
int set_frame_app_cap(sos_vaddr_t vaddr, seL4_CPtr cap)
{
    /* printf ("set_frame_app_cap 0x:%x, cap: %d\n", vaddr, cap); */
    assert(cap <= MAX_CAP_ID);
    assert(_is_valid_vaddr(vaddr));
    frame_table_entry* e = _get_ft_entry(vaddr);
    int status = _frame_entry_status(e);
    if (status == FRAME_FREE_SOS || status == FRAME_FREE_APP)
    {
        assert(0);
    }
    else if(status == FRAME_UNINIT)
    {
        assert(0);
    }
    // we can not overlap app cap, otherwise, app cap maybe leek.
    else if(status == FRAME_SOS || status == FRAME_APP)
    {
        //FIXME add more status
        if (cap == 0)
        {
            assert(e->remap_cap != 0);
            e->remap_cap = 0;
        }
        else
        {
            assert(e->remap_cap == 0);
            e->remap_cap = cap;
        }
        return 0;
    }
    return -1;
}


uint32_t get_frame_app_cap(sos_vaddr_t vaddr)
{

    assert(_is_valid_vaddr(vaddr));
    frame_table_entry* e = _get_ft_entry(vaddr);
    assert(-1 != _frame_entry_status(e)); // just to verify status
    return e->remap_cap;
}

uint32_t get_frame_sos_cap(sos_vaddr_t vaddr)
{
    assert(_is_valid_vaddr(vaddr) );
    frame_table_entry* e = _get_ft_entry(vaddr);
    assert(-1 != _frame_entry_status(e)); // just to verify status
    return e->frame_cap;
}

// for app code section(icache)
void flush_sos_frame(seL4_Word vaddr)
{
    assert(_is_valid_vaddr(vaddr) && _valid_uvaddr(vaddr));
    frame_table_entry* e = _get_ft_entry(vaddr);
    assert(-1 != _frame_entry_status(e)); // just to verify status
	seL4_CPtr cap = e->frame_cap;
	seL4_ARM_Page_Unify_Instruction(cap, 0, seL4_PAGE_SIZE);
}


void set_uframe_owner(sos_vaddr_t vaddr, void* owner)
{
    assert(_is_valid_vaddr(vaddr) && _valid_uvaddr(vaddr));

    frame_table_entry* e = _get_ft_entry(vaddr);
    assert(e != NULL);
    assert(e->status == FRAME_APP);
    e->owner = owner;
}

void pin_frame(sos_vaddr_t vaddr)
{
    assert(_is_valid_vaddr(vaddr) && _valid_uvaddr(vaddr));
    struct frame_table_entry* e  = &(_frame_table[frame_translate_vaddr_to_index(vaddr)]);
    assert(e->status == FRAME_APP);
    _pin_frame(e);
}


int frame_swapin(uint32_t swap_number, sos_vaddr_t vaddr)
{
    assert(_is_valid_vaddr(vaddr) && _valid_uvaddr(vaddr));
    struct frame_table_entry* e  = &(_frame_table[frame_translate_vaddr_to_index(vaddr)]);
    assert(e->status == FRAME_APP);
    assert(e->remap_cap == 0);
    uint32_t version = 0;
    _pin_frame(e); // pin it. because current thread maybe kick out.
    int ret = do_swapin_frame(vaddr, swap_number, &version);
    if (ret != 0)
    {
        _unpin_frame(e);
        ERROR_DEBUG("do_frame_swapin swapnumber: %d, vaddr: 0x%x ret: %d\n", swap_number, vaddr);
        return ret;
    }
    assert(version > 0);
    assert(e->swap_frame_version == 0);
    e->swap_frame_version = version;
    e->swap_frame_number = swap_number;
    _unpin_frame(e);
    _clock_set_frame(e);
    return 0;
}


void set_uframe_dirty(sos_vaddr_t vaddr, bool dirty)
{
    assert(_is_valid_vaddr(vaddr) && _valid_uvaddr(vaddr));
    struct frame_table_entry* e  = &(_frame_table[frame_translate_vaddr_to_index(vaddr)]);
    assert(e->status == FRAME_APP);
    if (dirty)
    {
        e->ctrl |= FRAME_DIRTY_BIT;
    }
    else
    {
        e->ctrl &= ~FRAME_DIRTY_BIT;
    }
}

