    #ifndef _SWAP_H_
#define _SWAP_H_

#include <sel4/sel4.h>
#include "vm.h"

/* it also the size of `pagefile`*/
#define PAGEFILE_SIZE 1024*1024*1024
#define SWAPTABLE_ENTRY_AMOUNT 1024*1024*1024 / seL4_PAGE_SIZE


/*
*   frame table entry get its offset in frametable by the address get from ut_alloc
*   page table entry get its offset in pagetable by the first 20 bits with itself
*   However for swaptable, we need to maintain&find out the empty slot by ourself. 
*/

/*
*   In frametable.c, after we swap out a frame, we need to find a way to record the corresponding
*   swap_table_entry and pagetable entry.
*/


/*  
*   It is the offset of swap_table, which indicate the next empty, we should try to make it always 
*   point to the smallest offset.
*/
int next_emtpy_swap_table_entry;

typedef struct swap_table_entry 
{
    /* 
    *   after the frame swapped out, frame table won't contain the corresponding 
    *   fte, and frame table need to record corresponding swap_table_entry in case
    *   it needs to swap in the data in the future. And what we need to record is 
    *   only the status, the other fields in frame_table_entry like frame_cap will be
    *   generated the next time we try to swap in, so I think we do not need to 
    *   recorad that.
    */
    frame_entry_status fes;
    /*
    *   The offset of file it writes to. Should read from that offset when swap in.
    *   This also indicate the offset of swaptable where current swap_table_entry located
    */
    int offset;
    swap_table_entry * next_empty;

    // Leave it to 
    /*
    *   Throughout the whole process, only the app_vaddr won't change
    *   It is also what we will get when vm_fault is triggered, may move this to some
    *   data structure in frametable
    */
    // seL4_Word app_vaddr;
} swap_table_entry;

typedef swap_table_entry * swap_table;


/*
*   @sos_vaddr, the address going to copy frame from
*   @app_vaddr, the app_vaddr given by the app
*
*   @return return the swap_table offset or the pointer to the swap_table_entry, so that 
*   we can retrieve infos we need when swap in.
*/
int swap_out(seL4_Word sos_vaddr, frame_entry_status fes);

/*
*   @sos_vaddr, the address going to copy frame to
*   @offset, used to find the corresponding swap_table_entry
*/
int swap_in(int offset, seL4_Word sos_vaddr);


void init_swapping(void);

/*
*   @sos_vaddr: is the address returned by frame_alloc,
*   which in turn is the physical addr for APPs.
*   @offset: The offset of pagefile we're writing to
*/
bool write_to_pagefile(seL4_Word sos_vaddr, int offset);
bool read_from_pagefile(seL4_Word sos_vaddr, int offset);

#endif
