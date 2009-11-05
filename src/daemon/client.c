#include "client.h"
#include "globals.h"
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
    char buf[512];
    struct proto_msg msg;

    // init with CMD_HELLO
    if (proto_cmd_init(&msg, buf, sizeof(buf), CMD_ERROR))
        goto error;

    // 

    // add fields
    if (
            proto_write_int32(&msg, error)
        ||  proto_write_str(&msg, strerror(error))
    )
        goto error;

    // send
    if (client_msg(client, &msg))
        goto error;
    
    // dispose
    if (client_disconnected(client))
        goto error;

    // ok
    return 0;

error:
    return -1;
}


/**
 * [Client -> Server] CMD_HELLO
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
 * [Client -> Server] CMD_EXEC
 */
static int client_on_exec (struct proto_msg *msg, void *ctx)
{
    struct client *client = ctx;
    uint16_t len, argv_len;
    struct process_exec_info exec_info;
    char *arg, *env;
    int i;

    // verify that we are not attached to any process already
    if (client->proc)
        return EBUSY;
    
    // read path
    if (
            proto_read_uint16(msg, &len)
        ||  !(exec_info.path = alloca(len + 1))
        ||  _proto_read_str(msg, exec_info.path, len)
    )
        goto error;
    
    // read argv
    if (
            proto_read_uint16(msg, &argv_len)
        ||  !(exec_info.argv = alloca((1 + argv_len + 1) * sizeof(char *)))
    )
        goto error;

    // store argv[0]
    exec_info.argv[0] = exec_info.path;

    // log
    log_info("path=%u:%s, argv=%u:", len, exec_info.path, argv_len);

    for (i = 0; i < argv_len; i++) {
        // read arg
        if (
                proto_read_uint16(msg, &len)
            ||  !(arg = alloca(len + 1))
            ||  _proto_read_str(msg, arg, len)
        )
            goto error;       

        log_info("\targv[%i] : %s", i + 1, arg);

        exec_info.argv[i + 1] = arg;
    }

    // terminate
    exec_info.argv[i] = NULL;
    
    // XXX: envp
    exec_info.envp = alloca(1 * sizeof(char *));
    exec_info.envp[0] = NULL;

    // spawn new process
    if (process_create(&client->proc, &exec_info))
        goto error;
    
    // yay
    return 0;

error:
    return -1;
}

/**
 * Server-side command handlers
 */
struct proto_cmd_handler client_cmd_handlers[] = {
    {   CMD_HELLO,      client_on_hello     },
    {   CMD_EXEC,       client_on_exec      },
    {   0,              0                   }
};

/**
 * Client got a message
 */
static int client_on_msg (struct client *client, struct proto_msg *msg)
{
    int err = 0;

    // dispatch to command handler
    // XXX: what form of error handling?
    if ((err = proto_cmd_dispatch(client_cmd_handlers, msg, client)) < 0)
        // fatal error
        return client_error(client, errno);

    else if (err)
        // non-fatal error
        return client_error(client, err);

    // ok
    return err;
}

/**
 * Callback for readable SOCK_SEQPACKET socket
 */
static int client_on_read_seqpacket (int fd, short what, void *arg)
{
    struct client *client = arg;
    struct proto_msg msg;    
    char buf[ND_PROTO_MSG_MAX];

    // XXX: recv more than one!
    // init msg
    if (proto_msg_init(&msg, buf, sizeof(buf)))
        goto error;

    // recv
    if (proto_recv_seqpacket(fd, &msg))
        goto error;

    // handle message
    return client_on_msg(client, &msg);

error:
    // XXX: how return to select_loop?
    return client_disconnected(client);
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

