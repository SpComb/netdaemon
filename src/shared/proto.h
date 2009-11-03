#ifndef NETDAEMON_LIB_PROTOCOL_H
#define NETDAEMON_LIB_PROTOCOL_H

/**
 * @file
 *
 * Netdaemon message-based protocol
 */
#include <stddef.h>
#include <stdint.h>

/**
 * Protocol version, uint16_t
 */
enum proto_version {
    /** First version */
    PROTO_V1         = 1,
    
    /** Current version */
    PROTO_VERSION    =   PROTO_V1,
};

/**
 * Protocol commands, uint16_t
 */
enum proto_cmd {
    /**
     * Client -> Server:
     *  uint16_t    proto_version       best protocol version supported by the client
     *
     * Server -> Client:
     *  uint16_t    proto_version       best protocol version supported by the server
     *
     * Initial message sent by client to server when connecting, server replies with the same code. Used to negotiate
     * protocol version used; the version specified by the client takes precedence if the server replies with a newer
     * version, this is just used to indicate what the server could support.
     */
    CMD_HELLO    = 0x0001,

    /**
     * Client -> Server:
     *  uint16_t        path_len
     *  [path_len]      path
     *  uint16_t        arg_count
     *  [arg_count]     argv {
     *      uint16_t        arg_len
     *      [arg_len]       arg
     *  }
     *  uint16_t        env_count
     *  [env_count]     envp {
     *      uint16_t        env_len
     *      [env_len]       env
     *  }
     */
    CMD_EXEC    = 0x0101,

    /**
     * Server -> Client:
     *  int32_t        errno
     *  uint16_t        errmsg_len
     *  [errmsg_len]    errmsg
     */
    CMD_ERROR   = 0xff01,
};

/**
 * Maximum length of a protocol message: 64k
 */
#define ND_PROTO_MSG_MAX (64 * 1024)

/**
 * Protocol message, used for incoming and outgoing messages
 */
struct proto_msg {
    /** Message data */
    char *buf;

    /** Total length of message data */
    size_t len;

    /** Current offset into message*/
    size_t offset;
};

/**
 * Incoming message handler
 */
typedef int (*proto_cmd_handler_t) (struct proto_msg *msg, void *ctx);

/**
 * Incoming command -> handler mapping
 */
struct proto_cmd_handler {
    /** Command code */
    enum proto_cmd cmd;

    /** Handler function */
    proto_cmd_handler_t handler_func;
};

/**
 * Read a cmd from the given proto_msg, and then dispatch it to the correct handler
 */
int proto_cmd_dispatch (struct proto_cmd_handler cmd_handlers[], struct proto_msg *msg, void *ctx);

/**
 * Initialize a proto_msg using the given storage buffer
 */
int proto_msg_init (struct proto_msg *msg, char *buf, size_t len);

/**
 * Initialize a proto_msg using the given storage buffer and command code
 */
int proto_cmd_init (struct proto_msg *msg, char *buf, size_t len, enum proto_cmd cmd);

/**
 * Read fields
 */
int proto_read (struct proto_msg *msg, void *buf, size_t len);
int proto_read_uint16 (struct proto_msg *msg, uint16_t *val_ptr);

/**
 * Read a str of [len] bytes from the msg, storing it and a terminating NUL byte into buf, which mus store at least
 * len+1 bytes.
 */
int _proto_read_str (struct proto_msg *msg, char *buf, uint16_t len);

/**
 * Read a string-length prefix from the msg, alloca a suitable char array, and read the str into it, evaluating to the
 * alloca'd array.
 */


/**
 * Write fields
 */
int proto_write (struct proto_msg *msg, const void *buf, size_t len);
int proto_write_uint16 (struct proto_msg *msg, uint16_t val);
int proto_write_int32 (struct proto_msg *msg, int32_t val);

/**
 * Write a uint16_t-length-prefixed char array from the given zero-terminated string
 */
int proto_write_str (struct proto_msg *msg, const char *str);

/**
 * Write a uint16_t-count-prefixed array of char arrays from the given array of zero-terminated strings
 */
int proto_write_str_array (struct proto_msg *msg, const char *str_array[]);

/**
 * Send a message out on a SOCK_SEQPACKET socket
 */
int proto_send_seqpacket (int sock, struct proto_msg *msg);

/**
 * Recieve a message on a SOCK_SEQPACKET socket
 */
int proto_recv_seqpacket (int sock, struct proto_msg *msg);

#endif
