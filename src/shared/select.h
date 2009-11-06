#ifndef SHARED_SELECT_H
#define SHARED_SELECT_H

/**
 * @file
 *
 * select() loop implementation
 */
#include <stdbool.h>
#include <sys/queue.h>
#include <sys/time.h>

/**
 * FD events
 */
enum fd_events {
    FD_READ     = 0x01,
    FD_WRITE    = 0x02,
};

/**
 * Callback handler return codes
 */
enum select_handler_ret {
    /** Everything ok, continue as planned */
    SELECT_OK   = 0,

    /** Act based on errno */
    SELECT_ERR  = -1,
};

/**
 * Callback handler
 *
 * @return zero on success, -1 on errno, >0 for app-specific return code
 */
typedef int (*select_handler_t) (int fd, short what, void *arg);

/**
 * Per-fd state
 */
struct select_fd {
    /** The fd to watch */
    int fd;

    /** Callback */
    select_handler_t handler_func;
    void *handler_arg;

    /** Select for read */
    bool want_read;

    /** Select for write */
    bool want_write;

    /** Active (i.e. in the select_loop fd list) */
    bool active;

    /** Our entry in the select_loop */
    LIST_ENTRY(select_fd) loop_fds;
};

/**
 * Set up select_fd values
 */
int select_fd_init (struct select_fd *fd, int _fd, short mask, select_handler_t handler_func, void *handler_arg);

/**
 * Change read flag
 */
static inline void select_want_read (struct select_fd *fd, bool want_read)
{
    fd->want_read = want_read;
}

/**
 * Change write flag
 */
static inline void select_want_write (struct select_fd *fd, bool want_write)
{
    fd->want_write = want_write;
}

/**
 * Select-loop state
 */
struct select_loop {
    /** List of FDs to select on */
    LIST_HEAD(select_loop_fds, select_fd) fds;
};

/**
 * Initialize the given select loop
 */
void select_loop_init (struct select_loop *loop);

/**
 * Add the given select_fd to the select loop
 */
int select_loop_add (struct select_loop *loop, struct select_fd *fd);

/**
 * Remove the given select_fd from the select loop if active
 *
 * XXX: how safe is this when called from within select_loop_run?
 */
void select_loop_del (struct select_loop *loop, struct select_fd *fd);

/**
 * Run the select loop once with the given timeout
 *
 * Returns zero on success, <0 on errno, or whatever any select_fd handler happens to return
 */
int select_loop_run (struct select_loop *loop, struct timeval *tv);

/**
 * Run the select loop as an infinite main loop with signal handling
 */
int select_loop_main (struct select_loop *loop);

#endif
