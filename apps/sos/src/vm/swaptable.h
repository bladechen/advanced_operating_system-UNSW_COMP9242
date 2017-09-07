#ifndef _SWAP_TABLE_H_
#define _SWAP_TABLE_H_

#include <sel4/sel4.h>
#include "vm.h"


/* it also the size of `pagefile`*/
#define PAGEFILE_SIZE (20*1024*1024)
#define SWAPTABLE_ENTRY_AMOUNT DIVROUND(PAGEFILE_SIZE, seL4_PAGE_SIZE)

#include "vm.h"
#include "frametable.h"
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
// int next_emtpy_swap_table_entry;


// SWAP_IN_TO_MEM status does not exist, since if it swap into mem, this
// entry should be free for use,
enum swap_table_entry_status
{
    ENTRY_FREE = 0,
    SWAP_OUT_TO_FILE = 1,
    ENTRY_INVALID = 2
};

typedef struct swap_table_head
{
    int first;
    int last;

    int free_entries;
    int total_entries;
} swap_table_head;



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
    /*
    *   The offset/myself_index of file it writes to. Should read from that offset when swap in.
    *   This also indicate the offset of swaptable where current swap_table_entry located
    */
    int myself_index;
    int next_index;
    int prev_index;
    // struct swap_table_entry * next_empty;
    uint32_t version_number; // will be increased after swap_in or swap_out

    enum swap_table_entry_status status;

    // Leave it to
    /*
    *   Throughout the whole process, only the app_vaddr won't change
    *   It is also what we will get when vm_fault is triggered, may move this to some
    *   data structure in frametable
    */
    // seL4_Word app_vaddr;

} swap_table_entry;

typedef swap_table_entry * swap_table;


int do_swapout_frame(sos_vaddr_t vaddr,  // frame vaddr
                     uint32_t swap_frame_number_in, // the swap id recorded in page table
                     uint32_t swap_frame_version,
                     uint32_t *swap_frame_number_out // the swap id recorded in page table
                     );


// return error if swap_frame_number is invalid !
int do_swapin_frame(sos_vaddr_t vaddr,
                    uint32_t swap_frame_number,
                    uint32_t* swap_frame_version
                    );

int do_free_swap_frame(uint32_t swap_frame_number);


/* This function will be called in vm_bootstrap(), mainly used for initialize vnode, swap table and the doubly linked list queue.*/
int init_swapping(void);

/* This function will be called in frametable init, so that we do not need to malloc for the swap table */
int alloc_swaptable(void);

void init_swapping_vnode(void);

/*
*   @sos_vaddr: is the address returned by frame_alloc,
*   which in turn is the physical addr for APPs.
*   @offset: The offset of pagefile we're writing to
*/
bool write_to_pagefile(seL4_Word sos_vaddr, int offset);
bool read_from_pagefile(seL4_Word sos_vaddr, int offset);

#endif
