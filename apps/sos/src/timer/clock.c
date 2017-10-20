#include "comm/comm.h"
#include "proc/proc.h"
#include "timerlist.h"
#include "mapping.h"
#include <clock/clock.h>
#include "nfs/nfs.h"
#include "nfs/time.h"

#define verbose 5
#include <sys/debug.h>

static struct TimerUnit* g_timer = NULL;

// refer to i.mx6 manual - 3.2 AP interrupts
#define GPT_IRQ   87
#define EPIT1_IRQ 88
#define EPIT2_IRQ 89

// refer to i.mx6 manual chapter24
#define  EPIT1_MEMORY_MAP_START 0x20D0000
#define  EPIT1_MEMORY_SIZE      0x14


#define  EPIT2_MEMORY_MAP_START 0x20D4000
#define  EPIT2_MEMORY_SIZE      0x14


// refer to i.mx6 manual chapter30
#define  GPT_MEMORY_MAP_STRAT   0x2098000
#define  GPT_MEMORY_SIZE        0x28

volatile timestamp_t g_cur_timestamp_us = 0;

/* int start_timer(seL4_CPtr interrupt_ep) */
/* { */
/*     if (g_timer != NULL) // if already g_timer is not null, timer driver should be init already. */
/*     { */
/*         return CLOCK_R_OK; */
/*     } */
/*  */
/*     if (g_timedriver_is_init == 0) */
/*     { */
/*         _init_timedriver(interrupt_ep); */
/*         g_timedriver_is_init = 1; */
/*     } */
/*     struct ip_addr host; */
/*  */
/*     assert(ipaddr_aton(CONFIG_SOS_GATEWAY, &host)); */
/*     #<{(| int try = 5; |)}># */
/*     #<{(| while (try --) |)}># */
/*     #<{(| { |)}># */
/*     #<{(|      _unix_timestamp = udp_time_get(&host); |)}># */
/*     #<{(|      if (_unix_timestamp != 0) |)}># */
/*     #<{(|      { |)}># */
/*     #<{(|          break; |)}># */
/*     #<{(|      } |)}># */
/*     #<{(| } |)}># */
/*     #<{(| _unix_timestamp -= 2208988800U; |)}># */
/*     #<{(| COLOR_DEBUG(DB_DEVICE, ANSI_COLOR_RED, "boot unix time [%u]\n", _unix_timestamp); |)}># */
/*  */
/*     g_timer = init_timer_unit(MAX_REGISTERED_TIMER_CLOCK); */
/*     if (g_timer == NULL) */
/*     { */
/*         color_print(ANSI_COLOR_RED, "init global timer fail, maybe no enough memory!\n"); */
/*         return CLOCK_R_FAIL; */
/*     } */
/*     _enable_timerdriver(); */
/*     #<{(| conditional_panic(g_timer == NULL, "init global timer fail, maybe no enough memory!\n"); |)}># */
/*     return CLOCK_R_OK; */
/* } */

uint32_t register_timer(uint64_t delay, timer_callback_t callback, void *data)
{
    if (g_timer == NULL)
    {
        return CLOCK_R_UINT;
    }
    uint32_t id = get_current_timer_id(g_timer);
    int ret = 0;
    if (id == 0)
    {
        ret = attach_timer(g_timer, delay, callback, data, &id);
        /* color_print(ANSI_COLOR_GREEN, "attach_timer: %d\n", ret); */
    }
    else
    {
        ret = rettach_timer(g_timer, delay, callback, data, id);
        /* color_print(ANSI_COLOR_GREEN, "rettach_timer: %d\n", ret); */
    }
    return (ret != 0) ? 0: id;
}

int remove_timer(uint32_t id)
{
    if (g_timer == NULL)
    {
        return CLOCK_R_UINT;
    }
    return (dettach_timer(g_timer, id) == 0) ? CLOCK_R_OK: CLOCK_R_FAIL;
}

int timer_interrupt(void)
{
    if (g_timer == NULL)
    {
        return CLOCK_R_UINT;
    }
    check_expired(g_timer, time_stamp());
    return CLOCK_R_OK;
}

timestamp_t time_stamp(void)
{
    /* update_timestamp(); */
    return g_cur_timestamp_us/1000;
}

