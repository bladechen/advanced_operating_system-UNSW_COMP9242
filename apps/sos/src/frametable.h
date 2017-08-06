#ifndef _FRAMETABLE_H_
#define _FRAMETABLE_H_

#include <sel4/sel4.h>

/* Integer division, rounded up (rather than truncating) */
#define DIVROUND(a,b) (((a) + ((b) - 1)) / (b))
// derive seL4 page size from seL4_PageBits
#define seL4_PAGE_SIZE          (1 << seL4_PageBits)


typedef seL4_Word sos_paddr_t;
typedef seL4_Word sos_vaddr_t;

typedef struct frame_table_entry {
    // SmallPageObject cap mapping frame into SOS window
    seL4_ARM_Page page_cap;

    // Corresponding virtual address
    // seL4_Word vaddr;
} frame_table_entry;

typedef frame_table_entry *frame_table;

void frametable_init(void);
seL4_Word frame_alloc(seL4_Word * vaddr_ptr);
void frame_free(seL4_Word vaddr);


#endif /* _MAPPING_H_ */
