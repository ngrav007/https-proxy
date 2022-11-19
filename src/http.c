#include "http.h"

static int parse_response(Response *res, char *buffer, size_t buffer_l);
static int parse_response_fields(Response *res, char *buffer, size_t buffer_l);
static int parse_statusline(Response *res, char *response);
static char *parse_status(char *response, size_t *status_l, char **saveptr);
static size_t parse_contentlength(char *header, size_t header_l);
static unsigned int parse_maxage(char *cachecontrol, size_t cachecontrol_l);
static char *parse_cachecontrol(char *header, size_t *cachecontrol_l);
static int parse_request(Request *req, char *buffer, size_t buffer_l);
static int parse_request_fields(Request *req, char *buffer, size_t buffer_l);
static int parse_startline(Request *req, char *request);
static char *parse_method(char *header, size_t *method_len, char **saveptr);
static char *parse_path(char *header, size_t *path_l, char **saveptr);
static char *parse_host(char *header, size_t header_l, size_t *host_l);
static char *parse_port(char **host, size_t *host_l, char *path, size_t *port_l);
static char *parse_version_req(char *header, size_t *version_l, char **saveptr);
static char *parse_body(char *buffer, size_t buffer_l, size_t *body_l);
static char *parse_version_res(char *header, size_t *version_l, char **saveptr);
/* HTTP Functions ----------------------------------------------------------- */

/* HTTP_add_field
 *    Purpose: Adds a new field to a buffer containing an HTTP header with the
 *             given name and value. If the field already exists, the value is
 *             updated. If the field does not exist, it is added. The buffer is
 *             reallocated if necessary. The buffer must be null terminated.
 *
 */
int HTTP_add_field(char **buffer, char *field, char *value, size_t *buffer_l)
{
    if (buffer == NULL || *buffer == NULL || field == NULL || value == NULL || buffer_l == NULL) {
        fprintf(stderr, "Error: invalid params\n");
        return -1;
    }

    /* convert buffer to lowercase */
    char *buffer_lc = get_buffer_lc(*buffer, *buffer + *buffer_l);

    char *header_start = buffer_lc;
    char *header_end = strstr(buffer_lc, "\r\n\r\n");
    if (header_end == NULL) {
        fprintf(stderr, "Error: invalid header 0\n");
        return -1;
    }

    char *field_start;
    // char *field_end;
    char *new_buffer;
    size_t header_l = header_end - header_start;
    size_t field_l = strlen(field);
    size_t value_l = strlen(value);
    size_t offset = 0;
    size_t new_buffer_l;
    char *field_lc = get_buffer_lc(field, field + field_l);


    /* check if field already exists */
    field_start = strstr(buffer_lc, field_lc);
    if (field_start == NULL) {  // field does not exist
        new_buffer_l = *buffer_l + field_l + value_l + FIELD_SEP_L + CRLF_L;
        new_buffer = calloc(new_buffer_l + 1, sizeof(char));
        if (new_buffer == NULL) {
            fprintf(stderr, "Error: invalid header 1\n");
            free(buffer_lc);
            return -1;
        }
        fprintf(stderr, "*buffer = %s\n", *buffer);
        fprintf(stderr, "field = %s\n", field);
        fprintf(stderr, "value = %s\n", value);
        fprintf(stderr, "buffer_l = %zu\n", *buffer_l);
        fprintf(stderr, "field_l = %zu\n", field_l);
        fprintf(stderr, "value_l = %zu\n", value_l);
        fprintf(stderr, "header_l = %zu\n", header_l);
        fprintf(stderr, "new_buffer_l = %zu\n", new_buffer_l);

        memcpy(new_buffer, *buffer, header_l); // copy header
        offset += header_l;
        memcpy(new_buffer + offset, CRLF, CRLF_L); // add CRLF
        offset += CRLF_L;
        memcpy(new_buffer + offset, field, field_l); // copy field
        offset += field_l;
        memcpy(new_buffer + offset, FIELD_SEP, FIELD_SEP_L); // copy field sep
        offset += FIELD_SEP_L;
        memcpy(new_buffer + offset, value, value_l); // copy value
        offset += value_l;
        memcpy(new_buffer + offset, header_end, *buffer_l - header_l); // copy rest of header
        offset += *buffer_l - header_l;

        free(*buffer);
        *buffer = new_buffer;
        *buffer_l = new_buffer_l;
    }
    // } else { // field exists
    //     field_end = strstr(field_start, CRLF);
    //     if (field_end == NULL) {
    //         fprintf(stderr, "Error: invalid header 1\n");
    //         return -1;
    //     }

    //     new_buffer_l = *buffer_l + value_l + field_l - (field_end - field_start);
    //     new_buffer = calloc(new_buffer_l + 1, sizeof(char));
    //     if (new_buffer == NULL) {
    //         fprintf(stderr, "Error: invalid header 2\n");
    //         return -1;
    //     }

    //     memcpy(new_buffer, *buffer, field_start - *buffer); // copy header up to field
    //     offset += field_start - *buffer;
    //     memcpy(new_buffer + offset, field, field_l); // copy field
    //     offset += field_l;
    //     memcpy(new_buffer + offset, FIELD_SEP, FIELD_SEP_L); // copy field sep
    //     offset += FIELD_SEP_L;
    //     memcpy(new_buffer + offset, value, value_l); // copy value
    //     offset += value_l;
    //     memcpy(new_buffer + offset, field_end, *buffer_l - (field_end - *buffer)); // copy rest of header
    //     offset += *buffer_l - (field_end - *buffer);
    // }

    free(field_lc);
    free(buffer_lc);

    return 0;
}

