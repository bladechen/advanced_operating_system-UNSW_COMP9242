#include "comm/comm.h"
#include "timerlist.h"
#include "mapping.h"
#include <clock/clock.h>

#define verbose 5
#include <sys/debug.h>

static struct TimerUnit* g_timer = NULL;


seL4_CPtr badge_irq_ep(seL4_CPtr ep, seL4_Word badge);

#define IPG_CLOCK_FREQ 0x42 // 66MHZ, gpt and epit1/2 will use IPG.

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

// data structure copied from https://bitbucket.org/kevinelp/unsw-advanced-operating-systems/src/e7062457821afdb603a3f855fbb048408fdfefc6/libs/libplatsupport/src/mach/imx/gpt.c?at=master&fileviewer=file-view-default

/* EPIT CONTROL REGISTER BITS */
typedef enum {
    /*
     * This bit enables the EPIT.
     */
    EPIT_EN = 0,

    /*
     * By setting this bit, then when EPIT is disabled (EN=0), then
     * both Main Counter and Prescaler Counter freeze their count at
     * current count values.
     */
    EPIT_ENMOD = 1,

    /*
     *  This bit enables the generation of interrupt when a compare
     *  event occurs
     */
    EPIT_OCIEN = 2,

    /*
     * This bit is cleared by hardware reset. It controls whether the
     * counter runs in free running mode OR set and forget mode.
     */
    EPIT_RLD = 3,

    /*
     * Bits 4 - 15 determine the prescaler value by which the clock is divided
     * before it goes into the counter.
     *
     * The prescaler used is the value in these bits + 1. ie:
     *
     * 0x00 divide by 1
     * 0x01 divide by 2
     * 0x10 divide by 3
     *  .
     *  .
     *  .
     * 0xFFF divide by 4096
     *
     */
    EPIT_PRESCALER = 4,

    /*
     * This bit controls the counter data when the modulus register is
     * written. When this bit is set, all writes to the load register
     * will overwrite the counter contents and the counter will
     * subsequently start counting down from the programmed value.
     */
    EPIT_IOVW = 17,

    EPIT_SWR = 16,

    /*
     * These bits select the clock input used to run the counter. After
     * reset, the system functional clock is selected. The input clock
     * can also be turned off if these bits are set to 00. This field
     * value should only be changed when the EPIT is disabled.
     */
    EPIT_CLKSRC = 24
} epit_control_reg;


enum epit_clock_src
{
    EPIT_CLKSRC_OFF = 0b00,
    EPIT_CLKSRC_IPG= 0b01,
    EPIT_CLKSRC_HIGHFREQ = 0b10,
    EPIT_CLKSRC_LOWFREQ = 0b11,
};

/* Memory map for EPIT (Enhanced Periodic Interrupt Timer). */
struct epit_map {
    /* epit control register */
    uint32_t epitcr;
    /* epit status register */
    uint32_t epitsr;
    /* epit load register */
    uint32_t epitlr;
    /* epit compare register */
    uint32_t epitcmpr;
    /* epit counter register */
    uint32_t epitcnt;
};


typedef struct epit {
    volatile struct epit_map *epit_map;
    uint64_t counter_start;
    int mode;
    uint32_t irq;
    uint32_t prescaler;
} epit_t;