// this is for timerlist.c api
timestamp_t __timestamp_ms(void)
{
    return time_stamp();
}

int stop_timer(void)
{
    // in the timer callback function, should not destroy timer.
    if (g_timer == NULL)
    {
        return CLOCK_R_UINT;
    }

    if (get_current_timer_id(g_timer) != 0)
    {
        return CLOCK_R_FAIL;
    }
    destroy_timer_unit(g_timer);
    g_timer = NULL;
    return 0;
}


void handle_time_driver_cb(void)
{
    seL4_CPtr reply_cap = cspace_save_reply_cap(cur_cspace);
    assert(reply_cap > 0);
    memcpy((void*)(&g_cur_timestamp_us), get_ipc_buffer(pid_to_proc(1)), 8);
    timer_interrupt();

    seL4_MessageInfo_t r = seL4_MessageInfo_new(0, 0, 0, 1);

    seL4_SetTag(r);
    seL4_SetMR(0, 1);

    seL4_Send(reply_cap, r);
    assert(0 == cspace_free_slot(cur_cspace, reply_cap));
}

void map_timer_device(struct proc* proc, uint32_t phys)
{
    seL4_CPtr frame_cap = 0;
    // device address, nothing to do with frametable!
    int err = cspace_ut_retype_addr(phys,
                                seL4_ARM_SmallPageObject,
                                seL4_PageBits,
                                cur_cspace,
                                &frame_cap);
    conditional_panic(err, "Unable to retype device memory");
    assert(frame_cap > 0);

    /* Map in the page */
    err = map_page(frame_cap,
                   proc->p_resource.p_pagetable->vroot.cap,
                   APP_DEVICE_START + phys,
                   seL4_AllRights,
                   0);
    conditional_panic(err, "Unable to map_page");
}

int start_timer(seL4_CPtr fault_cap)
{
    if (g_timer != NULL)
    {
        return 0;
    }
    g_timer = init_timer_unit(MAX_REGISTERED_TIMER_CLOCK);
    assert(g_timer != NULL);

    if (pid_to_proc(1) != NULL)
    {
        return 0;
    }

    char* name = "time_driver";
    struct proc* process = proc_create((char*)(name), fault_cap);

    assert(process);
    assert(proc_load_elf(process, (char*)name));
    proc_attach_father(process, get_current_proc());

    int err;
    uint32_t aep_addr = ut_alloc(seL4_EndpointBits);
    conditional_panic(!aep_addr, "No memory for async endpoint");
    seL4_CPtr async_ep;
    err = cspace_ut_retype_addr(aep_addr,
                                seL4_AsyncEndpointObject,
                                seL4_EndpointBits,
                                process->p_resource.p_croot,
                                &async_ep);
    conditional_panic(err, "Failed to allocate c-slot for Interrupt endpoint");

    uint32_t ep_addr = ut_alloc(seL4_EndpointBits);
    conditional_panic(!aep_addr, "No memory for async endpoint");
    seL4_CPtr ep;
    err = cspace_ut_retype_addr(ep_addr,
                                seL4_EndpointObject,
                                seL4_EndpointBits,
                                process->p_resource.p_croot,
                                &ep);
    conditional_panic(err, "Failed to allocate c-slot for Interrupt endpoint");

    seL4_CPtr irq_cap = cspace_irq_control_get_cap (cur_cspace,seL4_CapIRQControl, GPT_IRQ);
    assert(irq_cap != 0);
    seL4_CPtr time_irq_cap = cspace_copy_cap(process->p_resource.p_croot, cur_cspace, irq_cap, seL4_AllRights);
    assert(time_irq_cap != 0);

    seL4_CPtr tcb_cap = cspace_copy_cap(process->p_resource.p_croot, cur_cspace, process->p_resource.p_tcb->cap, seL4_AllRights);
    assert(tcb_cap != 0);
    assert(tcb_cap == TIME_DRIVER_TCB_CAP);
    assert(time_irq_cap == TIME_DRIVER_IRQ_CAP);
    assert(ep == TIME_DRIVER_EP);
    assert(async_ep == TIME_DRIVER_AEP);
    map_timer_device(process, GPT_MEMORY_MAP_STRAT);
    map_timer_device(process, EPIT2_MEMORY_MAP_START);

    assert(0 == proc_start(process, 0, NULL));
    return 0;
}
