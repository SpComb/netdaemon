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

    /** Last status */
    enum proto_process_status status;
    int status_code;
};

/**
 * Allocate and return storage for new error msg
 */
int nd_store_error (struct nd_client *client, int err_code, const char *err_msg);

/**
 * Allocate and return storage for new process ID of given length (plus one NUL byte).
 */
int nd_store_process_id (struct nd_client *client, const char *process_id);

/**
 * Update cached process status
 */
int nd_update_status (struct nd_client *client, enum proto_process_status status, int code);


#endif
