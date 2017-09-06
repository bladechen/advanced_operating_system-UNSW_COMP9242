#include "vm/frametable.h"
#include "vfs/vfs.h"
#include "vfs/uio.h"
#include "vfs/vnode.h"
#include "vm/pagetable.h"
#include "swaptable.h"
#include <sel4/sel4.h>
#include <stdlib.h>


static swap_table_head QUEUE = {-1, -1, 0, 0};

// NOTICE, number 0 is used to represent invalid here, so the version number start from 1,
// and the first entry in swap table is not used;
static swap_table _swap_table = NULL;

static struct vnode * pagefile_vn = NULL;

static char* FILE_PATH = "nfs:pagefile";


// unit test for queue data structure
static void self_test();

static void init_swap_table_entry(int myself_index, int next_index,
    int prev_index, int version_number, enum swap_table_entry_status status)
{
    (_swap_table + myself_index)->myself_index = myself_index;
    (_swap_table + myself_index)->next_index = next_index;
    (_swap_table + myself_index)->prev_index = prev_index;
    (_swap_table + myself_index)->version_number = version_number;
    (_swap_table + myself_index)->status = status;
}

void init_swapping_vnode()
{
    assert(pagefile_vn == NULL);
    assert(0 == vfs_open(FILE_PATH, O_RDWR|O_CREAT, O_RDWR|O_CREAT, &pagefile_vn));
    assert(pagefile_vn != NULL);
}

int init_swapping()
{
    /* Init the `pagefile` vnode */
    init_swapping_vnode();

    /* Init swap table */
    _swap_table = (swap_table_entry *)malloc(SWAPTABLE_ENTRY_AMOUNT * sizeof(swap_table_entry));
    // TODO

    if (_swap_table == NULL)
    {
        assert(0);
        printf("swap table is NULL ~~~########\n");
        return ENOMEM;
    }

    // The first entry is actually skipped
    init_swap_table_entry(0, -1, -1, 0, ENTRY_INVALID);

    // The actual in used first entry
    init_swap_table_entry(1, 2, -1, 1, ENTRY_FREE);

    for(int i = 2; i < SWAPTABLE_ENTRY_AMOUNT - 1; i++)
    {
        init_swap_table_entry(i, i + 1, i - 1, 1, ENTRY_FREE);
    }

    // The last entry in _swap_table
    init_swap_table_entry(SWAPTABLE_ENTRY_AMOUNT - 1, -1, SWAPTABLE_ENTRY_AMOUNT - 2,
        1, ENTRY_FREE);

    /* Init doubly linked list data structure swap_table_head */
    QUEUE.first = 1;
    QUEUE.last = SWAPTABLE_ENTRY_AMOUNT - 1;
    QUEUE.free_entries = SWAPTABLE_ENTRY_AMOUNT - 1;
    QUEUE.total_entries = SWAPTABLE_ENTRY_AMOUNT - 1;

    self_test();
    return 0;
}


bool write_to_pagefile(seL4_Word sos_vaddr, int offset)
{
    assert(pagefile_vn != NULL);
    assert(IS_PAGE_ALIGNED(sos_vaddr));
    assert(IS_PAGE_ALIGNED(offset));
    struct iovec iov;
    struct uio u;
    uio_kinit(&iov, &u, (void*)sos_vaddr, seL4_PAGE_SIZE, offset, UIO_WRITE);
    assert(u.uio_resid == seL4_PAGE_SIZE);
    int ret = VOP_WRITE(pagefile_vn, &u);
    // we must make sure it return success, and it writes 4096bytes to the pagefile
    if (ret != 0 || u.uio_resid != 0)
    {
        return false;
    }
    return true;
}

