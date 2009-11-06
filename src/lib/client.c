#define _GNU_SOURCE
#include "client_internal.h"
#include "shared/proto.h"
#include "commands.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>

/**
 * Calc and return the next message ID to use
 */
static proto_msg_id_t nd_msg_id (struct nd_client *client)
{
    return ++client->last_id;
}

/**
 * Send a proto_msg to the service
 */
static int nd_send_msg (struct nd_client *client, struct proto_msg *msg)
{
    return proto_send_seqpacket(client->sock, msg);
}

int nd_create (struct nd_client **client_ptr, const struct nd_callbacks *cb_funcs, void *cb_arg)
{
    struct nd_client *client;

    // alloc
    if ((client = calloc(1, sizeof(*client))) == NULL)
        return -1;  // ENOMEM

    // init
    client->sock = -1;
    client->last_id = 1;

    // store
    client->cb_funcs = *cb_funcs;
    client->cb_arg = cb_arg;

    // ok
    *client_ptr = client;

    return 0;
}

int nd_open_unix (struct nd_client *client, const char *path)
{
    struct sockaddr_un sa;

    // validate
    if (strlen(path) >= sizeof(sa.sun_path)) {
        errno = ENAMETOOLONG;

        return -1;
    }

    // not already connected
    if (client->sock != -1) {
        errno = EALREADY;

        return -1;
    }
    
    // prep sockaddr
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, path);

    // construct socket
    if ((client->sock = socket(AF_UNIX, SOCK_SEQPACKET, 0)) < 0)
        goto error;

    // connect
    if (connect(client->sock, (struct sockaddr *) &sa, SUN_LEN(&sa)) < 0)
        goto error;

    return 0;

error:
    // cleanup
    if (client->sock != -1) {
        close(client->sock);

        client->sock = -1;
    }

    return -1;
}

int nd_cmd_hello (struct nd_client *client)
{
    char buf[512];
    struct proto_msg msg;

    // init with CMD_HELLO
    if (proto_cmd_init(&msg, buf, sizeof(buf), /* nd_msg_id(client) */ 0, CMD_HELLO))
        return -1;

    // add current proto version
    if (proto_write_uint16(&msg, PROTO_VERSION))
        return -1;

    // send
    return nd_send_msg(client, &msg);
}

int nd_cmd_start (struct nd_client *client, const char *path, const char **argv, const char **envp)
{
    char buf[ND_PROTO_MSG_MAX];
    struct proto_msg msg;

    // start CMD_EXEC
    if (proto_cmd_init(&msg, buf, sizeof(buf), nd_msg_id(client), CMD_START))
        goto error;

    // write fields
    if (
            proto_write_str(&msg, path)
        ||  proto_write_str_array(&msg, argv)
        ||  proto_write_str_array(&msg, envp)
    )
        goto error;
    
    // send
    if (nd_send_msg(client, &msg))
        goto error;

    // ok
    return 0;

error:
    return -1;
}

int nd_cmd_data (struct nd_client *client, enum proto_channel channel, const char *buf, size_t len)
{
    char msg_buf[ND_PROTO_MSG_MAX];
    struct proto_msg msg;

    if (proto_cmd_init(&msg, msg_buf, sizeof(msg_buf), nd_msg_id(client), CMD_DATA))
        goto error;

    // write fields
    if (
            proto_write_uint16(&msg, channel)
        ||  proto_write_buf(&msg, buf, len)
    )
        goto error;
    
    // send
    if (nd_send_msg(client, &msg))
        goto error;

    // ok
    return 0;

error:
    return -1;
}

int nd_cmd_attach (struct nd_client *client, const char *process_id)
{
    char msg_buf[4096];
    struct proto_msg msg;

    if (proto_cmd_init(&msg, msg_buf, sizeof(msg_buf), nd_msg_id(client), CMD_ATTACH))
        return -1;
    
    if (proto_write_str(&msg, process_id))
        return -1;

    if (nd_send_msg(client, &msg))
        return -1;

    // ok
    return 0;
}

int nd_cmd_list (struct nd_client *client)
{
    char msg_buf[4096];
    struct proto_msg msg;

    if (proto_cmd_init(&msg, msg_buf, sizeof(msg_buf), nd_msg_id(client), CMD_LIST))
        return -1;
    
    if (nd_send_msg(client, &msg))
        return -1;

    // ok
    return 0;
}

int nd_cmd_kill (struct nd_client *client, int sig)
{
    char msg_buf[512];
    struct proto_msg msg;

    if (proto_cmd_init(&msg, msg_buf, sizeof(msg_buf), nd_msg_id(client), CMD_KILL))
        return -1;
    
    if (proto_write_uint16(&msg, sig))
        return -1;

    if (nd_send_msg(client, &msg))
        return -1;

    // ok
    return 0;
}

/**
 * Poll for activity using select().
 *
 * @return <0 on error, timeout, 0 on activity
 */
