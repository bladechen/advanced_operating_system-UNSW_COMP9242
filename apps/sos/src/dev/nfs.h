#ifndef _NFS_H_
#define _NFS_H_
#include "comm/comm.h"
#include "comm/list.h"
#include "nfs/nfs.h"
#include "vfs/fs.h"
#include "vfs/vnode.h"
#include "vm/vm.h"
#include "vm/frame_table.h"
#include "vm/buffer_cache.h"
#define NFS_TIMEOUT (100)
#define NFS_DEVICE_NAME "nfs"
#define NFS_MAXIO 1000
struct nfs_fs;
// i see nfs as emu in os161.... so name convention is ev/ef...

struct nfs_cb_arg
{
    struct fhandle handler;
    volatile enum nfs_stat stat;
    struct semaphore* sem;
    fattr_t attr;
    struct uio* uio;

    nfscookie_t cookie; // need by nfs to iteratively communicate with it
    int remain_pos;

    int total_files;

};

// enum CACHE_NODE_STATUS_E
// {
//     CACHE_LOADING_BIT  = 0,
//     CACHE_FLUSHING_BIT = 1,
//     CACHE_DIRTY_BIT    = 2,
// };
//
// // linux use radix tree, 6bit for each level.
// // i am too lazy to make it dynamic, so make it only two levels.
// struct cache_node
// {
//     struct semaphore* sem;
//     uint32_t frame_num;
//     uint32_t version;
// };
//
// struct cache_dir
// {
//     struct cache_node* node;
// };
//
// struct cache_root
// {
//     // first 10bit, second 8 bit. so the maximum is 1GB file
//     struct cache_dir* dir; // two level radix tree,
//     //
// };

struct nfs_vnode
{
    struct vnode ev_v;
    // struct nfs_fs* ev_nfs;
    struct fhandle ev_handler;
    // struct semaphore* ev_sem;

    struct list_head ev_link;

    struct cache_root* root; // if root is NULL, cache for this file is not enabled.
};

struct nfs_fs
{
    struct fs ef_fs;

    struct nfs_vnode* ef_root;
    struct list ef_vnode_list;

};


void init_nfs(const struct fhandle* mnt_point);


int nfs_read_func(struct vnode* vn, struct uio* uio);
int nfs_write_func(struct vnode* vn, struct uio* uio);

#endif
