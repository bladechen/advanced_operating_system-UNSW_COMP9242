/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/****************************************************************************
 *
 *      $Id:  $
 *
 *      Description: Simple milestone 0 test.
 *
 *      Author:			Godfrey van der Linden
 *      Original Author:	Ben Leslie
 *
 ****************************************************************************/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <errno.h>
#include <sys/mman.h>
#include <stdio.h>

#include <sel4/sel4.h>
#include <sos.h>


#include "ttyout.h"

// Block a thread forever
// we do this by making an unimplemented system call.

#include <utils/page.h>

#define NPAGES 27
#define TEST_ADDRESS 0x20000000

/* called from pt_test */

int tty_debug_print(const char *fmt, ...);
static char buff[27 * 4096 * 2];
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

static void
pt_test( void )
{
    /* need a decent sized stack */
    char buf1[NPAGES * PAGE_SIZE_4K], *buf2 = NULL;

    /* check the stack is above phys mem */
    assert((void *) buf1 > (void *) TEST_ADDRESS);

    /* stack test */
    do_pt_test(buf1);
    /* tty_debug_print("begion malloc\n"); */

    /* heap test */
    buf2 = malloc(NPAGES * PAGE_SIZE_4K);
    /* tty_debug_print("malloc 0x%x\n", buf2); */
    assert(buf2);
    do_pt_test(buf2);
    free(buf2);
}

#define SHARED_PAGES 10
void test_share(char* ar)
{
    int* addr = (int*)(0x12345000);
    if (ar[0] == 'w')
    {
        int ret = sos_share_vm(addr, SHARED_PAGES * 4096, 1);
        sleep(10);

        assert(ret == 0);


        for (int i = 0; i < SHARED_PAGES * 4096 / 4; ++ i)
        {
            addr[i] = i;
            assert(addr[i] == i);
            /* printf ("writing %d to %p %d\n", i, addr + i, addr[i]); */
        }
    }
    else
    {
        int ret = sos_share_vm(addr, SHARED_PAGES * 4096, 0);
        sleep(10);
        assert(ret == 0);

        // TODO try to write
        for (int i = 0; i < SHARED_PAGES * 4096 / 4; ++ i)
        {
            /* printf ("%p %d %d\n", addr + i, addr[i], i); */
            assert(addr[i] == i);
            /* addr[i] = i + 1; */
        }
    }
}

void test_multi_alloc_share()
{
    int* addr = (int*)(0x32345000);
    int i = 0;
    while (1)
    {
        int ret = sos_share_vm((void*)((size_t)(addr) + i), SHARED_PAGES * 4096, 1);
        if (ret != 0)
        {
            break;
        }
        i += SHARED_PAGES * 4096;
    }
    printf ("alloc: %d\n", i );


}


void *my_mmap(void *start, size_t len, int prot, int flags, int fd, off_t off)
{
	void *ret;
    printf ("start: %p, len: %u\n", start, len);
	if (sizeof(off_t) > sizeof(long))
		if (((long)off & 0xfff) | ((long)((unsigned long long)off>>(12 + 8*(sizeof(off_t)-sizeof(long))))))
			start = (void *)-1;
	ret = syscall(192, start, len, prot, flags, fd, off>>12);
	return ret;
}

void test_readonly_mmap()
{
    void* tmp = NULL;
    void* addr = (void*)my_mmap(0, 4096, PROT_READ, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    assert(addr != (void*)(-1));
    *(int*)addr = 4;
    assert(0);
}

void test_mmap()
{
    void* tmp = NULL;
    void* addr = (void*)my_mmap(-1, 4096, PROT_READ, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    assert(addr != (void*)(-1));
    /* assert(0); */
}



#define LARGE_SIZE (8*1024*1024 / 4)

static int large_DATA_arr[LARGE_SIZE];
static int queue[300000][2] = {0};
static char hash[LARGE_SIZE] = {0};
int len = 0;

void thrash_test()
{
    sleep(20);
    len = 0;
    memset(hash, 0, sizeof(hash));
    int large_STACK_arr[LARGE_SIZE] ;

    /* int * large_HEAP_arr = (int *)malloc(LARGE_SIZE * sizeof(int)); */
    /* assert(large_HEAP_arr != NULL); */

    printf("begin thrash_test\n");
    for(int i = 0; i < LARGE_SIZE; i++)
    {
        large_STACK_arr[i] = i;
        large_DATA_arr[i] = i;
        /* *(large_HEAP_arr + i) = i; */
    }

    printf("thrash_test cmp stage1\n");
    int test_loops = LARGE_SIZE;

    for(int i = 0; i < test_loops; i++)
    {
        if (large_STACK_arr[i] != i)
            printf ("%d %d %p %p\n", large_STACK_arr[i], i, large_STACK_arr + i, &i);
        assert(large_STACK_arr[i] == i);
        assert(large_DATA_arr[i] == i);
    }
    sleep(5);

    int j,k;

    printf("thrash_test random\n");
    srand(sos_sys_time_stamp());
    test_loops = 1000;
    for(int i = 0; i < test_loops; i++)
    {
        j = rand() % LARGE_SIZE;
        while (hash[j] == 1)
        {
            j = rand() % LARGE_SIZE;
        }
        hash[j] = 1;
        k = rand() ;
        queue[len][0] = j;
        queue[len][1] = k;
        printf ("%d %d %d\n",i, j, k);
        len ++;

        large_STACK_arr[j] = k;
        assert(large_STACK_arr[j] == k);

        large_DATA_arr[j] = k;
        assert(large_DATA_arr[j] == k);

    }

    printf("thrash_test cmp stage2\n");
    for (int i = 0; i < len ; ++ i)
    {
        if (large_STACK_arr[queue[i][0]] != queue[i][1])
            printf ("%d %d %d %p %p\n", large_STACK_arr[queue[i][0]], queue[i][1],  i, large_STACK_arr + queue[i][0], &i);

        assert(large_DATA_arr[queue[i][0]] == queue[i][1]);
        assert(large_STACK_arr[queue[i][0]] == queue[i][1]);
    }

}

int main(int argc, char** argv){
    /* initialise communication */
    /* int * p = (int*)(0x20001000U); */
    /* *p = 10000; */

    printf("task:\tHello world, I'm\ttty_test! argc: %d %p %p\n", argc, &argc);
    /* do_pt_test(buff); */
    if (argc < 2)
    {
        printf ("my_app [sleep_second]\n");
        return -1;
    }


    test_mmap();
    /* test_multi_alloc_share(); */
    /* thrash_test(); */
    test_share(argv[1]);
    /* pt_test(); */
    /* tty_debug_print("finish pt_test\n"); */

    /* sos_process_create(); */
    int i = 0;
    printf("bye: %d\n", sos_my_id());
    return 0;
}
