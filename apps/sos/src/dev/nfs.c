// The SOS file system features a flat directory structure.
// one level directory!!!!
#include "nfs.h"
#include "vfs/vfs.h"
#include "vfs/stat.h"
#include "vfs/uio.h"
#include "clock/clock.h"
#include "coroutine/synch.h"


static struct nfs_fs _nfs_handler;

static
int
_nfs_close(const struct fhandle* handle);


static int _nfs_loadvnode(struct nfs_fs *ef,
                          const struct fhandle* fh ,
                          int isdir,
                          struct  nfs_vnode **ret);


static void
_nfs_creat_cb(uintptr_t token, enum nfs_stat stat, fhandle_t *fh, fattr_t *fattr);


static inline int _rpc_stat2sos_err(enum rpc_stat stat)
{
    if (stat == RPC_OK)
    {
        return 0;
    }
    else if (stat == RPCERR_NOMEM)
    {
        return ENOMEM;
    }
    else if (stat == RPCERR_NOBUF)
    {
        return EAGAIN;
    }
    else if (stat == RPCERR_COMM)
    {
        return ECOMM;
    }
    else if (stat == RPCERR_NOSUP)
    {
        return EACCES;
    }
    assert(0);
    return -1;
}

static inline int _nfs_stat2sos_err(enum nfs_stat stat)
{
    if (stat == NFS_OK)
    {
        return 0;
    }
    else if (stat == NFSERR_PERM
        || stat == NFSERR_ACCES)
    {
        return EPERM;
    }
    else if (stat == NFSERR_NOENT
             )
    {
        return ENOENT;
    }
    else if (stat == NFSERR_IO ||
             stat == NFSERR_WFLUSH ||
             stat == NFSERR_COMM)
    {
        return EIO;
    }
    else if (stat == NFSERR_NXIO ||
             stat == NFSERR_NODEV)
    {
        return ENODEV;
    }
    else if (stat == NFSERR_EXIST)
    {
        return EEXIST;
    }
    else if (stat == NFSERR_NOTDIR)
    {
        return ENOTDIR;
    }
    else if(stat == NFSERR_ISDIR)
    {
        return EISDIR;
    }
    else if (stat == NFSERR_FBIG)
    {
        return EFBIG;
    }
    else if (stat == NFSERR_NOSPC)
    {
        return ENOSPC;
    }
    else if (stat == NFSERR_NAMETOOLONG)
    {
        return ENAMETOOLONG;
    }
    else if (stat == NFSERR_ROFS)
    {
        return EROFS;
    }
    else if (stat == NFSERR_NOTEMPTY)
    {
        return ENOTEMPTY;
    }
    else if (stat == NFSERR_DQUOT)
    {
        return EDQUOT;
    }
    else if (stat == NFSERR_STALE)
    {
        return EINVAL;
    }
    assert(0);
    return -1;
}

static int _nfs_waitdone(struct nfs_cb_arg* arg)
{
    P(arg->sem);
    return arg->stat;
}

static inline void _dump_nfs_handler(const struct fhandle* a)
{
    for (int i = 0; i < FHSIZE; ++ i)
    {
        COLOR_DEBUG(DB_DEVICE, ANSI_COLOR_GREEN, "%02x", a->data[i]);
    }
    COLOR_DEBUG(DB_DEVICE, ANSI_COLOR_GREEN, "\n");
}

static inline bool _is_same_handler(const struct fhandle* a,
                                    const struct fhandle* b)
{

    for (int i = 0; i < FHSIZE; ++ i)
    {
        if (a->data[i] != b->data[i])
        {
            return false;
        }
    }
    return true;
}
static inline void _copy_handler(struct fhandle* to,
                                 const struct fhandle* from)
{

    memcpy(to->data, from->data, FHSIZE);
}

static struct nfs_vnode* _creat_nfs_vnode(void)
{
    struct nfs_vnode* v = malloc(sizeof (struct nfs_vnode));
    if (v == NULL)
    {
        return NULL;
    }
    link_init(&(v->ev_link));
    memset(&v->ev_handler, 0, sizeof (FHSIZE));
    memset(&v->ev_v, 0, sizeof (struct vnode));
    return v;

}
static void _destroy_nfs_vnode(struct nfs_vnode* v)
{
    COLOR_DEBUG(DB_DEVICE, ANSI_COLOR_GREEN, "destroy_nfs_vnode: %p\n", v);
    vnode_cleanup(&v->ev_v);
    list_del(&(v->ev_link));
}


