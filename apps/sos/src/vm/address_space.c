#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <sel4/types.h>
#include <cspace/cspace.h>
#include <mapping.h>
#include <ut_manager/ut.h>
#include <vm/vmem_layout.h>
#include <elf/elf.h>
#include <comm/list.h>

#define verbose 5
#include <sys/debug.h>
#include <sys/panic.h>

#include "address_space.h"


static int build_pagetable_link(struct pagetable* pt,  vaddr_t vaddr, int pages, seL4_ARM_VMAttributes vm, seL4_CapRights right );

struct addrspace *as_create(void)
{
    struct addrspace *as;
    as = malloc(sizeof (struct addrspace));
    if (as == NULL)
    {
        return NULL;
    }
    as->list = malloc(sizeof(struct list));
    if (as->list == NULL)
    {
        as_destroy(as);
        return NULL;
    }
    list_init(as->list);
    return as;
}

// int               as_copy(struct addrspace *src, struct addrspace **ret);
void as_destroy(struct addrspace * as)
{
    if ( as == NULL )
        return;

    struct list_head *current = NULL;
    struct list_head *tmp_head = NULL;

    list_for_each_safe(current, tmp_head, &(as->list->head))
    {
        struct as_region_metadata* tmp = list_entry(current, struct as_region_metadata, link);
        /* as_flush_region(as, tmp); XXX we may need it if we do paging(mmap)!*/
        as_destroy_region(as, tmp);
        free(tmp);
    }
    free(as->list);
    free(as);
    return;
}

static inline struct pagetable* as_get_page_table(struct addrspace* as)
{
    return as->proc->p_pagetable;
}
static struct as_region_metadata* as_create_region(void)
{
    struct as_region_metadata *temp = malloc(sizeof(*temp));
    temp->rwxflag = 0;
    temp->npages = 0;
    temp->region_vaddr = 0;
    temp->p_elfbase = NULL;
    link_init(&(temp->link));
    return temp;
}


/* static int page_index(vaddr_t addr) */
/* { */
/*     return addr >> 12; */
/* } */
static int convert_to_pages(size_t memsize)
{
    int pgsize = 0;
    if ( memsize != 0 )
        pgsize = 1 + ((memsize-1)>>seL4_PageBits);

    return pgsize;
}


static void loop_through_region(struct addrspace *as)
{
    struct list_head *current = NULL;
    struct list_head *tmp_head = NULL;

    list_for_each_safe(current, tmp_head, &(as->list->head))
    {
        struct as_region_metadata* tmp = list_entry(current, struct as_region_metadata, link);
        color_print(ANSI_COLOR_GREEN, " region : 0x%x, page: %d\n", tmp->region_vaddr, tmp->npages);
    }
}

static void as_add_region_to_list(struct addrspace *as,struct as_region_metadata *temp)
{
    list_add_tail( &(temp->link), &(as->list->head) );
    return;
}
static struct as_region_metadata* as_get_region_by_type(struct addrspace* as, int type)
{
    struct list_head *current = NULL;
    struct list_head *tmp_head = NULL;

    list_for_each_safe(current, tmp_head, &(as->list->head))
    {
        struct as_region_metadata* tmp = list_entry(current, struct as_region_metadata, link);
        if (tmp->type == type)
        {
            return tmp;
        }
    }
    return NULL;
}
int as_define_region(struct addrspace *as,
                     vaddr_t vaddr,
                     char* elf_region_start,
                     size_t elf_region_offset,
                     size_t memsz,
                     size_t filesz,
                     int readable,
                     int writeable,
                     int executable,
                     enum region_type type)
{


    struct as_region_metadata *region;
    region = as_create_region();
    if (region == NULL)
    {
        return ENOMEM;
    }
    region->elf_vaddr = vaddr;
    /* we align the start vaddr, so we should record the actuall starting addr where to load the file */
	/* Align the region. First, the base... */
	memsz += vaddr & ~(vaddr_t)  seL4_PAGE_MASK;
	vaddr &= seL4_PAGE_MASK;
    region->region_vaddr = vaddr;
    region->npages = convert_to_pages(memsz);
    region->rwxflag =  readable | writeable | executable;
    region->p_elfbase = elf_region_start;
    region->elf_offset = elf_region_offset;
    region->elf_size = filesz;
    region->type = type;

