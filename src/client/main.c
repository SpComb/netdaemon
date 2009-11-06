#define _GNU_SOURCE
#include "lib/client.h"
#include "shared/log.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <signal.h> // raise()
#include <unistd.h>

/**
 * Copy data from stdin to nd_client
 */
static int run_stdin (struct nd_client *client)
{
    char buf[4096];
    ssize_t len;

    // read in from stdin
    if ((len = read(STDIN_FILENO, buf, sizeof(buf))) < 0)
        return -1;
    
    else if (!len)
        // close stream
        return nd_stdin_eof(client);

    else
        // send out
        return nd_stdin_data(client, buf, len);

    // ok
    return 0;
}

/**
 * Run while attached to some process, shuffling data between stdin/client, until STDINT
 */
static int run_process (struct nd_client *client)
{
    int err;
    int nd_fd, maxfd;
    fd_set rfds, wfds;
    bool want_read, want_write;

    while (true) {
        // check nd
        if ((nd_fd = nd_poll_fd(client, &want_read, &want_write)) < 0)
            return -1;

        // set
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_SET(STDIN_FILENO, &rfds);

        if (want_read)
            FD_SET(nd_fd, &rfds);
        
        if (want_write)
            FD_SET(nd_fd, &wfds);

        maxfd = nd_fd + 1;
        
        // poll
        if ((err = select(maxfd, &rfds, &wfds, NULL, NULL)) < 0)
            return -1;
        
        // dispatch
        if ((FD_ISSET(nd_fd, &rfds) || FD_ISSET(nd_fd, &wfds))) {
            struct timeval tv = { 0, 0 };

            // handle activity on nd_client
            if ((err = nd_poll(client, &tv)) < 0)
                return -1;

            else if (err)
                return 0;

        } else if (FD_ISSET(STDIN_FILENO, &rfds)) {
            // activity on stdin
            if (run_stdin(client))
                return -1;
        }
    }

    return 0;
}

/**
 * Start a new process and remain attached to it
 */
static int cmd_start (struct nd_client *client, char **argv)
{
    char *envp[] = { NULL };
    int err;

    if (!argv[0]) {
        log_error("No exec path given");

        goto error;
    }

    // start a new process with argv given on the command-line
    // XXX: use environ for envp?
    if ((err = nd_start(client, argv[0], (const char **) argv + 1, (const char **) envp)) < 0) {
        log_errno("nd_start: %s", nd_error_msg(client));

        goto error;

    } else if (err > 0) {
        log_error("nd_start: %d: %s", nd_error(client), nd_error_msg(client));

        goto error;
    }

    // yay
    log_info("Attached to process: %s", nd_process_id(client));

    // remain attached
    if (run_process(client))
        goto error;

    // ok
    return 0;

error:
    return -1;    
}

/**
 * CLI Commands
 */
static const struct command {
    const char *name;

    int (*func) (struct nd_client *client, char **argv);

} commands[] = {

    { "start",      cmd_start           },
    { NULL,         NULL                }
};

/**
 * Got data from process on \a stream
 */
static int on_data (const char *buf, size_t len, FILE *stream)
{
    if (len) {
        // write out
        if (fwrite(buf, len, 1, stream) < 0)
            return -1;

    } else {
        // EOF
        if (fclose(stream) < 0)
            return -1;
    }

    // ok
    return 0;
}

/**
 * Got data from process on stdout,
 *
 * Copy to our stdout,
 */
static int on_stdout (struct nd_client *client, const char *buf, size_t len, void *arg)
{
    return on_data(buf, len, stdout);
}

/**
 * Got data from process on stderr,
 *
 * Copy to our stdoerr,
 */
static int on_stderr (struct nd_client *client, const char *buf, size_t len, void *arg)
{
    return on_data(buf, len, stderr);
}

/**
 * Process exited with exit status,
 *
 * Exit with the same status.
 */