/* GPT CONTROL REGISTER BITS */
typedef enum {
    /*
     * This bit enables the GPT.
     */
    GPT_EN = 0,

    /*
     * By setting this bit, then when EPIT is disabled (EN=0), then
     * both Main Counter and Prescaler Counter freeze their count at
     * current count values.
     */
    GPT_ENMOD = 1,

    /*
     * This read/write control bit enables the operation of the GPT
     *  during debug mode
     */
    GPT_DBGEN = 2,

    /*
     *  This read/write control bit enables the operation of the GPT
     *  during wait mode
     */
    GPT_WAITEN = 3,

    /*
     * This read/write control bit enables the operation of the GPT
     *  during doze mode
     */
    GPT_DOZEN = 4,

    /*
     * This read/write control bit enables the operation of the GPT
     *  during stop mode
     */
    GPT_STOPEN = 5,

    /*
     * bits 6-8 -  These bits selects the clock source for the
     *  prescaler and subsequently be used to run the GPT counter.
     */
    GPT_CLKSRC = 6,

    /*
     * Freerun or Restart mode.
     *
     * 0 Restart mode
     * 1 Freerun mode
     */
    GPT_FRR = 9,

    /*
     * Software reset.
     *
     * This bit is set when the module is in reset state and is cleared
     * when the reset procedure is over. Writing a 1 to this bit
     * produces a single wait state write cycle. Setting this bit
     * resets all the registers to their default reset values except
     * for the EN, ENMOD, STOPEN, DOZEN, WAITEN and DBGEN bits in this
     *  control register.
     */
    GPT_SWR = 15,

    /* Input capture channel operating modes */
    GPT_IM1 = 16, GPT_IM2 = 18,

    /* Output compare channel operating modes */
    GPT_OM1 = 20, GPT_OM2 = 23, GPT_OM3 = 26,

    /* Force output compare channel bits */
    GPT_FO1 = 29, GPT_FO2 = 30, GPT_FO3 = 31

} gpt_control_reg;


/* bits in the interrupt/status regiser */
enum gpt_interrupt_register_bits {

    /* Output compare interrupt enable bits */
    OF1IE = 0, OF2IE = 1, OF3IE = 2,

    /* Input capture interrupt enable bits */
    IF1IE = 3, IF2IE = 4,

    /* Rollover interrupt enabled */
    ROV = 5,
};

/* Memory map for GPT. */
struct gpt_map {
    /* gpt control register */
    uint32_t gptcr;
    /* gpt prescaler register */
    uint32_t gptpr;
    /* gpt status register */
    uint32_t gptsr;
    /* gpt interrupt register */
    uint32_t gptir;
    /* gpt output compare register 1 */
    uint32_t gptcr1;
    /* gpt output compare register 2 */
    uint32_t gptcr2;
    /* gpt output compare register 3 */
    uint32_t gptcr3;
    /* gpt input capture register 1 */
    uint32_t gpticr1;
    /* gpt input capture register 2 */
    uint32_t gpticr2;
    /* gpt counter register */
    uint32_t gptcnt;
};

/* typedef enum { */
/*     PERIODIC, */
/*     ONESHOT, */
/* } gpt_mode_t; */

typedef struct gpt {
    volatile struct gpt_map *gpt_map;
    int mode;
    uint32_t irq;
    uint32_t prescaler;
} gpt_t;

#define GPT_CLKSRC_IPG 0b001 // i am too lazy to copy other 7 clock src.
#define GPT_SR_CLEAR 0b111111

static gpt_t g_gpt;
static epit_t g_epit1;
static epit_t g_epit2;
static timestamp_t g_cur_timestamp_ms = 0;

