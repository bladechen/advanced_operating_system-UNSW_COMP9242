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
#include "vm/pagetable.h"
#include <sos.h>

static struct serial * serial_handler = NULL;


int sos_syscall_read(struct proc * proc, seL4_Word reply_cap)
{

}


// This function correspond to `sos_write` defined in APP scope in `sos.h`
int sos_syscall_print_to_console(struct proc * proc, seL4_Word reply_cap)
{
	if (serial_handler == NULL) {
		serial_handler = serial_init();
	}

    ipc_buffer_ctrl_msg * ctrl_msg = (ipc_buffer_ctrl_msg *)malloc(sizeof(ipc_buffer_ctrl_msg));

    memcpy(ctrl_msg, seL4_GetIPCBuffer()->msg, sizeof(ipc_buffer_ctrl_msg));


	// seL4_Word start_app_addr = seL4_GetMR(1);
    seL4_Word start_app_addr = ctrl_msg->start_app_buffer_addr;

    dprintf(0, "start_app_addr: 0x%x\n", start_app_addr);

    seL4_Word start_sos_addr = page_phys_addr(proc->p_pagetable, start_app_addr);

    // int offset = seL4_GetMR(2);
    int offset = ctrl_msg->offset;

    dprintf(0, "offset %d, reply_cap: %d, start_sos_addr: 0x%x, serial_handler: 0x%x\n", 
        offset, reply_cap, start_sos_addr, serial_handler);

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
	// and read data from shared buffer, then write corresponding
	// file/device

    return 0;
}



