#include "proc.h"
#include "vfs/vfs.h"
#include "vfs/uio.h"

static int _open_elf_file(char* file_name, struct vnode** ret)
{
    // TODO , maybe firstly we should check the file has 'x' permission
    struct vnode* tmp = NULL;
    *ret = NULL;
    int err = 0;
    err = vfs_open(file_name, O_RDONLY, O_RDONLY, &tmp);
    if (err != 0)
    {
        ERROR_DEBUG("open elf file failed: %d\n", err);
        return err;
    }
    *ret = tmp;
    return 0;
}


bool proc_load_elf(struct proc * process, const char* file_name)
{
    char file[1024] = "nfs:";
    if (strlen(file_name) >= 1000)
    {
        ERROR_DEBUG("load elf can not support too long elf file\n", strlen(file_name));
        return false;
    }
    strcpy(file + 4, file_name);
    assert(get_proc_status(process) == PROC_STATUS_INIT);
    struct vnode* elf_file_vnode = NULL;

    int ret = _open_elf_file(file, &elf_file_vnode);
    if (ret != 0)
    {
        return false;
    }
    assert(elf_file_vnode != NULL);
    char elf_header[seL4_PAGE_SIZE]; // coroutine stack size is at leat 2 pages!
    struct iovec iov;
    struct uio u;
    uio_kinit(&iov, &u, (void*)elf_header, seL4_PAGE_SIZE, 0, UIO_READ);
    ret = VOP_READ(elf_file_vnode, &u);
    if (ret != 0)
    {
        ERROR_DEBUG("read from elf file failed: %d\n", ret);
        vfs_close(elf_file_vnode);
        return false;
    }
    printf ("read elf header: %d\n", seL4_PAGE_SIZE - u.uio_resid);
    /* unsigned long elf_size = ret; */
    /* char * elf_base = elf_header; */
    /* if (elf_base == NULL) */
    /* { */
    /*     ERROR_DEBUG("cpio_get_file NULL\n"); */
    /*     return false; */
    /* } */
    /* conditional_panic(!elf_base, "Unable to locate cpio header"); */
    COLOR_DEBUG(DB_THREADS, ANSI_COLOR_GREEN, "elf_base: 0x%x, entry point: 0x%x   %s\n", (unsigned int)elf_header, (unsigned int)elf_getEntryPoint(elf_header), file);

    /*** load the elf image info, set up addrspace ***/
    // DATA and CODE region is set up by `vm_elf_load`
    //  in parent coroutine!
    int err = vm_elf_load(process->p_resource.p_addrspace, process->p_resource.p_pagetable->vroot.cap, elf_header, elf_file_vnode);
    if (err != 0)
    {
        vfs_close(elf_file_vnode);
        return false;
    }
    vfs_close(elf_file_vnode);
    return true;
}

