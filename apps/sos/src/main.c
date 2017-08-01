/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <cspace/cspace.h>

#include <cpio/cpio.h>
#include <nfs/nfs.h>
#include <elf/elf.h>
#include <serial/serial.h>

#include "network.h"
#include "elf.h"

#include "ut_manager/ut.h"
#include "vmem_layout.h"
#include "mapping.h"

#include <autoconf.h>

#define verbose 5
#include <sys/debug.h>
#include <sys/panic.h>

/* This is the index where a clients syscall enpoint will
 * be stored in the clients cspace. */
#define USER_EP_CAP          (1)
/* To differencient between async and and sync IPC, we assign a
 * badge to the async endpoint. The badge that we receive will
 * be the bitwise 'OR' of the async endpoint badge and the badges
 * of all pending notifications. */
#define IRQ_EP_BADGE         (1 << (seL4_BadgeBits - 1))
/* All badged IRQs set high bet, then we use uniq bits to
 * distinguish interrupt sources */
#define IRQ_BADGE_NETWORK (1 << 0)

#define TTY_NAME             CONFIG_SOS_STARTUP_APP
#define TTY_PRIORITY         (0)
#define TTY_EP_BADGE         (101)

/* The linker will link this symbol to the start address  *
 * of an archive of attached applications.                */
extern char _cpio_archive[];

const seL4_BootInfo* _boot_info;


#define IRQ_BADGE_TIMER (1 << 1)
#define IRQ_BADGE_TIMER1 (1 << 2)
/* struct { */
/*  */
/*     seL4_Word tcb_addr; */
/*     seL4_TCB tcb_cap; */
/*  */
/*     seL4_Word vroot_addr; */
/*     seL4_ARM_PageDirectory vroot; */
/*  */
/*     seL4_Word ipc_buffer_addr; */
/*     seL4_CPtr ipc_buffer_cap; */
/*  */
/*     cspace_t *croot; */
/*  */
/* } tty_test_process; */


/*
 * A dummy starting syscall
 */
#define SOS_SYSCALL0 0

seL4_CPtr _sos_ipc_ep_cap;
seL4_CPtr _sos_interrupt_ep_cap;

static unsigned long long cur_timestamp = 0;

/**
 * NFS mount point
 */
extern fhandle_t mnt_point;


static struct serial * serial_handler = NULL;

#define SYSCALL_IPC_PRINT_COLSOLE 2

int tstamp(void);

/* static volatile void *epit1; */
/* static volatile void *epit2; */
/* static volatile void *gpt; */
#define GPT_STATUS_REGISTER_CLEAR 0x3F


#define GPT_SR            (*((volatile uint32_t *) (gpt + 8)))
/* The linker will link this symbol to the start address  *
 * of an archive of attached applications.                */
extern char _cpio_archive[];

const seL4_BootInfo* _boot_info;

int tstamp(void) ;
seL4_IRQHandler t_cap;
seL4_CPtr t_irq_ep;

struct {

    seL4_Word tcb_addr;
    seL4_TCB tcb_cap;

    seL4_Word vroot_addr;
    seL4_ARM_PageDirectory vroot;

    seL4_Word ipc_buffer_addr;
    seL4_CPtr ipc_buffer_cap;

    cspace_t *croot;

} tty_test_process;


/*
 * A dummy starting syscall
 */
#define SOS_SYSCALL0 0

seL4_CPtr _sos_ipc_ep_cap;
seL4_CPtr _sos_interrupt_ep_cap;

/**
 * NFS mount point
 */
extern fhandle_t mnt_point;

/* Physical locations for counter registers, and macros for register offsets */
#define EPIT1_IRQ         88
#define EPIT1_PHYS_START  0x20D0000
#define EPIT1_PHYS_SIZE   20
#define EPIT1_CR          (*((volatile uint32_t *) epit1))
#define EPIT1_SR          (*((volatile uint32_t *) (epit1 + 4)))
#define EPIT1_LR          (*((volatile uint32_t *) (epit1 + 8)))
#define EPIT1_CMPR        (*((volatile uint32_t *) (epit1 + 12)))
#define EPIT1_CNR         (*((volatile uint32_t *) (epit1 + 16)))

#define EPIT2_IRQ         89
#define EPIT2_PHYS_START  0x20D4000
#define EPIT2_PHYS_SIZE   20
#define EPIT2_CR          (*((volatile uint32_t *) epit2))
#define EPIT2_SR          (*((volatile uint32_t *) (epit2 + 4)))
#define EPIT2_LR          (*((volatile uint32_t *) (epit2 + 8)))
#define EPIT2_CMPR        (*((volatile uint32_t *) (epit2 + 12)))
#define EPIT2_CNR         (*((volatile uint32_t *) (epit2 + 16)))

