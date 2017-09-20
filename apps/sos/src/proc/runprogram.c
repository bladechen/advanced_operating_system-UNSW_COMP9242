#include "proc.h"

int run_program(const char* name, int fault_cap, int argc, char** argv)
{
    struct proc* process = proc_create((char*)(name), fault_cap);
    if (process == NULL)
    {
        ERROR_DEBUG("proc_create %s failed\n", name);
        return -ENOMEM;
    }
    if (!proc_load_elf(process, (char*)name))
    {
        ERROR_DEBUG("proc_load_elf %s failed\n", name);
        proc_destroy(process);
        return -EINVAL;
    }
    proc_attach_father(process, get_current_proc());
    int ret = proc_start(process, argc, argv);
    if (ret != 0)
    {
        ERROR_DEBUG("proc_start %s failed\n", name);
        proc_destroy(process);
        return -ret;
    }
    return process->p_pid;
}
