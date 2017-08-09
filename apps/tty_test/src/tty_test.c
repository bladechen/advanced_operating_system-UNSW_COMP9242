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

#include <sel4/sel4.h>


#include "ttyout.h"

// Block a thread forever
// we do this by making an unimplemented system call.
static void
thread_block(void){
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_SetTag(tag);
    seL4_SetMR(0, 1);

    seL4_Call(SYSCALL_ENDPOINT_SLOT, tag);
}
void delay(int count)
{
    int sum;
    for (int i = 0; i < count; i ++)
        sum *= i;
    printf("%d\n", sum);
}

int main(void){
    /* initialise communication */
    ttyout_init();

    // 50000 maybe too large to udp lost packet
    /* for (int i = 0; i < 10000; i ++) */
    /* { */
    /*     printf ("helloworld"); */
    /* } */
    int a = 1;
    int *p = &a;
    do {
        printf("task:\tHello world, I'm\ttty_test!\n");
        p -= (4096 * (1<<6));
        printf("now read the stack addr %p, which should fault!!!\n", p);
        printf("%d\n", *p);
        *p = 100;
        printf("success!!\n", p);

        delay(1000000000);
        delay(1000000000);
        delay(1000000000);
        /* p -= (4096 * (1<<6)); */
        printf("now read the stack addr %p, which should fault!!!\n", p);
        *p = 100;
        printf("success!!\n", p);


        fflush(NULL);
        thread_block();
        // sleep(1);	// Implement this as a syscall
    } while(1);

    return 0;
}
