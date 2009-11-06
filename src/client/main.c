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

/**
 * Run while attached to some process, shuffling data between stdin/out/err, until STDINT
 */
static int run_process (struct nd_client *client)
{
    int err;

    while (true) {
        if ((err = nd_poll(client, NULL)) < 0)
            return -1;

        else if (err)
            return 0;

        log_debug("nd_poll tick");
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

    // start a new process
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

static const struct command {
    const char *name;

    int (*func) (struct nd_client *client, char **argv);

} commands[] = {
    { "start",      cmd_start           },
    { NULL,         NULL                }
};

static int on_data (const char *buf, size_t len, FILE *stream)
{
    // write out
    if (fwrite(buf, len, 1, stream) < 0)
        return -1;

    // ok
    return 0;
}

static int on_stdout (struct nd_client *client, const char *buf, size_t len, void *arg)
{
    return on_data(buf, len, stdout);
}

static int on_stderr (struct nd_client *client, const char *buf, size_t len, void *arg)
{
    return on_data(buf, len, stderr);
}

static int on_exit_ (struct nd_client *client, int status, void *arg)
{
    log_info("Process exited with status: %d", status);

    // neat trick
    exit(status);
    
    return 1;
}

static int on_kill (struct nd_client *client, int sig, void *arg)
{
    log_info("Process killed by signal: %d(%s)", sig, strsignal(sig));

    // neat trick
    raise(sig);

    return 1;
}

static const struct nd_callbacks callbacks = {
    .on_stdout      = on_stdout,
    .on_stderr      = on_stderr,
    .on_exit        = on_exit_,
    .on_kill        = on_kill,
};

static const struct option options[] = {
    { "help",       false,  NULL,   'h' },
    { "quiet",      false,  NULL,   'q' },
    { "verbose",    false,  NULL,   'v' },
    { "debug",      false,  NULL,   'D' },
    { "unix",       true,   NULL,   'u' },
    { 0,            0,      0,      0   }
};

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
    
    if (setup_client(&client, unix_path))
        EXIT_ERROR(EXIT_FAILURE, "setup_client");

    if (run_cmd(client, argv[optind], argv + optind + 1))
        EXIT_ERROR(EXIT_FAILURE, "run_cmd");

    // ok
    return EXIT_SUCCESS;
}