//
// vnode functions
//

// at bottom of this section


/*
 * VOP_EACHOPEN on files
 */
static
int
_nfs_eachopen(struct vnode *v, int openflags)
{
    /*
     * At this level we do not need to handle O_CREAT, O_EXCL,
     * O_TRUNC, or O_APPEND.
     *
     * Any of O_RDONLY, O_WRONLY, and O_RDWR are valid, so we don't need
     * to check that either.
     */

    (void)v;
    (void)openflags;

    return 0;
}

/*
 * VOP_EACHOPEN on directories
 */
static
int
_nfs_eachopendir(struct vnode *v, int openflags)
{
    switch (openflags & O_ACCMODE) {
        case O_RDONLY:
            break;
        case O_WRONLY:
        case O_RDWR:
        default:
            return EISDIR;
    }
    if (openflags & O_APPEND) {
        return EISDIR;
    }

    (void)v;
    return 0;
}

/*
 * VOP_RECLAIM
 *
 * Reclaim should make an effort to returning errors other than EBUSY.
 */
static
int
_nfs_reclaim(struct vnode *v)
{
    struct nfs_vnode *ev = v->vn_data;

    /*
     * Need all of these locks, e_lock to protect the device,
     * vfs_biglock to protect the fs-related material, and
     * vn_countlock for the reference count.
     */

    if (ev->ev_v.vn_refcount > 1)
    {
        /* consume the reference VOP_DECREF passed us */
        -- ev->ev_v.vn_refcount;

        return EBUSY;
    }
    assert(ev->ev_v.vn_refcount == 1);

    int result = _nfs_close(&ev->ev_handler);
    if (result)
    {
        return result;
    }
    _destroy_nfs_vnode(ev);

    return 0;
}

static void _nfs_read_cb(uintptr_t token, enum nfs_stat status, fattr_t *fattr, int count, void* data)
{
    struct nfs_cb_arg* arg = (struct nfs_cb_arg*)(token);
    arg->stat = status;
    if (status == NFS_OK)
    {
        assert(count <= arg->uio->uio_resid);
        uiomove(data, count, arg->uio);
    }
    V(arg->sem);
}

static int _nfs_read_op(const struct fhandle* hd,
                        struct uio* uio)
{
    struct nfs_cb_arg cb_argv;
    cb_argv.sem = sem_create("nfs", 0, -1);
    if (cb_argv.sem == NULL)
    {
        ERROR_DEBUG("sem_create error\n");
        return ENOMEM;
    }
    cb_argv.uio = uio;
    int ret = nfs_read(hd, uio->uio_offset, uio->uio_resid, &_nfs_read_cb, (uintptr_t)&cb_argv);
    if (ret != 0)
    {
        ERROR_DEBUG("nfs_read, rpc error code: %d\n", ret);
        sem_destroy(cb_argv.sem);
        return _rpc_stat2sos_err(ret);
    }

    ret = _nfs_waitdone(&cb_argv);
    sem_destroy(cb_argv.sem);
    if (ret)
    {
        ERROR_DEBUG("nfs_read, read_cb stat: %d\n", ret);
        return _nfs_stat2sos_err(ret);
    }
    return 0;
}
/*
 * VOP_READ
 */
static
int
_nfs_read(struct vnode *v, struct uio *uio)
{
    struct nfs_vnode *ev = v->vn_data;
    /* uint32_t amt; */
    size_t oldresid;
    /* int result; */
    /*  */
    assert(uio->uio_rw == UIO_READ);
    assert(uio->uio_resid <= seL4_PAGE_SIZE);
    /*  */
    while (uio->uio_resid > 0)
    {
        oldresid = uio->uio_resid;
        int result = _nfs_read_op(&(ev->ev_handler), uio);
        if (result)
        {
            ERROR_DEBUG("%p read file error: %d\n",ev, result);
            return result;
        }
        if (uio->uio_resid == oldresid)
        {
            COLOR_DEBUG(DB_DEVICE,ANSI_COLOR_YELLOW ,"%p no more bytes read from nfs, so return with %d read bytes\n",ev, uio->uio_resid);
            /* nothing read - EOF */
            break;
        }
    }
    return 0;
}