/* HTTP_got_header
 *    Purpose: Checks if a buffer contains a complete HTTP header.
 * Parameters: @buffer - buffer to check for header, must be null terminated
 *    Returns: true (1) if header is complete, false (0) if not
 */
bool HTTP_got_header(char *buffer)
{
    if (strstr(buffer, "\r\n\r\n") != NULL) {
        return true;
    } else {
        return false;
    }
}

/* Request Functions -------------------------------------------------------- */

/* Request_new
 *    Purpose: Creates a new Request initialized with null values.
 * Parameters: None
 *    Returns: Pointer to a new Request, or NULL if memory allocation fails.
 */
Request *Request_new(char *buffer, size_t buffer_l)
{
    Request *request = calloc(1, sizeof(struct Request));
    if (request == NULL) {
        return NULL;
    }

    parse_request(request, buffer, buffer_l);

    return request;
}

/* Request_create
 *    Purpose: Creates a new Request populated with the given values.
 * Parameters: @method - Pointer to a buffer containing the HTTP method
 *             @path - Pointer to a buffer containing the HTTP URI
 *             @version - Pointer to a buffer containing the HTTP version
 *             @host - Pointer to a buffer containing the HTTP host
 *             @port - Pointer to a buffer containing the HTTP port, can be NULL
 *                     and a default port will be used
 *             @body - Pointer to a buffer containing the HTTP body, can be NULL
 *   Returns: Pointer to a new Request, or NULL if memory allocation fails.
 * 
 * Note: All parameters must be null terminated.
 * 
 */
Request *Request_create(char *method, char *path, char *version, char *host, char *port, char *body)
{
    Request *request = calloc(1, sizeof(struct Request));
    if (request == NULL) {
        return NULL;
    }

    request->method = method;
    request->path = path;
    request->version = version;
    request->host = host;
    request->port = port;
    request->body = body;

    return request;
}

void Request_free(Request *req)
{
    if (req == NULL) {
        return;
    }

    free(req->method);
    free(req->path);
    free(req->version);
    free(req->host);
    free(req->port);
    free(req->body);
    free(req->raw);
    free(req);
}

void Request_print(void *req)
{
    if (req == NULL) {
        return;
    }

    Request *r = (Request *)req;

    fprintf(stderr, "[Request]\n");
    fprintf(stderr, "   Method: %s\n", r->method);
    fprintf(stderr, "     Path: %s\n", r->path);
    fprintf(stderr, "  Version: %s\n", r->version);
    fprintf(stderr, "     Host: %s\n", r->host);
    fprintf(stderr, "     Port: %s\n", r->port);
    fprintf(stderr, "     Body: %s\n", r->body);
}

