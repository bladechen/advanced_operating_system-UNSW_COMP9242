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

#define verbose -100
#include <sys/debug.h>
#include <sys/panic.h>

#include "address_space.h"
#include "comm/comm.h"
#include "frametable.h"

const char REGION_NAME[10][10] = {"CODE", "DATA", "STACK", "HEAP", "IPC", "IPC_SHARED_BUFFER", "OTHER"};
static void dump_region(struct as_region_metadata* region);
static int build_pagetable_link(struct pagetable* pt,  vaddr_t vaddr, int pages, seL4_ARM_VMAttributes vm, seL4_CapRights right );

static seL4_ARM_VMAttributes as_region_vmattrs(struct as_region_metadata* region);
static seL4_CapRights        as_region_caprights(struct as_region_metadata* region);



struct addrspace *as_create(void)
{
    struct addrspace *as;
    as = malloc(sizeof (struct addrspace));
    if (as == NULL)
    {
        return NULL;
    }
    as->elf_base = NULL;
    as->list = NULL;

    as->list = malloc(sizeof(struct list));
    if (as->list == NULL)
    {
        as_destroy(as);
        return NULL;
    }
    list_init(as->list);
    return as;
}

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
    assert(temp != NULL);
    temp->rwxflag = 0;
    temp->type = 100000; // invalid value
    temp->npages = 0;
    temp->region_vaddr = 0;
    temp->p_elfbase = NULL;
    temp->elf_vaddr = 0;
    temp->elf_offset = 0;
    temp->elf_size = 0;
    link_init(&(temp->link));
    return temp;
}

static int convert_to_pages(size_t memsize)
{
    int pgsize = 0;
    if ( memsize != 0 )
        pgsize = 1 + ((memsize-1)>>seL4_PageBits);

    return pgsize;
}

void loop_through_region(struct addrspace *as)
{
    struct list_head *current = NULL;
    struct list_head *tmp_head = NULL;

    list_for_each_safe(current, tmp_head, &(as->list->head))
    {
        struct as_region_metadata* tmp = list_entry(current, struct as_region_metadata, link);
        // dump_region(tmp);
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
                     char* elf_base,
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
    region->elf_vaddr = vaddr; // maybe not 4k aligned
    /* we align the start vaddr, so we should record the actuall starting addr where to load the file */
	/* Align the region. First, the base... */
	memsz += vaddr & (~(vaddr_t)seL4_PAGE_MASK); // floor
	vaddr &= seL4_PAGE_MASK;
    region->region_vaddr = vaddr;
    region->npages = convert_to_pages(memsz); //ceiling
    region->rwxflag =  readable | writeable | executable;
    region->p_elfbase = elf_base;
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
                            APP_PROCESS_STACK_TOP - APP_PROCESS_STACK_BOTTOM, 0,
                            /* APP_PROCESS_STACK_TOP - APP_PROCESS_STACK_BOTTOM, */
                            PF_R, PF_W, 0, STACK);

}
int as_define_heap (struct addrspace* as)
{
    return as_define_region(as,
                            APP_PROCESS_HEAP_START,
                            NULL, 0,
                            /* 0, 0, */
                            APP_PROCESS_HEAP_END - APP_PROCESS_HEAP_START, 0,
                            /* APP_PROCESS_HEAP_END- APP_PROCESS_HEAP_START, */
                            PF_R, PF_W, 0, HEAP);

}
int as_define_ipc(struct addrspace* as)
{
    int ret = as_define_region(as,
                            APP_PROCESS_IPC_BUFFER,
                            NULL, 0,
                            1 << seL4_PageBits, 0,
                            PF_R, PF_W, 0, IPC);
    if (ret != 0)
    {
        return ret;
    }
    struct as_region_metadata* r = as_get_region_by_type(as, IPC);
    assert(r != NULL);
    return build_pagetable_link(as_get_page_table(as), APP_PROCESS_IPC_BUFFER, 1, as_region_vmattrs(r), as_region_caprights(r));
}