    as_add_region_to_list(as, region);

    return 0;

}

int as_define_stack(struct addrspace* as, vaddr_t* stack_pointer)
{
    *stack_pointer = APP_PROCESS_STACK_TOP;
    return as_define_region(as,
                            APP_PROCESS_STACK_BOTTOM,
                            NULL, 0,
                            APP_PROCESS_STACK_TOP - APP_PROCESS_STACK_BOTTOM,
                            APP_PROCESS_STACK_TOP - APP_PROCESS_STACK_BOTTOM,
                            PF_R, PF_W, 0, STACK);

}
int as_define_heap (struct addrspace* as)
{
    return as_define_region(as,
                            APP_PROCESS_HEAP_START,
                            NULL, 0,
                            0, 0,
                            /* APP_PROCESS_HEAP_END- APP_PROCESS_HEAP_START, */
                            /* APP_PROCESS_HEAP_END- APP_PROCESS_HEAP_START, */
                            PF_R, PF_W, 0, HEAP);

}
int as_define_ipc(struct addrspace* as)
{
    int ret = as_define_region(as,
                            APP_PROCESS_IPC_BUFFER,
                            NULL, 0,
                            1 << seL4_PageBits, 0,
                            /* APP_PROCESS_HEAP_END- APP_PROCESS_HEAP_START, */
                            /* APP_PROCESS_HEAP_END- APP_PROCESS_HEAP_START, */
                            PF_R, PF_W, 0, IPC);
    if (ret != 0)
    {
        return -1;
    }
    struct as_region_metadata* r = as_get_region_by_type(as, IPC);
    assert(r != NULL);
    return build_pagetable_link(as_get_page_table(as), APP_PROCESS_IPC_BUFFER, 1, as_region_vmattrs(r), as_region_caprights(r));
}


static int build_pagetable_link(struct pagetable* pt,  
                                vaddr_t vaddr, 
                                int pages, 
                                seL4_ARM_VMAttributes vm, 
                                seL4_CapRights right)
{

    int i = 0;
    for (i = 0; i < (int) pages; i++)
    {
        int ret = alloc_page(pt, vaddr + i * (1 << seL4_PageBits), vm, right);
        if (ret != 0)
        {
            for (int j = 0; j < i ; j++)
            {
                free_page(pt, vaddr + j * (1 << seL4_PageBits));

            }
            return -1;
        }

    }
    return 0;
}

static void as_destroy_region_pages(struct pagetable* pt,
                                    struct as_region_metadata * region,
                                    uint32_t begin_addr,
                                    uint32_t npages)
{

    assert(pt != NULL &&  region != NULL);
    vaddr_t start = region->region_vaddr;
    vaddr_t end = region->region_vaddr  + ((((int)region->npages > npages)?(int)region->npages:npages) << 12);
    /* uint32_t tlb_hi, tlb_lo; */
    /* kprintf("age: %d\n", begin, npages); */
    /* kprintf("delete start: %x, page: %d\n", begin, npages); */
    for (int i=0; i < npages; i++)
    {
        vaddr_t vaddr_del = begin_addr + i* (1 << seL4_PageBits);
        if (vaddr_del >= start && vaddr_del < end)
        {
            free_page(pt, vaddr_del);
        }
    }
    return;
}

void  as_destroy_region(struct addrspace *as, struct as_region_metadata *to_del)
{


    assert(as != NULL && to_del != NULL);
    list_del_init(&(to_del->link));
    /* if (to_del->vn != NULL) */
    /* { */
    /*     VOP_DECREF(to_del->vn); */
    /*     to_del->vn = NULL; */
    /* } */
    as_destroy_region_pages(as_get_page_table(as), to_del, to_del->region_vaddr, to_del->npages);

}
seL4_ARM_VMAttributes as_region_vmattrs(struct as_region_metadata* region)
{
    if (region->rwxflag & PF_X)
    {
        return seL4_ARM_Default_VMAttributes;
    }
    else
    {
        return seL4_ARM_Default_VMAttributes|seL4_ARM_ExecuteNever;
    }
}

