#include "test.h"
#define verbose 5
#include "sys/debug.h"
#include "../frametable.h"

void m2_test(void) {
	/* Allocate 10 pages and make sure you can touch them all */
	for (int i = 0; i < 10; i++) {
	    /* Allocate a page */
	    seL4_Word vaddr;
	    frame_alloc(&vaddr);
	    assert(vaddr);

	    /* Test you can touch the page */
	    *((seL4_Word *)vaddr) = 0x37;
	    assert(*((seL4_Word *)vaddr) == 0x37);

	    dprintf(0, "Page #%d allocated at %x\n",  i, vaddr);
	}

	/* Test that you never run out of memory if you always free frames. */
	for (int i = 0; i < 50000; i++) {
	     /* Allocate a page */
	     seL4_Word vaddr;
	     seL4_Word page = frame_alloc(&vaddr);
	     assert(vaddr != 0);

	     /* Test you can touch the page */
	     *((seL4_Word *)vaddr) = 0x37;
	     assert(*((seL4_Word *)vaddr) == 0x37);

	     /* print every 1000 iterations */
	     if (i % 1000 == 0) {
	        dprintf(0, "Page #%d allocated at %x\n",  i, vaddr);
	     }

	     frame_free(page);
	}

	/* Test that you eventually run out of memory gracefully,
	   and doesn't crash */
	seL4_Word vaddr;
	seL4_Word last_vaddr;
	while (true) {
	     /* Allocate a page */
	     frame_alloc(&vaddr);
	     if (!vaddr) {
		    dprintf(0, "Out of memory!\n");
		    break;
	     }

	     /* Test you can touch the page */
	     *((seL4_Word *)vaddr) = 0x37;
	     assert(*((seL4_Word *)vaddr) == 0x37);

	     last_vaddr = vaddr;
	}
	dprintf(0, "after loop to exausting the ut_mem, the vaddr: %x\n", last_vaddr);
}