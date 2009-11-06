#include "commands.h"
#include "client_internal.h"

#include <errno.h>

#include <stdlib.h> // XXX: for abort()

// handle both CMD_ERROR and CMD_ABORT
static int cmd_error_abort (struct proto_msg *in, struct proto_msg *unused, void *ctx)
{
    struct nd_client *client = ctx;
    int32_t err_code;
    uint16_t len;
    char *err_msg;
    
    // read info
    if (
            proto_read_int32(in, &err_code)
        ||  proto_read_uint16(in, &len)
    )
        return -1;

    // alloc storage for error message
    if ((err_msg = nd_store_error(client, err_code, len)) == NULL)
        return -1;

    // read error message
    if (_proto_read_str(in, err_msg, len))
        return -1;
    
    switch (in->cmd) {
        case CMD_ABORT:    
            // ok - this was a failure
            errno = err_code;

            return -1;

        case CMD_ERROR:
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
    // nothing more to it
    return 0;
}

// attached to process
static int cmd_attached (struct proto_msg *in, struct proto_msg *unused, void *ctx)
{
    struct nd_client *client = ctx;

    uint16_t len;
    char *id;

    if (proto_read_uint16(in, &len))
        return -1;

    // alloc storage for ID
    if ((id = nd_store_process_id(client, len)) == NULL)
        return -1;

    // read process ID
    if (_proto_read_str(in, id, len))
        return -1;

    // yay
    return 0;
}

struct proto_cmd_handler client_command_handlers[] = {
    { CMD_OK,           cmd_ok                  },
    { CMD_ERROR,        cmd_error_abort         },
    { CMD_ABORT,        cmd_error_abort         },
    { 0,                NULL                    },
};

