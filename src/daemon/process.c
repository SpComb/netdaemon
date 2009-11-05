#include "process.h"
#include "shared/log.h"
#include "client.h"

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
    struct process *process;

    // alloc
    if ((process = calloc(1, sizeof(*process))) == NULL)
        return -1;

    // init
    LIST_INIT(&process->clients);

    // start
    if (process_spawn(process, exec_info) < 0)
        goto error;

    log_info("[%p] Spawned process '%s' -> pid=%d as '%s'", process, exec_info->path, process->pid, process_id(process));

    // ok
    *proc_ptr = process;

    return 0;

error:
    // XXX: proper cleanup
    free(process);

    return -1;
}

const char *process_id (struct process *proccess)
{
    // XXX: not yet known
    return "XXX";
}

int process_attach (struct process *process, struct client *client)
{
    // add to list
    LIST_INSERT_HEAD(&process->clients, client, process_clients);

    log_debug("[%p] Client [%p] attached", process, client);

    // ok
    return 0;
}

void process_detach (struct process *process, struct client *client)
{
    // remove from list
    LIST_REMOVE(client, process_clients);
    
    log_debug("[%p] Client [%p] detached", process, client);
}

