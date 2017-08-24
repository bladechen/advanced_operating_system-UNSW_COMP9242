#ifndef _FRAMETABLE_H_
#define _FRAMETABLE_H_

#include <sel4/sel4.h>
#include "vm.h"

// TODO we may reserve some critical frame for sos use only.
/* Integer division, rounded up (rather than truncating) */

// for sos memory only, not for application memory
// paddr for the untyped memory address
// vaddr for the sos mapped virtual address
typedef seL4_Word sos_paddr_t;
typedef seL4_Word sos_vaddr_t;


enum frame_entry_status
{
    FRAME_FREE = 0,
    FRAME_SOS  = 1,
    FRAME_APP  = 2,
};

typedef struct frame_table_entry
{
    seL4_CPtr   sos_cap;
    seL4_CPtr   app_cap;
    // index for the free frame.
    int next_free;
} frame_table_entry;

typedef frame_table_entry *frame_table;

void frametable_init(void);
sos_vaddr_t frame_alloc(sos_vaddr_t * vaddr_ptr);
void frame_free(seL4_Word vaddr);

// int sos_frame_remap(sos_vaddr_t in_vaddr, sos_vaddr_t out_vaddr, int right);

// sos_vaddr_t frame_alloc_readonly(sos_vaddr_t * vaddr_ptr);

int set_frame_app_cap(sos_vaddr_t vaddr, seL4_CPtr cap);

uint32_t get_frame_app_cap(sos_vaddr_t vaddr);
uint32_t get_frame_sos_cap(sos_vaddr_t vaddr);



void flush_sos_frame(sos_vaddr_t vaddr);// TODO why?

#endif