static void _nfs_readdir_cb(uintptr_t token,
                            enum nfs_stat status,
                            int num_files,
                            char* file_names[],
                            nfscookie_t nfscookie)
{
    struct nfs_cb_arg* arg = (struct nfs_cb_arg*)(token);
    arg->stat = status;
    if (arg->stat != NFS_OK)
    {
        V(arg->sem);
        return;
    }
    COLOR_DEBUG(DB_DEVICE, ANSI_COLOR_GREEN, "readdir_cb: previous cookie: %d, cur cookie: %d, cur filenums: %d, remain files: %d\n", arg->cookie, nfscookie, num_files, arg->remain_pos);
    arg->cookie = nfscookie;
    if ( arg->cookie != 0) // some files in file_names, otherwise, there is no more files for the offset in uio
    {
        arg->total_files += num_files;
        if (arg->remain_pos >= num_files)
        {
            arg->remain_pos -= num_files;
        }
        else
        {
            int file_pos = arg->remain_pos;
            uiomove(file_names[file_pos], strlen(file_names[file_pos]), arg->uio);
            arg->remain_pos = -1; // to signal finding the file
        }
    }
    V(arg->sem);
    return;
}

/*
 * VOP_READDIR
 */
static int
_nfs_getdirentry(struct vnode *v, struct uio *uio)
{
    assert(uio->uio_rw == UIO_READ);
    struct nfs_vnode *ev = v->vn_data;
    struct nfs_cb_arg cb_argv;
    cb_argv.sem = sem_create("nfs", 0, -1);
    cb_argv.uio = uio;
    cb_argv.remain_pos = uio->uio_offset;
    if (cb_argv.sem == NULL)
    {
        ERROR_DEBUG("sem_create error\n");
        return ENOMEM;
    }
    COLOR_DEBUG (DB_DEVICE, ANSI_COLOR_GREEN, "%p, nfs_getdirentry target pos: %d\n", ev, cb_argv.remain_pos);

    // TODO need lock further to deal with nfs caching.
    int ret = 0;
    int tmp = cb_argv.remain_pos;
    cb_argv.cookie = 0;
    cb_argv.total_files = 0;
    while (1)
    {
        ret = nfs_readdir(&(ev->ev_handler),cb_argv.cookie, &_nfs_readdir_cb, (uintptr_t)&cb_argv);
        if (ret != 0)
        {
            ERROR_DEBUG("%p nfs_readdir, rpc error: %d\n", ev, ret);
            ret = _rpc_stat2sos_err(ret);
            break;
        }
        ret = _nfs_waitdone(&cb_argv);
        if (ret != 0)
        {
            ERROR_DEBUG("%p nfs_readdir, cb error: %d\n", ev, ret);
            ret = _nfs_stat2sos_err(ret);
            break;
        }
        assert(cb_argv.stat == 0);
        if (cb_argv.remain_pos == -1)
        {
            ret = 0;
            break;
        }
        if (cb_argv.cookie == 0)
        {
            if (cb_argv.total_files == tmp)
            {
                COLOR_DEBUG(DB_DEVICE, ANSI_COLOR_YELLOW, "%p readdir pos is next free: %d\n", ev, tmp);
                char z = 0;
                uiomove(&z, 1, uio);
                ret = 0;
            }
            else
            {
                COLOR_DEBUG(DB_DEVICE, ANSI_COLOR_YELLOW, "%p readdir pos %d exceed the capability %d\n", ev, tmp, cb_argv.total_files);
                ret = ENOENT;
            }
            break;
        }
    }
    sem_destroy(cb_argv.sem);
    return ret;
}


void _nfs_write_cb(uintptr_t token, enum nfs_stat status, fattr_t *fattr, int count)
{
    struct nfs_cb_arg* arg = (struct nfs_cb_arg*)(token);
    arg->stat = status;
    if (status == NFS_OK)
    {
        uiomove(NULL, count, arg->uio);
    }
    V(arg->sem);
}

static int _nfs_write_op(const struct fhandle* hd,
                         struct uio* uio)
{
    struct nfs_cb_arg cb_argv;
    cb_argv.sem = sem_create("nfs", 0, -1);
    if (cb_argv.sem == NULL)
    {
        ERROR_DEBUG("sem_create error\n");
        return ENOMEM;
    }
    cb_argv.uio = uio;

