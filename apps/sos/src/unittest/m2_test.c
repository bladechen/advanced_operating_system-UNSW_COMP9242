#include "test.h"
#define verbose 5
#include "sys/debug.h"
#include "../frametable.h"

static void test_normal_alloc(void)
{
    /* Allocate 10 pages and make sure you can touch them all */
	for (int i = 0; i < 10; i++) {
	    /* Allocate a page */
	    seL4_Word vaddr = 0;
	    frame_alloc(&vaddr);
	    assert(vaddr);

	    /* Test you can touch the page */
	    *((seL4_Word *)vaddr) = 0x37;
	    assert(*((seL4_Word *)vaddr) == 0x37);

	    color_print(ANSI_COLOR_WHITE, "Page #%d allocated at 0x%x\n",  i, vaddr);
	}
}

static void test_alloc_free(void) {
    /* Test that you never run out of memory if you always free frames. */
	for (int i = 0; i < 10000; i++) {
	     /* Allocate a page */
	     seL4_Word vaddr = 0;
	     seL4_Word page = frame_alloc(&vaddr);
	     assert(vaddr != 0);

	     /* Test you can touch the page */
	     *((seL4_Word *)vaddr) = 0x37;
	     assert(*((seL4_Word *)vaddr) == 0x37);

	     /* print every 1000 iterations */
	     if (i % 1000 == 0) {
	        color_print(ANSI_COLOR_WHITE, "Page #%d allocated at 0x%x\n",  i, vaddr);
	     }

	     frame_free(page);
	}
    return;
}

static void test_multi_alloc_free(void) {
    /* Test that you never run out of memory if you always free frames. */

    seL4_Word start = 0;
	for (int i = 0; i < 5000; i++) {
	     /* Allocate a page */
	     seL4_Word vaddr = 0;
	     seL4_Word page = frame_alloc(&vaddr);
	     assert(vaddr != 0);
         if (start == 0) start = vaddr;

	     /* Test you can touch the page */
	     *((seL4_Word *)vaddr) = 0x37;
	     assert(*((seL4_Word *)vaddr) == 0x37);

	     /* print every 1000 iterations */
	     if (i % 1000 == 0) {
	        color_print(ANSI_COLOR_WHITE, "Page #%d allocated at 0x%x\n",  i, vaddr);
	     }

	     /* frame_free(page); */
	}
    for (int i = 0; i < 5000;i ++)
    {
        frame_free(start+ i * 4096);
    }
    return;
}


static void test_infinite_alloc(void)
{
    /* Test that you eventually run out of memory gracefully,
	   and doesn't crash */
	seL4_Word vaddr;
	seL4_Word last_vaddr = 0;
	while (true) {
	     /* Allocate a page */
	     frame_alloc(&vaddr);
	     if (!vaddr) {
		    color_print(ANSI_COLOR_RED, "Out of memory!\n");
		    break;
	     }

	     /* Test you can touch the page */
	     *((seL4_Word *)vaddr) = 0x37;
	     assert(*((seL4_Word *)vaddr) == 0x37);

	     last_vaddr = vaddr;
	}
	color_print(ANSI_COLOR_WHITE, "after loop to exausting the ut_mem, the vaddr: 0x%x\n", last_vaddr);
    return;
}

void m2_test(void) {

    test_normal_alloc();
    test_alloc_free();
    test_multi_alloc_free();
    test_infinite_alloc();
}
