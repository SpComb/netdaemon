#include "proto.h"

#include <string.h>
#include <arpa/inet.h>
#include <errno.h>

int proto_cmd_dispatch (struct proto_cmd_handler cmd_handlers[], struct proto_msg *msg, void *ctx)
{
    uint16_t cmd;
    struct proto_cmd_handler *cmd_handler;

    // read the command code
    if (proto_read_uint16(msg, &cmd))
        return -1;

    // find the right handler
    for (cmd_handler = cmd_handlers; cmd_handler->cmd && cmd_handler->handler_func; cmd_handler++) {
        if (cmd_handler->cmd == cmd)
            break;
    }

    if (!(cmd_handler->cmd && cmd_handler->handler_func)) {
        // no matching handler found
        errno = ENOTSUP;
        
        return -1;
    }

    // dispatch
    return cmd_handler->handler_func(msg, ctx);
}

int proto_msg_init (struct proto_msg *msg, char *buf, size_t len)
{
    msg->buf = buf;
    msg->len = len;
    msg->offset = 0;
    
    // ok
    return 0;
}

int proto_cmd_init (struct proto_msg *msg, char *buf, size_t len, enum proto_cmd cmd)
{
    if (proto_msg_init(msg, buf, len))
        return -1;
    
    // write command code
    if (proto_write_uint16(msg, cmd))
        return -1;

    // ok
    return 0;
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

int _proto_read_str (struct proto_msg *msg, char *buf, uint16_t len)
{
    // read str value
    if (proto_read(msg, buf, len))
        return -1;

    // nul-terminate
    buf[len] = '\0';

    // ok
    return 0;
}

int proto_write (struct proto_msg *msg, const void *buf, size_t len)
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

int proto_write_int32 (struct proto_msg *msg, int32_t val)
{
    val = htonl(val);

    return proto_write(msg, &val, sizeof(val));
}

int proto_write_str (struct proto_msg *msg, const char *str)
{
    size_t len = strlen(str);

    // write length prefix and data
    return (
            proto_write_uint16(msg, len)
        ||  proto_write(msg, str, len)
    );
}

int proto_write_str_array (struct proto_msg *msg, const char *str_array[])
{
    size_t count = 0;
    const char **str_ptr;
    
    // count
    for (str_ptr = str_array; *str_ptr; str_ptr++)
        count++;

    if (proto_write_uint16(msg, count))
        return -1;

    // add each
    for (str_ptr = str_array; *str_ptr; str_ptr++)
        if (proto_write_str(msg, *str_ptr))
            return -1;

    // ok
    return 0;
}

int proto_send_seqpacket (int sock, struct proto_msg *msg)
{
    ssize_t ret;

    if ((ret = send(sock, msg->buf, msg->offset, 0)) < 0)
        return -1;

    // XXX: for now, assume that we can send complete messages...
    if (ret < msg->offset) {
        errno = EMSGSIZE;

        return -1;
    }

    // ok
    return 0;
}

int proto_recv_seqpacket (int sock, struct proto_msg *msg)
{
    ssize_t ret;

    // try recv()
    if ((ret = recv(sock, msg->buf, msg->len, MSG_TRUNC)) < 0) {
        return -1;
    
    } else if (ret == 0) {
        // EOF
        errno = EINVAL;

        return -1;

    } else if (ret > msg->len) {
        // truncated
        errno = EMSGSIZE;

        return -1;
    }

    // set
    msg->len = ret;

    // ok
    return 0;
}

