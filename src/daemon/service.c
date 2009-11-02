#include "service.h"
#include "client.h"
#include "globals.h"
#include "shared/log.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <errno.h>

/**
 * Service socket's fd
 */
static inline int service_sock (struct service *service)
{
    return service->fd.fd;
}

/**
 * Get service name
 */
static const char *service_name (struct service *service)
{
    static char name[512];
    struct sockaddr_un sa;
    socklen_t sa_len = sizeof(sa);

    if (getsockname(service_sock(service), (struct sockaddr *) &sa, &sa_len) < 0)
        return "";

    strncpy(name, sa.sun_path, sizeof(name));

    return name;
}

/**
 * Select handler for accept() on service socket
 */
static int service_on_accept (int fd, short what, void *arg)
{
    struct service *service = arg;

    int client_sock;

    // try accept()
    if ((client_sock = accept(fd, NULL, 0)) < 0)
        return SELECT_ERR;

    log_info("Accept service connection on [%s]: fd=%d", service_name(service), client_sock);
    
    // construct client state
    if (client_add_seqpacket(client_sock) < 0) {
        log_warn("Dropping client connection: client_add_seqpacket: %s", strerror(errno));

        close(client_sock);

        return SELECT_ERR;
    }

    return SELECT_OK;
}

int service_open_unix (struct service **service_ptr, const char *path)
{
    struct service *service = NULL;
    struct sockaddr_un sa;
    struct stat st;
    int sock;

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
    if ((sock = socket(AF_UNIX, SOCK_SEQPACKET, 0)) < 0)
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
    if (bind(sock, (struct sockaddr *) &sa, SUN_LEN(&sa)) < 0)
        goto error;

    // and do listen
    if (listen(sock, SERVICE_LISTEN_BACKLOG) < 0)
        goto error;

    // init selectable
    select_fd_init(&service->fd, sock, FD_READ, service_on_accept, service);

    // activate
    select_loop_add(&daemon_select_loop, &service->fd);

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
    // remove from select loop
    select_loop_del(&daemon_select_loop, &service->fd);
    
    // close socket
    if (service->fd.fd >= 0)
        close(service->fd.fd);
    
    // release
    free(service);
}

