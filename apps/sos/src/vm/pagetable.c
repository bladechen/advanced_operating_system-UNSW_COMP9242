#include "pagetable.h"
#include "frametable.h"
#include "mapping.h"


#include "swaptable.h"
#include <sys/debug.h>




static bool _valid_page_addr(uint32_t addr)
{
    /* return IS_PAGE_ALIGNED(addr); */
    return ((addr & seL4_PAGE_MASK) != 0);
}

static bool _is_page_swap(uint32_t entity)
{
    return (entity & PAGE_SWAP_BIT);
}

static bool _is_page_dirty(uint32_t entity)
{
    return (entity & PAGE_DIRTY_BIT);
}

static void _set_page_dirty(uint32_t* entity)
{
    assert(!(_is_page_dirty(*entity)));
    *entity |= PAGE_DIRTY_BIT;
}

/* static void _reset_page_dirty(uint32_t* entity) */
/* { */
/*     assert((_is_page_dirty(*entity))); */
/*     *entity &= (~PAGE_DIRTY_BIT); */
/* } */

static bool _is_page_valid(uint32_t entity)
{
    return (entity & PAGE_VALID_BIT);
}

static void _set_page_valid(uint32_t* entity)
{
    assert(!(_is_page_valid(*entity)));
    *entity |= PAGE_VALID_BIT;
}

static void _reset_page_valid(uint32_t* entity)
{
    assert((_is_page_valid(*entity)));
    *entity &= (~PAGE_VALID_BIT);
}

static void _set_page_swap(uint32_t* entity)
{
    assert(!(_is_page_swap(*entity)));
    *entity |= PAGE_SWAP_BIT;
}

static void _reset_page_swap(uint32_t* entity)
{
    assert((_is_page_swap(*entity)));
    *entity &= (~PAGE_SWAP_BIT);
}

static void _set_page_frame(uint32_t* entity, uint32_t frame)
{
    *entity &= PAGE_CTRL_MASK;
    *entity |= frame;
}

static uint32_t _get_page_frame(uint32_t entity)
{
    return (entity & (seL4_PAGE_MASK));
}

struct pagetable* kcreate_pagetable(void)
{

    struct pagetable* pt = malloc(sizeof (struct pagetable));
    if (pt == NULL)
    {
        ERROR_DEBUG("malloc pagetable return NULL\n");
        return NULL;
    }

    pt->pt_list = NULL;
    pt->page_dir = NULL;
    clear_sos_object(&pt->vroot);

    pt->page_dir = (void*)kframe_alloc();
    if (pt->page_dir == NULL)
    {
        ERROR_DEBUG( "frame_alloc page_dir return NULL\n");
        destroy_pagetable(pt);
        return NULL;
    }
    // i don't know the address of seL4_CapInitThreadPD . but it doesn't matter.
    pt->vroot.cap = seL4_CapInitThreadPD;
    pt->alloc_func = kframe_alloc;
    pt->free_func = kframe_free;
    return pt;
}

struct pagetable* create_pagetable(void)
{
    struct pagetable* pt = malloc(sizeof (struct pagetable));
    if (pt == NULL)
    {
        ERROR_DEBUG("malloc pagetable return NULL\n");
        return NULL;
    }

    pt->pt_list = NULL;
    pt->page_dir = NULL;
    clear_sos_object(&pt->vroot);

    pt->page_dir = (void*)kframe_alloc(NULL);
    if (pt->page_dir == NULL)
    {
        ERROR_DEBUG( "frame_alloc page_dir return NULL\n");
        destroy_pagetable(pt);
        return NULL;
    }

    int ret = init_sos_object(&(pt->vroot), seL4_ARM_PageDirectoryObject, seL4_PageDirBits);
    if (ret != 0)
    {
        destroy_pagetable(pt);
        return NULL;
    }
    pt->alloc_func = uframe_alloc;
    pt->free_func = uframe_free;
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
                struct pagetable_entry* l1 = (pt->page_dir[i]);
                for (int j = 0; j < LEVEL2_PAGE_ENTRY_COUNT; ++ j)
                {
                    if (l1[j].entity != 0)
                    {
                        free_page(pt, (i << 22) | (j << 12));
                    }
                }
                kframe_free((sos_vaddr_t)pt->page_dir[i]);
                pt->page_dir[i] = NULL;
            }
        }
        kframe_free((sos_vaddr_t)pt->page_dir);
        pt->page_dir = NULL;
    }
    if (pt->pt_list != NULL)
    {
        for (struct sel4_pagetable* it = pt->pt_list;
             it != NULL;
             )
        {
            my_unmap_page_table(&(it->sel4_pt));
            struct sel4_pagetable* tmp = it;
            it = it->next;
            free(tmp);
        }

    }
    free_sos_object(&(pt->vroot), seL4_PageDirBits);
    free(pt);
    return;
}