int as_define_ipc_shared_buffer(struct addrspace * as)
{
    int ret = as_define_region(as,
                                APP_PROCESS_IPC_SHARED_BUFFER,
                                NULL, 0,
                                4 << seL4_PageBits, 0,
                                PF_R, PF_W, 0, IPC_SHARED_BUFFER);
    if (ret != 0)
    {
        return ret;
    }
    struct as_region_metadata* r = as_get_region_by_type(as, IPC_SHARED_BUFFER);
    assert(r != NULL);
    return build_pagetable_link(as_get_page_table(as), 
        APP_PROCESS_IPC_SHARED_BUFFER, 1, as_region_vmattrs(r), as_region_caprights(r));
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
            return ret;
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
    vaddr_t end = region->region_vaddr  + ((region->npages) << 12);
    for (int i=0; i < npages; i++)
    {
        vaddr_t vaddr_del = begin_addr + i * (1 << seL4_PageBits);
        if (vaddr_del >= start && vaddr_del < end)
        {
            free_page(pt, vaddr_del);
        }
        else
        {
            ERROR_DEBUG("invalid as_destroy_region_pages\n");
        }
    }
    return;
}

void  as_destroy_region(struct addrspace *as, struct as_region_metadata *to_del)
{
    assert(as != NULL && to_del != NULL);
    list_del_init(&(to_del->link));
    as_destroy_region_pages(as_get_page_table(as), to_del, to_del->region_vaddr, to_del->npages);

}

static seL4_ARM_VMAttributes as_region_vmattrs(struct as_region_metadata* region)
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

static seL4_CapRights as_region_caprights(struct as_region_metadata* region)
{
    seL4_CapRights right = 0;
    if (region->type == IPC)
    {
        return seL4_AllRights;
    }

    if (region->rwxflag & PF_W)
    {
        right |= seL4_CanWrite;
    }
    if (region->rwxflag & PF_R)
    {
        right |= seL4_CanRead;
    }
    return right;
}