int Request_compare(Request *req1, Request *req2)
{
    if (req1 == NULL || req2 == NULL) {
        return -1;
    }

    if (memcmp(req1->method, req2->method, req1->method_l) != 0) {
        return -1;
    }

    if (memcmp(req1->path, req2->path, req1->path_l) != 0) {
        return -1;
    }

    if (memcmp(req1->version, req2->version, req1->version_l) != 0) {
        return -1;
    }

    if (memcmp(req1->host, req2->host, req1->host_l) != 0) {
        return -1;
    }

    if (memcmp(req1->port, req2->port, req1->port_l) != 0) {
        return -1;
    }

    if (memcmp(req1->body, req2->body, req1->body_l) != 0) {
        return -1;
    }

    return 0;
}

/* Response Functions ------------------------------------------------------- */
Response *Response_new(char *message, size_t message_l)
{
    Response *response = calloc(1, sizeof(struct Response));
    if (response == NULL) {
        return NULL;
    }

    parse_response(response, message, message_l);

    return response;
}

void Response_free(void *response)
{
    if (response == NULL) {
        return;
    }

    Response *r = (Response *)response;

    free(r->version);
    free(r->status);
    free(r->cache_ctrl);
    free(r->body);
    free(r->raw);
    free(r);
}

unsigned long Response_size(Response *response)
{
    if (response == NULL) {
        return 0;
    }

    return response->raw_l;
}

char *Response_get(Response *response)
{
    if (response == NULL) {
        return NULL;
    }

    return response->raw;
}

void Response_print(void *response)
{
    if (response == NULL) {
        return;
    }

    Response *r = (Response *)response;

    fprintf(stderr, "[Response]\n");
    fprintf(stderr, "  Version: %s\n", r->version);
    fprintf(stderr, "   Status: %s\n", r->status);
    fprintf(stderr, "   Cache-Control: %s\n", r->cache_ctrl);
    fprintf(stderr, "   Content-Length: %ld\n", r->content_length);
}
int Response_compare(void *response1, void *response2);

/* Static Functions --------------------------------------------------------- */

/* parse_request
 *    Purpose: Initializes a Request with a buffer containing an HTTP request,
 *             the buffer must be null terminated. This function assumes that
 *             the buffer contains a valid HTTP request. If the buffer does not
 *             contain a valid HTTP request, the behavior is undefined.
 * Parameters: @req - Pointer to a Request to initialize
 *             @buffer - Pointer to a buffer containing an HTTP request
 *    Returns: None
 */
static int parse_request(Request *req, char *buffer, size_t buffer_l)
{
    if (req == NULL || buffer == NULL) {
        return -1;
    }

    char *buffer_lc = get_buffer_lc(buffer, buffer + buffer_l);
    if (parse_startline(req, buffer_lc) != 0) {
        return -1;
    }

    if (parse_request_fields(req, buffer_lc, buffer_l) != 0) {
        return -1;
    }

    /* store the raw request */
    req->raw = calloc(buffer_l + 1, sizeof(char));
    if (req->raw == NULL) {
        return -1;
    }

    memcpy(req->raw, buffer, buffer_l);
    req->raw_l = buffer_l;

    free(buffer_lc);

    return 0;

}

static int parse_response(Response *res, char *buffer, size_t buffer_l)
{
    if (res == NULL || buffer == NULL) {
        return -1;
    }
    
    if (parse_statusline(res, buffer) != 0) {
        return -1;
    }

    if (parse_response_fields(res, buffer, buffer_l) != 0) {
        return -1;
    }

    /* store the raw response */
    res->raw = calloc(buffer_l + 1, sizeof(char));
    if (res->raw == NULL) {
        return -1;
    }

    memcpy(res->raw, buffer, buffer_l);
    res->raw_l = buffer_l;

    return 0;
}
static int parse_response_fields(Response *res, char *buffer, size_t buffer_l)
{
    if (res == NULL || buffer == NULL) {
        return -1;
    }

    res->cache_ctrl = parse_cachecontrol(buffer, &res->cache_ctrl_l);

    if (res->cache_ctrl != NULL) {
        res->max_age = parse_maxage(res->cache_ctrl, res->cache_ctrl_l);
    }

    res->content_length = parse_contentlength(buffer, buffer_l);

    res->body = parse_body(buffer, buffer_l, &res->body_l);

    return 0;
}



    
static int parse_statusline(Response *res, char *response)
{
    if (res == NULL || response == NULL) {
        return -1;
    }
    char *saveptr = NULL;
    res->version = parse_version_res(response, &res->version_l, NULL);
    res->status = parse_status(response, &res->status_l, NULL);

    return 0;
}