static struct pagetable_entry* _get_pt_entry_addr(struct pagetable* pt, vaddr_t vaddr)
{
    assert(pt != NULL);
    assert(_valid_page_addr(vaddr) );
    int l1_index = (vaddr & LEVEL1_PAGE_MASK) >> 22;
    int l2_index = (vaddr & LEVEL2_PAGE_MASK) >> 12;
    if (pt->page_dir == NULL)
    {
        return NULL;
    }
    else if (pt->page_dir[l1_index] == NULL)
    {
        return NULL;

    }
    return &(pt->page_dir[l1_index][l2_index]);
}


static uint32_t _get_pagetable_entry(struct pagetable* pt, vaddr_t vaddr)
{
    struct pagetable_entry* e = _get_pt_entry_addr(pt, vaddr);
    return (e == NULL)? 0 : e->entity;
}


static int _insert_pagetable_entry(struct pagetable* pt, vaddr_t vaddr, uint32_t entity)
{
    assert(pt != NULL);
    uint32_t paddr = entity;
    assert(_valid_page_addr(vaddr) && _valid_page_addr(paddr) );
    int l1_index = (vaddr & LEVEL1_PAGE_MASK) >> 22;
    int l2_index = (vaddr & LEVEL2_PAGE_MASK) >> 12;
    if (pt->page_dir == NULL)
    {
        return -1;
    }
    if (pt->page_dir[l1_index] == NULL)
    {
        pt->page_dir[l1_index] = (void*)kframe_alloc(NULL);
        if (pt->page_dir[l1_index]  == NULL)
        {
            return -2;
        }
    }

    assert(pt->page_dir[l1_index][l2_index].entity == 0 ||  // first time mapping
           _is_page_swap(pt->page_dir[l1_index][l2_index].entity) || // swapped out
           (!_is_page_dirty(pt->page_dir[l1_index][l2_index].entity) && ( _is_page_dirty( paddr))) ); // write on readonly
    pt->page_dir[l1_index][l2_index].entity = paddr;
    return 0;
}

static void _insert_sel4_pt(struct pagetable* pt, struct sos_object* obj)
{
    assert(pt != NULL && obj->addr !=0 && obj->cap !=0);
    struct sel4_pagetable * p = malloc(sizeof (struct sel4_pagetable));
    assert(p != NULL);
    p->next = pt->pt_list;
    pt->pt_list = p;
    p->sel4_pt.addr = obj->addr;
    p->sel4_pt.cap = obj->cap;
    return;
}

static void _unmap_page_frame( paddr_t paddr)
{
    assert(paddr != 0 && IS_PAGE_ALIGNED(paddr));
    seL4_CPtr app_cap = get_frame_app_cap(paddr);
    assert(app_cap != 0);
    assert(0 == seL4_ARM_Page_Unmap(app_cap));
    cspace_delete_cap(cur_cspace, app_cap); //
    set_frame_app_cap(paddr, 0);
}

