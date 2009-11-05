#include "select.h"

#include <stdlib.h>
#include <sys/select.h>
#include <string.h>

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

int select_loop_run (struct select_loop *loop, struct timeval *tv)
{
    int fd_max = -1;
    struct select_fd *fd;
    int ret, err;

    fd_set rfds, wfds;

    // init sets
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);

    // setup fd_sets
    LIST_FOREACH(fd, &loop->fds, loop_fds) {
        // count max. fd
        if (fd->fd > fd_max)
            fd_max = fd->fd;

        // read?
        if (fd->want_read)
            FD_SET(fd->fd, &rfds);
        
        // write?
        if (fd->want_write)
            FD_SET(fd->fd, &wfds);
    }

    // do select
    if ((ret = select(fd_max + 1, &rfds, &wfds, NULL, tv)) < 0)
        goto error;

    // dispatch all results
    for (fd = LIST_FIRST(&loop->fds); ret > 0 && fd; fd = LIST_NEXT(fd, loop_fds) ) {
        // read?
        if (fd->active && FD_ISSET(fd->fd, &rfds)) {
            ret--;

            if ((err = fd->handler_func(fd->fd, FD_READ, fd->handler_arg)))
                break;
        }

        // write?
        if (fd->active && FD_ISSET(fd->fd, &wfds)) {
            ret--;

            if ((err = fd->handler_func(fd->fd, FD_WRITE, fd->handler_arg)))
                break;
        }
    }

    // done
    return 0;

error:
    return -1;    
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