static char *parse_status(char *response, size_t *status_l, char **saveptr)
{
    if (response == NULL || status_l == NULL) {
        return NULL;
    }

    char *status = NULL;
    if (saveptr != NULL && *saveptr != NULL) {
        status = *saveptr;
    } else {
        status = response;

        /* skip the version */
        status = strchr(status, ' ') + 1;   // skip the space
        if (status == NULL) {
            return NULL;
        }
    }
    fprintf(stderr, "%sstatus = %s%s\n", RED, status, reset);
    char *end = strchr(status, '\r');
    if (end == NULL) {
        return NULL;
    }

    size_t status_len = end - status;
    if (status_l != NULL) {
        *status_l = status_len;
    }

    char *s = calloc(status_len + 1, sizeof(char));
    if (s == NULL) {
        return NULL;
    }

    memcpy(s, status, status_len);

    if (saveptr != NULL) {
        while(isspace(*end)) {
            end++;
        }
        *saveptr = end;
    }

    return s;
}


    
/* parse_request_fields
 *    Purpose: Parses the fields of an HTTP request and initializes the Request
 *             with the values. This function assumes that the buffer contains
 *             a valid HTTP request. If the buffer does not contain a valid HTTP
 *             request, the behavior is undefined.
 * Parameters: @req - Pointer to a Request to initialize
 *             @buffer - Pointer to a buffer containing an HTTP request, must be
 *                       null terminated
 *             @buffer_l - Length of the buffer
 *    Returns: None
 */
static int parse_request_fields(Request *req, char *buffer, size_t buffer_l)
{
    if (req == NULL || buffer == NULL) {
        return -1;
    }

    req->host = parse_host(buffer, buffer_l, &req->host_l);
    req->port = parse_port(&req->host, &req->host_l, req->path, &req->port_l);
    req->body = parse_body(buffer, buffer_l, &req->body_l);

    return 0;
}

/* parse_method
 *    Purpose: Parses the HTTP method from a HTTP header.
 * Parameters: @header - Pointer to a buffer containing the HTTP header. The
 *                       buffer must be null terminated.
 *             @method_l - Pointer to a size_t to store the length of the
 *                         method, this value can be NULL.
 *             @saveptr - Pointer to a char to store the next character after
 *                        the method, ignoring any whitespace. Can be NULL.
 *    Returns: Pointer to a buffer containing the HTTP method, or NULL if the
 *             header does not contain a valid HTTP method or an error occurs.
 * 
 * Note: The returned buffer is allocated on the heap and must be freed by the
 *       caller.
 */
static char *parse_method(char *header, size_t *method_l, char **saveptr)
{
    if (header == NULL) {
        return NULL;
    }

    char *method = header;
    char *end = strchr(header, ' ');
    if (end == NULL) {
        return NULL;
    }

    size_t method_len = end - method;
    if (method_l != NULL) {
        *method_l = method_len;
    }
    
    char *m = calloc(method_len + 1, sizeof(char));
    if (m == NULL) {
        return NULL;
    }
    memcpy(m, method, method_len);

    if (saveptr != NULL) {
        while(isspace(*end)) {
            end++;
        }
        *saveptr = end;
    }

    return m;
}


/* parse_path
 *    Purpose: Parses the HTTP path from a HTTP header.
 * Parameters: @header - Pointer to a buffer containing the HTTP header. The
 *                       buffer must be null terminated.
 *             @path_l - Pointer to a size_t to store the length of the path,
 *                       can be NULL
 *             @saveptr - Pointer to a char to store the next character after
 *                        the path, ignoring any whitespace. Can be NULL.
 *    Returns: Pointer to a buffer containing the HTTP path, or NULL if the
 *             header does not contain a valid HTTP path or an error occurs.
 * 
 * Note: The returned buffer is allocated on the heap and must be freed by the
 *       caller.
 */
