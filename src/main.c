#include "http.h"
#include "cache.h"
#include "config.h"
#include "proxy.h"
#include <assert.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }
    // char request0[85] = "GET http://www.example.com/some/path HTTP/1.1\r\nHost: www.someschool.edu:666\r\n\r\n"; // 85 bytes
    // char request1[85] = "GET http://www.example.com/some/path HTTP/1.1\r\nHost: www.someschool.edu:666\r\n\r\n"; // 85 bytes
    // char request2[85] = "GET http://www.example.com/some/path HTTP/1.1\r\nHost: www.someschool.edu:666\r\n\r\n"; // 85 bytes
    // char request3[] =
    //     "GET http://www.example.com/some/path HTTP/1.1\r\n"
    //     "Host: www.someschool.edu:888\r\n\r\n";    // 86 bytes
    // char response4[] =
    //     "HTTP/1.1 200 OK\r\nContent-Type: "
    //     "text/html\r\nContent-Length: 47\r\n\r\n<html><body><h1>Hello "
    //     "World!</h1></body></html>";    // 148 bytes

    /* --- Testing Proxy --- */
    int port = atoi(argv[1]);
    fprintf(stderr, "Port: %d\n\n", port);
    Proxy_run(atoi(argv[1]), CACHE_SZ);

    return 0;
}
