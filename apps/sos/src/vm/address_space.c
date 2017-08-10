#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <sel4/types.h>
#include <cspace/cspace.h>
#include <mapping.h>
#include <ut_manager/ut.h>
#include <vm/vmem_layout.h>
#include <elf/elf.h>

#define verbose 5
#include <sys/debug.h>
#include <sys/panic.h>

#include "address_space.h"

/*  */
/* int elf_load(seL4_ARM_PageDirectory dest_vspace, char* elf_base) */
/* { */
/* 	 */
/* } */

seL4_CPtr get_IPCBufferCap_By_Addrspace(struct addrspace * as)
{

}
