#include "service.h"
#include "globals.h"
#include "shared/log.h"

// lib

#include <stdlib.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>

static const char *SERVICE_UNIX_PATH_DEFAULT = "run/netdaemon";

/**
 * Globals
 */
struct select_loop daemon_select_loop;

int main (int argc, char **argv)
{
    struct service *service;
    
    const char *service_unix_path = SERVICE_UNIX_PATH_DEFAULT;

    // init
    select_loop_init(&daemon_select_loop);

    // open service
    if (service_open_unix(&service, service_unix_path) < 0) {
        log_errno("service_open_unix: %s", service_unix_path);
        
        goto error;

    }

    log_info("Started service on UNIX socket: %s", service_unix_path);

    // run select loop
    log_info("Entering main loop");

    if (select_loop_main(&daemon_select_loop) < 0) {
        log_errno("select_loop_main");

        goto error;
    }

    return EXIT_SUCCESS;

error:
    return EXIT_FAILURE;
}