static char *parse_path(char *header, size_t *path_l, char **saveptr)
{
    if (header == NULL) {
        return NULL;
    }

    char *path;
    if (saveptr != NULL && *saveptr != NULL) {
        path = *saveptr;
    } else {
        path = header;
        path = strchr(header, ' ') + 1; // skip method
        if (path == NULL) {
            return NULL;
        }
    }
    
    char *end = strchr(path, ' ');
    if (end == NULL) {
        return NULL;
    }

    /* calculate the length of the path */
    size_t path_len = end - path;
    if (path_l != NULL) {
        *path_l = path_len;
    }

    /* allocate memory for the path and copy path */
    char *p = calloc(path_len + 1, sizeof(char));
    if (p == NULL) {
        return NULL;
    }
    memcpy(p, path, path_len);

    /* set the saveptr */
    if (saveptr != NULL) {
        while(isspace(*end)) {
            end++;
        }
        *saveptr = end;
    }

    return p;
}

/* parse_version
 *    Purpose: Parses the HTTP version from a HTTP header.
 * Parameters: @header - Pointer to a buffer containing the HTTP header. The
 *                       buffer must be null terminated.
 *             @version_l - Pointer to a size_t to store the length of the
 *                          version, can be NULL
 *             @saveptr - Pointer to a char to store the next character after
 *                        the version, ignoring any whitespace. Can be NULL.
 *    Returns: Pointer to a buffer containing the HTTP version, or NULL if the
 *             header does not contain a valid HTTP version or an error occurs.
 * 
 * Note: The returned buffer is allocated on the heap and must be freed by the
 *       caller.
 */
static char *parse_version_req(char *header, size_t *version_l, char **saveptr)
{
    if (header == NULL) {
        return NULL;
    }

    char *version;
    if (saveptr != NULL && *saveptr != NULL) {
        version = *saveptr;
    } else {
        version = header;
        version = strchr(header, ' ') + 1;  // skip method
        if (version == NULL) {
            return NULL;
        }

        version = strchr(version, ' ') + 1; // skip path
        if (version == NULL) {
            return NULL;
        }
    }
    
    char *end = strchr(version, '\r');
    if (end == NULL) {
        return NULL;
    }

    /* calculate the length of the version */
    size_t version_len = end - version;
    if (version_l != NULL) {
        *version_l = version_len;
    }

    /* allocate memory for the version and copy version */
    char *v = calloc(version_len + 1, sizeof(char));
    if (v == NULL) {
        return NULL;
    }

    memcpy(v, version, version_len);

    /* set the saveptr */
    if (saveptr != NULL) {
        while(isspace(*end)) {
            end++;
        }
        *saveptr = end;
    }



    return v;
}


/* parse_version
 *    Purpose: Parses the HTTP version from a HTTP header.
 * Parameters: @header - Pointer to a buffer containing the HTTP header. The
 *                       buffer must be null terminated.
 *             @version_l - Pointer to a size_t to store the length of the
 *                          version, can be NULL
 *             @saveptr - Pointer to a char to store the next character after
 *                        the version, ignoring any whitespace. Can be NULL.
 *    Returns: Pointer to a buffer containing the HTTP version, or NULL if the
 *             header does not contain a valid HTTP version or an error occurs.
 * 
 * Note: The returned buffer is allocated on the heap and must be freed by the
 *       caller.
 */
static char *parse_version_res(char *header, size_t *version_l, char **saveptr)
{
    if (header == NULL) {
        return NULL;
    }

    char *version = NULL;
    if (saveptr != NULL && *saveptr != NULL) {
        version = *saveptr;
    } else {
        version = header;
    }
    
    char *end = strchr(version, ' ');
    if (end == NULL) {
        return NULL;
    }

    /* calculate the length of the version */
    size_t version_len = end - version;
    if (version_l != NULL) {
        *version_l = version_len;
    }

    /* allocate memory for the version and copy version */
    char *v = calloc(version_len + 1, sizeof(char));
    if (v == NULL) {
        return NULL;
    }

    memcpy(v, version, version_len);

    /* set the saveptr */
    if (saveptr != NULL && *saveptr != NULL) {
        while(isspace(*end)) {
            end++;
        }
        *saveptr = end;
    }



    return v;
}

