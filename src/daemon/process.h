#ifndef DAEMON_PROCESS_H
#define DAEMON_PROCESS_H

/**
 * @file
 */
#include "shared/select.h"
#include "shared/proto.h"
#include <sys/types.h>
#include <sys/queue.h>

/**
 * Info required for process exec
 */
struct process_exec_info {
    /** Path to executable */
    const char *path;
    
    /** NULL-termianted list of argument strings, including argv[0] */
    const char **argv;
    
    /** NULL-terminated list of environment strings */
    const char **envp;
};

/**
 * Per-process state
 */
struct process {
    /** Daemon state we are running under */
    struct daemon *daemon;

    /** The process name */
    char *name;

    /** Currently running process ID */
    pid_t pid;

    /** stdin fd */
    int std_in;

    /** stdout/err fds in select loop */
    struct select_fd std_out, std_err;

    /** Current status */
    enum proto_process_status status;
    int status_code;

    /** List of attached clients */
    LIST_HEAD(process_clients, client) clients;

    /** Member of daemon process list */
    LIST_ENTRY(process) daemon_processes;
};

/**
 * Construct a new process state, and fork off to exec it with the given arguments/environment.
 *
 * @param daemon    daemon context
 * @param proc_ptr  returned process struct
 * @param exec_info info required for exec
 */
int process_start (struct daemon *daemon, struct process **proc_ptr, const struct process_exec_info *exec_info);

/**
 * Return the opaque process ID as a NUL-terminated string
 */
static inline const char *process_id (struct process *process)
{
    return process->name;
}



/**
 * Attach this client to this process, streaming out stdout/err data
 */
int process_attach (struct process *process, struct client *client);

/**
 * Detach given process
 */
void process_detach (struct process *process, struct client *client);

/**
 * Send data to process stdin.
 *
 * This garuntees that the given data segment will be written atomically
 */
int process_stdin_data (struct process *process, const char *buf, size_t len);

/**
 * Close the process's stdin
 */
int process_stdin_eof (struct process *process);

/**
 * Send signal to process
 */
int process_kill (struct process *process, int sig);

/**
 * Kill the process and bury the body.
 */
void process_destroy (struct process *process);

/**
 * Poll for changes in process state after SIGCHLD; this will greedily reap all children
 */
int process_reap (struct daemon *daemon);

#endif
