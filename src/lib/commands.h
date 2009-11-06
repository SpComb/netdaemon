#ifndef LIB_COMMANDS_H
#define LIB_COMMANDS_H

/**
 * Client-side command handlers
 */
#include "shared/proto.h"
#include "client.h"

/**
 * ctx should be the `struct nd_client *`
 */
extern struct proto_cmd_handler client_command_handlers[];

#endif
