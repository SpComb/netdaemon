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
     */
    CMD_HELLO    = 0x01,
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
 * Form a proto_msg from the given message, and then dispatch it to the correct handler based on the command code read
 */
int proto_cmd_dispatch (struct proto_cmd_handler cmd_handlers[], char *buf, size_t len, void *ctx);

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
 * Write fields
 */
int proto_write (struct proto_msg *msg, void *buf, size_t len);
int proto_write_uint16 (struct proto_msg *msg, uint16_t val);

#endif
