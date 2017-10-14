#include "buffer_cache.h"

static struct list _cache_node_head;
static int _free_thread_count ;
static struct list _free_thread;
#define MAX_CONCURRENT_THREAD 10


extern struct frame_table_head _buffer_cache;

void buffer_cache_bootstrap()
{
    list_init(&_free_thread);
    list_init(&_cache_node_head);
    for (int i = 0; i < MAX_CONCURRENT_THREAD; ++ i)
    {
        struct coro* tmp = create_coro(NULL, NULL);
        assert(tmp != NULL);
        list_add_tail(&tmp->_link, &(_free_thread.head));
    }
    _free_thread_count = MAX_CONCURRENT_THREAD;
}

void buffer_cache_init(struct cache_root** root,
                       load_func load,
                       flush_func flush)
{
    *root = NULL;
    struct cache_root* new = malloc(sizeof(struct cache_root));
    if (new == NULL)
    {
        return;
    }
    new->dir = (struct cache_dir*)(kframe_alloc());
    if (new->dir == NULL)
    {
        free(new);
        return;
    }
    new->read_cb = load;
    new->write_cb = flush;
    *root = new;
}

static int _total_page(int off, size_t len)
{
    if (off > off + len)
    {
        return 0;
    }
    int base = off & 0xFFFFF000;
    int tmp = off + len;
    int up = 0;
    if (tmp & 0xFFFFF000)
    {
        up =  (tmp >> 12)  + 1;
    }
    return up - base;
}

static inline int cache_l1_index(int page_index)
{
    assert(page_index / L2_SIZE < L1_SIZE);
    /* { */
    /*     return -1; */
    /* } */
    return page_index / L2_SIZE;
}

static uint32_t _frame_idx(uint32_t cache_node_frame_num)
{
    return (cache_node_frame_num & 0xFFFFF000) >> 12;

}

static inline int cache_l2_index(int page_index)
{
    return page_index % L2_SIZE;
    /* return (page_index) & 0x00 */
}

static inline bool cache_available(uint32_t frame_num)
{
    return _frame_num(frame_num) ; // FIXME
}

// TODO the filesize should be considered
static bool _already_cache_page(struct cache_root* root,
                                int page_index)
{
    int l1_idx = cache_l1_index(page_index);
    int l2_idx = cache_l2_index(page_index);
    if (root->dir[l1_idx] == NULL)
    {
        return false;
    }
    return cache_available(root->dir[l1_idx].node[l2_idx].frame_num);
}



static void _try_creat_new_dir(struct cache_root* root,
                                int page_index)
{
    int l1_idx = cache_l1_index(page_index);
    if (!root->dir[l1_idx])
    {
        root->dir[l1_idx] = (struct cache_node*)kframe_alloc();
        assert(root->dir[l1_idx] != NULL);
    }
}

static void _flush_frame(struct cache_root* root, void* data, size_t off)
{
    struct uio u;
    struct iovec iov;
    uio_kinit(&iov, &u, data, 4096, off, UIO_WRITE);
    assert(4096 == root->flush_func(root->vn, &u));
}

static int _alloc_used_frame(struct cache_root* root)
{

    struct list_head *current = NULL;
    struct list_head *tmp_head = NULL;

    list_for_each_safe(current, tmp_head, &(_cache_node_head.head))
    {
        struct cache_node* tmp = list_entry(current, struct cache_node, link);
        if (tmp->wait_thread_num == 0)
        {
            assert(tmp->cv == NULL);
            assert(_frame_idx(tmp->frame_num));
            list_del(&(tmp->link));
            assert(!(tmp->frame_num & 0xfff));
            if (tmp->frame_num & CACHE_DIRTY_BIT)
                _flush_frame(root, frame_addr(_frame_idx(tmp->frame_num)), tmp->offset);
            tmp->frame_num = 0;
            return _frame_idx(tmp->frame_num);
        }
    }
    return 0;
}

