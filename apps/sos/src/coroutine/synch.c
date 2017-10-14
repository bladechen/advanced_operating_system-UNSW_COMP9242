#include "comm/comm.h"
#include "synch.h"

// TODO the name should be const.


extern int sos_init_flag;
extern void sos_usleep(int usecs);
extern void rpc_timeout(int ms);

static struct wchan* create_wchan()
{
    struct wchan* wc = malloc(sizeof(struct wchan));
    if (wc == NULL)
    {
        return NULL;
    }
    list_init(&(wc->_head));
    /* wc->_head = NULL; */
    /* wc->_tail = NULL; */
    wc->_size  = 0;
    return wc;
}

static bool empty_wchan(struct wchan* wc)
{
    return (wc->_size == 0 && is_list_empty(&wc->_head));
}

static void wchan_sleep(struct wchan* wc)
{
    assert(wc != NULL);
    struct coroutine* coro = current_running_coro();
    wc->_size ++;
    struct chan_obj* obj = malloc(sizeof(struct chan_obj));
    //simply core the sos if not enough mem for it.....
    assert(obj != NULL);
    obj->_coro = coro;
    list_add_tail(&obj->_link_obj, &(wc->_head.head));
    /* obj->_next = wc->_head; */
    /* wc->_head = obj; */
    yield_coro();
    return;
}

static void wchan_wakeone(struct wchan* wc)
{
    assert(wc != NULL);
    assert(!is_list_empty(&wc->_head) && wc->_size > 0);
    wc->_size --;
    assert(list_front(&wc->_head) != NULL);
    struct chan_obj* tmp = list_entry(list_front(&wc->_head), struct chan_obj, _link_obj);
    struct coroutine* coro = tmp->_coro;
    assert(coro != NULL);
    list_del(&tmp->_link_obj);

    /* wc->_head = tmp->_next; */
    /* printf ("wchan_wakeone, size: %d, next: %p\n", */
    /*         wc->_size, wc->_head); */
    free(tmp);
    make_coro_runnable(coro);
    return;
}

static void wchan_wakeall(struct wchan* wc)
{
    struct list_head *current = NULL;
    struct list_head *tmp_head = NULL;

    list_for_each_safe(current, tmp_head, &(wc->_head.head))
    {
        struct chan_obj* tmp = list_entry(current, struct chan_obj, _link_obj);
        assert(tmp != NULL);
        struct coroutine* coro = tmp->_coro;
        assert(coro != NULL);
        list_del(&tmp->_link_obj);
        free(tmp);
        make_coro_runnable(coro);
    }
    assert(empty_wchan(wc));
}

static bool check_valid_wchan(struct wchan* wc)
{
    int t = 0;
    struct list_head *current = NULL;
    struct list_head *tmp_head = NULL;

    list_for_each_safe(current, tmp_head, &(wc->_head.head))
    {
        struct chan_obj* tmp = list_entry(current, struct chan_obj, _link_obj);
        assert(tmp != NULL);
        /* (void)tmp; */
        t ++ ;
        /* dump_region(tmp); */
    }

    /* for (struct chan_obj* i = wc->_head; i != NULL; i = i->_next) */
    /* { */
    /*     t ++; */
    /* } */
    return t == wc->_size;
}

struct semaphore* sem_create(const char* name, int count, int maximum)
{
    struct semaphore* sem = malloc(sizeof(struct semaphore));
    if (sem == NULL)
    {
        return NULL;
    }
    sem->_sem_name = name;
    sem->_sem_count = count;
    sem->_sem_wchan = create_wchan();
    sem->_max_count = maximum;
    if (sem->_sem_wchan == NULL)
    {
        sem_destroy(sem);
        return NULL;
    }
    return sem;
}

void sem_destroy(struct semaphore* sem)
{
    if (sem == NULL)
    {
        return;
    }

    if (sem->_sem_wchan != NULL)
    {
        assert(empty_wchan(sem->_sem_wchan));
        free(sem->_sem_wchan);
    }
    free(sem);
}

static void _block_wait(struct semaphore* sem)
{
    while(sem->_sem_count == 0)
    {
        sos_usleep(1000);
        rpc_timeout(1);
    }
}

void P(struct semaphore* sem)
{
    if (!sos_init_flag)
    {
        _block_wait(sem);
    }
    else
    {
        assert(check_valid_wchan(sem->_sem_wchan));
        assert(sem != NULL);
        while (sem->_sem_count == 0)
        {
            wchan_sleep(sem->_sem_wchan);
        }
        assert(check_valid_wchan(sem->_sem_wchan));
    }
    assert(sem->_sem_count > 0);
    sem->_sem_count --;
}

void V(struct semaphore* sem)
{
    assert(sem != NULL);
    if (!sos_init_flag)
    {
        sem->_sem_count ++;
        assert(sem->_sem_count > 0);
        return;
    }
    if (sem->_max_count != -1 && sem->_sem_count == sem->_max_count)
    {
        // if someone is blocking in P, but V does not wake that guy up, something wrong with _max_count setting!
        assert(empty_wchan(sem->_sem_wchan));
        return ;
    }
    sem->_sem_count ++;
    assert(check_valid_wchan(sem->_sem_wchan));
    assert(sem->_sem_count > 0);
    if (!empty_wchan(sem->_sem_wchan))
    {
        wchan_wakeone(sem->_sem_wchan);
    }
    assert(check_valid_wchan(sem->_sem_wchan));
    return;
}

struct cv *cv_create(const char *name)
{
    struct cv* cv = malloc(sizeof(struct cv));
    if (cv== NULL)
    {
        return NULL;
    }
    (void)name;
    cv->_cv_wchan = create_wchan();
    if (cv->_cv_wchan == NULL)
    {
        cv_destroy(cv);
        return NULL;
    }
    return cv;
}

void cv_destroy(struct cv* cv)
{
    if (cv != NULL)
    {
        if (cv->_cv_wchan != NULL)
        {
            assert(empty_wchan(cv->_cv_wchan));
            free(cv->_cv_wchan);
        }
        free(cv);
    }
}

void cv_wait(struct cv *cv)
{
    assert(cv != NULL);
    wchan_sleep(cv->_cv_wchan);
}

void cv_signal(struct cv *cv)
{
    assert(cv != NULL);
    wchan_wakeone(cv->_cv_wchan);
}

void cv_broadcast(struct cv *cv)
{
    assert(cv != NULL);
    wchan_wakeall(cv->_cv_wchan);
}
