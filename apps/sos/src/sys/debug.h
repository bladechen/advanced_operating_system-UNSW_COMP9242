/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdio.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_WHITE    "\x1b[37m"
#define ANSI_COLOR_RESET   "\x1b[0m"

void plogf(const char *msg, ...);

#define _dprintf(v, col, args...) \
            do { \
                if ((v) < verbose){ \
                    printf(col); \
                    plogf(args); \
                    printf("\033[0;0m"); \
                } \
            } while (0)

#define _force_printf(col, args...)\
    do {\
        printf (col);\
        plogf(args);\
        printf (ANSI_COLOR_RESET);\
    }while (0)

#define color_print(col, ...) _dprintf(0, col, __VA_ARGS__)

#define dprintf(v, ...) _dprintf(v, "\033[22;33m", __VA_ARGS__)

#define WARN(...) _dprintf(-1, "\033[1;31mWARNING: ", __VA_ARGS__)

#define NOT_IMPLEMENTED() printf("\033[22;34m %s:%d -> %s not implemented\n\033[;0m",\
                                  __FILE__, __LINE__, __func__);
#define DB_LOCORE      0x0001
#define DB_SYSCALL     0x0002
#define DB_INTERRUPT   0x0004
#define DB_DEVICE      0x0008
#define DB_THREADS     0x0010
#define DB_VM          0x0020
#define DB_EXEC        0x0040
#define DB_VFS         0x0080
#define DB_SEMFS       0x0100
#define DB_SFS         0x0200
#define DB_NET         0x0400
#define DB_NETFS       0x0800
#define DB_KMALLOC     0x1000

#define DB_TEST        0x2000 // only for temp debug purpose.

extern unsigned int dbflags;
/*
 * DEBUG() is for conditionally printing debug messages to the console.
 *
 * copied from os161, in every sos submodule,
 *  use it by COLOR_DEBUG(DB_VM, ANSI_xxx, "");
 *
 * throughout the kernel; then you can toggle whether these messages
 * are printed or not at runtime by setting the value of dbflags with
 * the debugger.

 * the debugger.
 *
 * Unfortunately, as of this writing, there are only a very few such
 * messages actually present in the system yet. Feel free to add more.
 *
 * DEBUG is a varargs macro. These were added to the language in C99.
 */
//((d & (dbflags)) ? _force_printf(col, __VA_ARGS__) : 0)
#define COLOR_DEBUG(d, col, ...) \
    if (d & dbflags)\
    {\
         _force_printf(col, __VA_ARGS__);\
    }

#define ERROR_DEBUG(...) _force_printf(ANSI_COLOR_RED, __VA_ARGS__)

#endif /* _DEBUG_H_ */
