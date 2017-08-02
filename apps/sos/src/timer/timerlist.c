#include "timerlist.h"

/* static struct TimerUnit*  */

extern timestamp_t __timestamp_ms();// timestamp_t is uint64_t
struct TimerUnit* init_timer_unit(int max_timer_count)
{
    struct TimerUnit* u = (struct TimerUnit*)malloc(sizeof(struct TimerUnit));
    if (u == NULL)
    {
        return NULL;
    }


    list_init(&(u->timer_list_head));

    u->free_timer = (struct TimerObjSlot*) malloc(sizeof(struct TimerObjSlot) * max_timer_count);
    u->max_timer_count = max_timer_count;
    if (u->free_timer == NULL)
    {
        destroy_timer_unit(u);
        return NULL;
    }

    // reserve the index 0 for invalid id, so do not use free_timer[0]
    for (int i = 1; i < max_timer_count; i ++)
    {
        u->free_timer[i].next_free = i + 1;
        u->free_timer[i].obj.id = i;
        u->free_timer[i].obj.status = 0;
    }
    u->free_timer[max_timer_count - 1].next_free = 0;
    u->next_free_timer_id = 1;

    return u;
}

void destroy_timer_unit(struct TimerUnit* timer_unit)
{
    if (timer_unit == NULL)
    {
        return ;
    }
    if (timer_unit->free_timer != NULL)
    {
        free(timer_unit->free_timer);
        timer_unit->free_timer = NULL;
    }
    struct list_head* pos, * tmp;
    list_for_each_safe(pos, tmp, &(timer_unit->timer_list_head.head))
    {
        struct TimerList* tl = list_entry(pos, struct TimerList, link_obj);
        link_detach(tl, link_obj);
        free(tl);
    }

    free(timer_unit);
    return;
}

static struct TimerList* create_timer_list(int timeout)
{
    struct TimerList* tl = (struct TimerList*)malloc(sizeof(struct TimerList));
    if (tl == NULL)
    {
        return NULL;
    }

    tl->timeout = timeout;
    list_init(&tl->timer_list);
    return tl;
}

static struct TimerList* get_timer_list(struct TimerUnit* unit, int timeout)
{
    struct TimerList* tl = NULL;

    list_for_each_entry (tl, &(unit->timer_list_head.head), link_obj)
    {
        if (tl->timeout == timeout)
        {
            return tl;
        }
    }
    tl = create_timer_list(timeout);

    /* list_add_tail(&(unit->timer_list_head.head), &(tl->link_obj)); */
    list_add_tail(&(tl->link_obj), &(unit->timer_list_head.head));


    return tl;

}

static struct TimerObj* alloc_free_timer_obj(struct TimerUnit* unit)
{
    if (unit->next_free_timer_id == 0)
    {
        return NULL;
    }
    struct TimerObj* obj = NULL;

    obj = &(unit->free_timer[unit->next_free_timer_id].obj);
    unit->next_free_timer_id = unit->free_timer[obj->id].next_free;
    obj->status |= TIMEROBJ_IN_USE;

    return obj;
}

static void _remove_from_timerlist(struct TimerUnit* unit, struct TimerObj* obj)
{
    (void) unit;
    if (obj->status & TIMEROBJ_IN_TIMERLIST )
    {
        assert(obj->status & TIMEROBJ_IN_USE);
        link_detach(obj, link_obj);
        obj->status &= (~TIMEROBJ_IN_TIMERLIST);
    }
}

static void _free_timer_obj(struct TimerUnit* unit, struct TimerObj* obj)
{

    if (obj->status & TIMEROBJ_IN_USE)
    {
        unit->free_timer[obj->id].next_free = unit->next_free_timer_id;
        unit->next_free_timer_id = obj->id;
        obj->status &= (~TIMEROBJ_IN_USE);
        assert(obj->status == 0);
        assert(list_empty(&(obj->link_obj)));
    }
}