    int ret = nfs_write(hd, uio->uio_offset, uio->uio_resid, uio->uio_iov->iov_base, &_nfs_write_cb, (uintptr_t)&cb_argv);
    if (ret != 0)
    {
        ERROR_DEBUG("nfs_write, rpc code: %d\n", ret);
        sem_destroy(cb_argv.sem);
        return _rpc_stat2sos_err(ret);

    }

    ret = _nfs_waitdone(&cb_argv);
    sem_destroy(cb_argv.sem);
    if (ret)
    {
        ERROR_DEBUG("nfs_write, cb err: %d\n", ret);
        return _nfs_stat2sos_err(ret);
    }
    return 0;
}
/*
 * VOP_WRITE
 */
static
int
_nfs_write(struct vnode *v, struct uio *uio)
{
    struct nfs_vnode *ev = v->vn_data;
    /* uint32_t amt; */
    size_t oldresid;
    /*  */
    assert(uio->uio_rw == UIO_WRITE);
    assert(uio->uio_resid <= seL4_PAGE_SIZE);
    while (uio->uio_resid > 0)
    {
        oldresid = uio->uio_resid;
        int result = _nfs_write_op(&(ev->ev_handler), uio);
        if (result)
        {
            ERROR_DEBUG("%p nfs_write err: %d\n", ev, result);
            return result;
        }
        if (uio->uio_resid == oldresid)
        {
            /* nothing writen ? FIXME*/
            ERROR_DEBUG("nothing written????\n");
            assert(0);
            break;
        }
    }
    return 0;

}

/*
 * VOP_IOCTL
 */
/* static */
/* int */
/* _nfs_ioctl(struct vnode *v, int op, const void* data) */
/* { */
/*     #<{(| */
/*      * No ioctls. */
/*      |)}># */
/*  */
/*     (void)v; */
/*     (void)op; */
/*     (void)data; */
/*  */
/*     return EINVAL; */
/* } */
/*  */
/*
 * VOP_STAT
 *
 */

static int _nfs_stat(struct vnode* v, struct stat* statbuf)
{
    return 0;
}
static
int
_nfs_stat_file(struct vnode *v, char* pathname, struct stat *statbuf)
{
    struct nfs_vnode *ev = v->vn_data;
    struct nfs_cb_arg cb_argv;
    cb_argv.sem = sem_create("nfs", 0, -1);
    if (cb_argv.sem == NULL)
    {
        ERROR_DEBUG("sem_create error\n");
        return ENOMEM;
    }
    int ret =  nfs_lookup(&(ev->ev_handler), pathname, &_nfs_creat_cb, (uintptr_t)&cb_argv);
    if (ret != 0)
    {
        ERROR_DEBUG("%p nfs_lookup, rpc: %d\n", ev, ret);
        sem_destroy(cb_argv.sem);
        return _rpc_stat2sos_err(ret);

    }

    ret = _nfs_waitdone(&cb_argv);
    sem_destroy(cb_argv.sem);
    if (ret)
    {
        ERROR_DEBUG("%p nfs_lookup, cb: %d\n", ev, ret);
        return _nfs_stat2sos_err(ret);
    }
    statbuf->st_type = cb_argv.attr.type;
    statbuf->st_mode = cb_argv.attr.mode;
    statbuf->st_size = cb_argv.attr.size;
    statbuf->st_atime = cb_argv.attr.atime.seconds;
    statbuf->st_ctime = cb_argv.attr.ctime.seconds;

    statbuf->st_atimensec = cb_argv.attr.atime.useconds * 1000;
    statbuf->st_ctimensec = cb_argv.attr.ctime.useconds * 1000;

    return 0;
}

static void
_remove_cb(uintptr_t token, enum nfs_stat status){
    struct nfs_cb_arg *arg = (struct nfs_cb_arg*)token;
    arg->stat = status;
}
/*
*	VOP_REMOVE for files
*/
static
int
_nfs_remove(struct vnode *v, const char * filename) 
{
	struct nfs_cb_arg arg;
	struct nfs_vnode *ev = v->vn_data;
    arg.stat = NFS_OK;

    assert(!nfs_remove(&(ev->ev_handler), filename, &_remove_cb, (uintptr_t)&arg));
    return arg.stat;
}

/*
 * VOP_GETTYPE for files
 */
static
int
_nfs_file_gettype(struct vnode *v, uint32_t *result)
{
    (void)v;
    /* *result = S_IFREG; */
    return 0;
}

