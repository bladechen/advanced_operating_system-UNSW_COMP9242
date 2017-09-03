#include "coro.h"
#include "vm/frametable.h"
#include "vm/vm.h"
#include "vm/vmem_layout.h"
#include "sys/debug.h"

/*#define DEBUG_CORO 1 */
static struct schedule schedule_obj;

static void replace_esp(struct context* jbf, void* esp)
{
    size_t tmp = (size_t)(jbf->_jmp);
    tmp += 32;
    *(uint32_t*)(tmp) = (uint32_t)(esp);
}


static inline void zero_context(struct context* jbf)
{
    memset(jbf->_jmp, 0, sizeof (jbf->_jmp));
}

static struct coroutine* new_coro();
static void set_running_coro(struct coroutine* co);
static void init_context(struct coroutine* coro);
static void schedule_coro();


void set_kproc_coro(struct proc* proc)
{
    schedule_obj._daemon->_proc = proc;
    proc->p_coro = schedule_obj._daemon;
}

void bootstrap_coro_env()
{
    schedule_obj._pending_list = (struct list*)(malloc(sizeof(struct list)));
    schedule_obj._block_list = (struct list*)(malloc(sizeof(struct list)));
    assert(schedule_obj._pending_list != NULL);
    assert(schedule_obj._block_list != NULL);
    list_init(schedule_obj._pending_list);
    list_init(schedule_obj._block_list);
    struct coroutine* co = new_coro();
    assert(co != NULL);
    schedule_obj._daemon = co;
    schedule_obj._stack_base = COROUTINE_STACK_START;
    co->_status = COROUTINE_RUNNING;
    set_running_coro(co);
#ifdef DEBUG_CORO
    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN,"daemon: 0x%p\n", co);
#endif
}

struct coroutine* current_running_coro()
{
    return schedule_obj._running;
}

static void set_running_coro(struct coroutine*co)
{
#ifdef DEBUG_CORO
    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN,"previous %p -> current running %p\n", schedule_obj._running, co);
#endif
    schedule_obj._running = co;
}

static struct coroutine* new_coro()
{
    struct coroutine* co = (struct coroutine*)(malloc(sizeof(struct coroutine)));
    if (co == NULL)
    {
        return NULL;
    }
    co->_status = COROUTINE_INIT;
    co->_entry = NULL;
    co->_argv = NULL;
    co->_stack_top = NULL;
    co->_stack_addr= NULL;
    co->_stack_sz = 0;
    link_init(&(co->_link));
    memset(&(co->_ctx), 0, sizeof(struct context));
    return co;
}


// alloc 2 guard + 1 stack , total 16K
// FIXME remove assert 0
static vaddr_t alloc_stack_mem()
{
    struct pagetable* pt = schedule_obj._daemon->_proc->p_pagetable;
    int ret = alloc_page(pt, schedule_obj._stack_base, seL4_ARM_Default_VMAttributes|seL4_ARM_ExecuteNever, seL4_CanRead );

    assert( 0 == ret);
    ret = alloc_page(pt, schedule_obj._stack_base + STACK_GUARD_SIZE,  seL4_ARM_Default_VMAttributes|seL4_ARM_ExecuteNever, seL4_CanRead | seL4_CanWrite);
    vaddr_t stack_base = schedule_obj._stack_base + STACK_GUARD_SIZE;
    assert (0 == ret);

    ret = alloc_page(pt, schedule_obj._stack_base + STACK_GUARD_SIZE + STACK_GUARD_SIZE,  seL4_ARM_Default_VMAttributes|seL4_ARM_ExecuteNever, seL4_CanRead | seL4_CanWrite);
    assert (0 == ret);

    ret = alloc_page(pt, schedule_obj._stack_base +  STACK_SIZE + STACK_GUARD_SIZE , seL4_ARM_Default_VMAttributes|seL4_ARM_ExecuteNever, seL4_CanRead );
    assert (0 == ret);
    schedule_obj._stack_base += 2 * STACK_GUARD_SIZE + STACK_SIZE;

    return stack_base;
}

static void free_stack_mem(vaddr_t vaddr)
{
    struct pagetable* pt = schedule_obj._daemon->_proc->p_pagetable;
    free_page(pt, vaddr - seL4_PAGE_SIZE);
    free_page(pt, vaddr );
    free_page(pt, vaddr + seL4_PAGE_SIZE);
    free_page(pt, vaddr + 2 * seL4_PAGE_SIZE);
}

