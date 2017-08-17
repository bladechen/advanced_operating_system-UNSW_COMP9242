#include "coroutine/coro.h"
#include "clock/clock.h"
#include "comm/comm.h"
#include "time.h"

/* time_t time_stamp ; */


struct timer_obj
{
    void* argv;
    coroutine_func cb;
    time_t exe_time;
};

struct timer_obj timer_list[100] ;
int  timer_count = 0;
void registe_timer(coroutine_func cb, void * argv, time_t next)
{

    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN,"registe_timer coro: %p, next: %llu\n\n", argv, (unsigned long long)next);
    for (int i = 0; i < timer_count ; i ++)
    {
        if (timer_list[i].argv == argv)
        {
            timer_list[i].exe_time = next;
            return;
        }
    }
    timer_list[timer_count].exe_time = next;
    timer_list[timer_count].argv = argv;
    timer_list[timer_count].cb = cb;
    timer_count ++;
}

void timer_cb(void* argv)
{
    resume_coro((struct coroutine*)(argv));
    return;
}
void coro_sleep(void* argv,int interval)
{
    struct coroutine* co = (struct coroutine*)(argv);
    time_t local = time_stamp()/1000;
    registe_timer(timer_cb, co, local + interval);
    yield_coro();
    return;
}
void func1(void* argv)
{
    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN,"start func1\n");
    for (int i = 0;i < 5; i ++)
    {
        COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN,"%p, func1 [%d]: %llu\n",&i, i, (long long unsigned int)(time_stamp() / 1000));
        coro_sleep(argv, 2);
        /* size_t p = (size_t)(&i); */
        /* *(int*)(p + 5000) = 1000; */
    }
    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN,"end func1\n");
}

void func2(void* argv)
{
    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN,"start func2\n");

    for (int i = 0;i < 10; i ++)
    {
        COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN,"%p, func2 [%d]: %llu\n",&i , i, (long long unsigned int)(time_stamp()/1000));
        coro_sleep(argv,4 );

    }
    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN,"end func2\n");
}

struct coroutine* co1;
struct coroutine* co2;
int init_test_coro()
{
    /* bootstrap_coro_env(); */

    co1 = create_coro(func1, NULL);
    co1->_argv = co1;

    co2 = create_coro(func2, NULL);
    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN,"co1 : 0x%p\n", co1);
    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN,"co2 : 0x%p\n", co2);
    co2->_argv = co2;
    /* func1(co1); */
    /* func2(co2); */

    return 0;
}

void coro_test_run()
{
    for (int i = 0; i < timer_count; i ++)
    {
        /* printf("[%d] exe_time: %llu, cur time: %llu\n",i,(unsigned long long)timer_list[i].exe_time ,(unsigned long long)time_stamp); */
        if (timer_list[i].exe_time <= time_stamp() / 1000)
        {
            timer_list[i].cb(timer_list[i].argv);
        }
    }
    if (co1->_status == COROUTINE_INIT)
    {
        /* restart_coro(co1, func1, NULL); */
        restart_coro(co1, func1, co1);
    }
    if (co2->_status == COROUTINE_INIT)
    {
        /* restart_coro(co2, func2, NULL); */
        restart_coro(co2, func2, co2);
    }
    /* if (co1->_status == COROUTINE_INIT && co2->_status == COROUTINE_INIT) */
    /* { */
    /*     break; //finish */
    /* } */
    /* loop_check(); */
    /* COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN,"\n\n\nin daemon loop\n"); */
    schedule_loop();
    /* COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN,"end  daemon loop\n\n"); */
}