/*
 * VOP_GETTYPE for directories
 */
/* static */
/* int */
/* _nfs_dir_gettype(struct vnode *v, uint32_t *result) */
/* { */
/*     (void)v; */
/*     #<{(| *result = S_IFDIR; |)}># */
/*     return 0; */
/* } */
/*  */
/* #<{(| */
/*  * VOP_ISSEEKABLE */
/*  |)}># */
/* static */
/* bool */
/* _nfs_isseekable(struct vnode *v) */
/* { */
/*     (void)v; */
/*     return true; */
/* } */
/*  */
/* #<{(| */
/*  * VOP_FSYNC */
/*  |)}># */
/* static */
/* int */
/* _nfs_fsync(struct vnode *v) */
/* { */
/*     (void)v; */
/*     return 0; */
/* } */
/*  */
/* #<{(| */
/*  * VOP_TRUNCATE */
/*  |)}># */
/* static */
/* int */
/* _nfs_truncate(struct vnode *v, off_t len) */
/* { */
/*     #<{(| struct nfs_vnode *ev = v->vn_data; |)}># */
/*     #<{(| return emu_trunc(ev->ev_emu, ev->ev_handle, len); |)}># */
/*     return -1; */
/* } */
/*  */


/*
 * Common file open routine (for both VOP_LOOKUP and VOP_CREATE).  Not
 * for VOP_EACHOPEN. At the hardware level, we need to "open" files in
 * order to look at them, so by the time VOP_EACHOPEN is called the
 * files are already open.
 */


static void
_nfs_creat_cb(uintptr_t token, enum nfs_stat stat, fhandle_t *fh, fattr_t *fattr){
    struct nfs_cb_arg *arg = (struct nfs_cb_arg*)token;
    arg->stat = stat;
    /* if(arg->fattr){ */
    /*     memcpy(arg->fattr, fattr, sizeof(*fattr)); */
    /* } */
    if (arg->stat == 0)
    {
        memcpy(&arg->attr, fattr, sizeof (fattr_t));
        _copy_handler(&arg->handler, fh);
    }
    V(arg->sem);
}


/*
 * Routine for closing a file we opened at the hardware level.
 * This is not necessarily called at VOP_LASTCLOSE time; it's called
 * at VOP_RECLAIM time.
 */
static
int
_nfs_close(const struct fhandle* handle)
{
    // nothing to do with nfs. XXX
    (void) handle;
    return 0;
}

static int
_nfs_creat(struct vnode *dir, const char *name, bool excl, mode_t mode,
           struct vnode **ret)
{
    (void) excl;

    COLOR_DEBUG(DB_DEVICE, ANSI_COLOR_GREEN,"nfs going to creat: [%s], mode: %d\n", name, mode);

    if (strlen(name) + 1 > NFS_MAXIO) {
        return ENAMETOOLONG;
    }
    struct nfs_vnode *ev = dir->vn_data;
    assert(ev == _nfs_handler.ef_root);
    struct nfs_fs *ef = dir->vn_fs->fs_data;
    struct nfs_vnode *newguy;

    struct nfs_cb_arg cb_argv;
    cb_argv.sem = sem_create("nfs", 0, -1);
    if (cb_argv.sem == NULL)
    {
        ERROR_DEBUG("sem_create error\n");
        return ENOMEM;
    }

    sattr_t sattr;

    /* create some files file */
    sattr.mode = 0b111111111;
    sattr.uid = 1;
    sattr.gid = 1;
    sattr.size = (mode & O_TRUNC) ? 0 : -1;
    sattr.atime.seconds = unix_time_stamp();
    sattr.mtime.seconds = unix_time_stamp();
    int rets = nfs_create(&(ev->ev_handler), name, &sattr, &_nfs_creat_cb, (uintptr_t)&cb_argv);
    if (rets != 0)
    {
        sem_destroy(cb_argv.sem);
        ERROR_DEBUG("%p nfs_create, rpc: %d\n", ev, rets);
        return _rpc_stat2sos_err(rets);
    }


    rets = _nfs_waitdone(&cb_argv);
    sem_destroy(cb_argv.sem);
    if (rets)
    {
        ERROR_DEBUG("%p nf_create cb: %d\n", ev, rets);
        return _nfs_stat2sos_err(rets);
    }

    rets = _nfs_loadvnode(ef, &(cb_argv.handler), 0, &newguy);

