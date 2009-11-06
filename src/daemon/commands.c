#include "commands.h"
#include "client.h"
#include "shared/log.h"
#include "process.h"
#include "errno.h"

// send CMD_ATTACHED reply
static int reply_cmd_attached (struct proto_msg *out, struct proto_msg *req, struct process *process)
{
    return (
            proto_cmd_reply(out, req, CMD_ATTACHED)
        ||  proto_write_str(out, process_id(process))
        ||  proto_write_uint16(out, process->status)
        ||  proto_write_uint16(out, process->status_code)
    );
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
    uint16_t len;
    struct process_exec_info exec_info;
    int i, err;

    // verify that we are not attached to any process already
    if (client->process)
        return EBUSY;
    
    // read path
    if (proto_read_str(req, &exec_info.path))
        return -1;
    
    // read argv
    if (
            proto_read_uint16(req, &len)
        ||  !(exec_info.argv = alloca((1 + len + 1) * sizeof(char *)))
    )
        return -1;

    // store argv[0]
    exec_info.argv[0] = exec_info.path;

    // log
    log_info("path=%s, argv=%u:", exec_info.path, len);

    for (i = 0; i < len; i++) {
        // read arg
        if (proto_read_str(req, &exec_info.argv[i + 1]))
            return -1;

        log_info("\targv[%i] : %s", i + 1, exec_info.argv[i + 1]);
    }

    // terminate
    exec_info.argv[i + 1] = NULL;
    
    // XXX: envp
    exec_info.envp = alloca(1 * sizeof(char *));
    exec_info.envp[0] = NULL;

    // go
    if ((err = client_start(client, &exec_info)))
        return err;

    // yay, respond with CMD_ATTACHED
    if (reply_cmd_attached(out, req, client->process))
        goto error;

    // good
    return 0;

error:
    // XXX: cleanup process

    return -1;
}

// write data chunk to process
static int cmd_data (struct proto_msg *req, struct proto_msg *out, void *ctx)
{
    struct client *client = ctx;
    uint16_t channel;
    const char *buf;
    size_t len;

    if (!client->process)
        // no process attached
        return ECHILD;

    // read packet
    if (
            proto_read_uint16(req, &channel)
        ||  proto_read_buf_ptr(req, &buf, &len)
    )
        return -1;

    log_info("channel=%u, data=%zu:%.*s", channel, len, (int) len, buf);

    // process
    return client_on_cmd_data(client, channel, buf, len); 
}

// attach to process
static int cmd_attach (struct proto_msg *req, struct proto_msg *out, void *ctx)
{
    struct client *client = ctx;
    const char *process_id;
    int err;
    
    if (proto_read_str(req, &process_id))
        return -1;
    
    log_info("process_id=%s", process_id);

    // process
    if ((err = client_attach(client, process_id)))
        return err;

    // respond with CMD_ATTACHED
    if (reply_cmd_attached(out, req, client->process))
        goto error;

    // good
    return 0;

error:
    // XXX: wut

    return -1;
} 

// send signal to process
static int cmd_kill (struct proto_msg *req, struct proto_msg *out, void *ctx)
{
    struct client *client = ctx;
    uint16_t sig;

    if (proto_read_uint16(req, &sig))
        return -1;

    log_info("sig=%u", sig);

    // process
    return client_kill(client, sig);
}

// get list of processes
static int cmd_list (struct proto_msg *req, struct proto_msg *out, void *ctx)
{
    struct client *client = ctx;
    struct process *process;
    uint16_t count = 0;

    // number of processes running
    LIST_FOREACH(process, &client->daemon->processes, daemon_processes)
        count++;
    
    log_info("-> count=%u", count);

    // reply
    if (
            proto_cmd_reply(out, req, CMD_LIST)
        ||  proto_write_uint16(out, count)
    )
        return -1;

    LIST_FOREACH(process, &client->daemon->processes, daemon_processes) {
        if (
                proto_write_str(out, process_id(process))
            ||  proto_write_uint16(out, process->status)
            ||  proto_write_uint16(out, process->status_code)
        )
            return -1;
    }

    // ok
    return 0;
}

/**
 * Server-side command handlers
 */
struct proto_cmd_handler daemon_command_handlers[] = {
    {   CMD_LIST,       cmd_list        },
    {   CMD_KILL,       cmd_kill        },
    {   CMD_ATTACH,     cmd_attach      },
    {   CMD_DATA,       cmd_data        },
    {   CMD_HELLO,      cmd_hello       },
    {   CMD_START,      cmd_start       },
    {   0,              0               }
};

