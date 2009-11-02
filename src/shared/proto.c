#include "proto.h"

#include <string.h>
#include <arpa/inet.h>
#include <errno.h>

int proto_cmd_dispatch (struct proto_cmd_handler cmd_handlers[], char *buf, size_t len, void *ctx)
{
    // prep msg to use
    struct proto_msg msg = {
        .buf    = buf, 
        .len    = len, 
        .offset = 0
    };
    
    uint16_t cmd;
    struct proto_cmd_handler *cmd_handler;

    // read the command code
    if (proto_read_uint16(&msg, &cmd))
        return -1;

    // find the right handler
    for (cmd_handler = cmd_handlers; cmd_handler->cmd && cmd_handler->handler_func; cmd_handler++) {
        if (cmd_handler->cmd == cmd)
            break;
    }

    if (!cmd_handler) {
        // no matching handler found
        errno = ENOTSUP;
        
        return -1;
    }

    // dispatch
    return cmd_handler->handler_func(&msg, ctx);
}

int proto_cmd_init (struct proto_msg *msg, char *buf, size_t len, enum proto_cmd cmd)
{
    msg->buf = buf;
    msg->len = len;
    msg->offset = 0;
    
    // write command code
    return proto_write_uint16(msg, cmd);
}

int proto_read (struct proto_msg *msg, void *buf, size_t len)
{
    if (msg->offset + len > msg->len) {
        errno = EOVERFLOW;

        return -1;
    }
    
    // store
    memcpy(buf, msg->buf + msg->offset, len);

    // update offset
    msg->offset += len;

    // ok
    return 0;
}

int proto_read_uint16 (struct proto_msg *msg, uint16_t *val_ptr)
{
    uint16_t val;

    if (proto_read(msg, &val, sizeof(val)))
        return -1;

    // convert
    *val_ptr = ntohs(val);

    // ok
    return 0;
}

int proto_write (struct proto_msg *msg, void *buf, size_t len)
{
    if (msg->offset + len > msg->len) {
        errno = EOVERFLOW;

        return -1;
    }

    // store
    memcpy(msg->buf + msg->offset, buf, len);

    // update offset
    msg->offset += len;

    // ok
    return 0;
}

int proto_write_uint16 (struct proto_msg *msg, uint16_t val)
{
    val = htons(val);

    return proto_write(msg, &val, sizeof(val));
}


