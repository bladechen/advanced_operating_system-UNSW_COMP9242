// #ifndef _SWAP_H_
// #define _SWAP_H_

// #include <sel4/sel4.h>
// #include "vm.h"
// #include "swaptable.h"

/* it also the size of `pagefile`*/
// #define PAGEFILE_SIZE (1024*1024*1024)
// #define SWAPTABLE_ENTRY_AMOUNT (PAGEFILE_SIZE / seL4_PAGE_SIZE)


/*
*   @sos_vaddr, the address going to copy frame from
*   @app_vaddr, the app_vaddr given by the app
*
*   @return return the swap_table offset or the pointer to the swap_table_entry, so that
*   we can retrieve infos we need when swap in.
*/
// int swap_out(seL4_Word sos_vaddr);

/*
*   @sos_vaddr, the address going to copy frame to
*   @offset, used to find the corresponding swap_table_entry
*/
// int swap_in(int offset, seL4_Word sos_vaddr);


// void init_swapping(void);

/*
*   @sos_vaddr: is the address returned by frame_alloc,
*   which in turn is the physical addr for APPs.
*   @offset: The offset of pagefile we're writing to
*/
// bool write_to_pagefile(seL4_Word sos_vaddr, int offset);
// bool read_from_pagefile(seL4_Word sos_vaddr, int offset);

// #endif
