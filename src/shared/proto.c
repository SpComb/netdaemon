#include "proto.h"

#include <string.h>
#include <arpa/inet.h>
#include <errno.h>

int proto_cmd_parse (struct proto_msg *msg)
{
    // read the message id and cmd
    if (
            proto_read_uint32(msg, &msg->id)
        ||  proto_read_uint16(msg, &msg->cmd)
    )
        return -1;

    // ok
    return 0;
}

int proto_cmd_dispatch (struct proto_cmd_handler cmd_handlers[], struct proto_msg *in, struct proto_msg *out, void *ctx)
{
    struct proto_cmd_handler *cmd_handler;

    // find the right handler
    for (cmd_handler = cmd_handlers; cmd_handler->cmd && cmd_handler->handler_func; cmd_handler++) {
        if (cmd_handler->cmd == in->cmd)
            break;
    }

    if (!(cmd_handler->cmd && cmd_handler->handler_func)) {
        errno = ENOTSUP;
        
        return -1;
    }

    // dispatch
    return cmd_handler->handler_func(in, out, ctx);
}

int proto_msg_init (struct proto_msg *msg, char *buf, size_t len)
{
    // buf
    msg->buf = buf;
    msg->len = len;
    msg->offset = 0;

    // info
    msg->id = 0;
    msg->cmd = 0;
    
    // ok
    return 0;
}

int proto_cmd_start (struct proto_msg *msg, uint32_t id, enum proto_cmd cmd)
{
    // store for reference
    msg->id = id;
    msg->cmd = cmd;

    // write id
    if (proto_write_uint32(msg, id))
        return -1;
    
    // write command code
    if (proto_write_uint16(msg, cmd))
        return -1;

    // ok
    return 0;
}

int proto_cmd_reply (struct proto_msg *msg, struct proto_msg *req, enum proto_cmd cmd)
{
    // just use the req's id as the resp id
    return proto_cmd_start(msg, req->id, cmd);
}

int proto_cmd_init (struct proto_msg *msg, char *buf, size_t len, uint32_t id, enum proto_cmd cmd)
{
    // init buffer
    if (proto_msg_init(msg, buf, len))
        return -1;

    // init cmd
    if (proto_cmd_start(msg, id, cmd))
        return -1;

    // ok
    return 0;
}

/**
 * Consume \a len bytes from the msg buf, returning a pointer to the start of the segment, or NULL in case of error
 */
static void *proto_seek (struct proto_msg *msg, size_t len)
{
    void *ptr;

    // check
    if (msg->offset + len > msg->len) {
        errno = EOVERFLOW;

        return NULL;
    }
    
    // return pointer
    ptr = msg->buf + msg->offset;

    // update offset
    msg->offset += len;

    return ptr;
}

/**
 * Consume bytes from the msg buf, up to and including the first byte matching the given value, or NULL if not found
 */
static const char *proto_seek_char (struct proto_msg *msg, char c)
{
    // start of str
    const char *start = msg->buf + msg->offset;
    const char *end;
       
    // seek to end char
    if ((end = memchr(start, c, msg->len - msg->offset)) == NULL)
        // not found
        return NULL;

    // update offset
    msg->offset += (end - start + 1);
    
    // found
    return start;
}

int proto_read (struct proto_msg *msg, void *out_buf, size_t len)
{
    void *msg_buf;

    // consume
    if ((msg_buf = proto_seek(msg, len)) == NULL)
        return -1;
   
    // store
    memcpy(out_buf, msg_buf, len);

    // ok
    return 0;
}

int proto_read_uint16 (struct proto_msg *msg, uint16_t *val_ptr)
{
    if (proto_read(msg, val_ptr, sizeof(*val_ptr)))
        return -1;

    // convert
    *val_ptr = ntohs(*val_ptr);

    // ok
    return 0;
}

int proto_read_uint32 (struct proto_msg *msg, uint32_t *val_ptr)
{
    if (proto_read(msg, val_ptr, sizeof(*val_ptr)))
        return -1;

    // convert
    *val_ptr = ntohl(*val_ptr);

    // ok
    return 0;
}

int proto_read_int32 (struct proto_msg *msg, int32_t *val_ptr)
{
    if (proto_read(msg, val_ptr, sizeof(*val_ptr)))
        return -1;

    // convert
    *val_ptr = ntohl(*val_ptr);

    // ok
    return 0;
}

int proto_read_buf_ptr (struct proto_msg *msg, const char **buf_ptr, size_t *len_ptr)
{
    uint16_t len;

    // read len
    if (proto_read_uint16(msg, &len))
        return -1;

    // get buf
    *buf_ptr = proto_seek(msg, len);

    // ret
    *len_ptr = len;

    return 0;
}

int proto_read_str (struct proto_msg *msg, const char **str_ptr)
{
    if ((*str_ptr = proto_seek_char(msg, '\0')) == NULL)
        return -1;

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

int proto_write_uint32 (struct proto_msg *msg, uint32_t val)
{
    val = htonl(val);

    return proto_write(msg, &val, sizeof(val));
}

int proto_write_int32 (struct proto_msg *msg, int32_t val)
{
    val = htonl(val);

    return proto_write(msg, &val, sizeof(val));
}

int proto_write_buf (struct proto_msg *msg, const char *buf, size_t len)
{
    // write length prefix and data
    return (
            proto_write_uint16(msg, len)
        ||  proto_write(msg, buf, len)
    );
}

int proto_write_str (struct proto_msg *msg, const char *str)
{
    size_t len = strlen(str);
    
    // write out, with NUL
    return proto_write(msg, str, len + 1);
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

