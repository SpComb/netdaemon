#ifndef DAEMON_DAEMON_H
#define DAEMON_DAEMON_H

/**
 * Daemon-wide state
 */
#include <sys/queue.h>

#include "service.h"
#include "process.h"
#include "shared/select.h"

struct daemon {
    /** List of service-ports */
    LIST_HEAD(daemon_services, service) services;

    /** List of running processes */
    LIST_HEAD(daemon_processes, process) processes;

    /** I/O reactor */
    struct select_loop select_loop;

    /** Still running? */
    bool running;
};

/**
 * Initialize daemon-state for use
 */
int daemon_init (struct daemon *daemon);

/**
 * Start an AF_UNIX service on the given path
 */
int daemon_service_unix (struct daemon *daemon, const char *path);

/**
 * Start a new process with the given parameters.
 */
int daemon_process_start (struct daemon *daemon, struct process **proc_ptr, const struct process_exec_info *exec_info);

/**
 * Run daemon mainloop
 */
int daemon_main (struct daemon *daemon);

#endif
