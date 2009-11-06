#include "commands.h"
#include "client.h"
#include "shared/log.h"
#include "process.h"
#include "errno.h"

/**
 * [Server -> Client] CMD_ATTACH
 *
 * Write out the content of the CMD_ATTACH packet based on the client's current process
 */
static int msg_attached (struct proto_msg *out, struct client *client)
{
    return proto_write_str(out, process_id(client->process));
}

/**
 * [Client -> Server] CMD_HELLO
 */
static int cmd_hello (struct proto_msg *req, struct proto_msg *out, void *ctx)
{
    struct client *client = ctx;
    uint16_t proto_version;

    if (proto_read_uint16(req, &proto_version))
        return -1;

    log_info("proto_version=%u", proto_version);
    
    // XXX: reply with CMD_HELLO
    return 0;
}

/**
 * [Client -> Server] CMD_START
 */
static int cmd_start (struct proto_msg *req, struct proto_msg *out, void *ctx)
{
    struct client *client = ctx;
    uint16_t len, argv_len;
    struct process_exec_info exec_info;
    char *arg, *env;
    int i;

    // verify that we are not attached to any process already
    if (client->process)
        return EBUSY;
    
    // read path
    if (
            proto_read_uint16(req, &len)
        ||  !(exec_info.path = alloca(len + 1))
        ||  _proto_read_str(req, exec_info.path, len)
    )
        goto error;
    
    // read argv
    if (
            proto_read_uint16(req, &argv_len)
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
                proto_read_uint16(req, &len)
            ||  !(arg = alloca(len + 1))
            ||  _proto_read_str(req, arg, len)
        )
            goto error;       

        log_info("\targv[%i] : %s", i + 1, arg);

        exec_info.argv[i + 1] = arg;
    }

    // terminate
    exec_info.argv[i + 1] = NULL;
    
    // XXX: envp
    exec_info.envp = alloca(1 * sizeof(char *));
    exec_info.envp[0] = NULL;

    // spawn new process
    if (daemon_process_start(client->daemon, &client->process, &exec_info))
        return errno;

    // XXX: this should be a client_attach(...) function
    {
        // attach
        if (process_attach(client->process, client))
            abort();
        
        // yay, respond with CMD_ATTACHED
        if (
                proto_cmd_reply(out, req, CMD_ATTACHED)
            ||  msg_attached(out, client)
        )
            abort();
    }

    // ok
    return 0;

error:
    return -1;
}

/**
 * Server-side command handlers
 */
struct proto_cmd_handler daemon_command_handlers[] = {
    {   CMD_HELLO,      cmd_hello     },
    {   CMD_START,      cmd_start     },
    {   0,              0             }
};

