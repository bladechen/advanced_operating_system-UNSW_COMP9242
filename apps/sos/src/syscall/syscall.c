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


int sos_syscall_read(struct proc * proc, seL4_Word reply_cap, ipc_buffer_ctrl_msg * ctrl_msg)
{

}


// This function correspond to `sos_write` defined in APP scope in `sos.h`
int sos_syscall_print_to_console(struct proc * proc, seL4_Word reply_cap, ipc_buffer_ctrl_msg * ctrl_msg)
{
	if (serial_handler == NULL) {
		serial_handler = serial_init();
	}

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

int sos_syscall_write(struct proc * proc, seL4_Word reply_cap, ipc_buffer_ctrl_msg * ctrl_msg)
{
	// read control info from IPC buffer
	// and read data from shared buffer, then write corresponding
	// file/device

    return 0;
}



void handle_syscall(seL4_Word badge, struct proc * app_process) {
    seL4_Word syscall_number;
    seL4_CPtr reply_cap;

    ipc_buffer_ctrl_msg * ctrl_msg = (ipc_buffer_ctrl_msg *)malloc(sizeof(ipc_buffer_ctrl_msg));
    memcpy(ctrl_msg, seL4_GetIPCBuffer()->msg, sizeof(ipc_buffer_ctrl_msg));

    // syscall_number = seL4_GetMR(0);
    syscall_number = ctrl_msg->syscall_number;
    dprintf(0, "syscall_number: %d, offset: %d, start_addr: 0x%x\n", 
        syscall_number, ctrl_msg->offset, ctrl_msg->start_app_buffer_addr);

    /* Save the caller */
    reply_cap = cspace_save_reply_cap(cur_cspace);
    assert(reply_cap != CSPACE_NULL);

    /* Process system call */
    switch (syscall_number) {
        case SOS_SYSCALL_IPC_PRINT_COLSOLE:
            // process the syscall
            sos_syscall_print_to_console(app_process, reply_cap, ctrl_msg);

            break;

        case SOS_SYSCALL_WRITE:
            break;
        case SOS_SYSCALL_READ:
            break;
        default:
            printf("%s:%d (%s) Unknown syscall %d\n",
                       __FILE__, __LINE__, __func__, syscall_number);
            /* proc_destroy(test_process); */
            /* we don't want to reply to an unknown syscall */
    }

    /* Free the saved reply cap */
    cspace_free_slot(cur_cspace, reply_cap);
}



