#include "pid.h"
#include "proc.h"
/*
*   Use pid % _pid_SIZE to retrieve the location on _pid
*   Design, the pid may greater than the size of _pid, so that
*   the reuse of pid will have to experience a longer time, which will
*   help to mitigate the problem may happen with pid reuse
*/
struct proc* _pid[PID_ARRAY_SIZE] = {NULL};

int _next_pid = 0;

int _free_pid_count = PID_ARRAY_SIZE;




static bool _valid_pid(int pid)
{
    return (pid >= 0 && pid < MAX_SUPPORTED_PID);
}
static inline int _pid_to_index(int pid)
{
    assert(_valid_pid(pid));
    return  pid % PID_ARRAY_SIZE;
}
struct proc* index_to_proc(int index)
{
    return _pid[_pid_to_index(index)];
}

static inline int _get_next_pid()
{
    if (_next_pid < MAX_SUPPORTED_PID)
    {
        return _next_pid ++;
    }
    else if(_next_pid == MAX_SUPPORTED_PID)
    {
        _next_pid = 0;
        return 0;
    }
    else
    {
        assert(0);
        return -1;
    }
}

// you need provide proc to install while alloc
int alloc_pid(struct proc* proc)
{
    if ( _free_pid_count == 0)
    {
        return -1;
    }
    assert(_free_pid_count > 0 && _free_pid_count <= PID_ARRAY_SIZE);
    int i = PID_ARRAY_SIZE;
    int pid = _next_pid;
    while(_pid[_pid_to_index(pid)] != NULL)
    {
        pid = _get_next_pid();
        -- i;
        assert(i >= 0);
    }

    // TODO set _next_pid + 1
    // _get_next_pid();
    _free_pid_count --;
    assert(i >= 0);
    assert(_pid[_pid_to_index(pid)] == NULL);
    _pid[_pid_to_index(pid)] = proc;
    return pid;
}

void free_pid(int pid)
{
    if (!_valid_pid(pid))
    {
        ERROR_DEBUG("why free_pid %d\n", pid);
        return;
    }
    assert(pid_to_proc(pid) != NULL);
    ++ _free_pid_count;
    _pid[_pid_to_index(pid)] = NULL;
}

struct proc* pid_to_proc(int pid)
{
    if (!_valid_pid(pid))
    {
        ERROR_DEBUG("why pid_to_proc %d\n", pid);
        return NULL;
    }
    struct proc * tmp = _pid[_pid_to_index(pid)];
    if (tmp == NULL)
    {
        ERROR_DEBUG("why pid_to_proc %d is NULL\n", pid);
        return NULL;
    }
    if (tmp->p_pid == pid)
    {
        return tmp;
    }
    ERROR_DEBUG("the pid %d is now %d\n", pid, tmp->p_pid == pid);
    return NULL;
}