void free_page(struct pagetable* pt, vaddr_t vaddr)
{
    assert(pt != NULL);
    vaddr &= seL4_PAGE_MASK;
    struct pagetable_entry* e = _get_pt_entry_addr(pt, vaddr);
    if (e == NULL)
    {
        return;
    }
    uint32_t entity = e->entity;
    if (entity == 0)
    {
        return;
    }

    // no real frame, only the cap reference to sos frame.
    if (entity & PAGE_SHARED_BIT)
    {
        seL4_CPtr cap = entity >> 12;
        // TODO merge with _unmap_page_frame
        assert(0 == seL4_ARM_Page_Unmap(cap));
        cspace_delete_cap(cur_cspace, cap); //
        e->entity = 0;
        return;
    }
    // two case, one is in frame, another is in swap
    // you should mark entity to zero, while finish free page
    sos_vaddr_t paddr = _get_page_frame(entity);
    if (pt->free_func == uframe_free && _is_page_swap(entity)) // the page is in swap area
    {
        COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, "do free swap frame %x %x\n",vaddr, entity);
        assert(paddr != 0);
        assert(0 == do_free_swap_frame(unshift_swapnumber(paddr)));
        e->entity = 0;
        return;
    }

    if (_is_page_valid(entity)) // the page is in frame
    {
        _unmap_page_frame(paddr);
        pt->free_func(paddr);
    }
    else
    {
        // invalid via second tick, but still using the frame.
        // if is pinned, means this frame is using by other process, skip it.
        if (!get_uframe_pinned(paddr))
        {
        /* assert(0 == get_frame_app_cap(paddr)); */
            assert(get_uframe_owner(paddr) == e);
            pt->free_func(paddr);
        }
        else
        {
            /* assert(get_uframe_owner(paddr) != e); */
            ERROR_DEBUG("vaddr is not owned by me: %x, skip it\n", paddr);
            set_uframe_owner(paddr, NULL); // make sure the underneath not call this, because it will the mem will be freed.
            // TODO
            /* set */
        }
    }
    e->entity = 0;
}

// FIXME maybe we need handle error case.
static int _map_page(struct pagetable* pt,
                     seL4_CPtr app_cap,
                     uint32_t vaddr,
                     seL4_ARM_VMAttributes vm_attr,
                     seL4_CapRights cap_right)
{
    assert(app_cap != 0);
    int ret = seL4_ARM_Page_Map(app_cap, pt->vroot.cap, vaddr, cap_right, vm_attr);
    if(ret == seL4_FailedLookup)
    {
        /* Assume the error was because we have no page table in sel4 kernel.*/
        struct sos_object sel4_pt;
        clear_sos_object(&sel4_pt);
        ret = map_page_table(pt->vroot.cap, vaddr, &sel4_pt);

        assert(ret == 0);
        if(!ret)
        {
            ret = seL4_ARM_Page_Map(app_cap, pt->vroot.cap, vaddr, cap_right, vm_attr);
            assert(ret == 0);
        }
        _insert_sel4_pt(pt, &sel4_pt);
    }
    return ret;
}

static int _map_page_frame(struct pagetable* pt,
                    vaddr_t vaddr,
                    paddr_t paddr,
                    seL4_ARM_VMAttributes vm_attr,
                    seL4_CapRights cap_right)
{
    assert(IS_PAGE_ALIGNED(vaddr) && IS_PAGE_ALIGNED(paddr));
    seL4_CPtr sos_cap = get_frame_sos_cap(paddr);
    assert(sos_cap != 0);
    seL4_CPtr app_cap = cspace_copy_cap(cur_cspace, cur_cspace, sos_cap, seL4_AllRights);
    int ret = _map_page(pt, app_cap, vaddr, vm_attr, cap_right);
    if (ret != 0)
    {
        ERROR_DEBUG("_map_page ERROR\n");
        cspace_delete_cap(cur_cspace, app_cap);
        return ret;
    }
    /* assert(app_cap != 0); */
    /* int ret = seL4_ARM_Page_Map(app_cap, pt->vroot.cap, vaddr, cap_right, vm_attr); */
    /* if(ret == seL4_FailedLookup) */
    /* { */
    /*     #<{(| Assume the error was because we have no page table in sel4 kernel.|)}># */
    /*     struct sos_object sel4_pt; */
    /*     clear_sos_object(&sel4_pt); */
    /*     ret = map_page_table(pt->vroot.cap, vaddr, &sel4_pt); */
    /*  */
    /*     assert(ret == 0); */
    /*     if(!ret) */
    /*     { */
    /*         ret = seL4_ARM_Page_Map(app_cap, pt->vroot.cap, vaddr, cap_right, vm_attr); */
    /*         assert(ret == 0); */
    /*     } */
    /*     _insert_sel4_pt(pt, &sel4_pt); */
    /* } */
    /* assert(ret == 0); */
    assert(0 == set_frame_app_cap(paddr, app_cap));
    return 0;
}

