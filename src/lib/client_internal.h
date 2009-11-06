#ifndef LIB_CLIENT_INTERNAL_H
#define LIB_CLIENT_INTERNAL_H

/**
 * Client interface internal details
 */
#include "client.h"
#include "shared/proto.h"

/**
 * Per-client state for the connection to the server
 */
struct nd_client {
    /** The communication socket */
    int sock;

    /** Callback info */
    struct nd_callbacks cb_funcs;
    void *cb_arg;

    /** ID of the last command sent */
    proto_msg_id_t last_id;

    /** Response code to last command */
    int last_res;

    /** Last error message */
    char *err_msg;

    /** Attached process ID */
    char *process_id;
};

/**
 * Allocate and return storage for new error msg of given length (plus one NUL byte).
 *
 * Returns NULL with errno if even that fails.
 */
char *nd_store_error (struct nd_client *client, int err_code, size_t err_msg_len);

/**
 * Allocate and return storage for new process ID of given length (plus one NUL byte).
 */
char *nd_store_process_id (struct nd_client *client, size_t process_id_len);

#endif