int dettach_timer(struct TimerUnit* unit, uint32_t id)
{
    if (id <= 0 || id >= (uint32_t)unit->max_timer_count )
    {
        return -1;
    }
    _remove_from_timerlist(unit, &(unit->free_timer[id].obj));
    _free_timer_obj(unit, &(unit->free_timer[id].obj));
    return 0;
}



static int _attach_timer(struct TimerUnit* unit, struct TimerObj* obj, int timeout)
{
    struct TimerList* tl = get_timer_list(unit, timeout);
    if (tl == NULL)
    {
        /* dettach_timer(unit, *id); */
        return -1;
    }
    assert(obj->status & TIMEROBJ_IN_USE);
    assert( ! (obj->status & TIMEROBJ_IN_TIMERLIST));
    obj->status |= TIMEROBJ_IN_TIMERLIST;
    list_add_tail(&(obj->link_obj), &(tl->timer_list.head) );
    return 0;
}

int attach_timer(struct TimerUnit* unit,  int timeout, timer_callback_t func, void* data, uint32_t* id)
{
    struct TimerObj * obj = alloc_free_timer_obj(unit);
    if (obj == NULL)
    {
        return -1;
    }
    obj->obj_exp = timeout + __timestamp_ms();
    obj->cb_func = func;
    obj-> cb_data = data;
    if (0 != _attach_timer(unit, obj, timeout))
    {
        _free_timer_obj(unit, obj);
        return -2;
    }
    *id = obj->id;
    return 0;
}

static void timer_notify(struct TimerObj* obj)
{
    assert(obj != NULL );
    obj->cb_func(obj->id, obj->cb_data);
}

static void remove_empty_timer_list(struct TimerUnit* unit)
{

    struct list_head* pos;
    struct list_head* tmp;

    list_for_each_safe(pos, tmp, &(unit->timer_list_head.head))
    {
        struct TimerList* tl = list_entry(pos, struct TimerList, link_obj);
        if (list_empty(&(tl->timer_list.head)))
        {
            link_detach(tl, link_obj);
            free(tl);
        }
    }

}

int rettach_timer(struct TimerUnit* unit, int timeout, timer_callback_t func, void* data, uint32_t id)
{
    if (id <= 0 || id >= (uint32_t)unit->max_timer_count)
    {
        return -1;
    }
    struct TimerObj* obj = &unit->free_timer[id].obj;
    if ((obj->status & TIMEROBJ_IN_USE) == 0)
    {
        return -2;
    }
    if (obj->status & TIMEROBJ_IN_TIMERLIST)
    {
        _remove_from_timerlist(unit, obj);
    }
    unit->free_timer[id].obj.cb_func = func;

    unit->free_timer[id].obj.obj_exp = timeout + __timestamp_ms();
    unit->free_timer[id].obj.cb_data = data;
    /* printf ("rettach: %d, %d\n", id, timeout); */
    if (0 != _attach_timer(unit, obj, timeout))
    {
        _free_timer_obj(unit, obj);
        return -3;
    }
    return 0;
}
int check_expired(struct TimerUnit* unit, int64_t cur_timestamp)
{
    int count = 0;
    struct TimerList* tl = NULL;
    if (cur_timestamp == 0)
    {
        cur_timestamp = __timestamp_ms();
    }
    list_for_each_entry (tl, &(unit->timer_list_head.head), link_obj)
    {
        struct list_head *tmp, *pos;
        list_for_each_safe(pos, tmp, &(tl->timer_list.head))
        {
            struct TimerObj* obj = list_entry(pos, struct TimerObj, link_obj);
            if (obj->obj_exp > cur_timestamp)
            {
                break;
            }
            assert(obj ->status & TIMEROBJ_IN_TIMERLIST);

            _remove_from_timerlist(unit, obj);
            /* link_detach(obj, link_obj); */
            /* obj->status &= (~TIMEROBJ_IN_TIMERLIST); */
            timer_notify(obj);
            if ((obj->status & TIMEROBJ_IN_TIMERLIST) == 0 )
            {
                _free_timer_obj(unit, obj);

            }
            count ++;
        }
    }

    remove_empty_timer_list(unit);
    return count;
}
