#include "test.h"
#define verbose 5
#include "sys/debug.h"
#include "vm/frametable.h"

#define ALLOC kframe_alloc
#define FREE  kframe_free

static void test_normal_alloc(void)
{
    /* Allocate 10 pages and make sure you can touch them all */
	for (int i = 0; i < 10; i++) {
	    /* Allocate a page */
	    seL4_Word vaddr = ALLOC();
	    /* frame_alloc(&vaddr); */
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
	     seL4_Word vaddr = ALLOC();

	     /* Test you can touch the page */
	     *((seL4_Word *)vaddr) = 0x37;
	     assert(*((seL4_Word *)vaddr) == 0x37);

	     /* print every 1000 iterations */
	     if (i % 1000 == 0) {
	        color_print(ANSI_COLOR_WHITE, "Page #%d allocated at 0x%x\n",  i, vaddr);
	     }

	     FREE(vaddr);
	}
    return;
}

static seL4_Word arr[1000000];
static int len = 0;
static void test_multi_alloc_free(void) {
    /* Test that you never run out of memory if you always free frames. */


    len = 0;
    seL4_Word tmp= 0;
	for (int i = 0; i < 5000000; i++) {
	     /* Allocate a page */
	     seL4_Word vaddr = ALLOC();
         if (vaddr == 0)
         {
             color_print(ANSI_COLOR_WHITE, "run out of mem, total pages: %d, 0x%08x\n", i, tmp);
             break;
         }
         arr[len ++] = vaddr;
         tmp = vaddr;


	     /* Test you can touch the page */
	     *((seL4_Word *)vaddr) = 0x3123912;
	     assert(*((seL4_Word *)vaddr) == 0x3123912);

	     /* print every 1000 iterations */
	     if (i % 1000 == 0) {
	        color_print(ANSI_COLOR_WHITE, "Page #%d allocated at 0x%x\n",  i, vaddr);
	     }

	}
    for (int i = 0; i < len;i ++)
    {
        FREE(arr[i]);
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
	     vaddr = ALLOC();
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

    printf ("begin m2_test\n");
    test_multi_alloc_free();
    test_normal_alloc();
    test_alloc_free();
    test_multi_alloc_free();
    test_infinite_alloc();
    test_multi_alloc_free();
    printf ("end m2_test\n");
}
