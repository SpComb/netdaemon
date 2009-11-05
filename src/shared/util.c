#include "util.h"

#include <unistd.h>

/**
 * Identify pipe ends
 */
enum pipe_end {
    PIPE_READ = 0,
    PIPE_WRITE = 1,
};

/**
 * Construct a pipe, returning the read end of the pipe via *fd_read, and the write end via *fd_write.
 */
int make_pipe (int *fd_read, int *fd_write)
{
    int fds[2];

    // make pipes
    if (pipe(fds) < 0)
        return -1;

    // set
    *fd_read = fds[PIPE_READ];
    *fd_write = fds[PIPE_WRITE];

    // ok
    return 0;
}

/**
 * Set the given O_* flags on the given fd
 */
int fd_flags (int fd, int set_flags)
{
    int flags;

    // get old flags
    if ((flags = fcntl(fd, F_GETFL)) < 0)
        return -1;

    // set new flags
    flags |= set_flags;

    // update
    if (fcntl(fd, F_SETFL, flags) < 0)
        return -1;

    // ok
    return 0;
}

