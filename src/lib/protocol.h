#ifndef NETDAEMON_LIB_PROTOCOL_H
#define NETDAEMON_LIB_PROTOCOL_H

/**
 * @file
 *
 * Netdaemon message-based protocol
 */

#include <stdint.h>

/**
 * Protocol version, uint16_t
 */
enum nd_proto_version {
    /** First version */
    ND_PROTO_V1         = 1,
    
    /** Current version */
    ND_PROTO_VERSION    =   ND_PROTO_V1,
};

/**
 * Protocol commands, uint16_t
 */
enum nd_proto_cmd {
    /**
     * Client -> Server:
     *  uint16_t    proto_version       best protocol version supported by the client
     *
     * Server -> Client:
     *  uint16_t    proto_version       best protocol version supported by the server
     */
    ND_CMD_HELLO    = 0x01,
};

#endif
