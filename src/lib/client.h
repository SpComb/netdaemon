#ifndef NETDAEMON_CLIENT_H
#define NETDAEMON_CLIENT_H

/**
 * @file
 *
 * Client interface for netdaemon
 */
#include <sys/time.h>

/**
 * Per-connection handle
 */
struct nd_client;

/**
 * User callbacks for events recieved from daemon
 */
struct nd_callbacks {
    /**  */
};

/**
 * Construct a new client state tied to given callbacks
 *
 * @param client_ptr    returned nd_client struct
 * @param cb_funcs      table of event callbacks
 * @param cb_arg        callback context argument
 */
int nd_create (struct nd_client **client_ptr, struct nd_callbacks *cb_funcs, void *cb_arg);

/**
 * Open the client connection to the server over an UNIX socket at the given path.
 *
 * @param path path to UNIX socket to connect() to
 *
 * @return zero on success, <0 on error
 */
int nd_open_unix (struct nd_client *client, const char *path);

/**
 * Send a CMD_HELLO message to the service
 *
 * XXX: do this automatically
 */
int nd_cmd_hello (struct nd_client *client);

/**
 * Send a CMD_START message to the service.
 *
 * This will launch a new process on the daemon, and automatically attach to it. Use nd_process_id() to retrieve the
 * new process's ID.
 *
 * @param path          path to executable
 * @param argv          NULL-terminated array of arguments, not including argv[0]
 * @param envp          NULL-terminated array of environment strings
 * @return zero on success, <0 on internal error, >0 on command error
 */
int nd_start (struct nd_client *client, const char *path, const char **argv, const char **envp);

/**
 * Get the ID of the currently attached process as a NUL-termintated string, or NULL if not attached.
 *
 * String is valid until the process ID next changes.
 */
const char *nd_process_id (struct nd_client *client);

/**
 * Poll for any event with the given timeout
 *
 * @param tv            timeout to wait for an event to happen
 */
int nd_poll (struct nd_client *client, struct timeval *tv);

/**
 * Return the error code associated with the most recent operation. This will either be zero or a positive integer.
 */
int nd_error (struct nd_client *client);

/**
 * Return the NUL-terminated error message aassociated with the most recent operation.
 *
 * Returned string is valid until the next error occurs.
 */
const char *nd_error_msg (struct nd_client *client);

/**
 * Force-close the given client connection, releasing any allocated resources and invalidating the nd_client*.
 */
void nd_destroy (struct nd_client *client);

#endif
