#ifndef _ADDRESS_SPACE_H_
#define _ADDRESS_SPACE_H_
#include "comm.h"
#include "list.h"
#include "vmem_layout.h"
#include "vm.h"


/*
 * Address space - data structure associated with the virtual memory
 * space of a process.
 */
enum region_type
{
    CODE,
    DATA,
    STACK,
    HEAP,
    IPC,
    OTHER,
};

enum region_permission
{
    PF_X,
    PF_W,
    PF_R,
};



struct as_region_metadata
{
    // TODO add a cap field

    vaddr_t region_vaddr;
    size_t  npages; //maximum region support pages, 4K

    // bit 0 is X  PF_X
    // bit 1 is W  PF_W
    // bit 2 is R  PF_R
    char rwxflag;

    enum region_type type;
    // Advanced part for demand loading
    // struct vnode *region_vnode;

    char* p_elfbase; // for further fault handler load code/data section into page/frame table
    // Link to the next data struct
    struct list_head link;
};

struct addrspace
{
    /* Put stuff here for your VM system */
    // Linked list of as_region_metadatas
    // struct as_region_metadata *list;
    struct list *list;
    // char is_loading;
};


struct addrspace *as_create(void);
// int               as_copy(struct addrspace *src, struct addrspace **ret);
void              as_destroy(struct addrspace *);

int               as_define_region(struct addrspace *as,
                                   vaddr_t vaddr, size_t memsz, size_t filesz,
                                   int readable,
                                   int writeable,
                                   int executable);

int               as_define_stack(struct addrspace* as, vaddr_t* stack_pointer);
int               as_define_heap (struct addrspace* as);
int               as_define_ipc  (struct addrspace* as);
int               as_define_mmap (struct addrspace* as); // TODO


// Additions
void              as_destroy_region(struct addrspace *as, struct as_region_metadata *to_del);

seL4_ARM_VMAttributes as_region_vmattrs(struct as_region_metadata* region);
seL4_CapRights        as_region_caprights(struct as_region_metadata* region);

/*
 * Functions in loadelf.c
 *    load_elf - load an ELF user program executable into the current
 *               address space. Returns the entry point (initial PC)
 *               in the space pointed to by ENTRYPOINT.
 */


// int elf_load(struct vnode *v, vaddr_t *entrypoint);
int elf_load(seL4_ARM_PageDirectory dest_as, char* elf_base);

// used in TCB configure
seL4_CPtr get_IPCBufferCap_By_Addrspace(struct addrspace * as);


#endif
