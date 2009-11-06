#include "daemon.h"
#include "shared/signal.h"
#include "shared/log.h"

#include <stdlib.h>
#include <errno.h>

/**
 * Signal handler for SIGCHLD
 */
int on_sigchld (void *arg)
{
    struct daemon *daemon = arg;

    log_info("trap");

    // have the process module deal with it
    return process_update(daemon);
}

int on_sigint (void *arg)
{
    struct daemon *daemon = arg;
    
    if (daemon->running) {
        log_info("Shutting down...");

        daemon->running = false;

        return 0;

    } else {
        log_info("Terminating");
        
        return -1;
    }

    return 0;
}

static struct signal_handler sigchld_handler, sigint_handler;


int daemon_init (struct daemon *daemon)
{
    // lists
    LIST_INIT(&daemon->services);
    LIST_INIT(&daemon->processes);

    // signal handlers
    if (
            signal_register(&sigchld_handler, SIGCHLD, on_sigchld, daemon)
        ||  signal_register(&sigint_handler,  SIGINT,  on_sigint,  daemon)
    )
        return -1;
    
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
    int err;

    // set flag
    daemon->running = true;

    // run select loop with signal handling
    while (daemon->running) {
        if ((err = select_loop_run(&daemon->select_loop, NULL)) < 0) {
            if (errno == EINTR) {
                // run signal handlers
                log_debug("select_loop_run: EINTR -> signal_run()");

                if ((err = signal_run()) < 0)
                    // quick exit
                    return -1;

                else if (!err)
                    // no signals handled
                    log_debug("Spurious EINTR, no signals handled...");

            } else {
                // fail
                return err;
            }

        } else {
            // XXX: check for signals anyways...
            if ((err = signal_run()) < 0)
                // quick exit
                return -1;

            else if (err)
                // no EINTR reported!
                log_debug("Signal without EINTR!");
        }
    }

    log_info("Exited main loop, cleaning up...");

    // XXX: clean up

    return 0;
}