/*
*   It's an auxilary function for load file contents into memory when calling `elf_load`
*   It's not used now, we can keep it for future usage.
*/
/* static int define_region_allocate_frame_and_page(unsigned long file_size, */
/*     unsigned long segment_size, */
/*     unsigned long vaddr, */
/*     unsigned long offset, */
/*     struct addrspace * dest_as, */
/*     char * source_addr, */
/*     enum region_type rt) */
/* { */
/*     // COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, " Try to create and define CODE region\n"); */
/*     // create and define data region */
/*     int err = as_define_region(dest_as, */
/*                     vaddr, */
/*                     source_addr, */
/*                     offset, */
/*                     segment_size, */
/*                     file_size, */
/*                     1, 2, 4, */
/*                     rt); */
/*     conditional_panic(err != 0, "Fail to create and define Region\n"); */
/*  */
/*     if (rt == CODE) */
/*     { */
/*         // should not exceed CODE segment vaddress range */
/*         assert(vaddr + segment_size < APP_CODE_DATA_END); */
/*     } else if (rt == DATA) */
/*     { */
/*         // should not exceed DATA segment vaddress range */
/*         // or APP_PROCESS_HEAP_END? I take the larger for safe currently */
/*         assert(vaddr + segment_size < APP_PROCESS_MMAP_END); */
/*     } */
/*  */
/*     // align the page */
/*     vaddr_t start_address = IS_PAGE_ALIGNED(vaddr); */
/*     // make it page align and last page may not fully filled */
/*     int num_pages = DIVROUND(segment_size, seL4_PAGE_SIZE); */
/*  */
/*     struct as_region_metadata * r ; */
/*     if (rt == CODE) */
/*     { */
/*         r = as_get_region_by_type(dest_as, CODE); */
/*     } else if (rt == DATA) { */
/*         r = as_get_region_by_type(dest_as, DATA); */
/*     } */
/*  */
/*     // allocate frame and create page table mapping */
/*     build_pagetable_link(as_get_page_table(dest_as), */
/*                         start_address, */
/*                         num_pages, */
/*                         as_region_vmattrs(r), */
/*                         as_region_caprights(r)); */
/*  */
/*     return 0; */
/* } */
/*  */
/*
*   Load elf info from .elf file, set up address space but does not load contents into memory.
*/
int vm_elf_load(struct addrspace* dest_as, seL4_ARM_PageDirectory dest_vspace, char* elf_file)
{
    /* Ensure that the ELF file looks sane. */
    if (elf_checkFile(elf_file))
    {
        return EINVAL;
    }
    dest_as->elf_base = elf_file;

    int num_headers = elf_getNumProgramHeaders(elf_file);
    int i;
    for (i = 0; i < num_headers; i++)
    {

        /* Skip non-loadable segments (such as debugging data). */
        if (elf_getProgramHeaderType(elf_file, i) != PT_LOAD)
            continue;

        /* Fetch information about this segment. */
        uint32_t off = elf_getProgramHeaderOffset(elf_file, i);
        /* source_addr = elf_file + elf_getProgramHeaderOffset(elf_file, i); */
        uint32_t file_size = elf_getProgramHeaderFileSize(elf_file, i);
        uint32_t segment_size = elf_getProgramHeaderMemorySize(elf_file, i);
        uint32_t vaddr = elf_getProgramHeaderVaddr(elf_file, i);
        uint32_t flags = elf_getProgramHeaderFlags(elf_file, i);
        char * name = elf_getSectionName(elf_file, i);


        // fill in corresponding region infos
        if (strcmp(name, ".text") == 0)
        {
            COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, " Try to create and define CODE region offset from elf at %u\n", off);
            int err = as_define_region(dest_as,
                    vaddr,
                    elf_file,
                    off,
                    segment_size,
                    file_size,
                    flags & PF_R, flags & PF_W, flags &PF_X,
                    CODE);
            conditional_panic(err != 0, "Fail to create and define CODE Region\n");
        }
        else if (strcmp(name, ".rodata") == 0)
        {

            COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, " Try to create and define DATA region offset from elf at %u\n", off);
            int err = as_define_region(dest_as,
                    vaddr,
                    elf_file,
                    off,
                    segment_size,
                    file_size,
                    flags & PF_R, flags & PF_W, flags & PF_X,
                    DATA);
            conditional_panic(err != 0, "Fail to create and define DATA Region\n");
        }
        else
        {
            ERROR_DEBUG( "the section %s need load, but not handled\n", name);
        }

        dprintf(0, " * Loading segment %08x-->%08x\n", (int)vaddr, (int)(vaddr + segment_size));
    }

    return 0;
}

// static void dump_region(struct as_region_metadata* region)
// {
//     COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, "region type: %s\n", REGION_NAME[(region->type)]);
//     COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, "* vaddr start at: 0x%x with pages: %u, elf_base: 0x%x, filesz: %u, file_offset: %u, elf vaddr:0x%x, permission: %x\n",
//                 region->region_vaddr,
//                 region->npages,
//                 region->p_elfbase,
//                 region->elf_size,
//                 region->elf_offset,
//                 region->elf_vaddr,
//                 (int)(region->rwxflag));
//     return;
// }

