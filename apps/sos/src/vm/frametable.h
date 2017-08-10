#ifndef _FRAMETABLE_H_
#define _FRAMETABLE_H_

#include <sel4/sel4.h>

// TODO we may reserve some critical frame for sos use only.
/* Integer division, rounded up (rather than truncating) */
#define DIVROUND(a,b) (((a) + ((b) - 1)) / (b))
// derive seL4 page size from seL4_PageBits
#define seL4_PAGE_SIZE          (1 << seL4_PageBits)

#define seL4_FRAME_MASK  (0xFFFFF000U)

#define seL4_MAX_FREE_FRAME_POOL (4000) // 4000 * 4k = 16M


// for sos memory only, not for application memory
typedef seL4_Word sos_paddr_t;
typedef seL4_Word sos_vaddr_t;


enum frame_table_error
{
    FRAME_TABLE_SUCCESS = 0,
    FRAME_TABLE_PARA_ERROR = -1,
    FRAME_TABLE_INVALID_STATUS = -2,
    FRAME_TABLE_SEL4_ERROR = -3,

};

enum frame_entry_status
{

    FRAME_FREE = 0,
    FRAME_SOS  = 1,
    FRAME_APP  = 2,

};
typedef struct frame_table_entry {
    // SmallPageObject cap mapping frame into SOS window
    seL4_CPtr   sos_cap;
    seL4_CPtr   app_cap;
    // index for the free frame.
    int next_free;
} frame_table_entry;

typedef frame_table_entry *frame_table;

void frametable_init(void);
sos_vaddr_t frame_alloc(sos_vaddr_t * vaddr_ptr);
void frame_free(seL4_Word vaddr);


int set_frame_app_cap(sos_vaddr_t vaddr, seL4_CPtr cap);

uint32_t get_frame_app_cap(sos_vaddr_t vaddr);
uint32_t get_frame_sos_cap(sos_vaddr_t vaddr);



#endif /* _MAPPING_H_ */
