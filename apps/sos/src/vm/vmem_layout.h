/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _MEM_LAYOUT_H_
#define _MEM_LAYOUT_H_

/* Address where memory used for DMA starts getting mapped.
 * Do not use the address range between DMA_VSTART and DMA_VEND */
#define DMA_VSTART          (0x10000000)
#define DMA_SIZE_BITS       (22)
#define DMA_VEND            (DMA_VSTART + (1ull << DMA_SIZE_BITS))


/* Starting virtual address for SOS managed frames*/
#define WINDOW_START 0x20000000
#define WINDOW_END   0x60000000


#define COROUTINE_STACK_START 0x60000000
#define COROUTINE_STACK_END   0x70000000

/* Operations to get/recover the frame number*/
#define PAGE_SHIFT(X)      ((X) >> seL4_PageBits)
#define PAGE_UNSHIFT(X)    ((X) << seL4_PageBits)

/* From this address onwards is where any devices will get mapped in
 * by the map_device function. You should not use any addresses beyond
 * here without first modifying map_device */
#define DEVICE_START        (0xB0000000)

#define ROOT_VSTART         (0xC0000000)

/* Constants for how SOS will layout the address space of any
 * processes it loads up */

#define APP_CODE_DATA_START          (0x000000000)
#define APP_CODE_DATA_END            (0x200000000)


// maximum (256M - 4K * 2) for heap
#define APP_PROCESS_HEAP_START_GUARD  (0x20000000)
#define APP_PROCESS_HEAP_START        (0x20001000)
// #define APP_PROCESS_HEAP_END          (0x2FFFE000)
#define APP_PROCESS_HEAP_END          (0x21FFE000)
#define APP_PROCESS_HEAP_END_GUARD    (0x30000000)

// TODO reserved for the extended part.
#define APP_PROCESS_MMAP_START        (0x30000000)
#define APP_PROCESS_MMAP_END          (0x50000000)


// maximum (32M - 4K * 2) for stack
#define APP_PROCESS_STACK_BOTTON_GUARD (0x8E000000)
#define APP_PROCESS_STACK_BOTTOM      (0x8E001000)
#define APP_PROCESS_STACK_TOP         (0x8FFFE000)
#define APP_PROCESS_STACK_TOP_GUARD   (0x90000000)

// #define PROCESS_STACK_TOP   (0x90000000) is different from this design?


#define APP_PROCESS_IPC_BUFFER  (0xA0000000)
#define APP_PROCESS_IPC_GUARD   (0xA0001000)

#define APP_PROCESS_VMEM_START  (0xC0000000)

#define APP_PROCESS_SCRATCH     (0xD0000000)


#endif /* _MEM_LAYOUT_H_ */
