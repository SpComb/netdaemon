#define _GNU_SOURCE /* for O_CLOEXEC */
#include "process.h"
#include "shared/log.h"
#include "shared/util.h"
#include "client.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>

/**
 * Read-cctivity on process fd
 */
static int process_on_read (struct process *process, enum proto_channel channel, int fd)
{
    char buf[4096];
    ssize_t ret;
    struct client *client;

    // read chunk
    if ((ret = read(fd, buf, sizeof(buf))) < 0)
        goto error;

    else if (ret == 0)
        // eof
        select_loop_del(&process->daemon->select_loop, &process->std_out);

    // pass off to each attached client
    LIST_FOREACH(client, &process->clients, process_clients) {
        // callback
        client_on_process_data(process, channel, buf, ret, client);
    }
    
    // ok
    return 0;

error:
    // XXX: kill process?
    return -1;    
}

/**
 * Activity on process stdout
 */
static int process_on_stdout (int fd, short what, void *ctx)
{
    struct process *process = ctx;
    
    return process_on_read(process, CHANNEL_STDOUT, fd);
}

/**
 * Activity on process stderr
 */
static int process_on_stderr (int fd, short what, void *ctx)
{
    struct process *process = ctx;
    
    return process_on_read(process, CHANNEL_STDERR, fd);
}

/**
 * Set of process's stdin/out/err io info
 */
struct process_io_info {
    int std_in, std_out, std_err;
};

/**
 * Perform exec() with the given parameters
 */
static void _process_exec (const struct process_exec_info *exec_info, const struct process_io_info *io_info)
    __attribute__ ((noreturn));

static void _process_exec (const struct process_exec_info *exec_info, const struct process_io_info *io_info)
{
    // setup stdin/out/err fds
    if (
            dup2(io_info->std_in,  STDIN_FILENO ) < 0
        ||  dup2(io_info->std_out, STDOUT_FILENO) < 0
        ||  dup2(io_info->std_err, STDERR_FILENO) < 0
    )
        FATAL_ERRNO("dup2");

    // close old fds
    if (io_info->std_in != STDIN_FILENO)
        close(io_info->std_in);

    if (io_info->std_out != STDOUT_FILENO)
        close(io_info->std_out);

    if (io_info->std_err != STDERR_FILENO)
        close(io_info->std_err);
    
    // exec
    if (execve(exec_info->path, exec_info->argv, exec_info->envp) < 0)
        // fail
        FATAL_ERRNO("execve: %s", exec_info->path);

    else
        FATAL("execve returned success");
}

/**
 * Spawn a new process and execute with given params
 */
static int process_spawn (struct process *process, const struct process_exec_info *exec_info)
{
    struct process_io_info exec_io, proc_io;

    // verify exec early
    if (access(exec_info->path, X_OK) < 0)
        return -1;

    // create stdin/out/err pipes
    if (
            make_pipe(&exec_io.std_in,  &proc_io.std_in)
        ||  make_pipe(&proc_io.std_out, &exec_io.std_out)
        ||  make_pipe(&proc_io.std_err, &exec_io.std_err)
    )
        goto error;

    // set flags
    if (
            fd_flags(proc_io.std_in,  O_CLOEXEC|O_NONBLOCK)
        ||  fd_flags(proc_io.std_out, O_CLOEXEC|O_NONBLOCK)
        ||  fd_flags(proc_io.std_err, O_CLOEXEC|O_NONBLOCK)
    )
        goto error;

    log_debug("[%p] stdin -> %d, stdout -> %d, stderr -> %d", process, proc_io.std_in, proc_io.std_out, proc_io.std_err);

    // setup proc's io
    process->std_in = proc_io.std_in;

    if (
            select_fd_init(&process->std_out, proc_io.std_out, FD_READ, process_on_stdout, process)
        ||  select_fd_init(&process->std_err, proc_io.std_err, FD_READ, process_on_stderr, process)
    )
        goto error;

    // activate IO
    if (
            select_loop_add(&process->daemon->select_loop, &process->std_out)
        ||  select_loop_add(&process->daemon->select_loop, &process->std_err)
    )
        goto error;

    // perform fork
    if ((process->pid = fork()) < 0)
        goto error;

    else if (process->pid == 0)
        // child performs exec()
        _process_exec(exec_info, &exec_io);

    else
        // child started, parent continues
        return 0;

error:
    // XXX: close pipes?
    return -1;
}

int process_start (struct daemon *daemon, struct process **proc_ptr, const struct process_exec_info *exec_info)
{
    struct process *process;

    // alloc
    if ((process = calloc(1, sizeof(*process))) == NULL)
        return -1;

    // init
    process->daemon = daemon;
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

/**
 * Update process state
 */
static int process_update (struct process *process, int status)
{
    if (WIFEXITED(status)) {
        log_info("[%p] Exited with status=%d", process, WEXITSTATUS(status));

    } else if (WIFSIGNALED(status)) {
        log_info("[%p] Exited with signal=%d", process, WTERMSIG(status));

    } else {
        log_warn("[%p] Unknown status=%d", process, status);
    }

    return 0;
}

int process_reap (struct daemon *daemon)
{
    pid_t pid;
    int status;
    struct process *process;

    // figure out which child(ren) want(s) our attention
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // find process
        LIST_FOREACH(process, &daemon->processes, daemon_processes) {
            if (process->pid == pid)
                break;
        }

        if (!process) {
            // eek, unknown child!
            log_warn("Unknown child process updated!");

        } else {
            // update
            if (process_update(process, status))
                return -1;
        }
    }

    // ignore ECHILD: no children to wait on
    if (pid < 0 && errno != ECHILD)
        // uh oh
        return -1;

    else // pid >= 0
        // ok
        return 0;
}

