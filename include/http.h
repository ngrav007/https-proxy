#ifndef _HTTP_H_
#define _HTTP_H_

#include "colors.h"
#include "config.h"
#include "utility.h"

#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct HTTP_Header {
    char *method;
    char *path;
    char *host;
    char *port;

    size_t path_l;
    size_t host_l;
    size_t port_l;
    size_t method_l;
} HTTP_Header;

bool HTTP_got_header (char *buffer);
int HTTP_parse(HTTP_Header *header, char *buffer, size_t len);
void HTTP_free_header(void *header);
void HTTP_free_request(void *request);
void HTTP_print_request(void *request);
short HTTP_get_port(HTTP_Header *request);
long HTTP_get_max_age(char *httpstr);
ssize_t HTTP_get_content_length(char *httpstr);
ssize_t HTTP_body_len(char *httpstr, size_t len);
ssize_t HTTP_header_len(char *httpstr);

char *parse_path(char *header, size_t *len);
char *parse_method(char *header, size_t *len);
char *parse_host(char *header, size_t *host_len, char **port, size_t *port_len);
char *parse_header_raw   (char *message, size_t *len);
char *parse_header_lower (char *message, size_t *len);
long parse_contentLength(char *header);
char *removeSpaces      (char *str, int size);
unsigned int parse_maxAge(char *header);
char *make_ageField(unsigned int age);

#endif /* _HTTP_H_ */
