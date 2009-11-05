#ifndef DAEMON_COMMANDS_H
#define DAEMON_COMMANDS_H

/**
 * Client command handling
 */
#include "shared/proto.h"
#include "client.h"

/**
 * Server-side command handlers. These take the `struct client *` as the context argument.
 */
extern struct proto_cmd_handler daemon_command_handlers[];

#endif
