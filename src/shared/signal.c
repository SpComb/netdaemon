#include "signal.h"
#include "shared/log.h"

#include <string.h>

/**
 * Main list of all signal handlers
 */
static LIST_HEAD(signal_handlers, signal_handler) handlers;

/**
 * Total number of outstanding signal handler calls
 */
volatile sig_atomic_t ncalls;

static void signal_handler (int sig)
{
    struct signal_handler *handler;

    // find handler
    LIST_FOREACH(handler, &handlers, signal_handlers) {
        if (handler->signal == sig)
            break;
    }

    if (handler) {
        // count
        handler->ncalls++;
        ncalls++;

    } else {
        // XXX: fail
        log_warn("Unknown signal: %d", sig);
    }
}

int signal_init (void)
{
    LIST_INIT(&handlers);

    return 0;
}

int signal_register (struct signal_handler *state, int signal, signal_func func, void *arg)
{
    struct sigaction sa;

    // store
    state->signal = signal;
    state->func = func;
    state->func_arg = arg;
    state->ncalls = 0;

    // prep
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;

    // add to list
    LIST_INSERT_HEAD(&handlers, state, signal_handlers);

    // register handler
    if (sigaction(signal, &sa, NULL) < 0)
        goto error;

    // ok
    return 0;

error:
    // remove from list
    LIST_REMOVE(state, signal_handlers);    

    return -1;
}

int signal_run (void)
{
    struct signal_handler *handler;
    int ret = 0, count = 0;

    // fast-path
    if (!ncalls)
        return 0;

    // look for calls
    LIST_FOREACH(handler, &handlers, signal_handlers) {
        while (handler->ncalls) {
            // count
            ncalls--;
            handler->ncalls--;
            count++;

            // invoke
            if ((ret = handler->func(handler->func_arg)) <0)
                return ret;
        }
    }

    return count;
}

