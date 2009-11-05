#ifndef DAEMON_CLIENT_H
#define DAEMON_CLIENT_H

/**
 * @file
 *
 * Client sessions
 */
#include "daemon.h"
#include "process.h"

/**
 * Per-client connection state
 */
struct client {
    /** Daemon we are running under */
    struct daemon *daemon;

    /** socket IO info */
    struct select_fd fd;

    /** Attached process */
    struct process *process;

    /** Process's consumer list */
    LIST_ENTRY(client) process_clients;
};

/**
 * Construct a new client and activate it.
 *
 * @param daemon daemon we are running under
 * @param client_ptr returned client struct if not NULL
 * @param sock connected socket fd of the SOCK_SEQPACKET type
 */
int client_add_seqpacket (struct daemon *daemon, int sock);

#endif