    if (rets)
    {
        ERROR_DEBUG("%p nfs_create load new vnode error\n", ev);

        _nfs_close(&(cb_argv.handler));
        return rets;
    }
    *ret = &newguy->ev_v;
    return 0;
}

/*
 * VOP_LOOKUP
 */
static
int
_nfs_lookup(struct vnode *dir, char *pathname, struct vnode **ret)
{
    struct nfs_vnode *ev = dir->vn_data;
    assert(ev == _nfs_handler.ef_root);
    struct nfs_fs *ef = dir->vn_fs->fs_data;
    struct nfs_vnode *newguy = NULL;

    struct nfs_cb_arg cb_argv;
    cb_argv.sem = sem_create("nfs", 0, -1);
    if (cb_argv.sem == NULL)
    {
        ERROR_DEBUG("sem_create error\n");
        return ENOMEM;
    }
    int rets =  nfs_lookup(&(ev->ev_handler), pathname, &_nfs_creat_cb, (uintptr_t)&cb_argv);
    if (rets != 0)
    {
        ERROR_DEBUG("%p nfs_lookup, rpc %d\n", ev, rets);
        sem_destroy(cb_argv.sem);
        return _rpc_stat2sos_err(rets);
    }

    rets = _nfs_waitdone(&cb_argv);
    sem_destroy(cb_argv.sem);
    if (rets != 0)
    {
        ERROR_DEBUG("%p nfs_lookup, cb %d\n", ev, rets);
        return _nfs_stat2sos_err(rets);
    }
    rets = _nfs_loadvnode(ef, &(cb_argv.handler), 0, &newguy);
    if (rets)
    {
        ERROR_DEBUG("%p nfs_lookup load new vnode error\n", ev);

        _nfs_close(&(cb_argv.handler));
        return rets;
    }
    *ret = &newguy->ev_v;
    return 0;
}

static
int
_nfs_lookparent(struct vnode *dir, char *pathname, struct vnode **ret,
                char *buf, size_t len)
{
    *ret = NULL;
    // XXX we only support flat dir.
    if (strrchr(pathname, '/'))
    {
        ERROR_DEBUG("invalid pathname: %s while look parents\n", pathname);
        return EINVAL;
    }
    if (strlen(pathname) > len - 1)
    {
        ERROR_DEBUG("buf len is too small while look parents\n", pathname);
        return EINVAL;
    }
    strcpy(buf, pathname);
    *ret = dir;
    VOP_INCREF(*ret);
    return 0;
}

/*
 * VOP_MMAP
 */
/* static */
/* int */
/* _nfs_mmap(struct vnode *v) */
/* { */
/*     (void)v; */
/*     return ENOSYS; */
/* } */
/*  */
/* ////////////////////////////// */
/*  */
/* #<{(| */
/*  * Bits not implemented at all on nfs */
/*  |)}># */
/*  */
/* static */
/* int */
/* _nfs_symlink(struct vnode *v, const char *contents, const char *name) */
/* { */
/*     (void)v; */
/*     (void)contents; */
/*     (void)name; */
/*     return ENOSYS; */
/* } */
/*  */
/* static */
/* int */
/* _nfs_mkdir(struct vnode *v, const char *name, mode_t mode) */
/* { */
/*     (void)v; */
/*     (void)name; */
/*     (void)mode; */
/*     return ENOSYS; */
/* } */
/*  */
/* static */
/* int */
/* _nfs_link(struct vnode *v, const char *name, struct vnode *target) */
/* { */
/*     (void)v; */
/*     (void)name; */
/*     (void)target; */
/*     return ENOSYS; */
/* } */


/*  */
/* static */
/* int */
/* _nfs_rmdir(struct vnode *v, const char *name) */
/* { */
/*     (void)v; */
/*     (void)name; */
/*     return ENOSYS; */
/* } */
/*  */
/* static */
/* int */
/* _nfs_rename(struct vnode *v1, const char *n1, */
/*             struct vnode *v2, const char *n2) */
/* { */
/*     (void)v1; */
/*     (void)n1; */
/*     (void)v2; */
/*     (void)n2; */
/*     return ENOSYS; */
/* } */

//////////////////////////////

