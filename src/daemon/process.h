#ifndef DAEMON_PROCESS_H
#define DAEMON_PROCESS_H

/**
 * @file
 */
#include <sys/types.h>
#include <sys/queue.h>


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
    /** Currently running process ID */
    pid_t pid;

    /** Member of daemon process list */
    LIST_ENTRY(process) daemon_processes;
};

/**
 * Construct a new process state, and fork off to exec it with the given arguments/environment.
 *
 * @param proc_ptr  returned process struct
 * @param exec_info info required for exec
 */
int process_start (struct process **proc_ptr, const struct process_exec_info *exec_info);


#endif
