#include "client.h"
#include "shared/proto.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <errno.h>

/**
 * Send a proto_msg to the service
 */
int nd_send_msg (struct nd_client *client, struct proto_msg *msg)
{
    return proto_send_seqpacket(client->sock, msg);
}

int nd_open_unix (struct nd_client **client_ptr, const char *path)
{
    struct nd_client *client;
    struct sockaddr_un sa;

    // validate
    if (strlen(path) >= sizeof(sa.sun_path)) {
        errno = ENAMETOOLONG;

        return -1;
    }
    
    // prep sockaddr
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, path);

    // alloc
    if ((client = calloc(1, sizeof(*client))) == NULL)
        goto error;  // ENOMEM


    // construct socket
    if ((client->sock = socket(AF_UNIX, SOCK_SEQPACKET, 0)) < 0)
        goto error;

    // connect
    if (connect(client->sock, (struct sockaddr *) &sa, SUN_LEN(&sa)) < 0)
        goto error;


    // ok
    *client_ptr = client;

    return 0;

error:
    // cleanup
    if (client)
        nd_destroy(client);    
    
    return -1;
}

int nd_cmd_hello (struct nd_client *client)
{
    char buf[512];
    struct proto_msg msg;

    // init with CMD_HELLO
    if (proto_cmd_init(&msg, buf, sizeof(buf), 0, CMD_HELLO))
        return -1;

    // add current proto version
    if (proto_write_uint16(&msg, PROTO_VERSION))
        return -1;

    // send
    return nd_send_msg(client, &msg);
}

int nd_cmd_start (struct nd_client *client, const char *path, const char *argv[], const char *envp[])
{
    char buf[ND_PROTO_MSG_MAX];
    struct proto_msg msg;

    // start CMD_EXEC
    if (proto_cmd_init(&msg, buf, sizeof(buf), 0, CMD_START))
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

void nd_destroy (struct nd_client *client)
{
    if (client->sock)
        close(client->sock);

    free(client);
}

