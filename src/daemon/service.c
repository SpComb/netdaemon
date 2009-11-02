#include "service.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <errno.h>

int service_open_unix (struct service **service_ptr, const char *path)
{
    struct service *service = NULL;
    struct sockaddr_un sa;
    struct stat st;

    // validate
    if (strlen(path) >= sizeof(sa.sun_path)) {
        errno = ENAMETOOLONG;

        return -1;
    }
    
    // prep sockaddr
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, path);

    // alloc
    if ((service = calloc(1, sizeof(*service))) == NULL)
        goto error;  // ENOMEM


    // construct socket
    if ((service->sock = socket(AF_UNIX, SOCK_SEQPACKET, 0)) < 0)
        goto error;
    
    // test for existing socket
    if (stat(path, &st) < 0) {
        if (errno != ENOENT)
            goto error;

    } else if (S_ISSOCK(st.st_mode)) {
        if (unlink(path) < 0)
            goto error;

    } else {
        // exists as non-socket file
        errno = ENOTSOCK;

        goto error;
    }

    // bind to path
    if (bind(service->sock, (struct sockaddr *) &sa, SUN_LEN(&sa)) < 0)
        goto error;

    // and do listen
    if (listen(service->sock, SERVICE_LISTEN_BACKLOG) < 0)
        goto error;

    // ok
    *service_ptr = service;
    
    return 0;

error:
    // cleanup
    if (service)
        service_destroy(service);

    return -1;
}

void service_destroy (struct service *service)
{
    if (service->sock >= 0)
        close(service->sock);

    free(service);
}