/* parse_startline
 *    Purpose: Parses the HTTP start line from a HTTP request header. This
 *             function will parse the HTTP method, path, and version from the
 *             start line. Any whitespace will be ignored. Assumes the start
 *             line is the first line in the header. The request must be
 *             null terminated.
 * Parameters: @header - Pointer to a buffer containing the HTTP header. The
 */
static int parse_startline(Request *req, char *request)
{
    if (req == NULL || request == NULL) {
        return -1;
    }

    char *saveptr = NULL;
    char *method = parse_method(request, &req->method_l, &saveptr);
    if (method == NULL) {
        return -1;
    }
    req->method = method;

    char *path = parse_path(request, &req->path_l, &saveptr);
    if (path == NULL) {
        return -1;
    }
    req->path = path;

    char *version = parse_version_req(request, &req->version_l, &saveptr);
    if (version == NULL) {
        return -1;
    }
    req->version = version;

    return EXIT_SUCCESS;
}

/* parse_host
 *    Purpose: Parses a Host field from a HTTP header. The header must be null
 *             terminated. The Host field is the only field that is parsed. If
 *             the Host field is not found, the function will return NULL.
 * Parameters: @header - Pointer to a buffer containing the HTTP header. The
 *                       buffer must be null terminated.
 *            @host_l - Pointer to a size_t to store the length of the host,
 *                      can be NULL
 *   Returns: Pointer to a buffer containing the host, or NULL if the header
 *            does not contain a valid host or an error occurs.
 * 
 * Note: The returned buffer is allocated on the heap and must be freed by the
 *       caller.
 */
static char *parse_host(char *header, size_t header_l, size_t *host_l)
{
    if (header == NULL) {
        return NULL;
    }

    char *header_lc = get_buffer_lc(header, header + header_l);

    char *host = strstr(header_lc, HOST);
    if (host == NULL) {
        return NULL;
    }

    /* skip field name and any whitespace after the colon */
    host += 6; // Skip "host:"
    while (isspace(*host)) {
        host++;
    }

    /* find the end of the Host field */
    char *end = strchr(host, '\r');
    if (end == NULL) {
        return NULL;
    }

    size_t host_len = end - host;
    if (host_l != NULL) {
        *host_l = host_len;
    }

    char *h = calloc(host_len + 1, sizeof(char));
    if (h == NULL) {
        return NULL;
    }

    memcpy(h, host, host_len);

    free(header_lc);

    return h;
}

static char *parse_port(char **host, size_t *host_l, char *path, size_t *port_l)
{
    char *colon = NULL, *port = NULL, *end = NULL;
    size_t port_len = 0;
    size_t new_host_len = *host_l;
    if (*host != NULL) {
        colon = strchr(*host, ':');
        if (colon != NULL) {
            port = colon + 1;
            end = port;
            while (isdigit(*end)) {
                end++;
            }
            port_len = end - port;
            if (port_l != NULL) {
                *port_l = port_len;
            }

            new_host_len = colon - *host; // update host length
        }
    }
    
    if (path != NULL && port == NULL) {
        port = strrchr(path, ':');
        port++;
        
        /* check if the port is a number */
        if (isdigit(*port)) {
            end = port;
            while (isdigit(*end)) {
                end++;
            }

            port_len = end - port;
            if (port_l != NULL) {
                *port_l = port_len;
            }
        } else {
            port = NULL;
        }
    } 

    /* Default port */
    fprintf(stderr, "Using default port\n");
    fprintf(stderr, "port: %s\n", port);
    if (port == NULL) {
        port = HTTP_PORT;
        port_len = HTTP_PORT_L;
        if (port_l != NULL) {
            *port_l = port_len;
        }
    }

    char *p = calloc(port_len + 1, sizeof(char));
    if (p == NULL) {
        return NULL;
    }
    memcpy(p, port, port_len);

    if (new_host_len != *host_l) {
        char *h = calloc(new_host_len + 1, sizeof(char));
        if (h == NULL) {
            return NULL;
        }
        memcpy(h, *host, new_host_len);
        free(*host);
        *host = h;
        if (host_l != NULL) {
            *host_l = new_host_len;
        }
    }

    return p;
}