// gpt used for interrupt to update time_stamp
// epit2 used for background tick
static void setup_regular_clock(seL4_CPtr interrupt_ep)
{
    g_gpt.gpt_map = map_device((void *) GPT_MEMORY_MAP_STRAT, GPT_MEMORY_SIZE);

    /*
    following the initial seq from manual, i am not good at hardware, but it works.
    1. Disable GPT by setting EN=0 in GPT_CR register.
    2. Disable GPT interrupt register (GPT_IR).
    3. Configure Output Mode to unconnected/ disconnected—Write zeros in OM3, OM2, and OM1 in GPT_CR
    4. Disable Input Capture Modes—Write zeros in IM1 and IM2 in GPT_CR
    5. Change clock source CLKSRC to the desired value in GPT_CR register.
    6. Assert the SWR bit in GPT_CR register.
    7. Clear GPT status register (GPT_SR) (i.e., w1c).
    8. Set ENMOD=1 in GPT_CR register, to bring GPT counter to 0x00000000.
    9. Enable GPT (EN=1) in GPT_CR register.
    10. Enable GPT interrupt register (GPT_IR).
    */
    g_gpt.gpt_map->gptcr  = 0;
    g_gpt.gpt_map->gptir = 0;

    g_gpt.gpt_map->gptcr = BIT(GPT_SWR);
    g_gpt.gpt_map->gptcr = GPT_CLKSRC_IPG << GPT_CLKSRC;
    g_gpt.gpt_map->gptcr |= GPT_ENMOD ; // set output pin?
    g_gpt.gpt_map->gptpr = IPG_CLOCK_FREQ; // scale to 1MHZ
    g_gpt.gpt_map->gptsr = GPT_SR_CLEAR;
    g_gpt.gpt_map->gptcr1 = 10000; // every 1/1MHZ * 10000 = 10ms to trigger updating the time_stamp.

    g_gpt.gpt_map->gptcr &= (~BIT(GPT_SWR));

    g_gpt.gpt_map->gptir = 1;
    g_gpt.irq = enable_irq(GPT_IRQ, interrupt_ep);

    g_gpt.gpt_map->gptcr |= 1;

    g_epit2.epit_map = map_device((void *) EPIT2_MEMORY_MAP_START, EPIT2_MEMORY_SIZE);
    /*
    1. Disable the EPIT - set EN=0 in EPIT_EPITCR.
    2. Disable EPIT ouput - program OM=00 in the EPIT_EPITCR.
    3. Disable EPIT interrupts.
    4. Program CLKSRC to desired clock source in EPIT_EPITCR.
    5. Clear the EPIT status register (EPIT_EPITSR), that is, write "1" to clear (w1c).
    6. Set ENMOD= 1 in the EPIT_EPITCR, to bring the EPIT Counter to defined state
    (EPIT_EPITLR value or 0xFFFF_FFFF).
    7. Enable EPIT - set (EN=1) in the EPIT_EPITCR
    8. Enable the EPIT interrupts.
    */

    g_epit2.epit_map->epitcr = 0;
    g_epit2.epit_map->epitcr = BIT(EPIT_SWR);

    //FIXME
    g_epit2.epit_map->epitcr = (EPIT_CLKSRC_IPG << EPIT_CLKSRC) | /* Clock source = IPG */
        (IPG_CLOCK_FREQ << EPIT_PRESCALER) | /* Set the prescaler */
        BIT(EPIT_IOVW) | /* Overwrite counter immediately on write */
        BIT(EPIT_RLD) | /* Reload counter from modulus register on overflow */
        BIT(EPIT_ENMOD) ; /* Count from modulus on restart */
    g_epit2.epit_map->epitcmpr = 0; // FIXME
    g_epit2.counter_start = 1000000000; // make this big enough so that it takes more time for this timer to roll over.
    g_epit2.epit_map->epitlr = g_epit2.counter_start;
    while (g_epit2.epit_map->epitlr != g_epit2.counter_start)
    {
        g_epit2.epit_map->epitlr = g_epit2.counter_start;
    }
    g_epit2.epit_map->epitcr &= (~BIT(EPIT_SWR));

    g_epit2.epit_map->epitcr |= 1;
    return;
}

static void setup_timer_interrupt(seL4_CPtr interrupt_ep)
{
    g_epit1.epit_map = map_device((void *) EPIT1_MEMORY_MAP_START, EPIT1_MEMORY_SIZE);

    g_epit1.epit_map->epitcr = 0;

    g_epit1.epit_map->epitcr = BIT(EPIT_SWR);
    g_epit1.epit_map->epitcr = (EPIT_CLKSRC_IPG << EPIT_CLKSRC) | /* Clock source = IPG */
        (0x042 << EPIT_PRESCALER) | /* Set the prescaler */
        BIT(EPIT_IOVW) | /* Overwrite counter immediately on write */
        BIT(EPIT_RLD) | /* Reload counter from modulus register on overflow */
        BIT(EPIT_OCIEN) | /* Enable interrupt on comparison event */
        BIT(EPIT_ENMOD) | /* Count from modulus on restart */
        0;
    g_epit1.epit_map->epitcmpr = 0;
    int counterValue = 1000000; // FIXME 1s
    g_epit1.epit_map->epitlr = counterValue;
    while (g_epit1.epit_map->epitlr != counterValue) {
        g_epit1.epit_map->epitlr = counterValue;
    }
    g_epit1.epit_map->epitcr &= (~BIT(EPIT_SWR));
    g_epit1.irq = enable_irq(EPIT1_IRQ, interrupt_ep);
    /* Interrupt when compare with 0. */

    g_epit1.epit_map->epitcr |= 1;
}

