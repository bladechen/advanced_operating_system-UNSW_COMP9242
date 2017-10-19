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
#include "setjmp.h"
#include <cspace/cspace.h>
#include <nfs/nfs.h>
#include <elf/elf.h>
#include <serial/serial.h>
#include <clock/clock.h>
#include "network.h"
#include "elf.h"
#include "comm/comm.h"
#include "ut_manager/ut.h"
#include "vm/vmem_layout.h"
#include <cpio/cpio.h>
#include "mapping.h"
#include "vfs/vfs.h"
#include <autoconf.h>

#define verbose 5
#include <sys/debug.h>
#include <sys/panic.h>

#include "unittest/test.h"
#include "dev/console.h"
#include "dev/nfs.h"

#include "vm/frametable.h"
#include "vm/address_space.h"
#include "proc/proc.h"
#include "fs/file.h"

#include "syscall/syscall.h"
#include <sos.h>


volatile bool sos_init_flag = false;

/* uint32_t dbflags = 0xFFFFFFFF | (DB_VM); */
uint32_t dbflags = 0;
/* uint32_t dbflags = 0xFFFFFFFF ; */
/* uint32_t dbflags = 0 ;//0xFFFFFFFF; */

extern int test_coro();

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

#define SOSH_NAME             CONFIG_SOS_STARTUP_APP

/* The linker will link this symbol to the start address  *
 * of an archive of attached applications.                */
extern char _cpio_archive[];

const seL4_BootInfo* _boot_info;


seL4_CPtr _sos_ipc_ep_cap;
seL4_CPtr _sos_interrupt_ep_cap;

/**
 * NFS mount point
 */
extern fhandle_t mnt_point;

void update_timestamp(void);
void handle_epit1_irq(void);
void handle_gpt_irq(void);
void handle_time_driver_cb(void);

void syscall_loop(seL4_CPtr ep)
{
    while (1) {
        seL4_Word badge;
        seL4_Word label;
        seL4_MessageInfo_t message;

        message = seL4_Wait(ep, &badge);
        label = seL4_MessageInfo_get_label(message);

        if(badge & IRQ_EP_BADGE)
        {
            /* Interrupt */
            if (badge & IRQ_BADGE_NETWORK)
            {
                /* printf ("handle net irq\n"); */
                network_irq();
            }
            // currently no use for epit1, should not here
            if (badge & IRQ_EPIT1_BADGE)
            {
                assert(0);
                /* handle_epit1_irq(); */
            }
            if (badge & IRQ_GPT_BADGE) // move to seperate process
            {
                assert(0);
                /* handle_gpt_irq(); */
            }
        }
        else if(label == seL4_VMFault)
        {
            COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, "pid[%d] vm fault at 0x%08x, pc = 0x%08x [%s, %d]\n",
                        badge,
                        seL4_GetMR(1),
                        seL4_GetMR(0),
                        seL4_GetMR(2) ? "Instruction Fault" : "Data fault", seL4_GetMR(3));
            struct proc* p = pid_to_proc(badge);
            if (p != NULL)
            {
                handle_vm_fault(pid_to_proc(badge), seL4_GetMR(0), seL4_GetMR(1), seL4_GetMR(3));
            }
            else
            {
                ERROR_DEBUG("non-existing proc id: %d !\n", badge);
                // there should be bug in sos itself.
                assert(0);
            }
        }
        else if(label == seL4_NoFault)
        {
            if (badge == 1) // pid(1) is time driver, and sos forbid all its syscall
            {
                handle_time_driver_cb();
            }
            else
            {
                assert(badge >= 1);
                struct proc* p = pid_to_proc(badge);
                if (p != NULL)
                {
                    handle_syscall(badge, pid_to_proc(badge));
                }
                else
                {
                    ERROR_DEBUG("non-existing proc id: %d !\n", badge);
                    assert(0);
                }
            }
        }
        else
        {
            ERROR_DEBUG("Rootserver got an unknown message\n");
        }
        //coroutine schedule routine.
        schedule_loop();
        // clear zombie process
        recycle_process();
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
    /* not delete, because we need to ensure every time the elf file got compiled if modified. */
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
    if(verbose > 0) {
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

/*
 * Main entry point - called by crt.
 */

void start_time_driver(seL4_CPtr);
int main(void) {

#ifdef SEL4_DEBUG_KERNEL
    seL4_DebugNameThread(seL4_CapInitThreadTCB, "SOS:root");
#endif

    dprintf(0, "\nSOS Starting...\n");

    _sos_init(&_sos_ipc_ep_cap, &_sos_interrupt_ep_cap);

    /* Initialise the network hardware */
    network_init(badge_irq_ep(_sos_interrupt_ep_cap, IRQ_BADGE_NETWORK));

    vfs_bootstrap();
    init_kern_file_table();
    init_console();
    init_nfs(&mnt_point);

    vm_bootstrap();
    proc_bootstrap();

    /* init_test_coro(); */

    start_timer(_sos_ipc_ep_cap); // because we load time driver as seperate process, so _sos_ipc_ep_cap is needed instead of _sos_interrupt_ep_cap
    m1_test(); // will dump timestamp on minicom every second.

    /* Start the sos shell */
    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "create sosh process...\n");
    // pid should be 1
    assert(run_program(SOSH_NAME, _sos_ipc_ep_cap, 0, NULL) >= 1);
    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "create sosh success...\n");

    /* m2_test(); */
    dprintf(0, "\nSOS entering syscall loop\n");
    sos_init_flag = true;
    syscall_loop(_sos_ipc_ep_cap);
    ERROR_DEBUG("\nSOS exits loop\n");

    return 0;
}


