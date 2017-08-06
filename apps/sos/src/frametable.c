#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <sel4/types.h>
#include <cspace/cspace.h>
#include <mapping.h>
#include <ut_manager/ut.h>
#include <vmem_layout.h>

#define verbose 5
#include <sys/debug.h>
#include <sys/panic.h>

#include "frametable.h"

static frame_table _frame_table;
static seL4_Word _ut_hi;
static seL4_Word _ut_lo;

/* function declarations */
static int allocate_frame_table(seL4_Word frame_table_start_vaddr, uint32_t frame_table_size);


/* function definitions */

/* 1-to-1 mapping for physical address to virtual address
 * virtual address to frame_table array index is also 1-to-1 mapping*/

// return corresponding physical address
seL4_Word frame_translate_vaddr_to_paddr(seL4_Word vaddr) {
    return (vaddr - WINDOW_START + _ut_lo);
}
// return corresponding virtual address
seL4_Word frame_translate_paddr_to_vaddr(seL4_Word paddr) {
    return (paddr - _ut_lo + WINDOW_START);
}

seL4_Word frame_translate_vaddr_to_index(seL4_Word vaddr) {
	return (PAGE_SHIFT(vaddr - WINDOW_START));
}

seL4_Word frame_translate_index_to_vaddr(uint32_t index) {
	return (PAGE_UNSHIFT(index + WINDOW_START));
}

/* Initialize frame table */
void frametable_init(void) {
	int err;
    uint32_t frame_table_size;
    uint32_t frames_to_be_managed;
	seL4_Word frame_table_start_vaddr;

    // Die on double init
    conditional_panic((_frame_table != NULL), "Frame table has already been initialised");

    // initialize ut_lo and ut_hi
    ut_find_memory(&_ut_lo, &_ut_hi);

    /* dynamically calculate the size of frame_table */
    frames_to_be_managed = DIVROUND((_ut_hi - _ut_lo), seL4_PAGE_SIZE);
    frame_table_size = frames_to_be_managed * sizeof(frame_table_entry);
    // do this instead of directly WINDOW_START is to keep the address aligned
    // otherwise it may execeed the WINDOW_START when doing allocation
    frame_table_start_vaddr = 
    	WINDOW_START - DIVROUND(frame_table_size, seL4_PAGE_SIZE) * seL4_PAGE_SIZE;

    err = allocate_frame_table(frame_table_start_vaddr, frame_table_size);
    conditional_panic(err, "Failed to allocate frame table");

    // after allocation, we need to update _ut_lo, since part of ut_mem have
    // been allocated as frame table
    // ut_find_memory(&_ut_lo, &_ut_hi);
    // _ut_lo += DIVROUND(frame_table_size, seL4_PAGE_SIZE) * seL4_PAGE_SIZE;

    assert(frame_table_start_vaddr > DMA_VEND);

    _frame_table = (frame_table) frame_table_start_vaddr;


    dprintf(0, "After initialization: ut_lo:%x ut_high:%x\n", _ut_lo, _ut_hi);
    dprintf(0, "virtual address of frame_table: %x\n", _frame_table);
}

/* allocate frame_table */
static int allocate_frame_table(seL4_Word frame_table_start_vaddr, uint32_t frame_table_size) {
	int i;
    seL4_Word cur_vaddr = frame_table_start_vaddr;
    seL4_Word num_frames = DIVROUND(frame_table_size, seL4_PAGE_SIZE);

    for(i = 0; i < num_frames; i++) {
        seL4_Word paddr = ut_alloc(seL4_PageBits);
        // dprintf(0, "allocated frame paddr:%x\n", paddr);
        if (paddr) {
            int err;
		    seL4_Word temp_cap; 
		    err = cspace_ut_retype_addr(paddr, 
		    	seL4_ARM_SmallPageObject, 
		    	seL4_PageBits,
		        cur_cspace, 
		        &temp_cap);
		    conditional_panic(err, "Failed to retype frame");

		    err = map_page(temp_cap, 
		    	seL4_CapInitThreadPD, 
		    	cur_vaddr, 
		    	seL4_AllRights,
		        seL4_ARM_Default_VMAttributes);
		    conditional_panic(err, "Failed to map page of frame into SOS");

            cur_vaddr += seL4_PAGE_SIZE;
        } else {
        	return 1;
        }
    }

    dprintf(0, "allocated %d pages in range\n", i);

    return 0;
}

/* allocate a frame */
seL4_Word frame_alloc(seL4_Word * vaddr_ptr) {
	seL4_Word paddr = ut_alloc(seL4_PageBits);

	// dprintf(0, "in frame_alloc, paddr get from ut_alloc: %x\n", paddr);

	if (paddr == 0) {
		*vaddr_ptr = (seL4_Word)NULL;
		return (seL4_Word)NULL;
	} else {
		seL4_Word vaddr = frame_translate_paddr_to_vaddr(paddr);

		int err;
	    seL4_Word temp_cap; 
	    err = cspace_ut_retype_addr(paddr, 
	    	seL4_ARM_SmallPageObject, 
	    	seL4_PageBits,
	        cur_cspace, 
	        &temp_cap);
	    conditional_panic(err, "Failed to retype frame");

	    err = map_page(temp_cap, 
	    	seL4_CapInitThreadPD, 
	    	vaddr, 
	    	seL4_AllRights,
	        seL4_ARM_Default_VMAttributes);
	    conditional_panic(err, "Failed to map page of frame into SOS");

	    uint32_t index = frame_translate_vaddr_to_index(vaddr);

	    (_frame_table + index)->page_cap = temp_cap;
	    (_frame_table + index)->vaddr = vaddr;

	    *vaddr_ptr = vaddr;
	    return vaddr;
	}
}

void frame_free(seL4_Word vaddr){
	int err;
    uint32_t index;
    seL4_Word paddr;
    frame_table_entry *fte;

    // Die if uninitialised
    conditional_panic((_frame_table == NULL), "Frame table uninitialised");

    paddr = frame_translate_vaddr_to_paddr(vaddr);
    index = frame_translate_vaddr_to_index(vaddr);
    fte = _frame_table + index;

    // unmap from our window
    err = seL4_ARM_Page_Unmap(fte->page_cap);
    conditional_panic(err, "Failed to unmap page from SOS window\n");

    // Delete SmallPageObject cap from SOS cspace
    err = cspace_delete_cap(cur_cspace, fte->page_cap);
    conditional_panic(err, "Failed to delete SmallPageObject cap from SOS cspace\n");

    // return the frame to ut
    ut_free(paddr, seL4_PageBits);

    // clear the table entry
    fte->page_cap = NULL;
    fte->vaddr = NULL;
}