seL4_CapRights as_region_caprights(struct as_region_metadata* region)
{
    seL4_CapRights right = 0;
    if (region->type == IPC)
    {
        return seL4_AllRights;
    }

    if (region->rwxflag & PF_W)
    {
        right |=  seL4_CanWrite;
    }
    if (region->rwxflag & PF_R)
    {
        right |=  seL4_CanRead;
    }
    return right;
}







static int define_region_allocate_frame_and_page(unsigned long file_size, 
    unsigned long segment_size, 
    unsigned long vaddr, 
    unsigned long offset,
    struct addrspace * dest_as,
    char * source_addr,
    enum region_type rt) 
{
    // color_print(ANSI_COLOR_GREEN, " Try to create and define CODE region\n");
    // create and define data region
    int err = as_define_region(dest_as, 
                    vaddr, 
                    source_addr, 
                    offset, 
                    segment_size, 
                    file_size, 
                    1, 2, 4, 
                    rt);
    conditional_panic(err != 0, "Fail to create and define %s Region\n", rt);

    if (rt == CODE) 
    {
        // should not exceed CODE segment vaddress range
        assert(vaddr + segment_size < APP_CODE_DATA_END);
    } else if (rt == DATA)
    {
        // should not exceed DATA segment vaddress range
        // or APP_PROCESS_HEAP_END? I take the larger for safe currently
        assert(vaddr + segment_size < APP_PROCESS_MMAP_END); 
    }

    // align the page
    vaddr_t start_address = PAGE_ALIGN(vaddr);
    // make it page align and last page may not fully filled
    int num_pages = DIVROUND(segment_size, seL4_PAGE_SIZE);  

    struct as_region_metadat * r ;
    if (rt == CODE) 
    {
        r = as_get_region_by_type(dest_as, CODE);
    } else if (rt == DATA) {
        r = as_get_region_by_type(dest_as, DATA);
    }
    
    // allocate frame and create page table mapping
    build_pagetable_link(as_get_page_table(as), 
                        start_address, 
                        num_pages, 
                        as_region_vmattrs(r), 
                        as_region_caprights(r));
}

int vm_elf_load(struct addrspace* dest_as, seL4_ARM_PageDirectory dest_vspace, char* elf_base)
{
    int err;

    /* Ensure that the ELF file looks sane. */
    if (elf_checkFile(elf_file))
    {
        return seL4_InvalidArgument;
    }

    int num_headers = elf_getNumProgramHeaders(elf_file);
    for (i = 0; i < num_headers; i++) 
    {
        char *source_addr;
        unsigned long flags, file_size, segment_size, vaddr, offset;

        /* Skip non-loadable segments (such as debugging data). */
        if (elf_getProgramHeaderType(elf_file, i) != PT_LOAD)
            continue;

        /* Fetch information about this segment. */
        offset = elf_getProgramHeaderOffset(elf_file, i);
        source_addr = elf_file + elf_getProgramHeaderOffset(elf_file, i);
        file_size = elf_getProgramHeaderFileSize(elf_file, i);
        segment_size = elf_getProgramHeaderMemorySize(elf_file, i);
        vaddr = elf_getProgramHeaderVaddr(elf_file, i);
        flags = elf_getProgramHeaderFlags(elf_file, i);
        char * name = elf_getSectionName(elf_file, i);

        // fill in corresponding region infos
        if (strcmp(name, ".text") == 0) 
        {
            color_print(ANSI_COLOR_GREEN, " Try to create and define CODE region\n");
            define_region_allocate_frame_and_page(file_size, 
                                                segment_size, 
                                                vaddr, 
                                                offset,
                                                dest_as,
                                                source_addr,
                                                CODE); 
        } else if (strcmp(name, '.rodata') == 0) 
        {
            color_print(ANSI_COLOR_GREEN, " Try to create and define DATA region\n");
            define_region_allocate_frame_and_page(file_size, 
                                                segment_size, 
                                                vaddr, 
                                                offset,
                                                dest_as,
                                                source_addr,
                                                DATA); 
        }

        /* Copy it across into the vspace. */
        dprintf(0, " * Loading segment %08x-->%08x\n", (int)vaddr, (int)(vaddr + segment_size));

        // TODO: load elf contents into memory, will write a similar function
        err = load_segment_into_vspace(dest_vspace, source_addr, segment_size, file_size, vaddr,
                                       get_sel4_rights_from_elf(flags) & seL4_AllRights);
        conditional_panic(err != 0, "Elf loading failed!\n");

        // TODO: after load the loading the data we may need to set the CODE segment as readonly
        if (strcmp(name, ".text") == 0) 
        {

        }
    }

    return 0;
}

