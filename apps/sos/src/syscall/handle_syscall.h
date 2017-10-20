#include "comm/comm.h"
#include "sos.h"

static inline void destroy_reply_cap(seL4_Word *cap)
{
    if (*cap != 0 )
    {
        cspace_free_slot(cur_cspace, *cap);
        *cap = 0;
    }

}
// caller should copy msg into ipc shared buffer
static inline int ipc_reply(struct ipc_buffer_ctrl_msg* ctrl, seL4_Word *reply_cap)
{
    assert(*reply_cap != 0 && ctrl != NULL);
    seL4_MessageInfo_t r = seL4_MessageInfo_new(0, 0, 0, IPC_CTRL_MSG_LENGTH);
    serialize_ipc_ctrl_msg(ctrl);
    assert(ctrl->offset <= APP_PROCESS_IPC_SHARED_BUFFER_SIZE);

    seL4_Send(*reply_cap, r);
    destroy_reply_cap(reply_cap);
    return 0;
}




void handle_block_sleep(void* argv);
void handle_block_read(void* argv);
