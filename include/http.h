#ifndef _HTTP_H_
#define _HTTP_H_

#include "colors.h"
#include "config.h"
#include "utility.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_PORT_S  "80"
#define DEFAULT_PORT_L  2
#define DEFAULT_PORT    80
#define DEFAULT_MAX_AGE 3600

typedef struct HTTP_Request {
    char *request;
    char *path;
    char *host;
    char *port;

    size_t request_l;
    size_t path_l;
    size_t host_l;
    size_t port_l;
} HTTP_Request;

typedef struct HTTP_Response {
    char *header;
    char *body;
    long max_age;

    size_t header_l;
    size_t body_l;
    size_t response_l;
} HTTP_Response;

HTTP_Request *HTTP_parse_request(char *request, size_t len);
HTTP_Response *HTTP_parse_response(char *response, size_t len);
void HTTP_free_request(void *request);
void HTTP_free_response(void *response);
void HTTP_print_request(void *request);
void HTTP_print_response(void *response);
short HTTP_get_port(HTTP_Request *request);
long HTTP_get_max_age(char *httpstr);
ssize_t HTTP_get_content_length(char *httpstr);
char *HTTP_response_to_string(HTTP_Response *request, long age, size_t *strlen,
                              bool is_from_cache);
size_t HTTP_body_len(char *httpstr, size_t len);
ssize_t HTTP_header_len(char *httpstr);

#endif /* _HTTP_H_ */
