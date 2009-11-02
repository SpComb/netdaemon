#include "client.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <errno.h>



int nd_open_unix (struct nd_client **client_ptr, const char *path)
{
    
}

void nd_destroy (struct nd_client *client)
{
    if (client->sock)
        close(client->sock);

    free(client);
}

