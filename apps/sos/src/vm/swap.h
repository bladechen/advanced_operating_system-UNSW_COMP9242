#ifndef _SWAP_H_
#define _SWAP_H_

#include <sel4/sel4.h>
#include "vm.h"

void init_swapping(void);

/*
*	@sos_vaddr: is the address returned by frame_alloc,
*	which in turn is the physical addr for APPs.
*	@offset: The offset of pagefile we're writing to
*/
bool write_to_pagefile(seL4_Word sos_vaddr, int offset);
bool read_from_pagefile(seL4_Word sos_vaddr, int offset);

#endif