// For temporary reference
// /*
//  * Inject data into the given vspace.
//  * TODO: Don't keep these pages mapped in
//  */
// static int load_segment_into_vspace(seL4_ARM_PageDirectory dest_as,
//                                     char *src, unsigned long segment_size,
//                                     unsigned long file_size, unsigned long dst,
//                                     unsigned long permissions) {

//     /* Overview of ELF segment loading

//        dst: destination base virtual address of the segment being loaded
//        segment_size: obvious

//        So the segment range to "load" is [dst, dst + segment_size).

//        The content to load is either zeros or the content of the ELF
//        file itself, or both.

//        The split between file content and zeros is a follows.

//        File content: [dst, dst + file_size)
//        Zeros:        [dst + file_size, dst + segment_size)

//        Note: if file_size == segment_size, there is no zero-filled region.
//        Note: if file_size == 0, the whole segment is just zero filled.

//        The code below relies on seL4's frame allocator already
//        zero-filling a newly allocated frame.

//     */
      
      
//      assert(file_size <= segment_size); 
      
//      unsigned long pos; 
//     /*  */
//     /* #<{(| We work a page at a time in the destination vspace. |)}># */
//      pos = 0; 
//      while(pos < segment_size) { 
//          seL4_Word paddr; 
//          seL4_CPtr sos_cap, tty_cap; 
//          seL4_Word vpage, kvpage; 
//          unsigned long kdst; 
//          int nbytes; 
//          int err; 
      
//          kdst   = dst + PROCESS_SCRATCH; 
//          vpage  = PAGE_ALIGN(dst); 
//          kvpage = PAGE_ALIGN(kdst); 
//     /*  */
//     /*     #<{(| First we need to create a frame |)}># */
//          paddr = ut_alloc(seL4_PageBits); 
//          conditional_panic(!paddr, "Out of memory - could not allocate frame"); 
//          err = cspace_ut_retype_addr(paddr, 
//                                      seL4_ARM_SmallPageObject, 
//                                      seL4_PageBits, 
//                                      cur_cspace, 
//                                      &tty_cap); 
//          conditional_panic(err, "Failed to retype to a frame object"); 
//     /*  */
//     /*     #<{(| Copy the frame cap as we need to map it into 2 address spaces |)}># */
//          sos_cap = cspace_copy_cap(cur_cspace, cur_cspace, tty_cap, seL4_AllRights); 
//          conditional_panic(sos_cap == 0, "Failed to copy frame cap"); 
//     /*  */
//     /*     #<{(| Map the frame into tty_test address spaces |)}># */
//          err = map_page(tty_cap, dest_as, vpage, permissions, 
//                         seL4_ARM_Default_VMAttributes); 
//          conditional_panic(err, "Failed to map to tty address space"); 
//          // #<{(| Map the frame into sos address spaces |)}># 
//          err = map_page(sos_cap, seL4_CapInitThreadPD, kvpage, seL4_AllRights, 
//                         seL4_ARM_Default_VMAttributes); 
//          conditional_panic(err, "Failed to map sos address space"); 
      
//          // #<{(| Now copy our data into the destination vspace. |)}># 
//          nbytes = PAGESIZE - (dst & PAGEMASK); 
//          if (pos < file_size){ 
//              memcpy((void*)kdst, (void*)src, MIN(nbytes, file_size - pos)); 
//          } 
      
//          #<{(| Not observable to I-cache yet so flush the frame |)}># 
//          seL4_ARM_Page_Unify_Instruction(sos_cap, 0, PAGESIZE); 
      
//          pos += nbytes; 
//          dst += nbytes; 
//          src += nbytes; 
//      } 
//     return 0;
// }


// TODO
seL4_CPtr get_IPCBufferCap_By_Addrspace(struct addrspace * as)
{
    return 0;

}
