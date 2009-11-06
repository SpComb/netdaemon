#ifndef SHARED_UTIL_H
#define SHARED_UTIL_H

/**
 * Misc. util functions
 */
// for O_* flags
#include <fcntl.h>

/**
 * Construct a pipe, returning the read end of the pipe via *fd_read, and the write end via *fd_write.
 */
int make_pipe (int *fd_read, int *fd_write);

/**
 * Set the given O_* flags on the given fd, adding them to the already set flags.
 */
int fd_flags (int fd, int set_flags);

/**
 * Allocate and return a new string with the given contents
 */
char *strfmt (const char *fmt, ...);

#endif
