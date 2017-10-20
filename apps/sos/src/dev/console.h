#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include "vfs/vfs.h"
#include "comm/comm.h"
#include "coroutine/synch.h"

#include <serial/serial.h>
#define MAXIMUM_SERIAL_READ_BUFFER 4096
#define MAXIMUM_SERIAL_WRITE_BUFFER 4096
void (*console_read_cb)(void * argv);

// int serial_register_handler(struct serial *serial, void (*handler)((struct serial *) serial, char c));
struct serial_console
{
    struct serial* _serial_handler;

    char _read_buffer[MAXIMUM_SERIAL_READ_BUFFER];
    uint32_t _read_buf_head;
    volatile uint32_t _read_buf_size; // how many char in the buffer


    char _write_buffer[MAXIMUM_SERIAL_WRITE_BUFFER];

    // uint32_t _write_buf_size

    //
    struct semaphore* _read_sem;
    bool              _read_exclusive_flag;

    struct coroutine* _coro; // only for debug purpose
    // bool              _wait_read;


    // serial_read_handler _handler;
};
void init_console(void);
void destroy_console(void);

// When a program opens the file console it should access the console on the serial device. The console is a multiple writer, single reader device,
// i.e., more than one process can concurrently open the device for writing, but only one process can open the device for reading.
void giveup_console_read(void); //XXX ugly way called after close. it should be handled via vfs calling device ops.
#endif