bool read_from_pagefile(seL4_Word sos_vaddr, int offset)
{
    assert(pagefile_vn != NULL);
    assert(IS_PAGE_ALIGNED(sos_vaddr));
    assert(IS_PAGE_ALIGNED(offset));
    struct iovec iov;
    struct uio u;
    uio_kinit(&iov, &u, (void*)sos_vaddr, seL4_PAGE_SIZE, offset, UIO_READ);
    assert(u.uio_resid == seL4_PAGE_SIZE);
    int ret = VOP_READ(pagefile_vn, &u);
    // we must make sure it return success, and it reads 4096bytes from the page file
    if (ret != 0 || u.uio_resid != 0)
    {
        return false;
    }
    return true;
}


/*** Use doubly linked list as a queue ***/
/* After swap into mem, make the entry usable again */
// put the newly freed entry to tail
static int enqueue(swap_table_head * queue, swap_table_entry * ste)
{
    assert(queue->free_entries < queue->total_entries);

    assert(ste->next_index == -1 && ste->prev_index == -1 && ste->status == SWAP_OUT_TO_FILE);

    ste->prev_index = queue->last;
    if (queue->last != -1)
    {
        _swap_table[queue->last].next_index = ste->myself_index;
    }
    queue->last = ste->myself_index;

    if (queue->first == -1) // empty queue
    {
        queue->first = ste->myself_index;
    }

    queue->free_entries++;

    ste->status = ENTRY_FREE;

    return 0;
}

static void _remove_from_queue(swap_table_head * queue, swap_table_entry * ste)
{
    // entry in the middle, not affect head
    if (ste->next_index != -1 && ste->prev_index != -1)
    {
        _swap_table[ste->next_index].prev_index = ste->prev_index;
        _swap_table[ste->prev_index].next_index = ste->next_index;

    }
    // the very last available entry
    else if (queue->first == ste->myself_index && queue->last == ste->myself_index &&
        ste->next_index == -1 && ste->prev_index == -1)
    {
        assert(queue->free_entries == 1);
        queue->first = -1;
        queue->last = -1;
    }
    // the first entry pointed by queue->first
    else if(queue->first == ste->myself_index && ste->next_index != -1 && ste->prev_index == -1)
    {
        queue->first = ste->next_index;
        _swap_table[ste->next_index].prev_index = -1;
    }
    else if (queue->last == ste->myself_index && ste->prev_index != -1 && ste->next_index == -1)
    {
        queue->last = ste->prev_index;
        _swap_table[ste->prev_index].next_index = -1;
    }
    else
    {
        ERROR_DEBUG("strange entry status removed entry id %d [<-%d:%d->], queue first: %d, last: %d\n",
                    ste->myself_index, ste->prev_index, ste->next_index, queue->first, queue->last);
        assert(0);
    }

    ste->next_index = -1;
    ste->prev_index = -1;
    ste->status = SWAP_OUT_TO_FILE;
    ste->version_number++;
}

/* Get emtpy(free) entry */
static swap_table_entry * dequeue(swap_table_head * queue)
{
    assert(_swap_table != NULL);

    // nothing left, run out of pagefile
    if (queue->first == -1 && queue->last== -1)
    {
        assert(queue->free_entries == 0);
        return NULL;
    }
    assert(queue->first != -1 && queue->last != -1);
    swap_table_entry * ste = _swap_table + queue->first;
    assert(ste->status ==  ENTRY_FREE);
    _remove_from_queue(queue, ste);
    queue->free_entries --;
    return ste;
}



/* Interface open to the upper layer, actual operation functions */

