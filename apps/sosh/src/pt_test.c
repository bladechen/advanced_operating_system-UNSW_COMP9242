#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <ttyout.h>
#include <time.h>
#include <sys/time.h>
#include <utils/time.h>

/* Your OS header file */
#include <sos.h>

#define NPAGES 27
#define TEST_ADDRESS 0x20000000
#define PAGE_SIZE_4K 4096

/* called from pt_test */
static void
do_pt_test(char *buf)
{

    /* set */
    for (int i = 0; i < NPAGES; i++) {
	    buf[i * PAGE_SIZE_4K] = i;
    }

    /* check */
    for (int i = 0; i < NPAGES; i++) {
	    assert(buf[i * PAGE_SIZE_4K] == i);
    }
}

int
pt_test( void )
{
    /* need a decent sized stack */
    char buf1[NPAGES * PAGE_SIZE_4K], *buf2 = NULL;

    /* check the stack is above phys mem */
    assert((void *) buf1 > (void *) TEST_ADDRESS);

    /* stack test */
    do_pt_test(buf1);

    /* heap test */
    buf2 = malloc(NPAGES * PAGE_SIZE_4K);
    assert(buf2);
    do_pt_test(buf2);
    free(buf2);
    return 0;
}
