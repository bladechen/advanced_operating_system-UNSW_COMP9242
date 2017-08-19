
#include "comm/list.h"
#include "comm/comm.h"
#include "file.h"
#include "vfs/uio.h"
#include "vfs/vfs.h"
#include "vfs/vnode.h"
static struct files_table g_ftb;

void init_kern_file_table(void)
{

    list_init(&g_ftb.list_obj);
    return;
}
void destroy_kern_file_table(void)
{
    // the list should be empty, after all the process' fd table finishing cleaning
    assert(is_list_empty(&(g_ftb.list_obj)) == 1);
    destroy_list(&(g_ftb.list_obj));

    return;
}

static int get_file_stat(struct file* node)
{
    assert(node != NULL);
    int result = 0;

    result = VOP_STAT(node->v_ptr, &(node->f_stat));
    if ( result != 0)
    {
        return -result;
    }
    if (node->f_flags& O_APPEND)
    {
        node->f_pos = node->f_stat.st_size;
    }
    return 0;
}


static void __destroy_kern_file(struct file* fs)
{
    assert(fs != NULL);
    assert(fs->ref_count == 0);
    assert(is_linked(&(fs->link_obj)) == 0);

    if (fs->v_ptr != NULL)
    {
        /*
         * need flush file, but ... sys161
         */
        vfs_close(fs->v_ptr);
        fs->v_ptr = NULL;
    }
    free (fs);

    return;
}


int close_kern_file(struct file* fs)
{
    assert(fs != NULL);

    /*
     * when ref is >= 1, so do nothing but dec ref by 1
     */
    // using atomic can avoid race condition happened in vnode_decref/emufs_reclaim

    if (fs->ref_count != 1)
    {
        return 0;
    }
    else
    {

        /* struct files_table* ftb = fs->owner; */
        assert(fs->owner != NULL);

        assert(fs->ref_count == 0);
        link_detach(fs, link_obj);
        fs->owner = NULL;

        struct vnode* v_tmp = fs->v_ptr;
        fs->v_ptr = NULL;
        __destroy_kern_file(fs);

        if (v_tmp != NULL)
        {
            vfs_close(v_tmp);
        }

    }
    return 0;

}
static int __init_kern_file(struct file** retval, struct vnode* v, struct fs* f, int flags, mode_t mode)
{

    (void)f;
    struct file *node = malloc(sizeof(struct file));
    if (node == NULL)
    {
        return -ENOMEM;
    }

    (void) mode;
    assert(node != NULL);
    link_init(&node->link_obj);
    node->ref_count = 1;
    node->v_ptr = v;
    node->f_flags = flags;
    node->f_pos = 0;
    node->owner = &g_ftb;
    *retval = node;
    return 0;
}
static int __do_stdio_open(struct file** f, int fd)
{
    char con[10] = "console";
    struct file* tmp;
    *f = NULL;
    int flags = (fd == 0? O_RDONLY: O_WRONLY);
    struct vnode* v;

    int ret = 0;
    ret = vfs_open(con, flags, 0, &v);
    if (ret != 0)
    {
        ERROR_DEBUG("vfs_open: std[%d] failed\n", fd);
        return -ret ;
    }

    ret = __init_kern_file(&tmp, v, NULL, flags, 0);
    if (ret != 0)
    {

        ERROR_DEBUG("vfs_open: std[%d] failed, vm not enough\n", fd);
        vfs_close(v);
        return ret;
    }

    list_add_tail(&(tmp->link_obj),&(g_ftb.list_obj.head));

    *f = tmp;
    return 0;
}
int do_flip_open(struct file ** fp, int dfd, char* filename,int flags, mode_t mode )
{
    if (dfd == STDIN_FILENO || dfd == STDOUT_FILENO || dfd  == STDERR_FILENO)
    {
        return __do_stdio_open(fp, dfd);

    }
    struct vnode *v = NULL;
    struct file* node;

    int ret = vfs_open(filename, flags, mode, &v);
    if (ret != 0)
    {
        ERROR_DEBUG("vfs_open open file: [%s], flags: [%d], mode: [%d], failed: %d",
                 filename, flags, mode, ret);
        return -ret;
    }
    /* ERROR_DEBUG("%p\n", v); */
    assert(v != NULL);
    ret = __init_kern_file(&node, v, NULL, flags, mode);
    if (ret != 0)
    {
        ERROR_DEBUG("vfs_open open file: [%s], flags: [%d], mode: [%d], failed at init_file: %d\n",
                 filename, flags, mode, ret);
        vfs_close(v);
        return ret;
    }

    ret = get_file_stat(node);
    if (ret != 0)
    {
        __destroy_kern_file(node);
        return ret ;
    }


    list_add_tail(&(node->link_obj),&(g_ftb.list_obj.head));

    *fp = node;

    return 0;
}

