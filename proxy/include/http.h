#ifndef _HTTP_H_
#define _HTTP_H_

#include "colors.h"
#include "config.h"
#include "utility.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct Request {
    char *method;
    char *path;
    char *version;
    char *host;
    char *port;
    char *body;
    char *raw;

    size_t method_l;
    size_t path_l;
    size_t version_l;
    size_t host_l;
    size_t port_l;
    size_t body_l;
    size_t raw_l;

} Request;

typedef struct Response {
    char *uri;
    char *version;
    char *status;
    char *cache_ctrl; /* Cache-Control header field. */
    char *body;       /* body of the response message. */
    char *raw;        /* original "raw" response message. */

    long max_age;          /* max-age value from Cache-Control header. */
    size_t content_length; /* Content-Length value from header. */

    size_t uri_l;
    size_t version_l;
    size_t status_l;
    size_t cache_ctrl_l;
    size_t body_l;
    size_t raw_l;
} Response;

/* HTTP Functions */
bool HTTP_got_header(char *buffer);
int HTTP_add_field(char **buffer, size_t *buffer_l, char *field, char *value);

/* HTTP Request Functions */
Request *Request_new(char *buffer, size_t buffer_l);
Request *Request_create(char *method, char *path, char *version, char *host,
                        char *port, char *body);
Request *Request_copy(Request *req);
void Request_free(void *req);
void Request_print(void *req);
int Request_compare(void *req1, void *req2);

/* HTTP Response Functions */
Response *Response_new(char *method, size_t method_l, char *uri, size_t uri_l, char *msg, size_t msg_l);
void Response_free(void *response);
unsigned long Response_size(Response *response);
char *Response_get(Response *response);
void Response_print(void *response);
int Response_compare(void *response1, void *response2);
Response *Response_copy(Response *response);

/* HTTP Raw String Functions */
char *Raw_request(char *method, char *url, char *host, char *port, char *body, size_t *raw_l);

// int color_links(char **buffer, size_t *buffer_l, int color_flag);
int color_links(char **buffer, size_t *buffer_l, char **cache_keys, int num_keys);
// int foundKey(char *target, char *arr[], int arr_size);
int foundKey(char *target, int target_l, char **arr, int arr_size);
// int perfectKeyPrefix(char *pre, char *str);
int perfectKeyPrefix(char *pre, int pre_l, char *str);

#endif /* _HTTP_H_ */
