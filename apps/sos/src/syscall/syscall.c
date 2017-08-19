#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <cspace/cspace.h>
#include <serial/serial.h>
#include <clock/clock.h>
#include "comm/comm.h"
#include "vm/vmem_layout.h"
#include "handle_syscall.h"

#define verbose 5
#include <sys/debug.h>
#include <sys/panic.h>
#include "vm/frametable.h"
#include "vm/address_space.h"
#include "proc/proc.h"
#include "syscall.h"
#include "vm/pagetable.h"
#include <sos.h>

// used to replace the long switch case in `handle_syscall`
#define NUMBER_OF_SYSCALL   7

syscall_func syscall_func_arr[NUMBER_OF_SYSCALL] = {
    {.syscall=&sos_syscall_print_to_console, .will_block=false},
    {.syscall=&sos_syscall_read, .will_block=true},
    {.syscall=&sos_syscall_write, .will_block=false},
    {.syscall=&sos_syscall_open, .will_block=false},
    {.syscall=&sos_syscall_usleep, .will_block=true},
    {.syscall=&sos_syscall_time_stamp, .will_block=false},
    {.syscall=&sos_syscall_brk, .will_block=false}};

extern timestamp_t g_cur_timestamp_us;
struct serial * serial_handler = NULL;

/*
*   In M4, assume read from/write to console device
*/

int sos_syscall_read(struct proc * proc)
{
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "sos_syscall_read\n");
    /* dprintf(0, "[SOS] syscall read\n"); */

    // This nbyte won't be larger lan
    // int nbyte = proc->p_ipc_ctrl.offset;
    // int file = proc->p_ipc_ctrl.file_id;

    // seL4_Word start_app_addr = proc->p_ipc_ctrl.start_app_buffer_addr;
    // seL4_Word start_sos_addr = page_phys_addr(proc->p_pagetable, start_app_addr);

    // int read_len = 0;

    // while(read_len < nbyte) {
    //     // console read 1 char at a time, so advance by 1 every loop
    //     read_len++;
    restart_coro(proc->p_coro, handle_block_read, NULL);
    // }

    return 0;
}

int sos_syscall_open(struct proc * proc)
{
    return 0;
}

int sos_syscall_time_stamp(struct proc * proc)
{
    timestamp_t now = g_cur_timestamp_us;
    memcpy(get_ipc_buffer(proc), &now, 8);
    struct ipc_buffer_ctrl_msg ctrl;
    ctrl.ret_val = 0;
    ctrl.offset = 0;
    ipc_reply(&ctrl, &(proc->p_reply_cap));
    return 0;
}

// This function correspond to `sos_write` defined in APP scope in `sos.h`
int sos_syscall_print_to_console(struct proc * proc)
{
	if (serial_handler == NULL) {
		serial_handler = serial_init();
	}

	// seL4_Word start_app_addr = seL4_GetMR(1);

    seL4_Word start_sos_addr = (seL4_Word)get_ipc_buffer(proc);

    // int offset = seL4_GetMR(2);
    int offset = proc->p_ipc_ctrl.offset;


    int ret = serial_send(serial_handler, (char *)start_sos_addr, offset);

    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_YELLOW,
        "[sos] serial send len: %d \n",ret);
    struct ipc_buffer_ctrl_msg ctrl;
    ctrl.offset = 0;
    ctrl.ret_val = ret;
    ipc_reply(&ctrl, &(proc->p_reply_cap));
    return 0;
}

int sos_syscall_write(struct proc * proc)
{
	// read control info from IPC buffer
	// and read data from shared buffer, then write corresponding
	// file/device
    //TODO fixme
    sos_syscall_print_to_console(proc);
    return 0;
}


int sos_syscall_usleep(struct proc* proc)
{
    int msecond = *((int*)(get_ipc_buffer(proc)));
    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "proc %d, get sleep %d\n", proc->p_pid, msecond);
    restart_coro(proc->p_coro, handle_block_sleep, (void*)(msecond));
    return 0;
}

int sos_syscall_brk(struct proc * proc)
{
    seL4_Word newbrk = *((int*)(get_ipc_buffer(proc)));

    COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "proc %d, newbrk: 0x%x\n", proc->p_pid, newbrk);
    /* frame_alloc(NULL); */
    /* COLOR_DEBUG(DB_SYSCALL, ANSI_COLOR_GREEN, "enter as_get_heap_brk\n"); */


    struct addrspace *as = proc->p_addrspace;
    seL4_Word retbrk = 0;

    struct ipc_buffer_ctrl_msg ctrl;
    int ret = as_get_heap_brk(as, newbrk, &retbrk);
    ctrl.ret_val = ret;
    ctrl.offset = 4;
    if (ret == 0)
    {
        memcpy(get_ipc_buffer(proc), &retbrk, 4);
    }

    ipc_reply(&ctrl, &(proc->p_reply_cap));
    return 0;
}

void handle_syscall(seL4_Word badge, struct proc * app_process)
{
    seL4_Word syscall_number;
    seL4_CPtr reply_cap;

    struct ipc_buffer_ctrl_msg * ctrl_msg = &(app_process->p_ipc_ctrl);
    memcpy(ctrl_msg, seL4_GetIPCBuffer()->msg, sizeof(ipc_buffer_ctrl_msg));

    syscall_number = ctrl_msg->syscall_number;
    dprintf(0, "syscall_number: %d, offset: %d, start_addr: 0x%x\n",
        syscall_number, ctrl_msg->offset, ctrl_msg->start_app_buffer_addr);

    /* Save the caller */
    reply_cap = cspace_save_reply_cap(cur_cspace);
    assert(reply_cap != CSPACE_NULL);

    // in case the app process block, the reply_cap and message get flushed
    // we put these into `proc struct`
    app_process->p_reply_cap = reply_cap;

    if (syscall_number < 0 || syscall_number > NUMBER_OF_SYSCALL) {
        printf("%s:%d (%s) Unknown syscall %d\n",
                       __FILE__, __LINE__, __func__, syscall_number);
        assert("unknown syscall number!\n");
    }

    /* Invoke corresponding syscall */
    (*syscall_func_arr[syscall_number].syscall)(app_process);

    // If the syscall won't block we will process the reply_cap revoke here
    if (syscall_func_arr[syscall_number].will_block == false) {
        destroy_reply_cap(&(app_process->p_reply_cap));
    }

}



