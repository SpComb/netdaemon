#include "service.h"
#include "globals.h"
#include "shared/log.h"

// lib

#include <stdlib.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>

/**
 * Globals
 */
struct daemon daemon_state;

/**
 * Command-line options
 */
static const struct option options[] = {
    { "help",       false,  NULL,   'h' },
    { "quiet",      false,  NULL,   'q' },
    { "verbose",    false,  NULL,   'v' },
    { "debug",      false,  NULL,   'D' },
    { "unix",       true,   NULL,   'u' },
    { 0,            0,      0,      0   }
};

void help (const char *argv0)
{
    fprintf(stderr, "Usage: %s [options]\n", argv0);
}

int main (int argc, char **argv)
{
    const char *service_unix_path = NULL;
    int opt;

    // parse arguments
    while ((opt = getopt_long(argc, argv, "hqvDu:", options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                // display help
                help(argv[0]);
                
                return EXIT_SUCCESS;

            case 'q':
                // supress excess log output
                set_log_level(LOG_WARN);

                break;

            case 'v':
            case 'D':
                // display additional output
                set_log_level(LOG_DEBUG);

                break;

            case 'u':
                // listen on unix service
                service_unix_path = optarg;

                break;

            case '?':
                // useage error
                help(argv[0]);

                return EXIT_FAILURE;

            default:
                // getopt???
                FATAL("getopt_long returned unknown code %d", opt);
        }
    }

    // validate
    if (!service_unix_path)
        EXIT_WARN(EXIT_FAILURE, "No service (--unix) given");


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
        log_errno("daemon_main");

        goto error;
    }

    // done
    return EXIT_SUCCESS;

error:
    return EXIT_FAILURE;
}
