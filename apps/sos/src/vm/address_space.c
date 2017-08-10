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

int elf_load(seL4_ARM_PageDirectory dest_vspace, struct addrspace * dest_as, char* elf_base)
{
    int err;

    /* Ensure that the ELF file looks sane. */
    if (elf_checkFile(elf_file)){
        return seL4_InvalidArgument;
    }

    int num_headers = elf_getNumProgramHeaders(elf_file);
    for (i = 0; i < num_headers; i++) {
        char *source_addr;
        unsigned long flags, file_size, segment_size, vaddr;

        /* Skip non-loadable segments (such as debugging data). */
        if (elf_getProgramHeaderType(elf_file, i) != PT_LOAD)
            continue;

        /* Fetch information about this segment. */
        source_addr = elf_file + elf_getProgramHeaderOffset(elf_file, i);
        file_size = elf_getProgramHeaderFileSize(elf_file, i);
        segment_size = elf_getProgramHeaderMemorySize(elf_file, i);
        vaddr = elf_getProgramHeaderVaddr(elf_file, i);
        flags = elf_getProgramHeaderFlags(elf_file, i);

        /* Copy it across into the vspace. */
        dprintf(1, " * Loading segment %08x-->%08x\n", (int)vaddr, (int)(vaddr + segment_size));
        err = load_segment_into_vspace(dest_as, source_addr, segment_size, file_size, vaddr,
                                       get_sel4_rights_from_elf(flags) & seL4_AllRights);
        conditional_panic(err != 0, "Elf loading failed!\n");
    }

    return 0;
}


int elf_load(seL4_ARM_PageDirectory dest_as, char *elf_file) 
{
    int num_headers;
    int err;
    int i;

    /* Ensure that the ELF file looks sane. */
    if (elf_checkFile(elf_file)){
        return seL4_InvalidArgument;
    }

    num_headers = elf_getNumProgramHeaders(elf_file);
    for (i = 0; i < num_headers; i++) {
        char *source_addr;
        unsigned long flags, file_size, segment_size, vaddr;

        /* Skip non-loadable segments (such as debugging data). */
        if (elf_getProgramHeaderType(elf_file, i) != PT_LOAD)
            continue;

        /* Fetch information about this segment. */
        source_addr = elf_file + elf_getProgramHeaderOffset(elf_file, i);
        file_size = elf_getProgramHeaderFileSize(elf_file, i);
        segment_size = elf_getProgramHeaderMemorySize(elf_file, i);
        vaddr = elf_getProgramHeaderVaddr(elf_file, i);
        flags = elf_getProgramHeaderFlags(elf_file, i);

        /* Copy it across into the vspace. */
        dprintf(1, " * Loading segment %08x-->%08x\n", (int)vaddr, (int)(vaddr + segment_size));
        err = load_segment_into_vspace(dest_as, source_addr, segment_size, file_size, vaddr,
                                       get_sel4_rights_from_elf(flags) & seL4_AllRights);
        conditional_panic(err != 0, "Elf loading failed!\n");
    }

    return 0;
}

seL4_CPtr get_IPCBufferCap_By_Addrspace(struct addrspace * as) 
{

}