static bool init_stack(struct coroutine* coro)
{
    assert(coro != NULL);
    if (coro->_stack_addr != NULL)
    {
        return true;
    }
    assert(coro->_stack_top == NULL);
    coro->_stack_sz = STACK_SIZE;

    coro->_stack_addr = (void*)(alloc_stack_mem());
    if (coro->_stack_addr == 0)
    {
        return false;
    }
    coro->_stack_top = (void*)((size_t)(coro->_stack_addr)  +  coro->_stack_sz);
    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "coro: %p, has stack top at: %p\n", coro, coro->_stack_top );
    return true;
}


void restart_coro(struct coroutine* coro,  coroutine_func func, void* argv)
{
    coro->_entry = func;
    coro->_argv = argv;
    if (coro->_status == COROUTINE_READY)
    {
        printf ("coro %p alreay in the ready queue\n", coro);
        return;
    }
    make_coro_runnable(coro);
}

static void reset_coro(struct coroutine* coro)
{
    coro->_status = COROUTINE_INIT;
    coro->_argv = NULL;
    coro->_entry= NULL;
    init_context(coro);
}

static void schedule_start_coro(struct coroutine* coro)
{
    assert(coro == current_running_coro());
    assert(coro->_status == COROUTINE_RUNNING);

    /* assert() */
    if (coro->_entry)
    {
        coro->_entry(coro->_argv);
    }
    // finish running this coro, but still on this coro stack, we should not destroy the stack.
    reset_coro(coro);
    schedule_coro();
}

static void init_context(struct coroutine* coro)
{
#ifdef DEBUG_CORO
    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN,"init_context %p\n", coro);
#endif
    if (setjmp(coro->_ctx._jmp) != 0)
    {
#ifdef DEBUG_CORO
        COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "starting %p\n", current_running_coro());
#endif

        zero_context(&(current_running_coro()->_ctx));
        /* COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN,"starting %p, %p\n", current_running_coro(), coro); */
        /* assert(current_running_coro() == coro); */
        schedule_start_coro(current_running_coro());
    }
    /* int i = 0; */
    /* COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "after save_context %p, 0x%p\n", coro, &i); */
    /* for (int i = 0; i < 20; i ++) */
    /* { */
    /*     COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "[%d] 0x%x\n", i, coro->_ctx._jmp[i]); */
    /* } */
    assert(coro->_stack_top != NULL);
    // then we put our created stack into ctx
    if (coro->_stack_top )
    {
        replace_esp(&(coro->_ctx), (coro->_stack_top));
    }
    /* COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "after replace_esp %p\n", coro); */
    /* for (int i = 0; i < 20; i ++) */
    /* { */
    /*     COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "[%d] 0x%x\n", i, coro->_ctx._jmp[i]); */
    /* } */
}

static void _restore_ctx(struct coroutine* coro)
{
    assert(current_running_coro() == coro);
    /* COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "_restore_ctx %p\n", coro); */
    /* for (int i = 0; i < 20; i ++) */
    /* { */
    /*     COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "[%d] 0x%x\n", i, coro->_ctx._jmp[i]); */
    /* } */
    longjmp(coro->_ctx._jmp, 1);
}

static bool initial_coro(struct coroutine* coro)
{
    if (init_stack(coro) == false)
    {
        COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN,"init_stack failed\n");
        return false;
    }
    init_context(coro);
    return true;
}

void destroy_coro(struct coroutine* coro)
{
    if (coro == NULL)
    {
        return;
    }
    assert(coro->_status !=  COROUTINE_SUSPEND &&
           coro->_status != COROUTINE_SUSPEND &&
           coro->_status != COROUTINE_RUNNING);
    assert(list_empty(&(coro->_link)) == true);
    if (coro->_stack_addr != NULL)
    {
        free_stack_mem(coro->_stack_addr);
    }
    free(coro);
}
void make_coro_runnable(struct coroutine* coro)
{
    assert(coro->_status == COROUTINE_INIT ||
           coro->_status == COROUTINE_SUSPEND);
    coro->_status = COROUTINE_READY;
#ifdef DEBUG_CORO
    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN,"make_coro_runnable %p, %p\n", &coro->_link, coro);
