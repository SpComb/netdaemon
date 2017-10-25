# [Archive] netdaemon (2009)

***Archived for historical curiosity***, this project was done as coursework for the TKK S-38.3600 Unix Application Programming course, with the vision of being used for the [myottd2](https://github.com/SpComb/myottd2/) service.

See the original [README](README) for more technical details/examples.

# Learning Diary

Reformatted from the `Learning Diary.odt` document, this documentation is part of the coursework assignment.

## Introduction

My primary objective when starting this assignment was to produce something that might have some practical value, putting the emphasis on coming up with an project that might turn out useful for some scenario, and thence writing “real” code that would be suitable for actual use.

Naturally, a specification such as this ensures a very broad feature set, guaranteeing the impossibility of ever actually “finishing” the project, but the current implementation provides a robust infrastructure and fulfills the basic feature set.

## Use-case / Scenario

The design for the project is loosely based on a previous situation which would have required the development of a similar application.

Consider a web-based hosting service that allows registered users to use their web browser to create, run, control and configure hosted game servers, running on the hosting provider's servers. To isolate the servers run by a given user from other users and the provider's platform, each user's servers are run inside a lightweight virtualization container (using e.g. linux-vserver), providing what is essentially an extended chroot. However, the control services running on the host OS must be able to access and control the game servers running inside the containers, ideally in a robust fashion such that the game servers can operate independently of the control services, allowing the later to be upgraded without completely disrupting all services.

In this case, a solution would be to run a control daemon in each container, with the user's services running under this daemon's direct control, and then allowing the control services on the host OS to access to the control daemon in a controlled fashion, using e.g. an RPC-style mechanism to transmit commands and data.

## Design

Netdaemon (not the final name...) implements the control daemon, client library and example command-line client components of this infrastructure. The daemon  listens on an UNIX socket, and accepts connections from other processes. Using the command-based protocol, each client can be attached to one process at a time to receive process status updates and stdout/stderr output, as well as controlling the attached process by sending it signals or stdin input. Clients can start new processes, attach to existing ones, and list available processes.

The daemon offers a binary message-based protocol with request-response-event commands, transmitting integers, strings, buffers and arrays as data.

## Diary

The project is fairly broad and complex, with the final version consisting of roughly 4800 lines of .c and .h files. However, the code is highly modular, and hence incremental development was fairly straightforward.

The initial work consisted of setting up the version control, build infrastructure and project filesystem layout. This was mainly based on prior Makefiles written for other projects, which provided a structured layout with features like #include-based compile dependencies.

The first step was writing some skeleton code to lay out the various initial modules. The very first piece of code written was the daemon's service, creating an AF_UNIX/SOCK_SEQPACKET -based listening socket bound to some path. Next, some simple code for the client library to connect to this service.

The next, more difficult piece was writing the daemon's mainloop. Since I wanted to stick to a single thread, this would require using non-blocking I/O for all sockets and subprocesses. I wrote a simple select_fd/select_loop module, loosely modeled on libevent, and then used this to implement accept() for the service, passing the client socket to the daemon's client module.

Once the client and daemon were connected, the next step was writing the actual protocol code to send messages from the client to the daemon. This included using the select module to implement recv() for the daemon, and writing a proto module to offer buffer-based proto_read/write_* functions. At this point, the client can send() a CMD_HELLO packet to the daemon, which can recv() it, dispatch to the correct command-handler and decode it.

The next step was to start implementing the first “real” command, CMD_START. The client had to write out the executable path, arguments and environment into a packet, and let the daemon decode them. Next, implement the basic fork()+execve() code for the daemon's process module, so that the daemon's CMD_START module actually does something.

At this point some effort was required to fully work out the error/reply-handling semantics of the daemon's message-handling functionality. A fairly streamlined process resulted, involving per-message IDs, function error return codes for fatal/non-fatal errors, and properly handling abort/disconnect and replies.

Next, the process module was further implemented to properly use pipes for the spawned process' stdin/stdout/stderr channels, integrating these into the select loop, and passing received data back to the daemon's client using CMD_DATA.

At this point, the daemon's client knows that it is attached to a process and can send proper error/result replies to the client, but the client does not yet know how to recv() anything. The client library's API was designed to be synchronous/blocking, so that e.g. nd_start(client, “/bin/ls”, …); would send the CMD_START message to the daemon, and then wait for the CMD_ATTACHED/CMD_ERROR reply, and return accordingly.

This was implemented using an asynchronous-send + blocking-wait, to first send() a command out, and then recv() commands until we get a reply to the issued command. Implementing this was simple thanks to the per-message ID.

Next, the various bits were refined, and then the rest of the features implemented. One very important piece was proper signal handling in the daemon process. This was integrated into the select_loop such that if a system call returns with EINTR, the daemon's main loop can immediately check the signal handler state and run the non-stub signal handlers. Support for SIGINT and SIGCHLD was added, handling the waitpid(), updating the status for the relevant process, notifying any attached clients, and eventually disposing of the process state once all clients have detached.

At this point the client could now mimick its attached process fairly well, streaming the actual stdout/stderr output, as well as reacting to exit/kill. The next step was sending input from stdin on the client to the daemon, which was somewhat more difficult, since the client had to implement its own select() loop and negotiate with the client library to effectively mutliplex I/O between on stdin and the UNIX socket.

Next, all that was left was to implement the remaining commands (CMD_ATTACH, CMD_KILL, CMD_LIST, proper CMD_HELLO handshake), which was fairly easy now that the general structure of the code was in place.

## Issues

Handling EOF properly on the pipes also ended up taking a fair amount of effort, particularly due to the implementation leaking pipe fds around fork/exec, but this was fixed to not rely on O_CLOEXEC.

The code was tested in a fairly ad-hoc manner, with there not really being enough time to do systematic unit/coverage-testing. As the internal pieces were developed further, the 'main' functions of the daemon/client slowly evolved from dumb testing stubs to more sensible command-line interfaces.

The error handling scheme used in the code works fairly well, with most system/fatal errors being communicated via errno, and the daemon's command handlers using a more complicated negative-zero-positive return code scheme to indicate connection-specific abort/success/error conditions, which are then automatically handled by the generic packet-handling code.

One big issue which remained largely unsolved was the interaction between the select loop iterating through the list of FDs, and the FD handlers causing FDs to be removed from the select loop. The issue was not entirely solved, but currently works well enough (although presumably relying on some measure of luck). This would eventually need to be fixed for the general case using something that properly supports concurrent iteration/modification.

## Further Development
The scope of the project leaves a lot of possibilities open for further work on the code. For instance, the client/daemon communication code could be further improved to also support stream-based TCP connections, allowing these process-control daemons to be distributed across a network.

Additionally, the daemon could support more security features, such as authentication of client connections, and then requiring authorization for starting/attaching to processes. Being able to control the chroot or UID of spawned processes could also have advantages in some use cases.

The daemon could also offer more flexible options for process I/O, perhaps being capable of redirecting process output to a log file.

Identification of processes could also be improved, particularly to facilitate the case of having automatic database-backed systems manage the processes.
