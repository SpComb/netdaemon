#include "client.h"
#include "globals.h"
#include "shared/log.h"
#include "shared/proto.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

static int client_sock (struct client *client)
{
    return client->fd.fd;
}

/**
 * [Client -> Server] CMD_HELLO :
 *  uint16_t    proto_version
 */
static int client_on_hello (struct proto_msg *msg, void *ctx)
{
    struct client *client = ctx;
    uint16_t proto_version;
    int err = 0;

    if ((err = proto_read_uint16(msg, &proto_version)))
        return err;

    log_info("proto_version=%u", proto_version);

    return err;
}

/**
 * Server-side command handlers
 */
struct proto_cmd_handler client_cmd_handlers[] = {
    {   CMD_HELLO,      client_on_hello     },
    {   0,              0                   }
};


/**
 * Client disconnected, dispose of it
 *
 * @returns SELECT_ERR
 */
static int client_disconnected (struct client *client)
{
    // remove from select loop
    select_loop_del(&daemon_select_loop, &client->fd);
    
    // release the socket
    close(client_sock(client));

    // XXX: breaks socket_loop_run
    // free(client)

    return SELECT_ERR;
}

/**
 * Client oops'd
 */
static int client_error (struct client *client, int error)
{
    // XXX: send an ND_CMD_ERROR message
    
    // dispose
    return client_disconnected(client);
}

/**
 * Client got a message
 */
static int client_on_msg (struct client *client, char *buf, size_t len)
{
    int err = 0;

    // dispatch to command handler
    // XXX: what form of error handling?
    if ((err = proto_cmd_dispatch(client_cmd_handlers, buf, len, client)) < 0)
        return client_error(client, -err);

    // ok
    return err;
}

/**
 * Callback for readable SOCK_SEQPACKET socket
 */
static int client_on_read_seqpacket (int fd, short what, void *arg)
{
    struct client *client = arg;

    char buf[ND_PROTO_MSG_MAX];
    ssize_t len;

    // try recv()
    if ((len = recv(fd, buf, sizeof(buf), MSG_TRUNC)) < 0)
        return SELECT_ERR;
    
    else if (len == 0)
        // eof
        return client_disconnected(client);

    else if (len > sizeof(buf))
        // truncated
        return client_error(client, EMSGSIZE);

    // handle message
    return client_on_msg(client, buf, len);
}

int client_add_seqpacket (int sock)
{
    struct client *client;

    // alloc
    if ((client = calloc(1, sizeof(*client))) == NULL)
        return -1; // ENOMEM


    // init fd state
    select_fd_init(&client->fd, sock, FD_READ, client_on_read_seqpacket, client);

    // activate
    select_loop_add(&daemon_select_loop, &client->fd);

    return 0;
}

