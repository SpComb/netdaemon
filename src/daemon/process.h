#ifndef DAEMON_PROCESS_H
#define DAEMON_PROCESS_H

/**
 * @file
 */
#include <sys/types.h>
#include <sys/queue.h>
#include <shared/select.h>

/**
 * Per-process communication channels
 */
enum process_fd {
    PROCESS_STDIN   = 0 /* STDIN_FILENO */,
    PROCESS_STDOUT  = 1 /* STDOUT_FILENO */,
    PROCESS_STDERR  = 2 /* STDERR_FILENO */,
};

/**
 * Info required for process exec
 */
struct process_exec_info {
    /** Path to executable */
    char *path;
    
    /** NULL-termianted list of argument strings, including argv[0] */
    char **argv;
    
    /** NULL-terminated list of environment strings */
    char **envp;
};

/**
 * Per-process state
 */
struct process {
    /** Daemon state we are running under */
    struct daemon *daemon;

    /** Currently running process ID */
    pid_t pid;

    /** stdin fd */
    int std_in;

    /** stdout/err fds in select loop */
    struct select_fd std_out, std_err;

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
const char *process_id (struct process *proccess);

/**
 * Attach this client to this process, streaming out stdout/err data
 */
int process_attach (struct process *process, struct client *client);

/**
 * Detach given process
 */
void process_detach (struct process *process, struct client *client);

#endif