// caller must be user page table
int set_page_writable(struct pagetable* pt,
                           vaddr_t vaddr,
                           seL4_ARM_VMAttributes vm_attr,
                           seL4_CapRights cap_right)
{
    // XXX we assume the page is mmaped in frametable
    assert(pt != NULL);
    assert(pt->free_func == uframe_free);
    vaddr &= seL4_PAGE_MASK;
    uint32_t entity = _get_pagetable_entry(pt, vaddr);
    /* if (!((entity != 0 && !_is_page_dirty(entity) && (seL4_CanWrite & cap_right)))) */
    /* { */
    /*     ERROR_DEBUG("%x, %d\n", entity, cap_right); */
    /* } */
    assert((entity != 0 && !_is_page_dirty(entity) && (seL4_CanWrite & cap_right)) );
    assert(!_is_page_swap(entity) );
    assert(_is_page_valid(entity));
    assert(_get_page_frame(entity));
    assert(get_uframe_dirty(_get_page_frame(entity)) == _is_page_dirty(_get_pt_entry_addr(pt, vaddr)->entity));

    _unmap_page_frame(_get_page_frame(entity)); // readonly , first unmap, then map into writable
    entity |= PAGE_DIRTY_BIT;

    assert(0 == _insert_pagetable_entry(pt, vaddr, entity));

    assert(0 == _map_page_frame(pt, vaddr, _get_page_frame(entity), vm_attr, cap_right));
    assert(get_uframe_owner(_get_page_frame(entity)) == _get_pt_entry_addr(pt, vaddr));
    /* set_uframe_owner(_get_page_frame(entity), _get_pt_entry_addr(pt, vaddr)); */
    set_uframe_dirty(_get_page_frame(entity), 1);
    clock_set_frame(_get_page_frame(entity));
    return 0;
}

int page_map(struct pagetable* pt,
             seL4_CPtr cap,
             vaddr_t vaddr,
             seL4_ARM_VMAttributes vm_attr,
             seL4_CapRights cap_right)
{
    assert(pt != NULL && cap != 0 && IS_PAGE_ALIGNED(vaddr));
    // caller is responsible for sanity check.
    uint32_t entity =  _get_pagetable_entry(pt, vaddr);
    assert(entity == 0);
    entity |= PAGE_SHARED_BIT;

    seL4_CPtr app_cap = cspace_copy_cap(cur_cspace, cur_cspace, cap, seL4_AllRights);
    assert(app_cap != 0);
    int ret = _map_page(pt, app_cap, vaddr, vm_attr, cap_right);
    /* int ret = seL4_ARM_Page_Map(app_cap, pt->vroot.cap, vaddr, cap_right, vm_attr); */
    /* if(ret == seL4_FailedLookup) */
    /* { */
    /*     struct sos_object sel4_pt; */
    /*     clear_sos_object(&sel4_pt); */
    /*     ret = map_page_table(pt->vroot.cap, vaddr, &sel4_pt); */
    /*  */
    /*     assert(ret == 0); */
    /*     if(!ret) */
    /*     { */
    /*         ret = seL4_ARM_Page_Map(app_cap, pt->vroot.cap, vaddr, cap_right, vm_attr); */
    /*         assert(ret == 0); */
    /*     } */
    /*     _insert_sel4_pt(pt, &sel4_pt); */
    /* } */
    assert(!ret);
    entity |= (app_cap << 12); // maximum cap also 1024 * 1024, so 20 bits
    assert(0 == _insert_pagetable_entry(pt, vaddr, entity));
    return ret;
}

