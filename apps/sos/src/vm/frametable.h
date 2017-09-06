#ifndef _FRAMETABLE_H_
#define _FRAMETABLE_H_

#include <sel4/sel4.h>
#include "vm.h"

// #define DEFAULT_UMEM_BYTES (300 * 1024 * 1024)
#define DEFAULT_UMEM_BYTES (10 * 1024 * 1024)
#define DEFAULT_KMEM_BYTES (50 * 1024 * 1024)

/* Integer division, rounded up (rather than truncating) */

// for sos memory only, not for application memory
// paddr for the untyped memory address
// vaddr for the sos mapped virtual address
typedef seL4_Word sos_paddr_t;
typedef seL4_Word sos_vaddr_t;


enum frame_entry_status
{
    FRAME_FREE_SOS = 0,
    FRAME_FREE_APP = 1,

    FRAME_SOS  = 2,
    FRAME_APP  = 3,

    FRAME_UNINIT = -1,
};


// this bit is only valid when the status is FRAME_APP
#define FRAME_CLOCK_TICK_BIT (1 << 0)
#define FRAME_PIN_BIT (1 << 1)
#define FRAME_DIRTY_BIT (1 << 2)

typedef struct frame_table_entry
{
    seL4_CPtr   frame_cap;
    int myself;
    int next; // last = -1//
    int prev; // prev = -1



    uint32_t swap_frame_version;
    uint32_t swap_frame_number;
    int ctrl;
    seL4_CPtr   remap_cap;
    enum frame_entry_status status;
    void* owner; // the page entry belong to, only make sense when APP_FRAME

    // bool clock_bit;

} frame_table_entry;

typedef frame_table_entry *frame_table;

void frametable_init(size_t umem, size_t kmem); // umem in bytes, which limited user mem, left mem is for sos itself, and for another type of mem. 0 stands for no limited
sos_vaddr_t uframe_alloc();
void uframe_free(sos_vaddr_t vaddr);



// for sos it self only. which means it can not be swapped out!
sos_vaddr_t kframe_alloc();
void kframe_free(sos_vaddr_t vaddr);


// int sos_frame_remap(sos_vaddr_t in_vaddr, sos_vaddr_t out_vaddr, int right);


int set_frame_app_cap(sos_vaddr_t vaddr, seL4_CPtr cap);

uint32_t get_frame_app_cap(sos_vaddr_t vaddr);
uint32_t get_frame_sos_cap(sos_vaddr_t vaddr);



void flush_sos_frame(sos_vaddr_t vaddr);// TODO why?

void set_uframe_owner(sos_vaddr_t vaddr, void* owner);


void pin_frame(sos_vaddr_t vaddr);

int frame_swapin(uint32_t swap_number, sos_vaddr_t vaddr);

void set_uframe_dirty(sos_vaddr_t vaddr, bool dirty);

#endif
