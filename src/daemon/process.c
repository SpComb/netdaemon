#include "process.h"
#include "shared/log.h"

#include <unistd.h>

/**
 * Perform exec() with the given parameters
 */
static void _process_exec (const struct process_exec_info *exec_info)
    __attribute__ ((noreturn));

static void _process_exec (const struct process_exec_info *exec_info)
{
    if (execve(exec_info->path, exec_info->argv, exec_info->envp) < 0)
        // fail
        FATAL_ERRNO("execve: %s", exec_info->path);

    else
        FATAL("execve returned success");
}

/**
 * Spawn a new process and execute with given params
 */
static int process_spawn (struct process *proc, const struct process_exec_info *exec_info)
{
    // verify exec early
    if (access(exec_info->path, X_OK) < 0)
        return -1;

    // perform fork
    if ((proc->pid = fork()) < 0)
        return -1;

    else if (proc->pid == 0)
        // child performs exec()
        _process_exec(exec_info);

    else
        // child started, parent continues
        return 0;
}

int process_start (struct process **proc_ptr, const struct process_exec_info *exec_info)
{
    struct process *proc;

    // alloc
    if ((proc = calloc(1, sizeof(*proc))) == NULL)
        return -1;

    // start
    if (process_spawn(proc, exec_info) < 0)
        goto error;

    // ok
    *proc_ptr = proc;

    return 0;

error:
    // XXX: proper cleanup
    free(proc);

    return -1;
}

