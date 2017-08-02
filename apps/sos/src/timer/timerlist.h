#ifndef _TIMER_LIST_H_
#define _TIMER_LIST_H_
#include "comm/include.h"
#include "comm/list.h"

#include <clock/clock.h>


// typedef void (*timer_callback_t)(uint32_t id, void *data);


enum E_TimerObj
{
    TIMEROBJ_IN_TIMERLIST = 1,
    TIMEROBJ_IN_USE = 2,

};

struct TimerObj
{

    struct list_head link_obj;

    int64_t obj_exp;

    timer_callback_t cb_func;

    void* cb_data;
    uint32_t id;

    char status; // bit

    // bool rettach;


};

struct TimerObjSlot
{
    uint32_t next_free;
    struct TimerObj obj;
};



struct TimerUnit
{
    struct list timer_list_head;

    struct TimerObjSlot* free_timer;

    uint32_t next_free_timer_id;

    int max_timer_count;

};

struct TimerList
{
    struct list timer_list;

    struct list_head link_obj;

    int timeout;

};


struct TimerUnit* init_timer_unit(int max_timer_count);
void destroy_timer_unit(struct TimerUnit* timer_unit);


int attach_timer(struct TimerUnit* unit, int timeout, timer_callback_t  func, void* data, uint32_t* id);
int dettach_timer(struct TimerUnit* unit, uint32_t id);

int rettach_timer(struct TimerUnit* unit, int timeout, timer_callback_t func, void* data, uint32_t id);


int check_expired(struct TimerUnit* unit, int64_t cur_timestamp);

#endif
