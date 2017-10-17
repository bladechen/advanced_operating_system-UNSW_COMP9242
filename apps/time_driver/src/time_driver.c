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
#include <clock/clock.h>

#include "ttyout.h"

int start_driver(seL4_CPtr interrupt_ep);
void handle_gpt_irq();

int main(int argc, char** argv)
{
    tty_debug_print("boot time driver!\n");

    assert(!seL4_TCB_BindAEP (TIME_DRIVER_TCB_CAP, TIME_DRIVER_AEP));

    assert(!seL4_IRQHandler_SetEndpoint(TIME_DRIVER_IRQ_CAP, TIME_DRIVER_AEP));
    start_driver(TIME_DRIVER_IRQ_CAP);

    assert(!seL4_IRQHandler_Ack(TIME_DRIVER_IRQ_CAP));

    while (1)
    {
        seL4_Word badge;

        seL4_Wait(TIME_DRIVER_EP, &badge);
        handle_gpt_irq();
    }


    tty_debug_print("exit time driver!\n");
    return 0;
}