static bool _try_alloc_free_frame(struct cache_root* root,
                                  int page_index)
{
    int l1_idx = cache_l1_index(page_index);
    int l2_idx = cache_l2_index(page_index);
    assert(root->dir[l1_idx] != NULL);
    if (_frame_num(root->dir[l1_idx].node[l2_idx].frame_num) == 0)
    {
        assert(root->dir[l1_idx].node[l2_idx].frame_num == 0);
        int id = alloc_frame(_buffer_cache);
        if (id == 0)
        {
            id = _alloc_used_frame(root);
            assert(id != 0); // need handle XXX
        }
        root->dir[l1_idx].node[l2_idx].frame_num = id << 12;
        assert(root->dir[l1_idx].node[l2_idx].cv == NULL);
        list_add_tail(&(root->dir[l1_idx].node[l2_idx].link), &_cache_node_head.head);
        return true;
    }
    return false;

}

static struct cache_node* _get_cache_node(struct cache_root* root, int page_index)
{
    int l1_idx = cache_l1_index(page_index);
    int l2_idx = cache_l2_index(page_index);
    return &(root->dir[l1_idx].node[l2_idx]);
}

static void _inc_frame_wait(struct cache_root* root, int page_index)
{
    struct cache_node* n = _get_cache_node(root, page_index);
    assert(n != NULL);
    assert(_frame_num(n->frame_num));
    ++ n->wait_thread_num;
}

struct cb_argv
{
    struct semaphore* sem;
    struct cache_root* root;
    int page;

};

void waitall_finish(struct list* l)
{

    struct list_head *current = NULL;
    struct list_head *tmp_head = NULL;

    list_for_each_safe(current, tmp_head, &(l->head))
    {
        P(sem);
    }

    list_for_each_safe(current, tmp_head, &(l->head))
    {

        struct coroutine* tmp = list_entry(current, struct coroutine, _link);
        list_del(&(tmp->_link));
        putback_thread(tmp);
    }
}

int buffer_cache_read(struct cache_root* root, struct uio* uio)
{
    assert(uio->uio_resid <= 4096);
    assert(root != NULL && uio != NULL);
    int base_page = (int)(uio->uio_offset) & 0xFFFFF000;
    size_t read_len = uio->uio_resid;

    int npage = _total_page((int)(uio->uio_offset), read_len);
    if (npage == 0)
    {
        ERROR_DEBUG("something interesting happens\n");
        return 0;
    }

    struct semaphore* sem = sem_creat("buffer_cache", 0, -1);

    struct list tmp_thread;
    list_init(&tmp_thread);
    int still_need_read_page = 0;

    for (int i = 0; i < npage; ++ i)
    {
        if (!_already_cache_page(root, base_page + i))
        {
            _try_creat_new_node(root, base_page + i);
            bool new = _try_alloc_free_frame(root, base_page + i);
            _inc_frame_wait(root, base_page + i);

            if (new)
            {
                struct cb_argv* ag = malloc(sizeof(struct cb_argv));
                assert(ag != NULL);
                ag->sem = sem;
                ag->page = i + base_page;
                ag->root = root;
                struct cache_node* n = _get_cache_node(root, base_page + i);
                n->frame_num |= CACHE_LOADING_BIT;
                assert(n ->cv == NULL);
                n->cv = cv_creat(NULL);
                assert(n->cv != NULL);
                struct coroutine* c = alloc_free_thread();
                if (c)
                {

                    list_add_tail(&c->_link, &(tmp_thread.head));
                    restart_coro(c, cache_load, ag);
                }
                else
                {
                    cache_load(ag);
                    waitall_finish(&tmp_thread);
                }

            }
            else
            {

                struct cache_node* n = _get_cache_node(root, base_page + i);
                if (n ->frame_num & CACHE_LOADING_BIT)
                {
                    n->wait_thread_num ++;
                    assert(n->cv != NULL);
                    cv_wait(n->cv);
                    n->wait_thread_num --;
                    if (n->wait_thread_num == 0)
                    {
                        cv_destroy(n->cv);
                        n->frame_num &= ~(CACHE_LOADING_BIT);

                    }
                }
            }

        }
    }
    waitall_finish(&tmp_thread);
    sem_destroy(sem);

}
