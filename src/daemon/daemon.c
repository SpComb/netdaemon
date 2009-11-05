#include "daemon.h"

#include <stdlib.h>

int daemon_init (struct daemon *daemon)
{
    // lists
    LIST_INIT(&daemon->services);
    LIST_INIT(&daemon->processes);
    
    // select loop
    select_loop_init(&daemon->select_loop);

    // ok
    return 0;
}

int daemon_service_unix (struct daemon *daemon, const char *path)
{
    struct service *service;

    // create
    if (service_open_unix(daemon, &service, path))
        return -1;

    // add
    LIST_INSERT_HEAD(&daemon->services, service, daemon_services);

    // ok
    return 0;
}

int daemon_process_start (struct daemon *daemon, struct process **proc_ptr, const struct process_exec_info *exec_info)
{
    struct process *process;

    // start
    if (process_start(daemon, &process, exec_info))
        return -1;

    // add
    LIST_INSERT_HEAD(&daemon->processes, process, daemon_processes);

    // ok
    *proc_ptr = process;

    return 0;
}

int daemon_main (struct daemon *daemon)
{
    // XXX: signal handling
    return select_loop_main(&daemon->select_loop);
}

