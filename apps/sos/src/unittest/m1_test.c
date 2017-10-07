#include "test.h"
#include "clock/clock.h"
#define verbose 5
#include "sys/debug.h"

extern uint32_t nfs_current_time();
extern seL4_CPtr _sos_interrupt_ep_cap;


static void delay(int count)
{
    int ret = 0;
    for (int i = 0; i < count; i ++)
        ret += i;
    color_print(ANSI_COLOR_GREEN, "delay: %d\n", ret);
}

static void test_start_stop_timer(void)
{
    color_print(ANSI_COLOR_WHITE, "start test_start_stop_timer\n");
    assert(0 == start_timer(_sos_interrupt_ep_cap));

    assert(0 == stop_timer());

    assert(CLOCK_R_UINT == stop_timer());
    assert(0 == start_timer(_sos_interrupt_ep_cap));
    assert(0 == start_timer(_sos_interrupt_ep_cap));
    assert(0 == start_timer(_sos_interrupt_ep_cap));
    assert(0 == stop_timer());


    color_print(ANSI_COLOR_WHITE, "end test_start_stop_timer\n");
    return;

}

void cb1(uint32_t id, void* data)
{
    color_print(ANSI_COLOR_YELLOW, "[%u, %llu], regular tick every 100ms \"%s\"\n",id, time_stamp(),(char*) data);
    assert(register_timer(100, cb1, data));
    return;
}

static void test_one_register(void)
{
    color_print(ANSI_COLOR_WHITE, "begin test_one_register\n");
    assert(0 == start_timer(_sos_interrupt_ep_cap));

    uint32_t id = register_timer(100, cb1, "test_one_register 100ms");

    assert(id > 0);

    color_print(ANSI_COLOR_WHITE, "[%d], register timer (100ms)\n", id);
    /* assert(0 == stop_timer()); */
    color_print(ANSI_COLOR_WHITE, "end test_one_register\n");
}
static void test_add_remove(void)
{

    color_print(ANSI_COLOR_WHITE, "begin test_one_register\n");
    assert(0 == start_timer(_sos_interrupt_ep_cap));
    uint32_t id = register_timer(1000, cb1, "test_add_remove");
    assert(id > 0);
    color_print(ANSI_COLOR_WHITE, "[%d], register timer for remove\n", id);
    assert(remove_timer(id) == 0);
    assert(remove_timer(id) == CLOCK_R_FAIL);
    assert(remove_timer(100) == CLOCK_R_FAIL);

    color_print(ANSI_COLOR_WHITE, "end test_one_register\n");
    return;

}

static void cb2(uint32_t id, void* data)
{
    static int count = 5;
    color_print(ANSI_COLOR_YELLOW, "[%u, %llu], %d left to be removed \"%s\"\n",id, time_stamp(), count, (char*) data);
    count --;
    if (count == 0)
    {
        return;
    }
    assert(id == register_timer(1000, cb2, data));

}
static void test_remove_later(void)
{
    color_print(ANSI_COLOR_WHITE, "begin test_remove_later\n");
    uint32_t id = register_timer(1000, cb2, "test_remove_later");
    assert(id > 0);
    color_print(ANSI_COLOR_WHITE, "[%d], test_remove_later, print only 5 times(1000ms)\n", id);

    color_print(ANSI_COLOR_WHITE, "end test_remove_later\n");
    return;

}


void cb3(uint32_t id, void* data)
{
    /* color_print(ANSI_COLOR_WHITE , "[%u, %llu], my timer: %llu \"%s\"\n",id, time_stamp(), time_stamp()/1000, (char*) data); */
    color_print(ANSI_COLOR_WHITE , "timer: %llu, nfs: %u\n", time_stamp()/1000, nfs_current_time());
    assert(id == register_timer(1000, cb3, data));
    return;
}
static void test_accuracy(void)
{

    uint32_t id = register_timer(1000, cb3, "test_accuracy");
    assert(id > 0);
    color_print(ANSI_COLOR_WHITE, "[%d], test_accuracy\n", id);

}

void cb4(uint32_t id, void* data)
{
    color_print(ANSI_COLOR_YELLOW, "[%d, %llu], trigger every %d\n",id, time_stamp() / 1000,*(int*)data );
    assert(id == register_timer(*(int*)data, cb4, data));
    return;
}
void test_multi_timer(void)
{
    static int interval[3];
    for (int i = 0; i < 3; i ++)
    {
        interval[i] = (i + 1) * 1000;
        assert(register_timer(interval[i], cb4, &interval[i]));
    }
    return;
}
void cb5(uint32_t id, void* data)
{
    *(int*) data += 1000;
    color_print( ANSI_COLOR_GREEN, "[%d, %llu], next time  interval %d\n", id, time_stamp() / 1000,*(int*)data);
    assert(id == register_timer(*(int*)data, cb5, data));
    return;
}

void test_move(void)
{
    static int interval = 1000;

    assert(register_timer(interval, cb5, &interval));

}
void m1_test(void)
{
    /* int count= 100; */
    /* while (count --) */
    {
        /* test_start_stop_timer(); */
        /* test_add_remove(); */
        /* #<{(| test_one_register(); // 100ms |)}># */
        /* test_remove_later(); */
        /* test_move(); */
        test_accuracy();
        /* test_multi_timer(); */
    }

    return;
}
