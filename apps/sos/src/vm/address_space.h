#ifndef _ADDRESS_SPACE_H_
#define _ADDRESS_SPACE_H_
#include "elf/elf.h"
#include "comm/comm.h"
#include "comm/list.h"
#include "vmem_layout.h"
#include "vm.h"
#include "pagetable.h"


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
    IPC_SHARED_BUFFER,
    OTHER,
};

enum fault_type
{
    FAULT_READ = 1,
    FAULT_WRITE = 2,
    FAULT_WRITE_ON_READONLY = 4,
};

static inline int sel4_fault_code_to_fault_type(int code)
{
    // refer this for detail
    // http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.100511_0401_10_en/ric1447333676062.html
    // [11]
    //     WnR
    //     Not read and write.
    //     Indicates what type of access caused the abort:
    //     0 = read
    //     1 = write.
    //
    //The following encodings are in priority order, highest first:
    //1. 0b000001 alignment fault
    //2. 0b000100 instruction cache maintenance fault
    //3. 0bx01100 1st level translation, synchronous external abort
    //4. 0bx01110 2nd level translation, synchronous external abort
    //5. 0b000101 translation fault, section
    //6. 0b000111 translation fault, page
    //7. 0b000011 access flag fault, section
    //8. 0b000110 access flag fault, page
    //9. 0b001001 domain fault, section
    //10. 0b001011 domain fault, page
    //11. 0b001101 permission fault, section
    //12. 0b001111 permission fault, page
    //13. 0bx01000 synchronous external abort, nontranslation
    //14. 0bx10110 asynchronous external abort
    //15. 0b000010 debug event.
    //Any unused encoding not listed is reserved.
    //
    // XXX we need handle section and page both!!
    // int ret = 0;
    if (code == 2055 || code == 2053)
    {
        return FAULT_WRITE;
    }
    else if (code == 2063 || code == 2061)
    {
        return FAULT_WRITE_ON_READONLY;
    }
    else if (code ==  7 || code == 5)
    {
        return FAULT_READ;
    }
    ERROR_DEBUG("need handle fault: %d\n", code);
    assert(0);
    return -1;

}

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
    struct pagetable* pt;
};



struct addrspace *as_create(struct pagetable* pt);
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
int               as_define_ipc_shared_buffer(struct addrspace * as);
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
int as_handle_page_fault(struct pagetable* pt, struct as_region_metadata * region, vaddr_t fault_addr, int fault_type);

int as_handle_elfload_fault(struct pagetable* pt, struct as_region_metadata* as, vaddr_t fault_addr, int fault_type);

void loop_through_region(struct addrspace *as);


int as_get_heap_brk(struct addrspace* as, uint32_t brk_in, uint32_t* brk_out);

#endif
