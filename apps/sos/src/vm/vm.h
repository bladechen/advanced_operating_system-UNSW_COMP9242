#ifndef _VM_H_
#define _VM_H_

#include "comm/comm.h"
typedef uint32_t vaddr_t;
typedef uint32_t paddr_t;

#define seL4_PAGE_BITS 12
void vm_bootstrap(void);
void vm_shutdown(void);
int vm_fault(vaddr_t vaddr);

#endif
