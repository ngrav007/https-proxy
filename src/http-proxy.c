#include "cache.h"
#include "config.h"
#include "http.h"
#include "proxy.h"

#include <assert.h>

int main(int argc, char **argv)
{
    // char incomplete_request[48] = "GET http://www.example.com/some/path HTTP/1.1\r\n\0";
    // char request0[80] =  "GET http://www.example.com/some/path HTTP/1.1\r\nHost: www.someschool.edu\r\n\r\n\0"; // 79 bytes
    // char request1[80] = "GET http://www.example.com/some/path HTTP/1.1\r\nHost: www.someschool.edu:666\r\n\r\n\0"; // 79 bytes
    // char request2[80] = "GET http://www.example.com/some/path:888 HTTP/1.1\r\nHost: www.someschool.edu\r\n\r\n\0"; // 79 bytes
    // char request3[84] = "GET http://www.example.com/some/path:888 HTTP/1.1\r\nHost: www.someschool.edu:999\r\n\r\n\0"; // 82 bytes
    // char response0[111] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 47\r\n\r\n<html><body><h1>HelloWorld!</h1></body></html>\0"; // 109 bytes
    // char response1[111] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 48\r\n\r\n<html><body><h1>HelloWorld!</h1></body></html>\0"; // 109 bytes
    // fprintf(stderr, "[request0] (%lu bytes)\n%s\n", sizeof(request0), request0);
    // fprintf(stderr, "[request1] (%lu bytes)\n%s\n", sizeof(request1), request1);
    // fprintf(stderr, "[request2] (%lu bytes)\n%s\n", sizeof(request2), request2);
    // fprintf(stderr, "[request3] (%lu bytes)\n%s\n", sizeof(request3), request3);
    // fprintf(stderr, "[response0] (%lu bytes)\n%s\n", sizeof(response0), response0);
    // fprintf(stderr, "[response1] (%lu bytes)\n%s\n", sizeof(response1), response1);
    // fprintf(stderr, "[incomplete_request] (%lu bytes)\n%s\n", sizeof(incomplete_request), incomplete_request);

    // /* TEST : HTTP_GOT_HEADER ----------------------------------------------- */    
    // bool test0 = HTTP_got_header(request0);
    // fprintf(stderr, "test-request0: %s\n", test0 ? "true" : "false");
    // bool test1 = HTTP_got_header(request1);
    // fprintf(stderr, "test-request1: %s\n", test1 ? "true" : "false");
    // bool test2 = HTTP_got_header(request2);
    // fprintf(stderr, "test-request2: %s\n", test2 ? "true" : "false");
    // bool test3 = HTTP_got_header(response0);
    // fprintf(stderr, "test-response0: %s\n", test3 ? "true" : "false");
    // bool test4 = HTTP_got_header(response1);
    // fprintf(stderr, "test-response1: %s\n", test4 ? "true" : "false");
    // bool test5 = HTTP_got_header(incomplete_request);
    // fprintf(stderr, "test-incomplete: %s\n", test5 ? "true" : "false");

    // /* TEST : HTTP_ADD_FIELD ------------------------------------------------ */
    // char *cache_response = calloc(1, sizeof(response0) + 1);
    // memcpy(cache_response, response0, sizeof(response0));
    // size_t cache_response_len = strlen(cache_response);
    // fprintf(stderr, "[cache_response] (%lu bytes)\n%s\n", cache_response_len, cache_response);
    // // Test Response without Age field 
    // int ret = HTTP_add_field(&cache_response, "Age", "50", &cache_response_len);
    // fprintf(stderr, "ret: %d\n", ret);
    // fprintf(stderr, "[cache_response] (%lu bytes)\n%s\n", cache_response_len, cache_response);
    // // Test Response with Age field
    // ret = HTTP_add_field(&cache_response, "Age", "30", &cache_response_len);
    // fprintf(stderr, "ret: %d\n", ret);
    // fprintf(stderr, "[cache_response] (%lu bytes)\n%s\n", cache_response_len, cache_response);
    // free(cache_response);

    /* TEST REQUEST */
    // fprintf(stderr, "[request0] (%lu bytes)\n%s\n", strlen(request0), request0);
    // Request *request = Request_new(request0, strlen(request0));
    // Request_print(request);
    // Request_free(request);

    // request = Request_new(request1, strlen(request1));
    // Request_print(request);
    // Request_free(request);

    // request = Request_new(request2, strlen(request2));
    // Request_print(request);
    // Request_free(request);

    // request = Request_new(request3, strlen(request3));
    // Request_print(request);
    // Request_free(request);

    // Request *cmp1 = Request_new(request0, strlen(request0));
    // Request *cmp2 = Request_new(request1, strlen(request1));
    // int result = Request_compare(cmp1, cmp2);
    // fprintf(stderr, "result: %d\n", result);
    // result = Request_compare(cmp2, cmp1);
    // fprintf(stderr, "result: %d\n", result);
    // result = Request_compare(cmp1, cmp1);
    // fprintf(stderr, "result: %d\n", result);
    // Request_free(cmp1);
    // Request_free(cmp2);

    // /* TEST RESPONSE */
    // Response *response = Response_new(response0, strlen(response0));
    // Response_print(response);
    // Response_free(response);

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int port = atoi(argv[1]);
    if (port <= 0) {
        fprintf(stderr, "Invalid port number: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    Proxy_run(port, 10);

    return EXIT_SUCCESS;
}
