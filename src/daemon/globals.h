#ifndef DAEMON_GLOBALS_H
#define DAEMON_GLOBALS_H

/**
 * @file
 *
 * Definitions of global variables for the daemon process
 */
#include "shared/select.h"

/** Global main select-loop */
extern struct select_loop daemon_select_loop;

#endif
