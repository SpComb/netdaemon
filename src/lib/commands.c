#include "commands.h"
#include "client_internal.h"
#include "shared/log.h" // only log_debug

#include <errno.h>

#include <stdlib.h> // XXX: for abort()

// handle both CMD_ERROR and CMD_ABORT
static int cmd_error_abort (struct proto_msg *in, struct proto_msg *unused, void *ctx)
{
    struct nd_client *client = ctx;
    int32_t err_code;
    const char *err_msg;
    
    if (
            proto_read_int32(in, &err_code)
        ||  proto_read_str(in, &err_msg)
    )
        return -1;

    // store error message
    if (nd_store_error(client, err_code, err_msg))
        return -1;

    switch (in->cmd) {
        case CMD_ABORT:
            log_debug("CMD_ABORT: id=%d, err_code=%d, err_msg=%s", in->id, err_code, err_msg);

            // ok - this was a failure
            errno = err_code;

            return -1;

        case CMD_ERROR:
            log_debug("CMD_ERROR: id=%d, err_code=%d, err_msg=%s", in->id, err_code, err_msg);

            // soft error
            return err_code;

        default:
            // eh?
            abort();
    }
}

// command executed ok
static int cmd_ok (struct proto_msg *in, struct proto_msg *unused, void *ctx)
{
    log_debug("CMD_OK: id=%d", in->id);

    // nothing more to it
    return 0;
}

// attached to process
static int cmd_attached (struct proto_msg *in, struct proto_msg *unused, void *ctx)
{
    struct nd_client *client = ctx;

    const char *process_id;
    uint16_t status, status_code;
    
    if (
            proto_read_str(in, &process_id)
        ||  proto_read_uint16(in, &status)
        ||  proto_read_uint16(in, &status_code)
    )
        return -1;

    log_debug("CMD_ATTACHED: id=%d, process_id=%s, status=%d:%d", in->id, process_id, status, status_code);

    // store new ID
    if (nd_store_process_id(client, process_id))
        return -1;

    // yay
    return 0;
}

// data from process
static int cmd_data (struct proto_msg *in, struct proto_msg *unused, void *ctx)
{
    struct nd_client *client = ctx;

    uint16_t channel;
    uint16_t len;
    char *buf;

    // read header
    if (
            proto_read_uint16(in, &channel)
        ||  proto_read_uint16(in, &len)
    )
        return -1;

    // alloc storage
    if ((buf = alloca(len + 1)) == NULL)
        return -1;

    // read data
    if (proto_read(in, buf, len))
        return -1;

    // terminate NUL for convenience
    buf[len] = '\0';

    // report
    log_debug("CMD_DATA: channel=%u, data=%u:%.*s", channel, len, (int) len, buf);

    // callback
    switch (channel) {
        case CHANNEL_STDOUT:
            return client->cb_funcs.on_stdout(client, buf, len, client->cb_arg);
        
        case CHANNEL_STDERR:
            return client->cb_funcs.on_stderr(client, buf, len, client->cb_arg);

        default:
            // unknown channel
            errno = ECHRNG;

            return -1;
    }
}

// process status changed
static int cmd_status (struct proto_msg *in, struct proto_msg *unused, void *ctx)
{
    struct nd_client *client = ctx;

    uint16_t status, code;

    // read
    if (
            proto_read_uint16(in, &status)
        ||  proto_read_uint16(in, &code)
    )
        return -1;

    // update
    log_debug("CMD_STATUS: status=%d, code=%d", status, code);

    if (nd_update_status(client, status, code))
        return -1;

    switch (status) {
        case PROCESS_RUN:
            // XXX: ignore
            return 0;

        case PROCESS_EXIT:
            // callback
            return client->cb_funcs.on_exit(client, code, client->cb_arg);

        case PROCESS_KILL:
            // callback
            return client->cb_funcs.on_kill(client, code, client->cb_arg);

        default:
            // wtf
            errno = EINVAL;

            return -1;
    }
}

struct proto_cmd_handler client_command_handlers[] = {
    { CMD_STATUS,       cmd_status              },
    { CMD_DATA,         cmd_data                },
    { CMD_ATTACHED,     cmd_attached            },
    { CMD_OK,           cmd_ok                  },
    { CMD_ERROR,        cmd_error_abort         },
    { CMD_ABORT,        cmd_error_abort         },
    { 0,                NULL                    },
};

