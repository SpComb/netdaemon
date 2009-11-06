#include "lib/client.h"
#include "shared/log.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

int main (int argc, char **argv)
{
    struct nd_client *client;
    int err;

    // XXX: parse args
    const char *client_unix_path = argv[1];

    // create
    if (nd_create(&client, NULL, NULL)) {
        log_errno("nd_create");

        goto error;
    }
    
    // connect
    if (nd_open_unix(client, client_unix_path) < 0) {
        log_errno("nd_open_unix: %s", client_unix_path);

        goto error;
    }

    log_info("Connected to UNIX socket: %s", client_unix_path) ;

    // greet
    if (nd_cmd_hello(client) < 0) {
        log_errno("nd_cmd_hello");

        goto error;
    }

    // start a new process
    if ((err = nd_start(client, argv[2], argv + 3, ((const char *[]){ NULL })) < 0)) {
        log_errno("nd_start: %s", nd_error_msg(client));

        goto error;

    } else if (err > 0) {
        log_error("nd_start: %d: %s", nd_error(client), nd_error_msg(client));

        goto error;
    }

    // yay
    log_info("Attached to process: %s", nd_process_id(client));


    // ok
    return EXIT_SUCCESS;

error:    
    return EXIT_FAILURE;
}
