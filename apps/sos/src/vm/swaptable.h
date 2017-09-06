#ifndef _SWAP_TABLE_H_
#define _SWAP_TABLE_H_


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
int next_emtpy_swap_table_entry;

struct swap_table_entry;

struct swap_table_entry
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
    *   The offset of file it writes to. Should read from that offset when swap in.
    *   This also indicate the offset of swaptable where current swap_table_entry located
    */
    int offset;
    struct swap_table_entry * next_empty;

    // Leave it to
    /*
    *   Throughout the whole process, only the app_vaddr won't change
    *   It is also what we will get when vm_fault is triggered, may move this to some
    *   data structure in frametable
    */
    // seL4_Word app_vaddr;
};

typedef struct swap_table_entry * swap_table;



inline int do_swapout_frame(sos_vaddr_t vaddr,  // frame vaddr
                     uint32_t swap_frame_number_in, // the swap id recorded in page table
                     uint32_t swap_frame_version,
                     uint32_t *swap_frame_number_out // the swap id recorded in page table
                     ){assert(0);};


// return error if swap_frame_number is invalid !
inline int do_swapin_frame(sos_vaddr_t vaddr,
                    uint32_t swap_frame_number,
                    uint32_t* swap_frame_version
                    ){assert(0);};

inline int do_free_swap_frame(uint32_t swap_frame_number){assert(0);};

#endif
