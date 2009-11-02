#ifndef DAEMON_CLIENT_H
#define DAEMON_CLIENT_H

/**
 * @file
 *
 * Client sessions
 */
#include "shared/select.h"

/**
 * Per-client connection state
 */
struct client {
    /** socket IO info */
    struct select_fd fd;
};

/**
 * Construct a new client and activate it.
 *
 * @param client_ptr returned client struct if not NULL
 * @param sock connected socket fd of the SOCK_SEQPACKET type
 */
int client_add_seqpacket (int sock);

#endif