int alloc_page(struct pagetable* pt,
               vaddr_t vaddr,
               seL4_ARM_VMAttributes vm_attr,
               seL4_CapRights cap_right)
{
    assert(pt != NULL);
    vaddr &= seL4_PAGE_MASK;

    uint32_t entity = _get_pagetable_entry(pt, vaddr);
    // 1. page not mapped, which is first time fault
    // 2. page swapped out
    // 3. page second tick marks as invalid
    assert(entity == 0 || _is_page_swap(entity) || !_is_page_valid(entity));

    char original_page[4096];
    bool frame_is_pinned = false;
    // case 3. page marks as invalid, but frame still in mem
    if (entity != 0 && !_is_page_valid(entity) && !_is_page_swap(entity) )
    {
        // XXX if the original frame is pinned,
        // means it is still in swapping, so skip this step, but need copy the page content.
        paddr_t paddr = _get_page_frame(entity);
        assert(paddr != 0);
        if (!get_uframe_pinned(paddr))
        {
            /* goto alloc_process; */
            assert(get_uframe_owner(paddr) == (void*)(_get_pt_entry_addr(pt, vaddr)));
            assert(get_uframe_dirty(paddr) == _is_page_dirty(_get_pt_entry_addr(pt, vaddr)->entity));
            // current fault on write, previous is readonly, now mark it writable
            if ((cap_right & seL4_CanWrite) && !get_uframe_dirty(paddr))
            {
                set_uframe_dirty(paddr, 1);
                _set_page_dirty(&(_get_pt_entry_addr(pt, vaddr)->entity));
            }
            // previous is writable, we should make it writable also, although current fault is read
            else if (!(cap_right & seL4_CanWrite) && get_uframe_dirty(paddr))
            {
                cap_right |= seL4_CanWrite;
            }
            assert(0 == _map_page_frame(pt, vaddr, paddr, vm_attr, cap_right));
            _set_page_valid(&(_get_pt_entry_addr(pt, vaddr)->entity));
            clock_set_frame(paddr);
            return 0;
        }
        else
        {
            frame_is_pinned = true;
            printf("original frame: 0x%x is pinned, now copy it to stack\n", paddr);
            set_uframe_owner(paddr, NULL); // make sure the underneath not call this, because it will the mem will be freed.
            memcpy(original_page, (void*)paddr, 4096);
        }
    }

    paddr_t paddr = pt->alloc_func(NULL); // may be switched out, if do swapping
    if (paddr == 0)
    {
        ERROR_DEBUG( "frame_alloc return NULL\n");
        return ENOMEM;
    }
    /* printf ("alloc page vaddr: %x, at paddr %x\n", vaddr, paddr); */

    // case 2. we need firstly swap in, and the remaining process in same as case 1
    if (_is_page_swap(entity))
    {
        assert(frame_is_pinned == false);
        assert(pt->free_func == uframe_free);
        cap_right &= (~seL4_CanWrite); // if swap in, mark it readonly
        assert(!(cap_right & seL4_CanWrite)); // the page swap in is always readonly
        COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, "fault addr: 0x%08x swap in 0x%08x\n", vaddr, entity);
        uint32_t swap_number = _get_page_frame(entity);
        assert(swap_number != 0);
        int ret = frame_swapin(swap_number, paddr);
        if (ret != 0)
        {
            ERROR_DEBUG("frame_swapin error ret: %d, now free vaddr: 0x%08x\n",ret, paddr);
            pt->free_func(paddr);
            return ret;
        }
        _reset_page_swap(&entity);
    }
    else if (frame_is_pinned)
    {
        struct pagetable_entry* e = _get_pt_entry_addr(pt, vaddr);
        // old information, we need clear it fist. because the original frame is pinned by other process
        // then used by other process
        assert(e->entity != 0);
        e->entity = 0;
        memcpy((void*)paddr, original_page, 4096);
    }

    _set_page_frame(&entity, paddr);

    if (cap_right & seL4_CanWrite)
    {
        entity |= PAGE_DIRTY_BIT;
    }
    else
    {
        entity &= (~PAGE_DIRTY_BIT);
    }
    _set_page_valid(&entity);
    /* entity |= PAGE_NOT_FIRST_LOAD; */
    int ret = _insert_pagetable_entry(pt, vaddr, (entity) );
    if (ret != 0)
    {
        pt->free_func(paddr);
        ERROR_DEBUG( "no enough mem for page table\n");
        return ENOMEM;
    }

    ret = _map_page_frame(pt, vaddr, paddr, vm_attr, cap_right);
    assert(ret == 0);
    // only user frame need this.
    if (pt->alloc_func == uframe_alloc)
    {
        set_uframe_owner(paddr, _get_pt_entry_addr(pt, vaddr));
        set_uframe_dirty(paddr, (cap_right & seL4_CanWrite)? 1: 0);
        set_uframe_user_vaddr(paddr, vaddr); // only for debug purpose...
    }
    return 0;
}

seL4_CPtr fetch_page_cap(struct pagetable* pt, vaddr_t vaddr)
{
    assert(pt != NULL);
    vaddr &= seL4_PAGE_MASK;

    uint32_t entity = _get_pagetable_entry(pt, vaddr);
    if (entity == 0)
    {
        return 0;
    }

    seL4_CPtr sos_cap = get_frame_sos_cap(_get_page_frame(entity));
    if (sos_cap == 0)
    {
        ERROR_DEBUG( "get vaddr 0x%x cap error\n", vaddr);
        return 0;
    }
    return sos_cap;
}