#define GPT_IRQ           87
#define GPT_PHYS_START    0x2098000
#define GPT_PHYS_SIZE     40
#define GPT_CR            (*((volatile uint32_t *) gpt))
#define GPT_PR            (*((volatile uint32_t *) (gpt + 4)))
#define GPT_IR            (*((volatile uint32_t *) (gpt + 12)))
#define GPT_OCR1          (*((volatile uint32_t *) (gpt + 16)))
#define GPT_OCR2          (*((volatile uint32_t *) (gpt + 20)))
#define GPT_OCR3          (*((volatile uint32_t *) (gpt + 24)))
#define GPT_ICR1          (*((volatile uint32_t *) (gpt + 28)))
#define GPT_ICR2          (*((volatile uint32_t *) (gpt + 32)))
#define GPT_CNT           (*((volatile uint32_t *) (gpt + 36)))



/* EPIT CONTROL REGISTER BITS */
typedef enum {
    /*
     * This bit enables the EPIT.
     */
    EN = 0,

    /*
     * By setting this bit, then when EPIT is disabled (EN=0), then
     * both Main Counter and Prescaler Counter freeze their count at
     * current count values.
     */
    ENMOD = 1,

    /*
     *  This bit enables the generation of interrupt when a compare
     *  event occurs
     */
    OCIEN = 2,

    /*
     * This bit is cleared by hardware reset. It controls whether the
     * counter runs in free running mode OR set and forget mode.
     */
    RLD = 3,

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
    PRESCALER = 4,

    /*
     * This bit controls the counter data when the modulus register is
     * written. When this bit is set, all writes to the load register
     * will overwrite the counter contents and the counter will
     * subsequently start counting down from the programmed value.
     */
    IOVW = 17,

    SWR = 16,

    /*
     * These bits select the clock input used to run the counter. After
     * reset, the system functional clock is selected. The input clock
     * can also be turned off if these bits are set to 00. This field
     * value should only be changed when the EPIT is disabled.
     */
    CLKSRC = 24
} epit_control_reg;


enum IPGConstants {
    IPG_CLK = 1, IPG_CLK_HIGHFREQ = 2, IPG_CLK_32K = 3
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

/* typedef enum { */
/*     PERIODIC, */
/*     ONESHOT */
/* } epit_mode_t; */

typedef struct epit {
    volatile struct epit_map *epit_map;
    uint64_t counter_start;
    int mode;
    uint32_t irq;
    uint32_t prescaler;
} epit_t;

epit_t epit;
epit_t epit2;

/*******************
 *** IRQ handler ***
 *******************/

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
    IM1 = 16, IM2 = 18,

    /* Output compare channel operating modes */
    OM1 = 20, OM2 = 23, OM3 = 26,

