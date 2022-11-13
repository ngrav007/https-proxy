#include "proxy.h"

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }
    // char request[] =
    //     "GET http://www.example.com/some/path HTTP/1.1\r\n"
    //     "Host: www.someschool.edu:888\r\n\r\n";    // 86 bytes

    // char response[] =
    //     "HTTP/1.1 200 OK\r\nContent-Type: "
    //     "text/html\r\nContent-Length: 47\r\n\r\n<html><body><h1>Hello "
    //     "World!</h1></body></html>";    // 148 bytes

    // /* --- Testing HTTP_Request --- */
    // HTTP_Request req = HTTP_parse_request(request, 86);
    // HTTP_print_request(req);
    // short port = HTTP_get_port(req->host);
    // HTTP_free_request(req);

    // /* --- Testing HTTP_Response --- */
    // struct HTTP_Response *res = HTTP_parse_response(response,
    // sizeof(response)); HTTP_print_response(res); size_t res_str_len = 0;
    // fprintf(stderr, "response body: %s\n", res->body);
    // char *res_str = HTTP_response_to_string(res, 42, &res_str_len);
    // fprintf(stderr, "%sResponse string:%s\n%s", BLU, reset, res_str);
    // free(res_str);
    // HTTP_free_response(res);

    /* --- Testing Proxy --- */
    int port = atoi(argv[1]);
    Proxy_run(port, CACHE_SZ);

    return 0;
}
