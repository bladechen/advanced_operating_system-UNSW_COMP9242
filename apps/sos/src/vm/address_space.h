#ifndef _ADDRESS_SPACE_H_
#define _ADDRESS_SPACE_H_
#include "elf/elf.h"
#include "comm/comm.h"
#include "comm/list.h"
#include "vmem_layout.h"
#include "vm.h"
#include "proc/proc.h"


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
    IPC_SHARED_BUFFER
    OTHER,
};

struct as_region_metadata
{

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
    vaddr_t elf_vaddr; // the vaddr specified by the elf file, which is the starting vaddr to load the elf binary
    size_t elf_offset; // the start loading elf file offset, corresponding to  elf_vaddr
    size_t elf_size; // actual loaded content from file
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
    char* elf_base; // will be set in elf_load(), corresponding value is the elf_base passed into
                    // the elf_load() function, and for now it is at least useful for proc_activate()
    struct proc* proc;
};



struct addrspace *as_create(void);
// int               as_copy(struct addrspace *src, struct addrspace **ret);// XXX need in fork
void              as_destroy(struct addrspace *);

int               as_define_region(struct addrspace *as,
                                   vaddr_t vaddr,
                                   char* elf_base,
                                   size_t elf_region_offset,
                                   size_t memsz,
                                   size_t filesz,
                                   int readable,
                                   int writeable,
                                   int executable,
                                   enum region_type type);

int               as_define_stack(struct addrspace* as, vaddr_t* stack_pointer);
int               as_define_heap (struct addrspace* as);
int               as_define_ipc  (struct addrspace* as);
int               as_define_mmap (struct addrspace* as); // TODO

struct as_region_metadata* as_get_region(struct addrspace* as, vaddr_t vaddr);



void as_destroy_region(struct addrspace *as, struct as_region_metadata *to_del);

/*
 * Functions in loadelf.c
 *    load_elf - load an ELF user program executable into the current
 *               address space. Returns the entry point (initial PC)
 *               in the space pointed to by ENTRYPOINT.
 */


// it is called in proc_create() to initialize the program
int vm_elf_load(struct addrspace* as, seL4_ARM_PageDirectory dest_vspace, char* elf_file);

// used in TCB configure proc.c
seL4_CPtr as_get_ipc_cap(struct addrspace * as);

/*
 *   Functions used in VM_Fault execution in main.c, which load or create corresponding frame
 *   when VM_Fault is triggered
 */
int as_handle_zerofilled_fault(struct pagetable* pt, struct as_region_metadata * region, vaddr_t fault_addr);

int as_handle_elfload_fault(struct pagetable* pt, struct as_region_metadata* as, vaddr_t fault_addr);

void loop_through_region(struct addrspace *as);
#endif
