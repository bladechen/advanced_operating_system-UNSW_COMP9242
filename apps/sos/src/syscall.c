#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <cspace/cspace.h>

#include <serial/serial.h>
#include <clock/clock.h>

#include "comm/comm.h"

#include "vm/vmem_layout.h"

#define verbose 5
#include <sys/debug.h>
#include <sys/panic.h>

#include "vm/frametable.h"
#include "vm/address_space.h"
#include "proc/proc.h"
#include "syscall.h"
#include <sos.h>

static struct serial * serial_handler = NULL;

int sos_syscall_print_to_console(struct proc * proc, seL4_Word reply_cap)
{
	if (serial_handler == NULL) {
		serial_handler = serial_init();
	}

	COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_YELLOW, "[sos] large buffer msg recieved from tty, in syscall.c..\n");

	seL4_Word start_app_addr = seL4_GetMR(1);

    seL4_Word start_sos_addr = page_phys_addr(proc->p_pagetable, start_app_addr);

    int offset = seL4_GetMR(2);

    int ret = serial_send(serial_handler, (char *)start_sos_addr, offset);

    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_YELLOW, 
        "[sos] large buffer ipc msg serial_send finish, in syscall.c, len: %d \n",ret);

    seL4_MessageInfo_t reply = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_SetMR(0, ret); // actually sent length
    seL4_Send(reply_cap, reply);
    return 0;
}

int sos_syscall_write(struct proc * proc, seL4_Word reply_cap)
{
	// read control info from IPC buffer
	
}