#endif
    list_add_tail( &coro->_link, &(schedule_obj._pending_list->head));
}
struct coroutine* create_coro(coroutine_func func, void* argv)
{
    struct coroutine* coro = new_coro();
    coro->_entry = func;
    coro->_argv = argv;
    if (initial_coro(coro) == false)
    {
        destroy_coro(coro);
        return NULL;
    }
    make_coro_runnable(coro);
    return coro;
}


void resume_coro(struct coroutine* coro)
{

    assert(current_running_coro() == schedule_obj._daemon);
    assert(coro->_status == COROUTINE_SUSPEND);
    if (setjmp(current_running_coro()->_ctx._jmp) == 0)
    {
        (( struct coroutine*)(current_running_coro()))->_status = COROUTINE_READY;
        assert(current_running_coro() != coro);
        set_running_coro(coro);

        coro->_status = COROUTINE_RUNNING;
        _restore_ctx(coro);
    }
    /* printf ("%p %p\n",current_running_coro(),  coro); */
    zero_context(&(current_running_coro()->_ctx));
    assert(current_running_coro()->_status == COROUTINE_RUNNING);
    /* current_running_coro()->_status = COROUTINE_READY; */
    /* assert(current_running_coro() != coro); */
    /* set_running_coro(coro); */
    /* coro->_status = COROUTINE_RUNNING; */
    /* _restore_ctx(coro); */
}


void schedule_loop()
{

    if (is_list_empty(schedule_obj._pending_list))
    {
        return;
    }
#ifdef DEBUG_CORO
    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN,"in schedule_loop %p\n", current_running_coro());
#endif
    assert(current_running_coro()->_status == COROUTINE_RUNNING);
    assert(current_running_coro()== schedule_obj._daemon);
    schedule_obj._daemon->_status = COROUTINE_READY;
    if (setjmp(schedule_obj._daemon->_ctx._jmp) == 0)
        schedule_coro();


    assert(current_running_coro()== schedule_obj._daemon);
    zero_context(&(schedule_obj._daemon->_ctx));
    return;
}

static void schedule_coro()
{
    struct coroutine* coro = NULL;
    if (is_list_empty(schedule_obj._pending_list ))
    {
        coro = schedule_obj._daemon; // daemon is always ready for schedule.
    }
    else
    {
        struct list_head* tmp = list_front(schedule_obj._pending_list);
        assert(tmp != NULL);
        coro = list_entry(tmp, struct coroutine, _link);

#ifdef DEBUG_CORO
        COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN,"schedule_coro front %p, %p\n", tmp, coro);
#endif
        list_del(tmp); // remove it
    }
    set_running_coro(coro);
    coro->_status = COROUTINE_RUNNING;
    _restore_ctx(coro);
    return;
}

void yield_coro(void)
{
    struct coroutine* _current = current_running_coro();
    if (setjmp(_current->_ctx._jmp) == 0)
    {
        _current->_status = COROUTINE_SUSPEND;
        schedule_coro();
    }

    zero_context(&(_current->_ctx));
    // continue running in _current;
    assert(current_running_coro() == _current);
    assert(current_running_coro()->_status == COROUTINE_RUNNING );
}


void shutdown_coro_env(void)
{
    if (current_running_coro() != schedule_obj._daemon)
    {
        return;
    }
    assert(current_running_coro()->_status == COROUTINE_RUNNING);

    struct list_head *current = NULL;
    struct list_head *tmp_head = NULL;

    list_for_each_safe(current, tmp_head, &(schedule_obj._pending_list->head))
    {
        struct coroutine* tmp = list_entry(current, struct coroutine, _link);
        list_del(&(tmp->_link));
        destroy_coro(tmp);
    }
    list_for_each_safe(current, tmp_head, &(schedule_obj._block_list->head))
    {
        struct coroutine* tmp = list_entry(current, struct coroutine, _link);
        list_del(&(tmp->_link));
        destroy_coro(tmp);
    }
    /* for (vaddr_t i = COROUTINE_STACK_START; i < _stack_base; i += (4096)) */
    /* { */
    /*  */
    /*  */
    /* } */
    /*  */
    destroy_coro(schedule_obj._daemon);
}