paddr_t page_phys_addr(struct pagetable* pt, vaddr_t vaddr)
{
    assert(pt != NULL);
    vaddr &= seL4_PAGE_MASK;
    uint32_t entity = _get_pagetable_entry(pt, vaddr);
    return _get_page_frame(entity);
}


bool is_page_loaded(struct pagetable* pt, vaddr_t vaddr)
{
    assert(pt != NULL);
    vaddr &= seL4_PAGE_MASK;
    struct pagetable_entry* e = _get_pt_entry_addr(pt, vaddr);
    if (e == NULL)
    {
        return false;
    }
    return (e->entity & PAGE_NOT_FIRST_LOAD);

}

void set_page_already_load(struct pagetable* pt, vaddr_t vaddr)
{
    assert(pt != NULL);
    vaddr &= seL4_PAGE_MASK;
    struct pagetable_entry* e = _get_pt_entry_addr(pt, vaddr);
    assert(!(e->entity & PAGE_NOT_FIRST_LOAD));
    e->entity |= PAGE_NOT_FIRST_LOAD;

}


// set the swap bit, and swap number
// return the old frame number
uint32_t set_page_swapout(struct pagetable_entry* page, uint32_t swap_frame, uint32_t paddr)
{
    uint32_t ret = _get_page_frame(page->entity) ;// make sure it mapped.
    assert(ret & seL4_PAGE_MASK);
    // XXX there is a special case that process A get this frame and set this frame swapout,
    // but process B is also try to use the page mapped into this frame. because it is pinned.
    // B alloc another frame, and left this one as dangle frame
    // so there is no information about this frame in this page now.
    assert(ret == paddr);
    if (ret != paddr)
    {
        ERROR_DEBUG("%x dangle frame: 0x%x\n",ret, paddr);
        return 0;
    }
    /* ERROR_DEBUG("set_page_swapout 0x%x 0x%x\n", paddr, ret); */

    assert(get_uframe_dirty(page->entity & seL4_PAGE_MASK) == _is_page_dirty(page->entity));
    assert(!_is_page_valid(page->entity)); // must invalid_page_frame first. previously first tick make it invalid

    assert(!_is_page_swap(page->entity) );
    _set_page_swap(&(page->entity)); // mark page swappout
    page->entity &= (~PAGE_DIRTY_BIT);
    _set_page_frame(&(page->entity), swap_frame); // record swap offset in page entry
    return ret;
}

// unmap frame, then set it invalid, but the frame is still occupied by the page,
// next tick will make it swapout
void invalid_page_frame(struct pagetable_entry* page)
{
    uint32_t ret = _get_page_frame(page->entity) ;// make sure it mapped.
    assert(ret & seL4_PAGE_MASK);
    assert(!_is_page_swap(page->entity) );
    assert(get_uframe_dirty(page->entity & seL4_PAGE_MASK) == _is_page_dirty(page->entity));
    COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, "invalid_page_frame entity: %x\n", page->entity);
    _unmap_page_frame(_get_page_frame(page->entity));
    _reset_page_valid(&page->entity);
}


void page_statistic(struct pagetable* pt, uint32_t* res, uint32_t* swap)
{
    *res = 0;
    *swap = 0;
    if (pt->page_dir != NULL)
    {
        for (int i = 0; i < LEVEL1_PAGE_ENTRY_COUNT; ++ i)
        {
            if (pt->page_dir[i] != NULL)
            {
                struct pagetable_entry* l1 = (pt->page_dir[i]);
                for (int j = 0; j < LEVEL2_PAGE_ENTRY_COUNT; ++ j)
                {
                    if (l1[j].entity != 0)
                    {
                        if (_is_page_swap(l1[j].entity))
                        {
                            (*swap) ++;
                        }
                        else if(_get_page_frame(l1[j].entity) && _is_page_valid(l1[j].entity))
                        {
                            if (get_uframe_owner(_get_page_frame(l1[j].entity)) == &(l1[j].entity))
                                (*res) ++;
                        }
                    }
                }
            }
        }
    }
}
