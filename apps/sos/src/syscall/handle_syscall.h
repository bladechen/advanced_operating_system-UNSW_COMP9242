#include "comm/comm.h"
static inline void reply_success( seL4_Word cap)
{
    seL4_MessageInfo_t reply = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_SetMR(0, 0);
    seL4_Send(cap, reply);
}

static inline void destroy_reply_cap(seL4_Word cap)
{
    cspace_free_slot(cur_cspace, cap);
}
void handle_block_sleep(void* argv);