static int on_exit_ (struct nd_client *client, int status, void *arg)
{
    log_info("Process exited with status: %d", status);

    // neat trick
    exit(status);
    
//    // return to run_process
//    return 1;
}

/**
 * Process terminated by signal.
 *
 * Attempt to simulate the same signal.
 */
static int on_kill (struct nd_client *client, int sig, void *arg)
{
    log_info("Process killed by signal: %d(%s)", sig, strsignal(sig));

    // neat trick
    raise(sig);

    // return to run_process
    // XXX: in case raise doesn't terminate us?
    return 1;
}

/**
 * Remote process event handlers
 */
static const struct nd_callbacks callbacks = {
    .on_stdout      = on_stdout,
    .on_stderr      = on_stderr,
    .on_exit        = on_exit_,
    .on_kill        = on_kill,
};

/**
 * Command-line options
 */
static const struct option options[] = {
    { "help",       false,  NULL,   'h' },
    { "quiet",      false,  NULL,   'q' },
    { "verbose",    false,  NULL,   'v' },
    { "debug",      false,  NULL,   'D' },
    { "unix",       true,   NULL,   'u' },
    { 0,            0,      0,      0   }
};

/**
 * Print usage/help info on stderr
 */
void help (const char *argv0)
{
    fprintf(stderr, "Usage: %s [options] [command] [--] [args [...]]\n", argv0);
}

/**
 * Construct a nd_client and connect to the given unix socket
 */
int setup_client (struct nd_client **client_ptr, const char *unix_path)
{
    struct nd_client *client = NULL;

    // create
    if (nd_create(&client, &callbacks, NULL)) {
        log_errno("nd_create");

        goto error;
    }
    
    // connect
    if (nd_open_unix(client, unix_path) < 0) {
        log_errno("nd_open_unix: %s", unix_path);

        goto error;
    }

    log_info("Connected to UNIX socket: %s", unix_path) ;

    // greet
    if (nd_cmd_hello(client) < 0) {
        log_errno("nd_cmd_hello");

        goto error;
    }

    // ok
    *client_ptr = client;

    return 0;

error:
    if (client)
        nd_destroy(client);

    return -1;
}

/**
 * Look up the CLI command of the given name, and run it with the given args
 */
int run_cmd (struct nd_client *client, const char *name, char **argv)
{
    const struct command *cmd;

    for (cmd = commands; cmd->name && cmd->func; cmd++) {
        if (strcasecmp(name, cmd->name) == 0)
            break;
    }

    if (cmd->name && cmd->func)
        return cmd->func(client, argv);
    else
        return -1;
}

int main (int argc, char **argv)
{
    int opt;
    const char *unix_path = NULL;
    
    // parse arguments
    while ((opt = getopt_long(argc, argv, "hqvDu:", options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                // display help
                help(argv[0]);
                
                return EXIT_SUCCESS;

            case 'q':
                // supress excess log output
                set_log_level(LOG_WARN);

                break;

            case 'v':
            case 'D':
                // display additional output
                set_log_level(LOG_DEBUG);

                break;

            case 'u':
                // connect to unix service
                unix_path = optarg;

                break;

            case '?':
                // useage error
                help(argv[0]);

                return EXIT_FAILURE;

            default:
                // getopt???
                FATAL("getopt_long returned unknown code %d", opt);
        }
    }

    // validate
    if (!unix_path)
        EXIT_WARN(EXIT_FAILURE, "No service (--unix) given");

    if (!argv[optind]) {
        // no command given
        help(argv[0]);
        
        return EXIT_FAILURE;
    }

    struct nd_client *client;
    
    // setup client state and connect
    if (setup_client(&client, unix_path))
        EXIT_ERROR(EXIT_FAILURE, "setup_client");

    // run as commanded
    if (run_cmd(client, argv[optind], argv + optind + 1))
        EXIT_ERROR(EXIT_FAILURE, "run_cmd");

    // ok
    return EXIT_SUCCESS;
}