/* Swap from memory to file */
int do_swapout_frame(sos_vaddr_t vaddr,
                     uint32_t swap_frame_number_in,
                     uint32_t swap_frame_version,
                     uint32_t *swap_frame_number_out)
{
    assert(vaddr);
    assert(swap_frame_number_in < SWAPTABLE_ENTRY_AMOUNT);
    assert(_swap_table != NULL);

    swap_table_entry * ste = NULL;
    if (swap_frame_number_in == 0 || swap_frame_version == 0)
    {
        ste = dequeue(&QUEUE);
        if (ste == NULL)
        {
            return ENOMEM;
        }
        assert(ste->myself_index > 0);
        assert(write_to_pagefile(vaddr, ste->myself_index * seL4_PAGE_SIZE) == true);
    } else
    {
        ste = _swap_table + swap_frame_number_in;
        if (ste->version_number == swap_frame_version)
        {
            // This condition indicates that this swapped data haven't been updated in the
            // pagefile since last swap in.
            _remove_from_queue(&QUEUE, ste);
        } else
        {
            ste = dequeue(&QUEUE);
            if (ste == NULL)
            {
                return ENOMEM;
            }
            assert(ste->myself_index > 0);
            assert(write_to_pagefile(vaddr, ste->myself_index * seL4_PAGE_SIZE) == true);
        }
    }

    ste->version_number++;
    *swap_frame_number_out = ste->myself_index;

    return 0;
}

/* Swap from file to memory */
int do_swapin_frame(sos_vaddr_t vaddr,
                    uint32_t swap_frame_number,
                    uint32_t* swap_frame_version)
{

    assert(vaddr);
    assert(swap_frame_number < SWAPTABLE_ENTRY_AMOUNT);
    assert(_swap_table != NULL);

    swap_table_entry * ste = _swap_table + swap_frame_number;

    assert(read_from_pagefile(vaddr, ste->myself_index * seL4_PAGE_SIZE) == true);

    ste->version_number++;

    *swap_frame_version = ste->version_number;

    assert(enqueue(&QUEUE, ste) == 0);

    return 0;
}

/* Free corresponding swaptable entry directly */
int do_free_swap_frame(uint32_t swap_frame_number)
{
    if (swap_frame_number > SWAPTABLE_ENTRY_AMOUNT || _swap_table == NULL)
    {
        return EINVAL;
    }

    swap_table_entry * ste = _swap_table + swap_frame_number;

    assert(enqueue(&QUEUE, ste) == 0);

    return 0;
}


static void self_test()
{
    printf("come into swap_table tests########\n");
    assert(QUEUE.free_entries == QUEUE.total_entries);
    assert(QUEUE.free_entries == SWAPTABLE_ENTRY_AMOUNT - 1);

    swap_table_entry * test_arr[100];

    for(int i=0; i< 100; i++)
    {
        test_arr[i] = dequeue(&QUEUE);
    }

    for(int i=0; i<100; i++)
    {
        enqueue(&QUEUE, test_arr[i]);
    }

    assert(QUEUE.free_entries == QUEUE.total_entries);
    assert(QUEUE.free_entries == SWAPTABLE_ENTRY_AMOUNT - 1);


    char string[4096];
    for(int i = 0; i < 4096; i++)
    {
        string[i] = (char)(i%26 + 65);
    }

    srand(666);

    for(int i = 0; i < 100; i++)
    {
        int offset = 4096 * (rand() % SWAPTABLE_ENTRY_AMOUNT);


        struct iovec iov;
        struct uio u;
        uio_kinit(&iov, &u, (void*)string, seL4_PAGE_SIZE, offset, UIO_WRITE);
        assert(u.uio_resid == seL4_PAGE_SIZE);
        int ret = VOP_WRITE(pagefile_vn, &u);
        assert(ret == 0 && u.uio_resid == 0);

        char * temp = (char *)malloc(sizeof(char) * 4096);
        uio_kinit(&iov, &u, (void*)temp, seL4_PAGE_SIZE, offset, UIO_READ);
        assert(u.uio_resid == seL4_PAGE_SIZE);
        ret = VOP_READ(pagefile_vn, &u);
        assert(ret == 0 && u.uio_resid == 0);

        if (strncmp(string, temp, 4096) != 0)
        {
            printf("%s \n",temp);
            printf("i: %d, offset: %d\n", i, offset);
        }

        // printf("%s \n",temp);
        // assert(strncmp(string, temp, 4096) == 0);
        free(temp);
    }

}