void inc_ref_file(struct file* f)
{
    ++ f->ref_count;
    /* mb_atomic_inc_int(&(f->ref_count)); */
}

/* static int __do_file_seek(struct file* f, off_t target_pos) */
/* { */
/*     #<{(| */
/*      * emufs did not have seek api */
/*      |)}># */
/*     f->f_pos = target_pos; */
/*     return 0; */
/* } */
/* static bool __is_seekable(struct file * f) */
/* { */
/*     return VOP_ISSEEKABLE(f->v_ptr); */
/* } */
/*  */
/* off_t kern_file_seek(struct file* f,  off_t pos, int whence) */
/* { */
/*     int ret = 0; */
/*     assert(f != NULL); */
/*     off_t cur_pos = f->f_pos; */
/*     assert(cur_pos >= 0); */
/*     if (__is_seekable(f) == 0) */
/*     { */
/*         return -ESPIPE; */
/*     } */
/*  */
/*     if (whence == SEEK_SET) */
/*     { */
/*         if (pos < 0) */
/*         { */
/*             ret = -EINVAL; */
/*             goto end_seek; */
/*         } */
/*         else */
/*         { */
/*             ret = __do_file_seek(f, pos); */
/*             goto end_seek; */
/*         } */
/*  */
/*     } */
/*     else if (whence == SEEK_END) */
/*     { */
/*         ret = get_file_stat(f); */
/*         if (ret != 0) */
/*         { */
/*             #<{(| ret = -ret; |)}># */
/*             goto end_seek; */
/*         } */
/*         if (f->f_stat.st_size + pos < 0) */
/*         { */
/*             ret = -EINVAL; */
/*             goto end_seek; */
/*         } */
/*  */
/*         ret = __do_file_seek(f, f->f_stat.st_size + pos); */
/*         goto end_seek; */
/*  */
/*     } */
/*     else if (whence == SEEK_CUR) */
/*     { */
/*         if (f->f_pos + pos < 0) */
/*         { */
/*             ret = -EINVAL; */
/*             goto end_seek; */
/*         } */
/*         ret =  __do_file_seek(f, f->f_pos + pos); */
/*         goto end_seek; */
/*  */
/*     } */
/*     else */
/*     { */
/*         ret = -EINVAL; */
/*         goto end_seek; */
/*     } */
/* end_seek: */
/*  */
/*     if (ret < 0) */
/*     { */
/*         return ret; */
/*     } */
/*     return f->f_pos; */
/* } */

int kern_file_read(struct file* f, char* buf, size_t buf_size, size_t* read_len)
{
    if (((f->f_flags & 3) != O_RDONLY) && ((f->f_flags & 3) != O_RDWR))
    {
        return -EBADF;
    }
    int ret = 0;
    struct iovec iov;
    struct uio u;

    size_t old = f->f_pos;
    uio_kinit(&iov, &u, buf, buf_size, f->f_pos, UIO_READ);
    ret = VOP_READ(f->v_ptr, &u);
    if (ret != 0)
    {

        return -ret;
    }
    f->f_pos = u.uio_offset;
    *read_len = f->f_pos - old;


    return 0;
}

int kern_file_write(struct file* f, const void * buf, size_t buf_size, size_t * read_len)
{
    if (((f->f_flags & 3) != O_WRONLY) && ((f->f_flags & 3) != O_RDWR))
    {
        return -EBADF;
    }
    int ret = 0;
    struct iovec iov;
    struct uio u;

    if (f->f_flags & O_APPEND)
    {
        ret = get_file_stat(f);
        if ( ret != 0)
        {

            return ret;
        }

    }
    size_t old = f->f_pos;
    uio_kinit(&iov, &u,(void *) buf, buf_size, f->f_pos, UIO_WRITE);
    ret = VOP_WRITE(f->v_ptr, &u);
    if (ret != 0)
    {
        return -ret;
    }
    f->f_pos = u.uio_offset;
    *read_len = f->f_pos - old;

    return 0;

}
