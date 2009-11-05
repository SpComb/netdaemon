#ifndef NETDAEMON_LIB_CLIENT_H
#define NETDAEMON_LIB_CLIENT_H

/**
 * @file
 *
 * Client interface for remote netdaemon
 */

/**
 * Per-client state for the connection to the server
 */
struct nd_client {
    /** The communication socket */
    int sock;
};

/**
 * Create a new client connection to the server over an UNIX socket at the given path.
 *
 * @param client_ptr returned nd_client struct
 * @param path path to UNIX socket to connect() to
 *
 * @return zero on success, <0 on error
 */
int nd_open_unix (struct nd_client **client_ptr, const char *path);

/**
 * Send a CMD_HELLO message to the service
 */
int nd_cmd_hello (struct nd_client *client);

/**
 * Send a CMD_START message to the service
 */
int nd_cmd_start (struct nd_client *client, const char *path, const char *argv[], const char *envp[]);

/**
 * Force-close the given client connection, releasing any allocated resources and invalidating the nd_client*.
 *
 * @param client opened nd_client to release
 */
void nd_destroy (struct nd_client *client);

#endif
