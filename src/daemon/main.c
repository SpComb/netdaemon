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
struct daemon daemon_state;

int main (int argc, char **argv)
{
    const char *service_unix_path = SERVICE_UNIX_PATH_DEFAULT;

    // init daemon
    if (daemon_init(&daemon_state)) {
        log_errno("daemon_init");

        goto error;
    }

    // open service
    if (daemon_service_unix(&daemon_state, service_unix_path) < 0) {
        log_errno("daemon_service_unix: %s", service_unix_path);
        
        goto error;

    }

    log_info("Started service on UNIX socket: %s", service_unix_path);

    // run select loop
    log_info("Entering main loop");

    if (daemon_main(&daemon_state) < 0) {
        log_errno("daemon_state");

        goto error;
    }

    // done
    return EXIT_SUCCESS;

error:
    return EXIT_FAILURE;
}
