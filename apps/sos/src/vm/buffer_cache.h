#ifndef _BUFFER_CACHE_H_
#define _BUFFER_CACHE_H_
#include "vm.h"
#include "vfs/vnode.h"
#include "coroutine/synch.h"
#include "frametable.h"
#include "vfs/uio.h"
#define BUFFER_CACHE_SIZE (1024 * 1024 * 20) // reserve 20M only for buffer cache while sos boots up

typedef int (*load_func)(struct vnode* v, struct uio *uio);

typedef int (*flush_func)(struct vnode* v, struct uio *uio);

enum CACHE_NODE_STATUS_E
{
    CACHE_LOADING_BIT  = 0,
    CACHE_FLUSHING_BIT = 1,
    CACHE_DIRTY_BIT    = 2,
    // CACHE_VALID_BIT    = 4,
    // CACHE_PIN_BIT      = 8,

};


// linux use radix tree, 6bit for each level.
// i am too lazy to make it dynamic, so make it only two levels.
struct cache_node
{
    struct cv* cv;
    uint32_t frame_num;
    struct list_head link;

    volatile int wait_thread_num;

    uint32_t offset;
    // struct cache_dir* parent;

    // uint32_t version;
};

struct cache_dir
{
    struct cache_node* node;
};

#define L1_SIZE (4096 /sizeof (struct cache_dir))
#define L2_SIZE (4096 /sizeof (struct cache_node))


struct cache_root
{
    int file_size;
    // first 10bit, second 8 bit. so the maximum is 1GB file
    struct cache_dir* dir; // two level radix tree,
    load_func read_cb;
    flush_func write_cb;

    struct vnode* vn;


    //
};

int buffer_cache_read(struct cache_root* root, struct uio* uio);

void buffer_cache_init(struct cache_root** root);
#endif
