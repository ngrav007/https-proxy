#include "cache.h"
#include "config.h"
#include "http.h"
#include "proxy.h"

#include <assert.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    short port = atoi(argv[1]);
    if (port <= 0) {
        fprintf(stderr, "Invalid port number: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    Proxy_run(port);

    return EXIT_SUCCESS;
}
