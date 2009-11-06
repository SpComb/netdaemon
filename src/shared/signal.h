#ifndef SHARED_SIGNAL_H
#define SHARED_SIGNAL_H

/**
 * @file
 *
 * Signal handling
 */
#include <sys/types.h>
#include <sys/queue.h>
#include <signal.h>

/**
 * Process-context signal handler function.
 */
typedef int (*signal_func) (void *arg);

/**
 * Per-signal handler state 
 */
struct signal_handler {
    /** Signal number */
    int signal;

    /** Lower handler */
    signal_func func;

    /** Context argument */
    void *func_arg;

    /** Number of calls */
    volatile sig_atomic_t ncalls;

    /** Part of list */
    LIST_ENTRY(signal_handler) signal_handlers;
};

/**
 * Init module state
 */
int signal_init (void);

/**
 * Register a signal handler
 *
 * @param state     structure to fill out and use for signal handling
 * @param signal    signal to handle
 * @param func      lower signal handler
 * @param arg       context argument for func
 */
int signal_register (struct signal_handler *state, int signal, signal_func func, void *arg);

/**
 * Scan for and run signal handlers.
 *
 * @return <0 if handler returns error, number of signals handled otherwise
 */
int signal_run (void);

#endif
