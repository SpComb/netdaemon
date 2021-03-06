#ifndef NETDAEMON_CLIENT_H
#define NETDAEMON_CLIENT_H

/**
 * @file
 *
 * Client interface for netdaemon
 */
#include <sys/time.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * Per-connection handle
 */
struct nd_client;

/**
 * User callbacks for events recieved from daemon
 *
 * These functions should return zero on normal execution, <0 on errno, and >0 to return with some app-specific code
 * from the running nd_* function.
 */
struct nd_callbacks {
    /** Recieved data from process on stdout. In case of EOF, len == 0 */
    int (*on_stdout) (struct nd_client *client, const char *buf, size_t len, void *arg);

    /** Recieved data from process on stderr. Incase of EOF, len == 0 */
    int (*on_stderr) (struct nd_client *client, const char *buf, size_t len, void *arg);

    /** Process exited */
    int (*on_exit) (struct nd_client *client, int status, void *arg);

    /** Process was killed by signal */
    int (*on_kill) (struct nd_client *client, int sig, void *arg);

    /** Process list entry */
    int (*on_list) (struct nd_client *client, const char *proccess_id, int status, int status_code, void *arg);
};

/**
 * Construct a new client state tied to given callbacks
 *
 * @param client_ptr    returned nd_client struct
 * @param cb_funcs      table of event callbacks
 * @param cb_arg        callback context argument
 */
int nd_create (struct nd_client **client_ptr, const struct nd_callbacks *cb_funcs, void *cb_arg);

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
 * Start a new process and automatically attach to it.
 * Use nd_process_id to retreieve the new process's ID.
 *
 * @param path          path to executable
 * @param argv          NULL-terminated array of arguments, not including argv[0]
 * @param envp          NULL-terminated array of environment strings
 * @return zero on success, <0 on internal error, >0 on command error
 */
int nd_start (struct nd_client *client, const char *path, const char **argv, const char **envp);

/**
 * Attach to a pre-existing process using the given name.
 *
 * Fails with ENOENT if the named process does not exist.
 */
int nd_attach (struct nd_client *client, const char *process_id);

/**
 * Retrieve a listing of processes from the server.
 *
 * This will call nd_callbacks.on_list once for each listed process, and then return.
 */
int nd_list (struct nd_client *client);

/**
 * Send data to stdin on the attached process.
 *
 * The data will be written atomically
 */
int nd_stdin_data (struct nd_client *client, const char *buf, size_t len);

/**
 * Send EOF on stdin to the attached process.
 *
 * It is an error to send any more data to the process after this succeeds.
 *
 * XXX: This is equivalent to `nd_stdin_data(client, "", 0)`
 */
int nd_stdin_eof (struct nd_client *client);

/**
 * Send a signal to the attached process
 */
int nd_kill (struct nd_client *client, int sig);

/**
 * Get the ID of the currently attached process as a NUL-termintated string, or NULL if not attached.
 *
 * String is valid until the process ID next changes.
 */
const char *nd_process_id (struct nd_client *client);

/**
 * Is the attached process running?
 *
 * @return <0 if not attached, 0 if not running, >0 if running
 */
int nd_process_running (struct nd_client *client);

/**
 * Return the FD used by nd_client which can be monitored for activity as per want_* before calling nd_poll.
 *
 * @return fd >= 0 when connected, or <0 when not connected
 */
int nd_poll_fd (struct nd_client *client, bool *want_read, bool *want_write);

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
