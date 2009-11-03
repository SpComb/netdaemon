#include "lib/client.h"
#include "shared/log.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

int main (int argc, char **argv)
{
    struct nd_client *client;

    // XXX: parse args
    const char *client_unix_path = argv[1];
    
    // connect
    if (nd_open_unix(&client, client_unix_path) < 0) {
        fprintf(stderr, "nd_open_unix: %s: %s", client_unix_path, strerror(errno));

        return EXIT_FAILURE;
    }

    // greet
    if (nd_cmd_hello(client) < 0) {
        log_errno("nd_cmd_hello");

        return EXIT_FAILURE;
    }

    // send exec command
    if (nd_cmd_exec(client,
        "foo",
        ((const char *[]){ "arg1", "arg2", NULL }),
        ((const char *[]){ "foo=bar", NULL })
    )) {
        log_errno("nd_cmd_exec");

        return EXIT_FAILURE;
    }

    sleep(10);

    // ok
    return EXIT_SUCCESS;

}