/*
 * Routines that fail
 *
 * It is kind of silly to write these out each with their particular
 * arguments; however, portable C doesn't let you cast function
 * pointers with different argument signatures even if the arguments
 * are never used.
 *
 * The BSD approach (all vnode ops take a vnode pointer and a void
 * pointer that's cast to a op-specific args structure) avoids this
 * problem but is otherwise not very appealing.
 */

/* static */
/* int */
/* _nfs_void_op_isdir(struct vnode *v) */
/* { */
/*     (void)v; */
/*     return EISDIR; */
/* } */

static
int
_nfs_uio_op_isdir(struct vnode *v, struct uio *uio)
{
    (void)v;
    (void)uio;
    return EISDIR;
}

static
int
_nfs_uio_op_notdir(struct vnode *v, struct uio *uio)
{
    (void)v;
    (void)uio;
    return ENOTDIR;
}

static
int
_nfs_name_op_notdir(struct vnode *v, const char *name)
{
    (void)v;
    (void)name;
    return ENOTDIR;
}

static
int
_nfs_readlink_notlink(struct vnode *v, struct uio *uio)
{
    (void)v;
    (void)uio;
    return EINVAL;
}

static
int
_nfs_creat_notdir(struct vnode *v, const char *name, bool excl, mode_t mode,
                  struct vnode **retval)
{
    (void)v;
    (void)name;
    (void)excl;
    (void)mode;
    (void)retval;
    return ENOTDIR;
}

/* static */
/* int */
/* _nfs_symlink_notdir(struct vnode *v, const char *contents, const char *name) */
/* { */
/*     (void)v; */
/*     (void)contents; */
/*     (void)name; */
/*     return ENOTDIR; */
/* } */
/*  */
/* static */
/* int */
/* _nfs_mkdir_notdir(struct vnode *v, const char *name, mode_t mode) */
/* { */
/*     (void)v; */
/*     (void)name; */
/*     (void)mode; */
/*     return ENOTDIR; */
/* } */
/*  */
/* static */
/* int */
/* _nfs_link_notdir(struct vnode *v, const char *name, struct vnode *target) */
/* { */
/*     (void)v; */
/*     (void)name; */
/*     (void)target; */
/*     return ENOTDIR; */
/* } */
/*  */
/* static */
/* int */
/* _nfs_rename_notdir(struct vnode *v1, const char *n1, */
/*                    struct vnode *v2, const char *n2) */
/* { */
/*     (void)v1; */
/*     (void)n1; */
/*     (void)v2; */
/*     (void)n2; */
/*     return ENOTDIR; */
/* } */
/*  */
static
int
_nfs_lookup_notdir(struct vnode *v, char *pathname, struct vnode **result)
{
    (void)v;
    (void)pathname;
    (void)result;
    return ENOTDIR;
}

static
int
_nfs_lookparent_notdir(struct vnode *v, char *pathname, struct vnode **result,
                       char *buf, size_t len)
{
    (void)v;
    (void)pathname;
    (void)result;
    (void)buf;
    (void)len;
    return ENOTDIR;
}


/* static */
/* int */
/* _nfs_truncate_isdir(struct vnode *v, off_t len) */
/* { */
/*     (void)v; */
/*     (void)len; */
/*     return ENOTDIR; */
/* } */

//////////////////////////////

/*
 * Function table for nfs files.
 */
static const struct vnode_ops nfs_fileops = {
    .vop_magic = VOP_MAGIC,	/* mark this a valid vnode ops table */

    .vop_eachopen = _nfs_eachopen,
    .vop_reclaim = _nfs_reclaim,

    .vop_read = _nfs_read,
    .vop_readlink = _nfs_readlink_notlink,
    .vop_getdirentry = _nfs_uio_op_notdir,
    .vop_write = _nfs_write,
    .vop_gettype = _nfs_file_gettype,

    .vop_stat = _nfs_stat,
    .vop_creat = _nfs_creat_notdir,
    .vop_remove = _nfs_name_op_notdir,

    .vop_lookup = _nfs_lookup_notdir,
    .vop_lookparent = _nfs_lookparent_notdir,
};

/*
 * Function table for _nfs directories.
 */
static const struct vnode_ops nfs_dirops = {
    .vop_magic = VOP_MAGIC,	/* mark this a valid vnode ops table */

    .vop_eachopen = _nfs_eachopendir,
    .vop_reclaim = _nfs_reclaim,
    .vop_stat_file = _nfs_stat_file,

    .vop_read = _nfs_uio_op_isdir,
    .vop_readlink = _nfs_uio_op_isdir,
    .vop_getdirentry = _nfs_getdirentry,
    .vop_lookup = _nfs_lookup,
    .vop_creat = _nfs_creat,
    .vop_lookparent = _nfs_lookparent,
    .vop_remove = _nfs_remove,
};