static char *parse_cachecontrol(char *header, size_t *cachecontrol_l)
{
    if (header == NULL) {
        return NULL;
    }

    char *header_lc = get_buffer_lc(header, header + strlen(header));
    char *cachecontrol = strstr(header_lc, CACHECONTROL);
    if (cachecontrol == NULL) {
        return NULL;
    }

    /* skip field name and any whitespace after the colon */
    cachecontrol += CACHECONTROL_L; // Skip "Cache-Control:"
    while (isspace(*cachecontrol)) {
        cachecontrol++;
    }

    /* find the end of the Cache-Control field */
    char *end = strchr(cachecontrol, '\r');
    if (end == NULL) {
        return NULL;
    }

    size_t cachecontrol_len = end - cachecontrol;
    if (cachecontrol_l != NULL) {
        *cachecontrol_l = cachecontrol_len;
    }

    char *c = calloc(cachecontrol_len + 1, sizeof(char));
    if (c == NULL) {
        return NULL;
    }

    memcpy(c, cachecontrol, cachecontrol_len);

    free(header_lc);

    return c;
}

static unsigned int parse_maxage(char *cachecontrol, size_t cachecontrol_l)
{
    if (cachecontrol == NULL) {
        return 0;
    }

    char *cachecontrol_lc = get_buffer_lc(cachecontrol, cachecontrol + cachecontrol_l);

    char *maxage = strstr(cachecontrol_lc, "max-age="); // case insensitive, GNU extension // TODO (check if ok)
    if (maxage == NULL) {
        return 0;
    }

    /* skip field name and any whitespace after the colon */
    maxage += 8; // Skip "max-age="
    while (isspace(*maxage)) {
        maxage++;
    }

    /* find the end of the Cache-Control field */
    char *end = strchr(maxage, ',');
    if (end == NULL) {
        end = maxage + cachecontrol_l;  // end of string
    }

    size_t maxage_len = end - maxage;

    char *m = calloc(maxage_len + 1, sizeof(char));
    if (m == NULL) {
        return 0;
    }

    memcpy(m, maxage, maxage_len);

    unsigned int maxage_int = atoi(m);
    free(m);

    free(cachecontrol_lc);

    return maxage_int;
}

static size_t parse_contentlength(char *header, size_t header_l)
{
    if (header == NULL) {
        return 0;
    }

    char *header_lc = get_buffer_lc(header, header + header_l);

    char *contentlength = strstr(header_lc, CONTENTLENGTH);
    if (contentlength == NULL) {
        return 0;
    }

    /* skip field name and any whitespace after the colon */
    contentlength += CONTENTLENGTH_L; // Skip "Content-Length:"
    while (isspace(*contentlength)) {
        contentlength++;
    }

    /* find the end of the Content-Length field */
    char *end = strchr(contentlength, '\r');
    if (end == NULL) {
        return 0;
    }

    size_t contentlength_len = end - contentlength;

    char *c = calloc(contentlength_len + 1, sizeof(char));
    if (c == NULL) {
        return 0;
    }

    memcpy(c, contentlength, contentlength_len);
    size_t contentlength_int = strtoul(c, NULL, 10);

    free(c);
    free(header_lc);

    return contentlength_int;
}

static char *parse_body(char *buffer, size_t buffer_l, size_t *body_l)
{
    if (buffer == NULL) {
        return NULL;
    }

    char *b;
    char *body = strstr(buffer, HEADER_END); // case insensitive, GNU extension // TODO (check if ok)
    if (body == NULL) {
        return NULL;
    }

    body += HEADER_END_L;   // Skip "\r\n\r\n"

    size_t body_len = buffer_l - (body - buffer);
    if (body_l != NULL) {
        *body_l = body_len;
    }

    if (body_len == 0) {    // No body
        return NULL;
    } else {                // Body
        b = calloc(body_len + 1, sizeof(char));
        if (b == NULL) {
            return NULL;
        }

        memcpy(b, body, body_len);
    }

    return b;
}
