#ifndef DAEMON_SERVICE_H
#define DAEMON_SERVICE_H

/**
 * @file
 *
 * Remote services, listening sockets.
 */
#include "shared/select.h"

/**
 * Listen backlog used
 */
#define SERVICE_LISTEN_BACKLOG 5

/**
 * A listening service socket that accept()'s connections.
 */
struct service {
    /** socket IO info */
    struct select_fd fd;
};

/**
 * Construct a new service listen()'ing on the UNIX socket at the given path.
 *
 * This uses an AF_UNIX SOCK_SEQPACKET to handle connection-oriented ordered message streams.
 *
 * @param service_ptr returned service struct
 * @param path path to UNIX socket
 * @return zero on success, <0 on error
 */
int service_open_unix (struct service **service_ptr, const char *path);



/**
 * Close the service socket and release any resources associated with the service itself
 */
void service_destroy (struct service *service);

#endif
