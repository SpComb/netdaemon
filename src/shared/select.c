#include "select.h"

#include <stdlib.h>
#include <sys/select.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

int select_fd_init (struct select_fd *fd, int _fd, short mask, select_handler_t handler_func, void *handler_arg)
{
    memset(fd, 0, sizeof(*fd));

    fd->fd = _fd;
    fd->handler_func = handler_func;
    fd->handler_arg = handler_arg;
    fd->want_read = mask & FD_READ;
    fd->want_write = mask & FD_WRITE;
    fd->active = false;

    // ok
    return 0;
}

void select_fd_deinit (struct select_fd *fd)
{
    assert(!fd->active);

    fd->fd = -1;
}

void select_loop_init (struct select_loop *loop)
{
    // clear
    LIST_INIT(&loop->fds);
}

int select_loop_add (struct select_loop *loop, struct select_fd *fd)
{
    // add to fd list
    LIST_INSERT_HEAD(&loop->fds, fd, loop_fds);

    fd->active = true;

    // ok
    return 0;
}

void select_loop_del (struct select_loop *loop, struct select_fd *fd)
{
    if (fd->active) {
        // remove from fd list
        // XXX: this leaves our ->next pointer valid, but hmm
        LIST_REMOVE(fd, loop_fds);

        fd->active = false;
    }
}

/**
 * Set up the given fd_set's from the given select_loop, and return the maximum fd found, or -1 on error.
 */
static int select_loop_build (struct select_loop *loop, fd_set *rfds, fd_set *wfds)
{
    int fd_max = -1;
    struct select_fd *fd;

    // init sets
    FD_ZERO(rfds);
    FD_ZERO(wfds);

    // setup fd_sets
    LIST_FOREACH(fd, &loop->fds, loop_fds) {
        // count max. fd
        if (fd->fd > fd_max)
            fd_max = fd->fd;

        // read?
        if (fd->want_read)
            FD_SET(fd->fd, rfds);
        
        // write?
        if (fd->want_write)
            FD_SET(fd->fd, wfds);
    }

    // ok
    return fd_max;
}

/**
 * Dispatch to active handlers.
 *
 * If any handler returns with EAGAIN, simply skip it. Otherwise, if any handler returns nonzero, return that
 * error code.
 */
static int select_loop_dispatch (struct select_loop *loop, fd_set *rfds, fd_set *wfds, int count)
{
    int err;
    struct select_fd *fd;

    for (fd = LIST_FIRST(&loop->fds); count > 0 && fd; fd = LIST_NEXT(fd, loop_fds) ) {
        // read?
        if (fd->active && FD_ISSET(fd->fd, rfds)) {
            count--;

            if ((err = fd->handler_func(fd->fd, FD_READ, fd->handler_arg)))
                goto fd_error;
        }

        // write?
        if (fd->active && FD_ISSET(fd->fd, wfds)) {
            count--;

            if ((err = fd->handler_func(fd->fd, FD_WRITE, fd->handler_arg)))
                goto fd_error;
        }

        continue;

fd_error:
        if (err == -1 && errno == EAGAIN ) 
            // just skip to next
            continue;
                
        else
            // break select loop
            return err;
    }

    // ok
    return 0;
}

int select_loop_run (struct select_loop *loop, struct timeval *tv)
{
    int fd_max;
    int ret, err;

    fd_set rfds, wfds;

    // prep
    if ((fd_max = select_loop_build(loop, &rfds, &wfds)) < 0)
        return -1;

    // do select
    if ((ret = select(fd_max + 1, &rfds, &wfds, NULL, tv)) < 0)
        return -1;

    // dispatch all results
    if ((err = select_loop_dispatch(loop, &rfds, &wfds, ret)) < 0)
        return err;

    // done
    return 0;
}

int select_loop_main (struct select_loop *loop)
{
    while (true) {
        if (select_loop_run(loop, NULL))
            return -1;
    }

    // ok
    return 0;
}

