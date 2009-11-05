#include "client.h"
#include "commands.h"
#include "shared/log.h"
#include "shared/proto.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>

static int client_sock (struct client *client)
{
    return client->fd.fd;
}

/**
 * Send the given proto_msg to this client
 */
static int client_msg (struct client *client, struct proto_msg *msg)
{
    return proto_send_seqpacket(client_sock(client), msg);
}

/**
 * Destroy the given client, releasing any resources
 */
void client_destroy (struct client *client)
{
    // remove from select loop if added
    select_loop_del(&client->daemon->select_loop, &client->fd);
    
    // release the socket
    close(client_sock(client));

    // detach from process if attached
    if (client->process)
        process_detach(client->process, client);

    // release state
    free(client);
}

/**
 * Client disconnected, dispose of it
 */
static void client_disconnected (struct client *client)
{
    // XXX: breaks socket_loop_run
    client_destroy(client);
}


/**
 * Fatal client error. Attempt to send a terminal error packet, and close the connection
 */
static void client_abort (struct client *client, int error)
{
    char buf[512];
    struct proto_msg msg;

    // build CMD_ERROR packet
    if (proto_cmd_init(&msg, buf, sizeof(buf), CMD_ABORT, 0))
        goto error;

    // add fields
    if (
            proto_write_int32(&msg, error)
        ||  proto_write_str(&msg, strerror(error))
    )
        goto error;

    // send
    if (client_msg(client, &msg))
        goto error;
    
    // ok
    return;

error:
    // warn
    log_warn_errno("[%p] Unable to send CMD_ABORT", client);
}

/**
 * Reply to the given command message with the given reply code, using either CMD_OK or CMD_ERROR.
 */
static int client_reply (struct client *client, struct proto_msg *req, int error)
{
    // XXX: use reply-buf from client_on_msg instead?
    char buf[512];
    struct proto_msg msg;

    // init out-msg
    if (proto_msg_init(&msg, buf, sizeof(buf)))
        return -1;

    // build CMD_* packet
    if (proto_cmd_reply(&msg, req, error ? CMD_ERROR : CMD_OK))
        return -1;

    // write reply
    if (error && (
            proto_write_int32(&msg, error)
        ||  proto_write_str(&msg, strerror(error))
    ))
        return -1;

    // send
    if (client_msg(client, &msg))
        return -1;

    // ok
    return 0;
}


/**
 * Client got a message.
 *
 * Decode the command packet, dispatch it to the correct command handler and send the appropriate reply.
 *
 * Returns an error in case the client did something horrible and should be disconnected.
 */
static int client_on_msg (struct client *client, struct proto_msg *request)
{
    struct proto_msg reply;
    char buf[ND_PROTO_MSG_MAX];
    int err = 0;

    // prep reply packet
    if (proto_msg_init(&reply, buf, sizeof(buf)))
        goto error;

    // dispatch to command handler
    if ((err = proto_cmd_dispatch(daemon_command_handlers, request, &reply, client)) < 0)
        // system error
        goto error;

    else if (reply.cmd)
        // send reply packet
        err = client_msg(client, &reply);

    else
        // generic reply; err=0 -> success, or err>0 -> non-fatal error reply
        err = client_reply(client, request, err);

    // ok
    return err;
        
error:
    // error while handling req    
    client_abort(client, errno);

    return -1;
}

/**
 * Callback for readable SOCK_SEQPACKET socket
 */
static int client_on_read_seqpacket (int fd, short what, void *arg)
{
    struct client *client = arg;
    struct proto_msg msg;    
    char buf[ND_PROTO_MSG_MAX];

    // XXX: recv more than one packet!
    // init msg
    if (proto_msg_init(&msg, buf, sizeof(buf)))
        goto error;

    // recv
    if (proto_recv_seqpacket(fd, &msg))
        goto error;

    // handle message
    if (client_on_msg(client, &msg))
        goto error;

    // ok
    return 0;

error:
    // disconnect client
    client_disconnected(client);
    
    // XXX: how safely return to select_loop?
    return -1;
}

int client_add_seqpacket (struct daemon *daemon, int sock)
{
    struct client *client;

    // alloc
    if ((client = calloc(1, sizeof(*client))) == NULL)
        return -1; // ENOMEM

    // init
    client->daemon = daemon;

    // init fd state
    select_fd_init(&client->fd, sock, FD_READ, client_on_read_seqpacket, client);

    // activate
    select_loop_add(&daemon->select_loop, &client->fd);

    return 0;
}

