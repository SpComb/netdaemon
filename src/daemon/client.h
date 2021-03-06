#ifndef DAEMON_CLIENT_H
#define DAEMON_CLIENT_H

/**
 * @file
 *
 * Client sessions
 */
#include "daemon.h"
#include "process.h"
#include "shared/proto.h"

/**
 * Per-client connection state
 */
struct client {
    /** Daemon we are running under */
    struct daemon *daemon;

    /** socket IO info */
    struct select_fd fd;

    /** Protocol version agreed upon in handshake */
    enum proto_version version;

    /** Attached process */
    struct process *process;

    /** Process's consumer list */
    LIST_ENTRY(client) process_clients;
};

/**
 * Construct a new client and activate it.
 *
 * @param daemon daemon we are running under
 * @param client_ptr returned client struct if not NULL
 * @param sock connected socket fd of the SOCK_SEQPACKET type
 */
int client_add_seqpacket (struct daemon *daemon, int sock);

/**
 * Client got data from attached process.
 *
 * XXX: should not be a 'public' interface
 */
void client_on_process_data (struct process *process, enum proto_channel channel, const char *buf, size_t len, void *ctx);

/**
 * Client got EOF from attached process
 */
void client_on_process_eof (struct process *process, enum proto_channel channel, void *ctx);

/**
 * Client's attached process changed status
 */
void client_on_process_status (struct process *process, enum proto_process_status status, int code, void *ctx);

/**
 * Send data to process
 */
int client_on_cmd_data (struct client *client, enum proto_channel channel, const char *buf, size_t len);

/**
 * Start process and attach to it
 */
int client_start (struct client *client, const struct process_exec_info *exec_info);

/**
 * Attach to process
 */
int client_attach (struct client *client, const char *process_id);

/**
 * Send signal to attached process
 */
int client_kill (struct client *client, int sig);

#endif