//

//
// Whole-filesystem functions
//

/*
 * FSOP_SYNC
 */
static
int
_nfs_sync(struct fs *fs)
{
    (void)fs;
    return 0;
}

/*
 * FSOP_GETVOLNAME
 */
static
const char *
_nfs_getvolname(struct fs *fs)
{
    /* We don't have a volume name beyond the device name */
    (void)fs;
    return NULL;
}

/*
 * FSOP_GETROOT
 */
static
int
_nfs_getroot(struct fs *fs, struct vnode **ret)
{

    assert(fs != NULL);

    struct nfs_fs* ef = fs->fs_data;

    assert(ef != NULL);
    assert(ef->ef_root != NULL);

    VOP_INCREF(&ef->ef_root->ev_v);
    *ret = &ef->ef_root->ev_v;
    return 0;
}

/*
 * FSOP_UNMOUNT
 */
static
int
_nfs_unmount(struct fs *fs)
{
    /* Always prohibit unmount, as we're not really "mounted" */
    (void)fs;
    return EBUSY;
}

static const struct fs_ops  _nfs_ops= {

    .fsop_sync = _nfs_sync,
    .fsop_getvolname = _nfs_getvolname,
    .fsop_getroot = _nfs_getroot,
    .fsop_unmount = _nfs_unmount,
};



static int _nfs_loadvnode(struct nfs_fs *ef,
                          const struct fhandle* fh ,
                          int isdir,
                          struct  nfs_vnode **ret)
{
    struct nfs_vnode *ev;

    struct list_head *current = NULL;
    struct list_head *tmp_head = NULL;
    list_for_each_safe(current, tmp_head, &(ef->ef_vnode_list.head))
    {
        ev = list_entry(current, struct nfs_vnode, ev_link);

        if (_is_same_handler(&(ev->ev_handler) , fh) )
        {

            VOP_INCREF(&ev->ev_v);
            *ret = ev;
            return 0;
        }
    }

    /* Didn't have one; create it */

    ev = _creat_nfs_vnode();
    if (ev == NULL)
    {
        ERROR_DEBUG("create new vnode for nfs without enough mem\n");
        return ENOMEM;
    }

    _copy_handler(&ev->ev_handler, fh);
    COLOR_DEBUG(DB_DEVICE, ANSI_COLOR_GREEN, "creating new nfs_vnode [%p] with handler: \n", ev);
    _dump_nfs_handler(&(ev->ev_handler));
    COLOR_DEBUG(DB_DEVICE, ANSI_COLOR_GREEN, "\n");

    int result = vnode_init(&ev->ev_v, isdir ? &nfs_dirops : &nfs_fileops,
                        &ef->ef_fs, ev);
    if (result)
    {
        _destroy_nfs_vnode(ev);
        return result;
    }

    list_add_tail(&(ev->ev_link), &(ef->ef_vnode_list.head));
    *ret = ev;
    return 0;
}


static void _nfs_handle_timeout(uint32_t id, void* argv)
{
    (void) argv;
    nfs_timeout();
    assert(id == register_timer(NFS_TIMEOUT, _nfs_handle_timeout, NULL));
}

// because network.c already init it, we simplify our code by faking mount(put mount_handler to ef_root), the nadd dev
void init_nfs(const struct fhandle* mnt_point)
{
    // tick every 100ms will cause any packets dropped by NFS to be picked up again.
    assert(register_timer(NFS_TIMEOUT, _nfs_handle_timeout, NULL));
    list_init(&_nfs_handler.ef_vnode_list);
    /* _nfs_handler.ef_root = _creat_nfs_vnode(); */
    _nfs_handler.ef_fs.fs_data = &_nfs_handler;
    _nfs_handler.ef_fs.fs_ops = & _nfs_ops;
    assert(0 ==  _nfs_loadvnode(&_nfs_handler, mnt_point, 1, &(_nfs_handler.ef_root)));
    assert(_nfs_handler.ef_root != NULL);
    assert(0 == vfs_addfs(NFS_DEVICE_NAME, &(_nfs_handler.ef_fs)));
}
