#ifndef _FRAMETABLE_H_
#define _FRAMETABLE_H_

#include <sel4/sel4.h>

/* Integer division, rounded up (rather than truncating) */
#define DIVROUND(a,b) (((a) + ((b) - 1)) / (b))
// derive seL4 page size from seL4_PageBits
#define seL4_PAGE_SIZE          (1 << seL4_PageBits)

#define seL4_FRAME_NUMBER_MASK  ()

#define seL4_MAX_FREE_FRAME_POOL (4000) // 4000 * 4k = 16M


typedef seL4_Word sos_paddr_t;
typedef seL4_Word sos_vaddr_t;

typedef struct frame_table_entry {
    // SmallPageObject cap mapping frame into SOS window
    seL4_CPtr   sos_cap;
    seL4_CPtr   app_cap;
    // index for the free frame.
    int next_free;
} frame_table_entry;

typedef frame_table_entry *frame_table;

void frametable_init(void);
seL4_Word frame_alloc(seL4_Word * vaddr_ptr);
void frame_free(seL4_Word vaddr);


#endif /* _MAPPING_H_ */