int as_handle_elfload_fault(struct pagetable* pt, struct as_region_metadata* r, vaddr_t fault_addr)
{
    assert(pt != NULL && r != NULL &&r->p_elfbase != NULL);
    assert(!(fault_addr &(~seL4_PAGE_MASK)));
    assert(r->npages * seL4_PAGE_SIZE >= r->elf_size);
    // COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, "current process 0x%x fault addr: 0x%x\n", get_current_app_proc(), fault_addr);
    const struct as_region_metadata region = *r;

    uint32_t file_copy_addr = 0;
    uint32_t file_copy_bytes = 0;
    uint32_t vm_copied_addr_offset = 0;
    uint32_t zero_gap = region.elf_vaddr - region.region_vaddr;
    /* dump_region(r); */

    if (fault_addr + seL4_PAGE_SIZE <= region.elf_vaddr)
    {
        /* COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, "case 1\n"); */
        // nothing
    }
    else if (fault_addr >= region.elf_vaddr + region.elf_size)
    {
        /* COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, "case 2\n"); */
        // nothing
    }
    else if (fault_addr <= region.elf_vaddr &&
        fault_addr + seL4_PAGE_SIZE >= region.elf_vaddr && 
        fault_addr + seL4_PAGE_SIZE <= region.elf_vaddr + region.elf_size)
    {
        file_copy_addr = region.elf_offset;
        vm_copied_addr_offset =  region.elf_vaddr - fault_addr;
        file_copy_bytes = seL4_PAGE_SIZE  - vm_copied_addr_offset;
        /* COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, "case 3 %u\n", file_copy_bytes); */
    }
    else if (fault_addr >= region.elf_vaddr && 
        fault_addr + seL4_PAGE_SIZE <= region.elf_vaddr + region.elf_size)
    {
        /* COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, "case 4\n"); */
        file_copy_bytes = seL4_PAGE_SIZE;
        vm_copied_addr_offset = 0;
        file_copy_addr = region.elf_offset + (fault_addr - region.region_vaddr -zero_gap);

    }
    else if (fault_addr <= region.elf_vaddr && 
        fault_addr + seL4_PAGE_SIZE >= region.elf_vaddr + region.elf_size)
    {
        file_copy_bytes = region.elf_size;
        file_copy_addr = region.elf_offset;
        vm_copied_addr_offset = region.elf_vaddr - fault_addr;
    }
    else if (fault_addr >= region.elf_vaddr && 
        fault_addr <= region.elf_vaddr + region.elf_size && 
        fault_addr +seL4_PAGE_SIZE >= region.elf_vaddr + region.elf_size )
    {
        vm_copied_addr_offset = 0;
        file_copy_bytes = (region.elf_vaddr + region.elf_size) - fault_addr;
        file_copy_addr = region.elf_offset + (fault_addr - region.region_vaddr -zero_gap);
    }
    else
    {
        // something we forget to handle.
        assert(0);
    }

    int ret = as_handle_zerofilled_fault(pt, r, fault_addr);
    if (ret != 0)
    {
        return ret;
    }
    paddr_t paddr = page_phys_addr(pt, fault_addr);
    assert(paddr != 0);
    // calculate the copy region

    /* COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, "vm off: %u, file addr: %u, bytes: %d\n", vm_copied_addr_offset, file_copy_addr,file_copy_bytes ); */

    file_copy_addr += (uint32_t) r->p_elfbase;
    COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, "alloc vaddr: 0x%x at paddr: 0x%x\n", fault_addr, paddr);
    COLOR_DEBUG(DB_VM, ANSI_COLOR_GREEN, "* copy file at 0x%x to physical mem 0x%x with %d bytes\n", file_copy_addr, paddr + vm_copied_addr_offset, file_copy_bytes);
    if (file_copy_bytes)
    {
        memcpy((void*)(paddr + vm_copied_addr_offset), (void*)file_copy_addr, file_copy_bytes);
    }
    flush_sos_frame(paddr);
    return 0;
}

int as_handle_zerofilled_fault(struct pagetable* pt, struct as_region_metadata * region, vaddr_t fault_addr)
{

    assert(pt != NULL && region != NULL);
    assert((fault_addr &(~seL4_PAGE_MASK)) == 0);
    int ret = alloc_page(pt,
                         fault_addr,
                         as_region_vmattrs(region),
                         as_region_caprights(region));

    return ret;
}

seL4_CPtr as_get_ipc_cap(struct addrspace * as)
{
    return fetch_page_cap(as->proc->p_pagetable, APP_PROCESS_IPC_BUFFER);
}

struct as_region_metadata* as_get_region(struct addrspace* as, vaddr_t vaddr)
{
    assert (as != NULL);
    assert (as->list != NULL);
    assert (!(vaddr & (~seL4_PAGE_MASK)));
    struct as_region_metadata* cur = NULL;
    struct list_head* head = &(as->list->head);
    list_for_each_entry(cur, head, link)
    {
        if (cur->region_vaddr <=  vaddr && (cur->region_vaddr + seL4_PAGE_SIZE * cur->npages) > vaddr)
        {
            return cur;
        }
    }
    return NULL;
}

