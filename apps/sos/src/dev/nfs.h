#ifndef _NFS_H_
#define _NFS_H_
#include "comm/comm.h"
#include "comm/list.h"
#include "nfs/nfs.h"
#include "vfs/fs.h"
#include "vfs/vnode.h"
#define NFS_DEVICE_NAME "nfs"
#define NFS_MAXIO 1000
struct nfs_fs;
// i see nfs as emu in os161.... so name convention is ev/ef...

struct nfs_cb_arg
{
    struct fhandle handler;
    enum nfs_stat stat;
    struct semaphore* sem;
    fattr_t attr;

    struct uio* uio;
};

struct nfs_vnode
{
    struct vnode ev_v;
    // struct nfs_fs* ev_nfs;
    struct fhandle ev_handler;
    // struct semaphore* ev_sem;
    struct list_head ev_link;
};

struct nfs_fs
{
    struct fs ef_fs;

    struct nfs_vnode* ef_root; //XXX maybe no use
    struct list ef_vnode_list;

};


void init_nfs(const struct fhandle* mnt_point);

#endif