    /* Force output compare channel bits */
    FO1 = 29, FO2 = 30, FO3 = 31

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

typedef enum {
    PERIODIC,
    ONESHOT,
} gpt_mode_t;

typedef struct gpt {
    volatile struct gpt_map *gpt_map;
    int mode;
    uint32_t irq;
    uint32_t prescaler;
} gpt_t;

gpt_t gpt;


static int send2nc(struct serial* serial, char* data, int len)
{
    return serial_send(serial, (data), (len));
}

// try best to send buf to serial, no retry at server side, let client do retry.
static void handle_ipc_print_console(seL4_CPtr session)
{
    int msg_len = seL4_GetMR(1);
    color_print(ANSI_COLOR_YELLOW, "[sos] recieved from tty, len: %d\n", msg_len);
    seL4_IPCBuffer* ipc_buffer = seL4_GetIPCBuffer();
    char* msg = (char*)(ipc_buffer->msg + 2);
    int ret = send2nc(serial_handler, msg, msg_len);
    color_print(ANSI_COLOR_YELLOW, "[sos] serial_send finish, len: %d\n", ret);

    seL4_MessageInfo_t reply = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_SetMR(0, ret);
    seL4_Send(session, reply);
    return;
}

void handle_syscall(seL4_Word badge, int num_args) {
    seL4_Word syscall_number;
    seL4_CPtr reply_cap;


    syscall_number = seL4_GetMR(0);

    /* Save the caller */
    reply_cap = cspace_save_reply_cap(cur_cspace);
    assert(reply_cap != CSPACE_NULL);

    /* Process system call */
    switch (syscall_number) {
    case SOS_SYSCALL0:
        dprintf(0, "syscall: thread made syscall 0!\n");

        seL4_MessageInfo_t reply = seL4_MessageInfo_new(0, 0, 0, 1);
        seL4_SetMR(0, 0);
        seL4_Send(reply_cap, reply);

        break;


    case SYSCALL_IPC_PRINT_COLSOLE:
        color_print(ANSI_COLOR_YELLOW, "[sos] SYSCALL_IPC_PRINT_COLSOLE\n");

        handle_ipc_print_console(reply_cap);


        break;



    default:
        printf("%s:%d (%s) Unknown syscall %d\n",
                   __FILE__, __LINE__, __func__, syscall_number);
        /* we don't want to reply to an unknown syscall */

    }

    /* Free the saved reply cap */
    cspace_free_slot(cur_cspace, reply_cap);
}

void delay(int count)
{
    int sum = 0;
    for (int i = 0; i < count; i ++)
    {
        sum =  sum + i;

    }
    color_print(ANSI_COLOR_GREEN, "delay %d\n", sum);
    return ;
}

void syscall_loop(seL4_CPtr ep) {

    while (1) {
        seL4_Word badge;
        seL4_Word label;
        seL4_MessageInfo_t message;

        message = seL4_Wait(ep, &badge);
        label = seL4_MessageInfo_get_label(message);
        if(badge & IRQ_EP_BADGE)
        {
            color_print(ANSI_COLOR_GREEN, "receive irq: %x\n", badge);
            int interrup_handled = 0;
            /* Interrupt */
            if (badge & IRQ_BADGE_NETWORK)
            {
                network_irq();
                interrup_handled = 1;
            }
            if (badge & IRQ_BADGE_TIMER1)
            {
                color_print(ANSI_COLOR_GREEN, "enter timer1 handler %x\n", badge);
                int err = seL4_IRQHandler_Ack(gpt.irq);
                gpt.gpt_map->gptsr = 0x3f;
                interrup_handled = 1;

            }
            if (badge & IRQ_BADGE_TIMER)
            {

                color_print(ANSI_COLOR_GREEN, "enter timer2 handler %x\n", badge);
                interrup_handled = 1;

                /* delay(1000000000); */
                /* color_print(ANSI_COLOR_GREEN, "enter timer handler\n"); */
                epit.epit_map->epitcr &= (~1);


                if (epit.epit_map->epitsr) {
                    epit.epit_map->epitsr = 1;

                    /* if (epit.mode != PERIODIC) { */
                    /*     #<{(| disable the epit if we don't want it to be periodic |)}># */
                    /*     #<{(| this has to be done as the epit is configured to */
                    /*      * reload the timer value after irq - this isn't desired */
                    /*      * if we are periodic |)}># */
                    /*     epit.epit_map->epitcr &= ~(BIT(EN)); */
                    /* } */
                }
                static int a = 0;
                /* delay(1000000000); */
                tstamp();
                /* GPT_SR = GPT_STATUS_REGISTER_CLEAR; */
                volatile int i = 1;
                /* volatile int s = 1; */
                /* color_print(ANSI_COLOR_GREEN, "timer irq occurs: %d, sr: %d, %d, %d\n", a ++, epit.epit_map->epitsr, epit.epit_map->epitlr, epit.epit_map->epitcr); */
                /* tstamp(); */
                int err = seL4_IRQHandler_Ack(t_cap);

                epit.epit_map->epitcr |= 1;
                conditional_panic(err, "Failed to acknowledge GPT interrupt\n");
                /* continue; */

            }
            if (interrup_handled == 0)
            {
                color_print(ANSI_COLOR_GREEN, "interesting interrupt, badge: 0x%x\n", badge);
            }

        }

        else if(label == seL4_VMFault){
            /* Page fault */
            dprintf(0, "vm fault at 0x%08x, pc = 0x%08x, %s\n", seL4_GetMR(1),
                    seL4_GetMR(0),
                    seL4_GetMR(2) ? "Instruction Fault" : "Data fault");

            assert(!"Unable to handle vm faults");
        }else if(label == seL4_NoFault) {
            /* System call */
            handle_syscall(badge, seL4_MessageInfo_get_length(message) - 1);

        }else{
            printf("Rootserver got an unknown message\n");
        }
    }
}


static void print_bootinfo(const seL4_BootInfo* info) {
    int i;

    /* General info */
    dprintf(1, "Info Page:  %p\n", info);
    dprintf(1,"IPC Buffer: %p\n", info->ipcBuffer);
    dprintf(1,"Node ID: %d (of %d)\n",info->nodeID, info->numNodes);
    dprintf(1,"IOPT levels: %d\n",info->numIOPTLevels);
    dprintf(1,"Init cnode size bits: %d\n", info->initThreadCNodeSizeBits);

    /* Cap details */
    dprintf(1,"\nCap details:\n");
    dprintf(1,"Type              Start      End\n");
    dprintf(1,"Empty             0x%08x 0x%08x\n", info->empty.start, info->empty.end);
    dprintf(1,"Shared frames     0x%08x 0x%08x\n", info->sharedFrames.start,
                                                   info->sharedFrames.end);
    dprintf(1,"User image frames 0x%08x 0x%08x\n", info->userImageFrames.start,
                                                   info->userImageFrames.end);
    dprintf(1,"User image PTs    0x%08x 0x%08x\n", info->userImagePTs.start,
                                                   info->userImagePTs.end);
    dprintf(1,"Untypeds          0x%08x 0x%08x\n", info->untyped.start, info->untyped.end);

    /* Untyped details */
    dprintf(1,"\nUntyped details:\n");
    dprintf(1,"Untyped Slot       Paddr      Bits\n");
    for (i = 0; i < info->untyped.end-info->untyped.start; i++) {
        dprintf(1,"%3d     0x%08x 0x%08x %d\n", i, info->untyped.start + i,
                                                   info->untypedPaddrList[i],
                                                   info->untypedSizeBitsList[i]);
    }

    /* Device untyped details */
    dprintf(1,"\nDevice untyped details:\n");
    dprintf(1,"Untyped Slot       Paddr      Bits\n");
    for (i = 0; i < info->deviceUntyped.end-info->deviceUntyped.start; i++) {
        dprintf(1,"%3d     0x%08x 0x%08x %d\n", i, info->deviceUntyped.start + i,
                                                   info->untypedPaddrList[i + (info->untyped.end - info->untyped.start)],
                                                   info->untypedSizeBitsList[i + (info->untyped.end-info->untyped.start)]);
    }

    dprintf(1,"-----------------------------------------\n\n");

    /* Print cpio data */
    dprintf(1,"Parsing cpio data:\n");
    dprintf(1,"--------------------------------------------------------\n");
    dprintf(1,"| index |        name      |  address   | size (bytes) |\n");
    dprintf(1,"|------------------------------------------------------|\n");
    for(i = 0;; i++) {
        unsigned long size;
        const char *name;
        void *data;

        data = cpio_get_entry(_cpio_archive, i, &name, &size);
        if(data != NULL){
            dprintf(1,"| %3d   | %16s | %p | %12d |\n", i, name, data, size);
        }else{
            break;
        }
    }
    dprintf(1,"--------------------------------------------------------\n");
}

void start_first_process(char* app_name, seL4_CPtr fault_ep) {
    int err;

    seL4_Word stack_addr;
    seL4_CPtr stack_cap;
    seL4_CPtr user_ep_cap;

    /* These required for setting up the TCB */
    seL4_UserContext context;

    /* These required for loading program sections */
    char* elf_base;
    unsigned long elf_size;

    /* Create a VSpace */
    tty_test_process.vroot_addr = ut_alloc(seL4_PageDirBits);
    conditional_panic(!tty_test_process.vroot_addr,
                      "No memory for new Page Directory");
    err = cspace_ut_retype_addr(tty_test_process.vroot_addr,
                                seL4_ARM_PageDirectoryObject,
                                seL4_PageDirBits,
                                cur_cspace,
                                &tty_test_process.vroot);
    conditional_panic(err, "Failed to allocate page directory cap for client");

    /* Create a simple 1 level CSpace */
    tty_test_process.croot = cspace_create(1);
    assert(tty_test_process.croot != NULL);

    /* Create an IPC buffer */
    tty_test_process.ipc_buffer_addr = ut_alloc(seL4_PageBits);
    conditional_panic(!tty_test_process.ipc_buffer_addr, "No memory for ipc buffer");
    err =  cspace_ut_retype_addr(tty_test_process.ipc_buffer_addr,
                                 seL4_ARM_SmallPageObject,
                                 seL4_PageBits,
                                 cur_cspace,
                                 &tty_test_process.ipc_buffer_cap);
    conditional_panic(err, "Unable to allocate page for IPC buffer");

    /* Copy the fault endpoint to the user app to enable IPC */
    user_ep_cap = cspace_mint_cap(tty_test_process.croot,
                                  cur_cspace,
                                  fault_ep,
                                  seL4_AllRights,
                                  seL4_CapData_Badge_new(TTY_EP_BADGE));
    /* should be the first slot in the space, hack I know */
    assert(user_ep_cap == 1);
    assert(user_ep_cap == USER_EP_CAP);

    /* Create a new TCB object */
    tty_test_process.tcb_addr = ut_alloc(seL4_TCBBits);
    conditional_panic(!tty_test_process.tcb_addr, "No memory for new TCB");
    err =  cspace_ut_retype_addr(tty_test_process.tcb_addr,
                                 seL4_TCBObject,
                                 seL4_TCBBits,
                                 cur_cspace,
                                 &tty_test_process.tcb_cap);
    conditional_panic(err, "Failed to create TCB");

    /* Configure the TCB */
    err = seL4_TCB_Configure(tty_test_process.tcb_cap, user_ep_cap, TTY_PRIORITY,
                             tty_test_process.croot->root_cnode, seL4_NilData,
                             tty_test_process.vroot, seL4_NilData, PROCESS_IPC_BUFFER,
                             tty_test_process.ipc_buffer_cap);
    conditional_panic(err, "Unable to configure new TCB");

    /* Provide a logical name for the thread -- Helpful for debugging */
#ifdef SEL4_DEBUG_KERNEL
    seL4_DebugNameThread(tty_test_process.tcb_cap, app_name);
#endif

    /* parse the cpio image */
    dprintf(1, "\nStarting \"%s\"...\n", app_name);
    elf_base = cpio_get_file(_cpio_archive, app_name, &elf_size);
    conditional_panic(!elf_base, "Unable to locate cpio header");

    /* load the elf image */
    err = elf_load(tty_test_process.vroot, elf_base);
    conditional_panic(err, "Failed to load elf image");


    /* Create a stack frame */
    stack_addr = ut_alloc(seL4_PageBits);
    conditional_panic(!stack_addr, "No memory for stack");
    err =  cspace_ut_retype_addr(stack_addr,
                                 seL4_ARM_SmallPageObject,
                                 seL4_PageBits,
                                 cur_cspace,
                                 &stack_cap);
    conditional_panic(err, "Unable to allocate page for stack");

    /* Map in the stack frame for the user app */
    err = map_page(stack_cap, tty_test_process.vroot,
                   PROCESS_STACK_TOP - (1 << seL4_PageBits),
                   seL4_AllRights, seL4_ARM_Default_VMAttributes);
    conditional_panic(err, "Unable to map stack IPC buffer for user app");

    /* Map in the IPC buffer for the thread */
    err = map_page(tty_test_process.ipc_buffer_cap, tty_test_process.vroot,
                   PROCESS_IPC_BUFFER,
                   seL4_AllRights, seL4_ARM_Default_VMAttributes);
    conditional_panic(err, "Unable to map IPC buffer for user app");

    /* Start the new process */
    memset(&context, 0, sizeof(context));
    context.pc = elf_getEntryPoint(elf_base);
    context.sp = PROCESS_STACK_TOP;
    seL4_TCB_WriteRegisters(tty_test_process.tcb_cap, 1, 0, 2, &context);
}

static void _sos_ipc_init(seL4_CPtr* ipc_ep, seL4_CPtr* async_ep){
    seL4_Word ep_addr, aep_addr;
    int err;

    /* Create an Async endpoint for interrupts */
    aep_addr = ut_alloc(seL4_EndpointBits);
    conditional_panic(!aep_addr, "No memory for async endpoint");
    err = cspace_ut_retype_addr(aep_addr,
                                seL4_AsyncEndpointObject,
                                seL4_EndpointBits,
                                cur_cspace,
                                async_ep);
    conditional_panic(err, "Failed to allocate c-slot for Interrupt endpoint");

    /* Bind the Async endpoint to our TCB */
    err = seL4_TCB_BindAEP(seL4_CapInitThreadTCB, *async_ep);
    conditional_panic(err, "Failed to bind ASync EP to TCB");


    /* Create an endpoint for user application IPC */
    ep_addr = ut_alloc(seL4_EndpointBits);
    conditional_panic(!ep_addr, "No memory for endpoint");
    err = cspace_ut_retype_addr(ep_addr,
                                seL4_EndpointObject,
                                seL4_EndpointBits,
                                cur_cspace,
                                ipc_ep);
    conditional_panic(err, "Failed to allocate c-slot for IPC endpoint");
}


static void _sos_init(seL4_CPtr* ipc_ep, seL4_CPtr* async_ep){
    seL4_Word dma_addr;
    seL4_Word low, high;
    int err;

    /* Retrieve boot info from seL4 */
    _boot_info = seL4_GetBootInfo();
    conditional_panic(!_boot_info, "Failed to retrieve boot info\n");
    if(verbose > 0){
        print_bootinfo(_boot_info);
    }

    /* Initialise the untyped sub system and reserve memory for DMA */
    err = ut_table_init(_boot_info);
    conditional_panic(err, "Failed to initialise Untyped Table\n");
    /* DMA uses a large amount of memory that will never be freed */
    dma_addr = ut_steal_mem(DMA_SIZE_BITS);
    conditional_panic(dma_addr == 0, "Failed to reserve DMA memory\n");

    /* find available memory */
    ut_find_memory(&low, &high);

    /* Initialise the untyped memory allocator */
    ut_allocator_init(low, high);

    /* Initialise the cspace manager */
    err = cspace_root_task_bootstrap(ut_alloc, ut_free, ut_translate,
                                     malloc, free);
    conditional_panic(err, "Failed to initialise the c space\n");

    /* Initialise DMA memory */
    err = dma_init(dma_addr, DMA_SIZE_BITS);
    conditional_panic(err, "Failed to intiialise DMA memory\n");

    /* Initialiase other system compenents here */

    _sos_ipc_init(ipc_ep, async_ep);
}

static inline seL4_CPtr badge_irq_ep(seL4_CPtr ep, seL4_Word badge) {
    seL4_CPtr badged_cap = cspace_mint_cap(cur_cspace, cur_cspace, ep, seL4_AllRights, seL4_CapData_Badge_new(badge | IRQ_EP_BADGE));
    conditional_panic(!badged_cap, "Failed to allocate badged cap");
    return badged_cap;
}
static seL4_CPtr
enable_irq(int irq, seL4_CPtr aep) {
    seL4_CPtr cap;
    int err;
    /* Create an IRQ handler */
    cap = cspace_irq_control_get_cap(cur_cspace, seL4_CapIRQControl, irq);
    conditional_panic(!cap, "Failed to acquire and IRQ control cap");
    /* Assign to an end point */
    err = seL4_IRQHandler_SetEndpoint(cap, aep);
    conditional_panic(err, "Failed to set interrupt endpoint");
    /* Ack the handler before continuing */
    err = seL4_IRQHandler_Ack(cap);
    conditional_panic(err, "Failure to acknowledge pending interrupts");
    return cap;
}

int start_timer1(seL4_CPtr interrupt_ep)
{
    /* _timer_queue = timer_queue_init(256000); */

    gpt.gpt_map = map_device((void *) GPT_PHYS_START, GPT_PHYS_SIZE);

    gpt.gpt_map->gptcr  = 0;
    gpt.gpt_map->gptir = 0;

    gpt.gpt_map->gptcr = BIT(GPT_SWR); // SWR
    /* gpt.gpt_map->gptcr = (0b001) <<OM1  | (0b001<<GPT_CLKSRC) | BIT(GPT_ENMOD); */
    gpt.gpt_map->gptcr = 0b00000000000000000000000001111110;
    gpt.gpt_map->gptpr = 0x042;
    gpt.gpt_map->gptsr = 0x3f;
    gpt.gpt_map->gptir = 1;
    gpt.gpt_map->gptcr1= 1000000;

    gpt.gpt_map->gptcr &= (~BIT(GPT_SWR)); // SWR
    gpt.irq = enable_irq(GPT_IRQ, interrupt_ep);
    gpt.gpt_map->gptcr |=1;


}
int start_timer(seL4_CPtr interrupt_ep) {
    t_irq_ep = interrupt_ep;
    /* _timer_queue = timer_queue_init(256000); */

    epit.epit_map = map_device((void *) EPIT1_PHYS_START, EPIT1_PHYS_SIZE);
    epit2.epit_map = map_device((void *) EPIT2_PHYS_START, EPIT2_PHYS_SIZE);

    /* gpt.gpt_map = map_device((void *) GPT_PHYS_START, GPT_PHYS_SIZE); */
    /*  */
    /* gpt.gpt_map->gptcr  = 0; */
    /* gpt.gpt_map->gptir = 0; */
    /*  */
    /* gpt.gpt_map->gptcr = BIT(GPT_SWR); // SWR */
    /* gpt.gpt_map->gptcr = (0b011) <<OM1 | BIT(GPT_FRR) | (0b001<<GPT_CLKSRC) | BIT(GPT_ENMOD); */
    /* gpt.gpt_map->gptpr = 0x042; */
    /* gpt.gpt_map->gptir = 1; */
    /* gpt.gpt_map->gptcr1= 1000000; */
    /*  */
    /*  */
    /* gpt.gpt_map->gptcr &= (~BIT(GPT_SWR)); // SWR */
    /* gpt.irq= enable_irq(GPT_IRQ, interrupt_ep); */
    /*  */


    epit2.epit_map->epitcr = 0;
    epit2.epit_map->epitcr = BIT(SWR);

    epit2.epit_map->epitcr = (0b10 << CLKSRC) | /* Clock source = IPG */
        (0x042 << PRESCALER) | /* Set the prescaler */
        BIT(IOVW) | /* Overwrite counter immediately on write */
        BIT(RLD) | /* Reload counter from modulus register on overflow */
        /* BIT(OCIEN) | #<{(| Enable interrupt on comparison event |)}># */
        BIT(ENMOD) | /* Count from modulus on restart */
        0;
    /* epit2.epit_map->epitcmpr = 0; */
    int counterValue = 1000000000;
    epit2.epit_map->epitlr = counterValue;
    while (epit2.epit_map->epitlr != counterValue) {
        epit2.epit_map->epitlr = counterValue;
    }
    epit2.epit_map->epitcr &= (~BIT(SWR));
    /* t_cap = enable_irq(EPIT1_IRQ, interrupt_ep); */
    /* Interrupt when compare with 0. */

    epit2.epit_map->epitcr |= 1;


    /* Disable EPIT. */
    epit.epit_map->epitcr = 0;


    /* Configure EPIT. */
    epit.epit_map->epitcr = BIT(SWR);
    epit.epit_map->epitcr = (0b10 << CLKSRC) | /* Clock source = IPG */
        (0x042 << PRESCALER) | /* Set the prescaler */
        BIT(IOVW) | /* Overwrite counter immediately on write */
        BIT(RLD) | /* Reload counter from modulus register on overflow */
        BIT(OCIEN) | /* Enable interrupt on comparison event */
        BIT(ENMOD) | /* Count from modulus on restart */
        0;
    epit.epit_map->epitcmpr = 0;
    counterValue = 1000000;
    epit.epit_map->epitlr = counterValue;
    while (epit.epit_map->epitlr != counterValue) {
        epit.epit_map->epitlr = counterValue;
    }
    epit.epit_map->epitcr &= (~BIT(SWR));
    t_cap = enable_irq(EPIT1_IRQ, interrupt_ep);
    /* Interrupt when compare with 0. */

    epit.epit_map->epitcr |= 1;

    /*
     *
     * The CLKSRC field in the GPT_CR register selects the clock source. The CLKSRC field
     * value should be changed only after disabling the GPT (EN=0).
     * The software sequence to be followed while changing clock source is:
     *
     * 1. Disable GPT by setting EN=0 in GPT_CR register.
     * 2. Disable GPT interrupt register (GPT_IR).
     * 3. Configure Output Mode to unconnected/ disconnected—Write zeros in OM3, OM2,
     * and OM1 in GPT_CR
     * 4. Disable Input Capture Modes—Write zeros in IM1 and IM2 in GPT_CR
     * 5. Change clock source CLKSRC to the desired value in GPT_CR register.
     * 6. Assert the SWR bit in GPT_CR register.
     * 7. Clear GPT status register (GPT_SR) (i.e., w1c).
     * 8. Set ENMOD=1 in GPT_CR register, to bring GPT counter to 0x00000000.
     * 9. Enable GPT (EN=1) in GPT_CR register.
     * 10. Enable GPT interrupt register (GPT_IR).
     * /

    /* #<{(| Reset GPT |)}># */
    /* GPT_CR = 0b00000000000000000000000000000000; */
    /*  */
    /* GPT_SR = GPT_STATUS_REGISTER_CLEAR; */
    /*  */
    /* GPT_OCR1 = 100000; */
    /* GPT_PR = 65; */
    /* GPT_IR = 0b00000000000000000000000000000000; */
    /* #<{(| GPT_CR |= 0b001000000 ; |)}># */
    /* #<{(| GPT_CR |= BIT(SWR)| BIT(ENMOD); |)}># */
    /* GPT_CR = 0b00000000000000000000000001111110 ; */

    /* GPT_CNT = 100000; */

    /* GPT_PR = 0; */


    /* GPT_CR   |= 0b00000000000000000000000000000001; */
    /* GPT_IR = BIT(ROV); */
    /* GPT_IR = 1; */

    /* Reset EPIT1 */
    /* EPIT1_CR = 0b00000000000000010000000000000000; */

    // need to spin for ~10 cycles
    // Reason: frequency mismatch against main CPU - not totally sure
    //   how fast things are. Waiting for GPT reset to take effect.
    /* volatile int i = 10; */
    /* while (i-- > 0); */

    /* Set up GPT. Enables compare channels, set clock source, etc */
    /* 0b 0000 0000 0000 0000 0000 0010 0111 1110

          31: force output compare 3. 0 = don't force it please
          30: force ocr2. 0 = don't force
          29: force ocr1. 0 = don't
          28-26: output channel 3 operating mode. 000 = output disconnected.
          25-23: output dc
          22-20: output dc
          19-18: IM2. Input capture channel 2 operating mode. 0 = capture disabled.
          17-16: IM1. same
          15: software reset. 0, because we already reset.
          14-10: read-only reserved. 0
          9: Free run or restart mode. 0 = no free run mode.
          8-6: clock source select. 001: peripheral clock.
          5-2: various enables. We want all of these = 1.
          1: GPT enable mode. Set to 1 = resets on disable.
          0: enable. later

     */
    /* GPT_CR = 0b00000000000000000000000001111110; */

    /* Set up prescaler. Divide by 66 = 1us */
    /* (divide by 3300 = 20kHz = .00005 s = 0.05ms = 50祍) */
    /* GPT_PR = 65; */

    /* Zero out status register. */
    /* GPT_SR = GPT_STATUS_REGISTER_CLEAR; */

    /* Interrupt register. */
    /* GPT_IR = 0b00000000000000000000000000000001; */

    /* Value in the compare register. 100000 us == 100ms. 100000 ticks */
    /* GPT_OCR1 = 100000; */

    /* 0b 0000 0001 0010 1110 0000 0100 0010 1010
       31-26: reserved, always zero
       25-24: clock source. 01 = peripheral clock
       23-22: output mode. 00 = don't care
       21: stop mode enable. 1 = enabled always
       20: reserved, always zero
       19: Wait enabled. 1 = always enabled
       18: debug enabled. 1 = always on
       17: IOVW, counter overwrite enable. 1 = resetting countdown each time
       16: software reset. 0 = not resetting it right now.
       15-4: prescaler.  = 0000 0100 0010 (divide by 66, giving 1MHz res)
       3: reload control. 1 = reload from modulus register.
       2: output compare interrupt enable = 0, definitely no interrupts please
       1: EPIT enable mode. 1 = reset on re-enable.
       0: enable. set this later.
     */
    /* EPIT1_CR = 0b00000001001011100000010000011010; */
    /*  */
    /* #<{(| Zero out status register. |)}># */
    /* EPIT1_SR = 0; */
    /* #<{(| Set up Load Register (referred to as modulus register in EPIT1_CR). */
    /*    Set compare value to 100000, 100ms fire. Should line up with GPT interrupts. */
    /*  |)}># */
    /* EPIT1_LR = 100000; */
    /*  */
    /* #<{(| Countdown timer - reach zero to fire. |)}># */
    /* EPIT1_CMPR = 0; */
    /*  */
    /* #<{(| Start accepting GPT interrupts |)}># */
    /*  */
    /* #<{(| Enable GPT |)}># */
    /*  */
    /* Enable EPIT1 */
    /* EPIT1_CR |= 0b00000000000000000000000000000001; */

    return 0;
}

static int cur_ts_us = 0;

int tstamp(void) {
    int cur = epit2.epit_map->epitcnt;
    static int last = 0;
    /* color_print(ANSI_COLOR_GREEN, "%d->", cur); */
    if (cur > last)
    {
        cur_timestamp += (1000000000 - cur + last) / 1000;
        cur_ts_us += (1000000000 - cur + last) % 1000;
    }

    else
    {
        cur_timestamp += (last - cur) / 1000;
        cur_ts_us += (last - cur) % 1000;
    }
    if (cur_ts_us >= 1000)
    {
        cur_timestamp ++;
        cur_ts_us  -= 1000;
    }
    if (last != 0)
    {
        color_print(ANSI_COLOR_GREEN, "%d %llu\n", cur - last, cur_timestamp);
    }

    last = cur;
    return 0;
    /* return uptime + (100000 - EPIT1_CNR); */
}

/*
 * Main entry point - called by crt.
 */
int main(void) {

#ifdef SEL4_DEBUG_KERNEL
    seL4_DebugNameThread(seL4_CapInitThreadTCB, "SOS:root");
#endif

    dprintf(0, "\nSOS Starting...\n");

    _sos_init(&_sos_ipc_ep_cap, &_sos_interrupt_ep_cap);

    /* Initialise the network hardware */
    network_init(badge_irq_ep(_sos_interrupt_ep_cap, IRQ_BADGE_NETWORK));


    start_timer(badge_irq_ep(_sos_interrupt_ep_cap, IRQ_BADGE_TIMER));

    start_timer1(badge_irq_ep(_sos_interrupt_ep_cap, IRQ_BADGE_TIMER1));

    serial_handler = serial_init();

    /* Start the user application */
    start_first_process(TTY_NAME, _sos_ipc_ep_cap);

    /* Wait on synchronous endpoint for IPC */
    dprintf(0, "\nSOS entering syscall loop\n");
    syscall_loop(_sos_ipc_ep_cap);

    /* Not reached */
    return 0;
}


uint64_t __timestamp_ms()
{
    // FIXME
    return 0;
}