static int nd_poll_select (struct nd_client *client, struct timeval *tv)
{
    int ret;

    fd_set rfds;

    // set
    FD_ZERO(&rfds);
    FD_SET(client->sock, &rfds);

    // poll
    if ((ret = select(client->sock + 1, &rfds, NULL, NULL, tv)) < 0) {
        return -1;

    } else if (ret == 0) {
        // timeout
        errno = ETIMEDOUT;
        
        return -1;

    } else {
        // ok, activity on sock
        return 0;
    }
}

/**
 * Recieve one message using the given timeout.
 *
 * In case of internal error, connection abort or timeout, return -1. In case we handled an event message, return 0. In
 * case we handled a command reply (success or command-error stored in ->last_res), return 1.
 */
static int nd_poll_internal (struct nd_client *client, struct timeval *tv)
{
    struct proto_msg msg;
    char buf[ND_PROTO_MSG_MAX];
    int err;

    if (tv)
        // wait timeout
        if (nd_poll_select(client, tv))
            return -1;

    // setup msg buf
    if (proto_msg_init(&msg, buf, sizeof(buf)))
        return -1;

    // recieve the message
    if (proto_recv_seqpacket(client->sock, &msg))
        return -1;

    // handle it
    if ((err = proto_cmd_dispatch(client_command_handlers, &msg, NULL, client)) < 0) {
        // internal error
        return -1;

    } else if (msg.id) {
        // should only have one outstanding command at a time
        if (msg.id == client->last_id) {
            // command response
            client->last_res = err;

            return 1;

        } else {
            // command mismatch!
            errno = EBADMSG;

            return -1;
        }

    } else {
        client->last_res = err;

        // event
        return 0;
    }
}

/**
 * Handle messages until we get a reply to the command that we just sent, then handle it and return the appropriate
 * error code.
 */
static int nd_poll_cmd (struct nd_client *client)
{
    struct timeval *tv = NULL;
    int err;

    // XXX: use a "default" timeout

    // poll until we get something other than a routine event
    while ((err = nd_poll_internal(client, tv)) == 0)
        ;

    if (err < 0)
        // instant failure
        return -1;
    
    else // err > 0
        // got message reply
        return client->last_res;
}

int nd_start (struct nd_client *client, const char *path, const char **argv, const char **envp)
{
    // send the command
    if (nd_cmd_start(client, path, argv, envp))
        return -1;

    // wait for and return reply
    return nd_poll_cmd(client);
}

int nd_attach (struct nd_client *client, const char *process_id)
{
    // send the command
    if (nd_cmd_attach(client, process_id))
        return -1;

    // wait for and return reply
    return nd_poll_cmd(client);
}

int nd_list (struct nd_client *client)
{
    // send the command
    if (nd_cmd_list(client))
        return -1;

    // wait for and return reply
    return nd_poll_cmd(client);
}

int nd_stdin_data (struct nd_client *client, const char *buf, size_t len)
{
    // send
    if (nd_cmd_data(client, CHANNEL_STDIN, buf, len))
        return -1;

    // wait for and return reply
    return nd_poll_cmd(client);
}

int nd_stdin_eof (struct nd_client *client)
{
    // send zero
    if (nd_cmd_data(client, CHANNEL_STDIN, "", 0))
        return -1;

    // wait for and return reply
    return nd_poll_cmd(client);
}

int nd_kill (struct nd_client *client, int sig)
{
     // send the command
    if (nd_cmd_kill(client, sig))
        return -1;

    // wait for and return reply
    return nd_poll_cmd(client);   
}

const char *nd_process_id (struct nd_client *client)
{
    return client->process_id;
}

int nd_process_running (struct nd_client *client)
{
    if (!client->process_id)
        return -1;

    return client->status == PROCESS_RUN;
}

int nd_poll_fd (struct nd_client *client, bool *want_read, bool *want_write)
{
    *want_read = true;
    *want_write = false;

    return client->sock;
}

int nd_poll (struct nd_client *client, struct timeval *tv)
{
    int err;
    
    if ((err = nd_poll_internal(client, tv)) < 0)
        return -1;

    else if (err > 0)
        // XXX: unexpected, this indicates that we got a response to a command - we shouldn't have anything like that outstanding
        abort();

    else
        // ok
        return client->last_res;
}

int nd_error (struct nd_client *client)
{
    return client->last_res;
}

const char *nd_error_msg (struct nd_client *client)
{
    return client->err_msg;
}

void nd_destroy (struct nd_client *client)
{
    if (client->sock)
        close(client->sock);

    free(client->err_msg);
    free(client->process_id);

    free(client);
}

int nd_store_error (struct nd_client *client, int err_code, const char *err_msg)
{
    if (client->err_msg)
        // dispose of old one
        free(client->err_msg);

    // store new one
    if ((client->err_msg = strdup(err_msg)) == NULL)
        return -1;

    client->last_res = err_code;

    return 0;
}

int nd_store_process_id (struct nd_client *client, const char *process_id)
{
    if (client->process_id)
        // dispose
        free(client->process_id);

    // alloc new
    if ((client->process_id = strdup(process_id)) == NULL)
        return -1;
    
    return 0;
}

int nd_update_status (struct nd_client *client, enum proto_process_status status, int code)
{
    // store
    client->status = status;
    client->status_code = code;

    return 0;
}
