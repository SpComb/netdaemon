#include "service.h"

// lib

#include <stdlib.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>

static const char *SERVICE_UNIX_PATH_DEFAULT = "run/netdaemon";


int main (int argc, char **argv)
{
    struct service *service;
    
    const char *service_unix_path = SERVICE_UNIX_PATH_DEFAULT;

    // open service
    if (service_open_unix(&service, service_unix_path) < 0) {
        fprintf(stderr, "service_open_unix: %s: %s\n", service_unix_path, strerror(errno));

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
