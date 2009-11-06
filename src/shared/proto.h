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
 * Protocol message ID.
 *
 * zero for server -> client
 * ++n for client -> server -> client
 */
typedef uint32_t proto_msg_id_t;

/**
 * Per-process data channels
 */
enum proto_channel {
    CHANNEL_STDIN   = 0 /* STDIN_FILENO */,
    CHANNEL_STDOUT  = 1 /* STDOUT_FILENO */,
    CHANNEL_STDERR  = 2 /* STDERR_FILENO */,
};

/**
 * Process status types
 */
enum proto_process_status {
    PROCESS_RUN     = 1,    ///< process is running
    PROCESS_EXIT    = 2,    ///< process has exit()'d or returned from main with exit status
    PROCESS_KILL    = 3,    ///< process was terminated by signal code
};

/**
 * Protocol commands, uint16_t.
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
    CMD_HELLO       = 0x0001,

    /**
     * Client -> Server: start new process and attach to it
     *  [uint16_t]      path
     *  [uint16_t]      argv {
     *      [uint16_t]      arg
     *  }
     *  [uint16_t]      envp {
     *      [uint16_t]      env
     *  }
     */
    CMD_START       = 0x0101,

    /**
     * Client -> Server: attach to an existing process
     *  [uint16_t]      proc_id
     */
    CMD_ATTACH      = 0x0102,

    /**
     * Server -> Client: attached to given process
     *  [uint16_t]      proc_id
     */
    CMD_ATTACHED    = 0x0110,

    /**
     * Server -> Client: data from process stdout/err
     * Client -> Server: data to process stdin
     *  uint16_t        channel (CHANNEL_*)
     *  [uint16_t]      data
     *
     * These data segments are ordered and interleaved atomically.
     *
     * If data is zero-length, this indicates EOF
     * XXX: replace with CMD_EOF?
     */
    CMD_DATA        = 0x0201,

    /**
     * Server -> Client: process status changed
     *  uint16_t        process_status      one of PROCESS_*
     *  uint16_t        status_code         value depends on process_status (PROCESS_EXIT/PROCESS_KILL)
     */
    CMD_STATUS      = 0x0202,

    /**
     * Server -> Client: Associated command executed ok, no specific reply data
     */
    CMD_OK          = 0xff00,

    /**
     * Server -> Client: Associated command failed with error
     *  int32_t         err_code
     *  [uint16_t]      err_msg
     */
    CMD_ERROR       = 0xfff0,

    /**
     * Server -> Client: Terminal protocol/system error, connection will be closed
     *  int32_t         err_code
     *  [uint16_t]      err_msg
     */
    CMD_ABORT       = 0xffff,
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

    /** Per-message handle */
    proto_msg_id_t id;

    /** Command code */
    uint16_t cmd;
};

/**
 * Incoming message handler.
 *
 * The incoming message will be given as \a in, and should be read by the handler using the proto_read_* commands.
 *
 * Optionally, the handler may also build a new packet in \a out to send back using the proto_cmd_init(out, ...)
 * command.
 *
 * In case of system error (e.g. memory allocation failure, I/O error, etc), these should set an error code in errno,
 * and return -1.
 *
 * In case of non-fatal protocol errors, these should simply return the relevant error code as a positive integer.
 */
typedef int (*proto_cmd_handler_t) (struct proto_msg *in, struct proto_msg *out, void *ctx);

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
 * Read a cmd from the given proto_msg, and then dispatch it to the correct handler.
 *
 * The incoming message should be given via \a in, and an initialized proto_msg via \a out for optional use by the
 * command handler. Test for non-zero out->cmd on return to handle outgoing message.
 *
 * As per proto_cmd_handler_t, this will set errno and return -1 in case of system error, or return a positive error
 * code in case of non-fatal protocol error.
 *
 * Returns ENOTSUP if no matching command handler was found.
 */
int proto_cmd_dispatch (struct proto_cmd_handler cmd_handlers[], struct proto_msg *in, struct proto_msg *out, void *ctx);

/**
 * Initialize a proto_msg buffer for use using the given storage buffer.
 */
int proto_msg_init (struct proto_msg *msg, char *buf, size_t len);

/**
 * Initialize a protocol command in the given proto_msg with the given message ID and command code.
 */
int proto_cmd_start (struct proto_msg *msg, uint32_t id, enum proto_cmd cmd);

/**
 * Initialize a protocol command in the given proto_msg as a reply to the given second message with given reply command.
 */
int proto_cmd_reply (struct proto_msg *msg, struct proto_msg *req, enum proto_cmd cmd);

/**
 * Initialize the given proto_msg buf and cmd in the same step
 */
int proto_cmd_init (struct proto_msg *msg, char *buf, size_t len, uint32_t id, enum proto_cmd cmd);

/**
 * Read fields
 */
int proto_read (struct proto_msg *msg, void *buf, size_t len);
int proto_read_uint16 (struct proto_msg *msg, uint16_t *val_ptr);
int proto_read_uint32 (struct proto_msg *msg, uint32_t *val_ptr);
int proto_read_int32 (struct proto_msg *msg, int32_t *val_ptr);

/**
 * Read a uint16-prefixed byte array from the msg, returning a pointer to the first char and the length
 */
int proto_read_buf_ptr (struct proto_msg *msg, const char **buf_ptr, size_t *len_ptr);

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
int proto_write_uint32 (struct proto_msg *msg, uint32_t val);
int proto_write_int32 (struct proto_msg *msg, int32_t val);

/**
 * Write a uint16_t-length-prefixed byte array from the given buffer
 */
int proto_write_buf (struct proto_msg *msg, const char *buf, size_t len);

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