int start_timer(seL4_CPtr interrupt_ep)
{
    g_timer = init_timer_unit(MAX_REGISTERED_TIMER_CLOCK);
    if (g_timer == NULL)
    {
        color_print(ANSI_COLOR_RED, "init global timer fail, maybe no enough memory!\n");
        return -1;
    }
    /* conditional_panic(g_timer == NULL, "init global timer fail, maybe no enough memory!\n"); */
    setup_regular_clock(badge_irq_ep(interrupt_ep, IRQ_GPT_BADGE));
    setup_timer_interrupt(badge_irq_ep(interrupt_ep, IRQ_EPIT1_BADGE));
    return 0;
}

uint32_t register_timer(uint64_t delay, timer_callback_t callback, void *data);


int remove_timer(uint32_t id);



int timer_interrupt(void)
{
    color_print(ANSI_COLOR_GREEN, "timer: %llu\n", time_stamp());
    return 0;
}

void update_timestamp(void)
{

    static long long last_counter = 0;
    /* static long long last_usecond = 0; */
    long long cur_counter = g_epit2.epit_map->epitcnt; // every 1 count stands for 1us

    if (cur_counter > last_counter)
    {
        g_cur_timestamp_ms += (g_epit2.counter_start -  cur_counter + last_counter) / 1000;
        /* last_usecond += (g_epit2.counter_start - cur + last) % 1000; */
    }
    else
    {
        g_cur_timestamp_ms += (last_counter - cur_counter) / 1000;
        /* cur_ts_us += (last - cur) % 1000; */
    }

    /* if (last_usecond >= 1000) */
    /* { */
    /*     g_cur_timestamp_ms ++; */
    /*     last_usecond -= 1000; */
    /* } */

    last_counter = cur_counter;
    return;
}

timestamp_t time_stamp(void)
{
    update_timestamp();
    return g_cur_timestamp_ms;
}

timestamp_t __timestamp_ms(void)
{
    return time_stamp();
}


int stop_timer(void)
{
    g_gpt.gpt_map->gptcr  = 0;
    g_gpt.gpt_map->gptir = 0;

    g_epit1.epit_map->epitcr = 0;
    g_epit1.epit_map->epitcr = BIT(EPIT_SWR);

    g_epit2.epit_map->epitcr = 0;
    g_epit2.epit_map->epitcr = BIT(EPIT_SWR);

    destroy_timer_unit(g_timer);
    g_timer = NULL;
}


// timer int
void handle_epit1_irq(void)
{

    g_epit1.epit_map->epitcr &= (~1);
    g_epit1.epit_map->epitsr = 1;

    timer_interrupt();

    int err = seL4_IRQHandler_Ack(g_epit1.irq);

    g_epit1.epit_map->epitcr |= 1;
    conditional_panic(err, "Failed to acknowledge epit interrupt\n");
}

// background tick
void handle_gpt_irq(void)
{
    g_gpt.gpt_map->gptcr &= (~1);

    g_gpt.gpt_map->gptsr = 0x3f;

    update_timestamp();

    int err = seL4_IRQHandler_Ack(g_gpt.irq);
    g_gpt.gpt_map->gptcr |= (1);
    conditional_panic(err, "Failed to acknowledge gpt interrupt\n");
}
