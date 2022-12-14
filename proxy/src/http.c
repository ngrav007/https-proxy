#include "http.h"

static int parse_response(Response *res, char *buffer, size_t buffer_l);
static int parse_response_fields(Response *res, char *buffer, size_t buffer_l);
static int parse_statusline(Response *res, char *response);
static char *parse_status(char *response, size_t *status_l, char **saveptr);
static size_t parse_contentlength(char *header);
static long parse_maxage(char *cachecontrol, size_t cachecontrol_l);
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
static void set_field(char **f, size_t *f_l, char *v, size_t v_l);
/* HTTP Functions ----------------------------------------------------------- */

/* HTTP_add_field
 *    Purpose: Adds a new field to a buffer containing an HTTP header with the
 *             given name and value. If the field already exists, the value is
 *             updated. If the field does not exist, it is added. The buffer is
 *             reallocated if necessary. The buffer must be null terminated.
 * Parameters: @buffer - The buffer to add the field to, must be null terminated
 *             @buffer_l - A pointer to the length of the buffer
 *             @field - The name of the field to add
 *             @value - The value of the field to add
 *    Returns: 0 on success, -1 on failure
 */
int HTTP_add_field(char **buffer, size_t *buffer_l, char *field, char *value)
{
    if (buffer == NULL || *buffer == NULL || field == NULL || value == NULL || buffer_l == NULL) {
        print_error("[http-add-field] invalid parameter");
        return ERROR_FAILURE;
    }

    char *header_start, *header_end, *new_buffer;
    size_t header_l, field_l, value_l, new_buffer_l, offset = 0;
    header_start = *buffer;
    header_end   = strstr(header_start, HEADER_END);
    if (header_end == NULL) {
        print_error("[http-add-field] invalid header");
        return INVALID_HEADER;
    }

    header_l = header_end - header_start;
    field_l  = strlen(field);
    value_l  = strlen(value);

    /* check if field already exists */
    new_buffer_l = *buffer_l + field_l + value_l + FIELD_SEP_L + CRLF_L;
    new_buffer   = calloc(new_buffer_l + 1, sizeof(char));
    if (new_buffer == NULL) {
        print_error("[http-add-field] calloc failed");
        return ERROR_FAILURE;
    }

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
    *buffer   = new_buffer;
    *buffer_l = new_buffer_l;

    return EXIT_SUCCESS;
}

/* HTTP_got_header
 *    Purpose: Checks if a buffer contains a complete HTTP header.
 * Parameters: @buffer - buffer to check for header, must be null terminated
 *    Returns: true (1) if header is complete, false (0) if not
 */
bool HTTP_got_header(char *buffer)
{
    if (strstr(buffer, HEADER_END) != NULL) {
        return true;
    } else {
        return false;
    }
}

int HTTP_validate_request(Request *req)
{
    if (req == NULL) {
        return ERROR_FAILURE;
    }

    // ? Do All methods have different lengths? If we implement any more methods this wont work */
    /* Ensure GET requests are HTTP only */
    switch (req->method_l) {
        case GET_L:
            if (strncmp(req->method, GET, GET_L) == 0) {
                if (strncmp(req->path, HTTPS, HTTPS_L) == 0) {
                    return PROXY_AUTH_REQUIRED;
                }
            }
            break;
        case CONNECT_L:
            break;
        default:
            return ERROR_FAILURE;
    }

    return EXIT_SUCCESS;
}

/* Request Functions -------------------------------------------------------- */

/* Request_new
 *    Purpose: Creates a new Request initialized with null values.
 * Parameters: @buffer - A buffer containing the request
 *             @buffer_l - The length of the buffer
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
 *             @path - Pointer to a buffer containing the HTTP uri
 *             @version - Pointer to a buffer containing the HTTP version
 *             @host - Pointer to a buffer containing the HTTP host
 *             @port - Pointer to a buffer containing the HTTP port, can be NULL
 *                     and a default port will be used
 *             @body - Pointer to a buffer containing the HTTP body, can be NULL
 *   Returns: Pointer to a new Request, or NULL if memory allocation fails.
 *
 * Note: All parameters must be null terminated strings.
 */
Request *Request_create(char *method, char *path, char *version, char *host, char *port, char *body)
{
    Request *request = calloc(1, sizeof(struct Request));
    if (request == NULL) {
        return NULL;
    }
    request->method_l  = strlen(method);
    request->method    = strndup(method, request->method_l);
    request->path_l    = strlen(path);
    request->path      = strndup(path, request->path_l);
    request->version_l = strlen(version);
    request->version   = strndup(version, request->version_l);
    request->host_l    = strlen(host);
    request->host      = strndup(host, request->host_l);
    if (port != NULL) {
        request->port_l = strlen(port);
        request->port   = strndup(port, request->port_l);
    } else {
        request->port_l = DEFAULT_HTTP_PORT_L;
        request->port   = strndup(DEFAULT_HTTP_PORT, request->port_l);
    }
    if (body != NULL) {
        request->body_l = strlen(body);
        request->body   = strndup(body, request->body_l);
    } else {
        request->body_l = 0;
        request->body   = NULL;
    }

    return request;
}

/* Request_copy
 *    Purpose: Creates a new Request populated with the values of the given
 *             Request.
 * Parameters: @request - Pointer to a Request to copy
 *    Returns: Pointer to a new Request, or NULL if memory allocation fails.
 */
Request *Request_copy(Request *request)
{
    Request *r = calloc(1, sizeof(struct Request));
    if (r == NULL) {
        return NULL;
    }

    set_field(&r->method, &r->method_l, request->method, request->method_l);
    set_field(&r->path, &r->path_l, request->path, request->path_l);
    set_field(&r->version, &r->version_l, request->version, request->version_l);
    set_field(&r->host, &r->host_l, request->host, request->host_l);
    set_field(&r->port, &r->port_l, request->port, request->port_l);
    set_field(&r->body, &r->body_l, request->body, request->body_l);
    set_field(&r->raw, &r->raw_l, request->raw, request->raw_l);

    return r;
}

/* Request_free
 *    Purpose: Frees the memory allocated to a Request.
 * Parameters: @req - Pointer to the Request to free
 *    Returns: None
 */
void Request_free(void *req)
{
    if (req == NULL) {
        return;
    }

    Request *r = (Request *)req;

    free(r->method);
    free(r->path);
    free(r->version);
    free(r->host);
    free(r->port);
    free(r->body);
    free(r->raw);
    free(r);
}

/* Request_print
 *    Purpose: Prints the contents of a Request to stderr
 * Parameters: @req - Pointer to the Request to print
 *    Returns: None
 */
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

/* Request_compare
 *    Purpose: Compares two Requests to see if they are equal.
 * Parameters: @req1 - Pointer to the first Request to compare
 *             @req2 - Pointer to the second Request to compare
 *   Returns: true (1) if the Requests are equal, false (0) if not
 */
int Request_compare(void *req1, void *req2)
{
    if (req1 == NULL || req2 == NULL) {
        return ERROR_FAILURE;
    }

    Request *r1 = (Request *)req1;
    Request *r2 = (Request *)req2;

    if (memcmp(r1->method, r2->method, r1->method_l) != 0) {
        return FALSE;
    }

    if (memcmp(r1->path, r2->path, r1->path_l) != 0) {
        return FALSE;
    }

    if (memcmp(r1->version, r2->version, r1->version_l) != 0) {
        return FALSE;
    }

    if (memcmp(r1->host, r2->host, r1->host_l) != 0) {
        return FALSE;
    }

    if (memcmp(r1->port, r2->port, r1->port_l) != 0) {
        return FALSE;
    }

    if (memcmp(r1->body, r2->body, r1->body_l) != 0) {
        return FALSE;
    }

    return TRUE;
}

/* Response Functions ------------------------------------------------------- */

/* Response_new
 *    Purpose: Creates a new Response initialized with values parsed from a
 *             buffer containing a valid HTTP response.
 * Parameters: @msg - A buffer containing the response
 *             @msg_l - The length of the buffer
 *   Returns: Pointer to a new Response, or NULL if memory allocation fails.
 */
Response *Response_new(char *method, size_t method_l, char *uri, size_t uri_l, char *msg, size_t msg_l)
{
    Response *response = calloc(1, sizeof(struct Response));
    if (response == NULL) {
        return NULL;
    }

    response->uri_l = uri_l;
    response->uri   = calloc(uri_l + 1, sizeof(char));
    if (response->uri == NULL) {
        Response_free(response);
        return NULL;
    }
    memcpy(response->uri, uri, uri_l);

    response->raw_l = msg_l;
    response->raw   = calloc(msg_l + 1, sizeof(char));
    if (response->raw == NULL) {
        Response_free(response);
        return NULL;
    }
    memcpy(response->raw, msg, msg_l);
    if (memcmp(method, CONNECT, method_l) != 0) {
        int ret = parse_response(response, msg, msg_l);
        if (ret != 0) {
            Response_free(response);
            return NULL;
        }
    }

    return response;
}

/* Response_free
 *    Purpose: Frees the memory allocated to a Response.
 * Parameters: @response - Pointer to the Response to free
 *    Returns: None
 */
void Response_free(void *response)
{
    if (response == NULL) {
        return;
    }

    Response *r = (Response *)response;
    free(r->uri);
    free(r->version);
    free(r->status);
    free(r->cache_ctrl);
    free(r->body);
    free(r->raw);
    free(r);
}

/* Response_copy
 *    Purpose: Creates a copy of a Response.
 * Parameters: @response - Pointer to the Response to copy
 *    Returns: Pointer to a new Response, or NULL if memory allocation fails.
 */
Response *Response_copy(Response *response)
{
    if (response == NULL) {
        return NULL;
    }

    Response *r = calloc(1, sizeof(struct Response));
    if (r == NULL) {
        return NULL;
    }

    set_field(&r->uri, &r->uri_l, response->uri, response->uri_l);
    set_field(&r->version, &r->version_l, response->version, response->version_l);
    set_field(&r->status, &r->status_l, response->status, response->status_l);
    set_field(&r->cache_ctrl, &r->cache_ctrl_l, response->cache_ctrl, response->cache_ctrl_l);
    set_field(&r->body, &r->body_l, response->body, response->body_l);
    set_field(&r->raw, &r->raw_l, response->raw, response->raw_l);

    r->max_age        = response->max_age;
    r->content_length = response->content_length;

    return r;
}

/* Response_size
 *    Purpose: Returns the size of the given Response in bytes.
 * Parameters: @response - Pointer to the Response to get the size of
 *    Returns: The size of the Response in bytes
 */
unsigned long Response_size(Response *response)
{
    if (response == NULL) {
        return 0;
    }

    return response->raw_l;
}

/* Response_get
 *    Purpose: Returns a pointer to the raw data of the given Response.
 * Parameters: @response - Pointer to the Response to get the data of
 *   Returns: Pointer to the raw data of the Response
 */
char *Response_get(Response *response)
{
    if (response == NULL) {
        return NULL;
    }

    return response->raw;
}

/* Response_print
 *    Purpose: Prints the contents of a Response to stderr
 * Parameters: @response - Pointer to the Response to print
 *    Returns: None
 */
void Response_print(void *response)
{
    if (response == NULL) {
        return;
    }

    Response *r = (Response *)response;

    fprintf(stderr, "[Response]\n");
    fprintf(stderr, "  URI (%ld): %s\n", r->uri_l, r->uri);
    fprintf(stderr, "  Version (%ld): %s\n", r->version_l, r->version);
    fprintf(stderr, "  Status (%ld): %s\n", r->status_l, r->status);
    fprintf(stderr, "  CCtrl (%ld): %s\n", r->cache_ctrl_l, r->cache_ctrl);
    fprintf(stderr, "  Body (%ld): %s\n", r->body_l, r->body);
    fprintf(stderr, "  Raw Length: %ld\n", r->raw_l);
}

/* Response_compare
 *    Purpose: Compares two Responses to see if they are equal.
 * Parameters: @response1 - Pointer to the first Response to compare
 *             @response2 - Pointer to the second Response to compare
 *    Returns: true (1) if the Responses are equal, false (0) if not
 */
int Response_compare(void *response1, void *response2)
{
    if (response1 == NULL || response2 == NULL) {
        return ERROR_FAILURE;
    }

    Response *r1 = (Response *)response1;
    Response *r2 = (Response *)response2;

    return (memcmp(r1->uri, r2->raw, r1->uri_l) == 0);
}

/* Raw Functions ------------------------------------------------------------ */

/* Raw_request
 *    Purpose: Returns a raw HTTP request as a null terminated string. The
 *             request is formatted as follows:
 *               <method> <path> HTTP/1.1\r\n
 *               <host>:<port>\r\n | <host>\r\n
 *               \r\n
 *               <body>: <body>\r\n
 * Parameters: @method - Pointer to a buffer containing the HTTP method
 *             @uri - Pointer to a buffer containing the path
 *             @host - Pointer to a buffer containing the host
 *             @port - Port number
 *             @body - Pointer to a buffer containing the body, can be NULL
 *             @raw_l - Pointer to a size_t to store the length of the raw
 *                      request, can be NULL if the length is not needed
 *  Returns: Pointer to a buffer containing the raw HTTP request, or NULL if
 *          an error occurs.
 *
 * NOTE: The caller is responsible for freeing the memory allocated for the
 *       raw request string.
 */
char *Raw_request(char *method, char *url, char *host, char *port, char *body, size_t *raw_l)
{
    if (method == NULL || url == NULL) {
        print_error("http: raw-request: method and url are required");
        return NULL;
    }

    size_t method_l = 0, url_l = 0, host_l = 0, port_l = 0, body_l = 0;

    method_l = strlen(method); // Method (required)

    url_l = strlen(url); // url (required)

    if (host != NULL) {
        host_l = strlen(host); // Host (optional)
    }

    if (port != NULL) {
        port_l = strlen(port); // Port (optional)
    }

    if (body != NULL) {
        body_l = strlen(body); // Body (optional)
    }

    /* Calculate the raw string length */

    size_t raw_len = 0;
    /* Startline of HTTP Request */
    raw_len += method_l;           // Method
    raw_len += SPACE_L;            // Space
    raw_len += url_l;              // url
    raw_len += SPACE_L;            // Space
    raw_len += HTTP_VERSION_1_1_L; // HTTP version
    raw_len += CRLF_L;             // CRLF

    /* Host Field : Host: <host>\r\n */
    if (host_l > 0) {
        raw_len += HOST_L;  // Host
        raw_len += SPACE_L; // Space
        raw_len += host_l;  // Host
        raw_len += CRLF_L;  // CRLF
    }

    if (port_l > 0) {
        raw_len += port_l;  // Port
        raw_len += COLON_L; // Colon
    }

    raw_len += CRLF_L; // CRLF

    if (body_l > 0) {
        raw_len += body_l; // Body
    }

    /* allocate heap memory to store the raw string - null terminated */
    char *raw_request = calloc(raw_len + 1, sizeof(char));
    if (raw_request == NULL) {
        return NULL;
    }

    /* copy the method, url, and http version to the raw string */
    size_t offset = 0;
    memcpy(raw_request, method, method_l);
    offset += method_l;
    memcpy(raw_request + offset, SPACE, SPACE_L);
    offset += SPACE_L;
    memcpy(raw_request + offset, url, url_l);
    offset += url_l;
    memcpy(raw_request + offset, SPACE, SPACE_L);
    offset += SPACE_L;
    memcpy(raw_request + offset, HTTP_VERSION_1_1, HTTP_VERSION_1_1_L);
    offset += HTTP_VERSION_1_1_L;
    memcpy(raw_request + offset, CRLF, CRLF_L);
    offset += CRLF_L;

    /* add host field if supplied */
    if (host_l > 0) {
        memcpy(raw_request + offset, HTTP_HOST, HOST_L);
        offset += HOST_L;
        memcpy(raw_request + offset, SPACE, SPACE_L);
        offset += SPACE_L;
        memcpy(raw_request + offset, host, host_l);
        offset += host_l;

        /* add port if supplied */
        if (port_l > 0) {
            memcpy(raw_request + offset, COLON, COLON_L);
            offset += COLON_L;
            memcpy(raw_request + offset, port, port_l);
            offset += port_l;
        }
        memcpy(raw_request + offset, CRLF, CRLF_L);
        offset += CRLF_L;
    }

    /* add blank line */
    memcpy(raw_request + offset, CRLF, CRLF_L);
    offset += CRLF_L;

    /* add body if supplied */
    if (body_l > 0) {
        memcpy(raw_request + offset, body, body_l);
        offset += body_l;
    }

    *raw_l = raw_len;

#if DEBUG
    print_debug("http: [raw-request] Raw request: ")
    print_ascii(raw_request, raw_len);
#endif

    return raw_request;
}

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
#if DEBUG
    print_debug("http: [parse_request] Parsing request:");
    fprintf(stderr, "\n%s\n", buffer);
#endif

    char *buffer_lc = get_buffer_lc(buffer, buffer + buffer_l);
    if (parse_startline(req, buffer_lc) != 0) {
        return -1;
    }

    if (memcmp(req->method, PROXY_HALT, req->method_l) == 0) {
        free(buffer_lc);
        return HALT;
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

/* parse_response
 *    Purpose: Initializes a Response with a buffer containing an HTTP response
 * Parameters: @res - Pointer to a Response to initialize
 *             @buffer - Pointer to a buffer containing an HTTP response
 *             @buffer_l - The length of the buffer
 *   Returns: 0 on success, -1 on failure
 */
static int parse_response(Response *res, char *buffer, size_t buffer_l)
{
    if (res == NULL || buffer == NULL) {
        return -1;
    }

    char *buffer_lc = get_buffer_lc(buffer, buffer + buffer_l);
    if (parse_statusline(res, buffer) != 0) {
        return -1;
    }

    if (parse_response_fields(res, buffer_lc, buffer_l) != 0) {
        return -1;
    }
    free(buffer_lc);

    return 0;
}

/* parse_response_fields
 *    Purpose: Parses the fields of an HTTP response and initializes the given
 *             Response with the parsed data. Currently only parses the
 *             following fields:
 *              - Cache-Control
 *                - max-age
 *              - Content-Length
 *              - Body
 * Parameters: @res - Pointer to a Response to initialize
 *             @buffer - Pointer to a buffer containing an HTTP response
 *             @buffer_l - The length of the buffer
 *    Returns: 0 on success, -1 on failure
 */
static int parse_response_fields(Response *res, char *buffer, size_t buffer_l)
{
    if (res == NULL || buffer == NULL) {
        return -1;
    }

    res->cache_ctrl = parse_cachecontrol(buffer, &res->cache_ctrl_l);
    if (res->cache_ctrl != NULL) {
        res->max_age = parse_maxage(res->cache_ctrl, res->cache_ctrl_l);
    } else {
        res->max_age = DEFAULT_MAX_AGE;
    }
    res->content_length = parse_contentlength(buffer);
    res->body           = parse_body(buffer, buffer_l, &res->body_l);
    if (res->content_length == 0 && res->body_l > 0) { // TODO - ok? set contentl to bodyl if contentl is 0 (i.e. no field)
        res->content_length = res->body_l;
    }

    return 0;
}

/* parse_statusline
 *    Purpose: Parses the status line of an HTTP response and initializes the
 *             given Response with the parsed data.
 * Parameters: @res - Pointer to a Response to initialize
 *             @response - Pointer to a buffer containing an HTTP response
 *   Returns: 0 on success, -1 on failure
 */
static int parse_statusline(Response *res, char *response)
{
    if (res == NULL || response == NULL) {
        return -1;
    }
    char *sp     = NULL;
    res->version = parse_version_res(response, &res->version_l, &sp);
    res->status  = parse_status(response, &res->status_l, &sp);

    return 0;
}

/* parse_status
 *    Purpose: Parses the status code of an HTTP response.
 * Parameters: @response - Pointer to a buffer containing an HTTP response
 *             @status_l - Pointer to a size_t to store the length of the status
 *                         code
 *            @saveptr - Pointer to a char pointer to store the pointer to the
 *                       the next character after the status code, ignoring any
 *                       whitespace. If NULL, the pointer is not stored.
 *  Returns: Pointer to the status code, NULL on failure
 */
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
        status = strchr(status, ' ') + 1; // skip the version + space
        if (status == NULL) {
            return NULL;
        }
    }

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

    /* update the saved pointer, if given */
    if (saveptr != NULL) {
        while (isspace(*end)) {
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
    if (req->host == NULL) {
        char *path = req->path;
        /* populate host with path url */
        char *host_start = strchr(path, '/'); // skip the "//"
        if (host_start == NULL) {
            host_start = path;
        }
        char *host_end = strchr(host_start, '/');
        if (host_end == NULL) {
            host_end = path + strlen(path);
        }
        size_t host_len = host_end - host_start;
        set_field(&req->host, &req->host_l, host_start, host_len);
        if (req->host == NULL) {
            return INVALID_REQUEST;
        }
    }
    req->port = parse_port(&req->host, &req->host_l, req->path, &req->port_l);
    req->body = parse_body(buffer, buffer_l, &req->body_l);

    return EXIT_SUCCESS;
}

/* parse_method
 *    Purpose: Parses the HTTP method from a HTTP request.
 * Parameters: @request - Pointer to a buffer containing the HTTP request. The
 *                       buffer must be null terminated.
 *             @method_l - Pointer to a size_t to store the length of the
 *                         method, this value can be NULL.
 *             @saveptr - Pointer to a char to store the next character after
 *                        the method, ignoring any whitespace. Can be NULL.
 *    Returns: Pointer to a buffer containing the HTTP method, or NULL if the
 *             request does not contain a valid HTTP method or an error occurs.
 *
 * Note: The returned buffer is allocated on the heap and must be freed by the
 *       caller.
 */
static char *parse_method(char *request, size_t *method_l, char **saveptr)
{
    if (request == NULL) {
        return NULL;
    }

    char *method = request;
    char *end    = strchr(request, ' ');
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
        while (isspace(*end)) {
            end++;
        }
        *saveptr = end;
    }

    return m;
}

/* parse_path
 *    Purpose: Parses the HTTP path from a HTTP request.
 * Parameters: @request - Pointer to a buffer containing the HTTP request. The
 *                       buffer must be null terminated.
 *             @path_l - Pointer to a size_t to store the length of the path,
 *                       can be NULL
 *             @saveptr - Pointer to a char to store the next character after
 *                        the path, ignoring any whitespace. Can be NULL.
 *    Returns: Pointer to a buffer containing the HTTP path, or NULL if the
 *             request does not contain a valid HTTP path or an error occurs.
 *
 * Note: The returned buffer is allocated on the heap and must be freed by the
 *       caller.
 */
static char *parse_path(char *request, size_t *path_l, char **saveptr)
{
    if (request == NULL) {
        return NULL;
    }

    char *path;
    if (saveptr != NULL && *saveptr != NULL) {
        path = *saveptr;
    } else {
        path = request;
        path = strchr(request, ' ') + 1; // skip method
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
        while (isspace(*end)) {
            end++;
        }
        *saveptr = end;
    }

    return p;
}

/* parse_version_req
 *    Purpose: Parses the HTTP version from a HTTP request.
 * Parameters: @request - Pointer to a buffer containing the HTTP request. The
 *                       buffer must be null terminated.
 *             @version_l - Pointer to a size_t to store the length of the
 *                          version, can be NULL
 *             @saveptr - Pointer to a char to store the next character after
 *                        the version, ignoring any whitespace. Can be NULL.
 *    Returns: Pointer to a buffer containing the HTTP version, or NULL if the
 *             request does not contain a valid HTTP version or an error occurs.
 *
 * Note: The returned buffer is allocated on the heap and must be freed by the
 *       caller.
 */
static char *parse_version_req(char *request, size_t *version_l, char **saveptr)
{
    if (request == NULL) {
        return NULL;
    }

    char *version;
    if (saveptr != NULL && *saveptr != NULL) {
        version = *saveptr;
    } else {
        version = request;
        version = strchr(request, ' ') + 1; // skip method
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
        while (isspace(*end)) {
            end++;
        }
        *saveptr = end;
    }

    return v;
}

/* parse_version_res
 *    Purpose: Parses the HTTP version from a HTTP response.
 * Parameters: @response - Pointer to a buffer containing the HTTP response. The
 *                       buffer must be null terminated.
 *             @version_l - Pointer to a size_t to store the length of the
 *                          version, can be NULL
 *             @saveptr - Pointer to a char to store the next character after
 *                        the version, ignoring any whitespace. Can be NULL.
 *    Returns: Pointer to a buffer containing the HTTP version, or NULL if the
 *             response does not contain a valid HTTP version or an error occurs
 *
 * Note: The returned buffer is allocated on the heap and must be freed by the
 *       caller.
 */
static char *parse_version_res(char *res, size_t *version_l, char **saveptr)
{
    if (res == NULL) {
        return NULL;
    }

    char *version = NULL;
    if (saveptr != NULL && *saveptr != NULL) {
        version = *saveptr;
    } else {
        version = res;
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
        while (isspace(*end)) {
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
        return EXIT_FAILURE;
    }

    char *sp     = NULL;
    char *method = parse_method(request, &req->method_l, &sp);
    if (method == NULL) {
        return -1; // TODO: error handling - invalid http method
    }
    req->method = method;

    char *path = parse_path(request, &req->path_l, &sp);
    if (path == NULL) {
        return -1; // TODO: error handling - invalid http path
    }
    req->path = path;

    char *version = parse_version_req(request, &req->version_l, &sp);
    if (version == NULL) {
        return -1;
    }
    req->version = version;

    return EXIT_SUCCESS;
}

/* parse_host
 *    Purpose: Parses a Host field from a HTTP request. The Host field is the
 *             only field that is parsed. If the Host field is not found, the
 *             function will return NULL.
 * Parameters: @request - Pointer to a buffer containing the HTTP request header
 *             @host_l - Pointer to a size_t to store the length of the host,
 *                       can be NULL
 *   Returns: Pointer to a buffer containing the host, or NULL if the header
 *            does not contain a valid host or an error occurs.
 *
 * Note: The returned buffer is allocated on the heap and must be freed by the
 *       caller.
 */
static char *parse_host(char *request, size_t request_l, size_t *host_l)
{
    if (request == NULL) {
        return NULL;
    }

    char *request_lc = get_buffer_lc(request, request + request_l);

    char *host = strstr(request_lc, HOST);
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

    free(request_lc);

    return h;
}

/* parse_port
 *    Purpose: Parses a port from a host. If the host does not contain a port,
 *             it checks the path for the port. If the port is not found, the
 *             function will set the port to the default port (set in config.h)
 * Parameters: @host - Pointer to a buffer containing the host. The buffer must
 *                     be null terminated.
 *             @host_l - Length of the host, will be updated if the port is
 *                       found in the host
 *             @path - Pointer to a buffer containing the path. The buffer must
 *                     be null terminated.
 *             @port_l - Length of the path, will be updated if the port is
 * found in the path Returns: Pointer to a buffer containing the port, or NULL
 * if an error occurs
 */
static char *parse_port(char **host, size_t *host_l, char *path, size_t *port_l)
{
    char *colon = NULL, *port = NULL, *end = NULL;
    size_t port_len     = 0;
    size_t new_host_len = *host_l;
    if (*host != NULL) {
        colon = strchr(*host, ':');
        if (colon != NULL) {
            port = colon + 1;
            end  = port;
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
        if (port != NULL) {
            port++;
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
    }

    /* Default port */
    if (port == NULL) {
        port     = DEFAULT_HTTP_PORT;
        port_len = DEFAULT_HTTP_PORT_L;
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

/* parse_cachecontrol
 *    Purpose: Parses a Cache-Control field from a HTTP response header. The
 *             Cache-Control field is the only field that is parsed. If the
 *             Cache-Control field is not found, the function will return NULL.
 * Parameters: @res - Pointer to a buffer containing the HTTP response header.
 *                    The buffer must be null terminated.
 *             @cc_l - Pointer to a size_t to store the length of the
 *                     Cache-Control field, can be NULL
 *    Returns: Pointer to a buffer containing the Cache-Control field, or NULL
 *             if the header does not contain a valid Cache-Control field or an
 *             error occurs.
 */
static char *parse_cachecontrol(char *res, size_t *cc_l)
{
    if (res == NULL) {
        return NULL;
    }

    char *cachecontrol = strstr(res, CACHECONTROL);
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

    size_t cc_len = end - cachecontrol;
    if (cc_l != NULL) {
        *cc_l = cc_len;
    }

    char *c = calloc(cc_len + 1, sizeof(char));
    if (c == NULL) {
        return NULL;
    }

    memcpy(c, cachecontrol, cc_len);

    return c;
}

/* parse_maxage
 *    Purpose: Parses a max-age value from a Cache-Control field. The Cache-
 *             Control field must be null terminated. The max-age value is the
 *             only value that is parsed. If the max-age value is not found, the
 *             default max-age value (set in config.h) will be returned.
 * Parameters: @cachecontrol - Pointer to a buffer containing the Cache-Control
 *                             field.
 *             @cc_l - Length of the Cache-Control field
 *   Returns: The max-age value, or the default max-age value if the Cache-
 *            Control field does not contain a valid max-age value.
 */
static long parse_maxage(char *cachecontrol, size_t cc_l)
{
    if (cachecontrol == NULL) {
        return DEFAULT_MAX_AGE;
    }

    char *maxage                = strstr(cachecontrol, "max-age=");
    if (maxage == NULL) {
        return DEFAULT_MAX_AGE;
    }

    char m[MAX_DIGITS_LONG + 1] = {0};

    /* skip field name and any whitespace after the colon */
    while (!isdigit(*maxage)) {
        maxage++;
        if ((maxage - cachecontrol) >= (ssize_t)cc_l) {
            return DEFAULT_MAX_AGE;
        }
    }

    /* find the end of the Cache-Control field */
    char *end = maxage;
    while (isdigit(*end)) {
        end++;
        if ((end - cachecontrol) >= (ssize_t)cc_l) {
            break;
        }
    }

    size_t maxage_len = end - maxage;
    if (maxage_len > MAX_DIGITS_LONG) {
        return DEFAULT_MAX_AGE; // TODO handle this error
    }
    memcpy(m, maxage, maxage_len);

    unsigned int maxage_int = atoi(m);

    return maxage_int;
}

/* parse_contentlength
 *    Purpose: Parses a Content-Length field from a HTTP response header. The
 *             Content-Length field is the only field that is parsed. If the
 *             Content-Length field is not found, the function will return 0.
 * Parameters: @header - Pointer to a buffer containing the HTTP response
 *                       header. The buffer must be null terminated.
 *  Returns: The Content-Length value, or 0 if the header does not contain a
 *           valid Content-Length field.
 */
static size_t parse_contentlength(char *header)
{
    if (header == NULL) {
        return 0;
    }

    char c[MAX_DIGITS_LONG + 1];
    zero(c, MAX_DIGITS_LONG + 1);
    char *contentlength = strstr(header, CONTENTLENGTH);
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

    memcpy(c, contentlength, contentlength_len);
    size_t contentlength_int = strtoul(c, NULL, 10);

    return contentlength_int;
}

/* parse_body
 *    Purpose: Parses the body of a HTTP response. The body returned as a null
 *             terminated string.
 * Parameters: @message - Pointer to a buffer containing the HTTP response
 *             @message_l - Length of the HTTP response
 *             @body_l - Pointer to a size_t to store the length of the body,
 *                       can be NULL if the length is not needed
 *   Returns: Pointer to a buffer containing the body of the HTTP response, or
 *            NULL if an error occurs.
 */
static char *parse_body(char *message, size_t message_l, size_t *body_l)
{
    if (message == NULL) {
        return NULL;
    }

    char *b;
    char *body = strstr(message, HEADER_END);
    if (body == NULL) {
        return NULL;
    }

    body += HEADER_END_L; // Skip "\r\n\r\n"

    size_t body_len = message_l - (body - message);
    if (body_l != NULL) {
        *body_l = body_len;
    }

    if (body_len == 0) { // No body
        return NULL;
    } else { // Body
        b = calloc(body_len + 1, sizeof(char));
        if (b == NULL) {
            return NULL;
        }

        memcpy(b, body, body_len);
    }

    return b;
}

/* set_field
 *    Purpose: Sets the value of a field in a HTTP request or response header.
 *             If the field already exists, the value is updated. If the field
 *             does not exist, it is copied to the header.
 * Parameters: @f - Pointer to a buffer to store the field
 *             @f - Pointer to a size_t to store the length of the field
 *             @v - Pointer to a buffer containing the value
 *             @v - Length of the value
 * Returns: None
 *
 * Note: The caller is responsible for freeing the memory allocated for the
 *       field. If the length of the field is 0, the field is not allocated and
 *       set to NULL.
 */
static void set_field(char **f, size_t *f_l, char *v, size_t v_l)
{
    if (v == NULL) {
        return;
    }

    if (*f != NULL) {
        free(*f);
    }

    *f_l = v_l;

    *f = (v_l > 0) ? calloc(v_l + 1, sizeof(char)) : NULL;
    if (*f == NULL) {
        return;
    }
    memcpy(*f, v, v_l);
}

/* takes a (response) buffer of size buffer_l, and edits it to include 
   a style.color attribute for links (html anchor tags). Should instert style attribute BEFORE href attribute. 
   
   Update: Now takes a string array; Any links found in the buffer that is 
   also in the string array will be green. o.w. it is red.
   */
int color_links(char **buffer, size_t *buffer_l, 
                char **cache_keys, int num_keys)
{
    // fprintf(stderr, "Num keys: %d\n", num_keys);
    // fprintf(stderr, "[HTTP] KeyArray:\n");
    // int i = 0;
    // for (; i < num_keys; i++) {
    //     fprintf(stderr, "%s\n", cache_keys[i]);
    // }
    // fprintf(stderr, "Initializing new buffer of size: %ld\n", *buffer_l);
    char *new_buffer = calloc(*buffer_l + COLOR_L + 1, sizeof(char));
    if (new_buffer == NULL) {
        return ERROR_FAILURE;
    }

    int new_buffer_sz = 0;
    int new_buffer_cap = *buffer_l + COLOR_L;
    char *new_buffer_pointer = new_buffer;
    char *buffer_end = (*buffer) + *buffer_l; // last byte

    // loop, copying individual bytes until you reach end of original buffer being copied over. 
    char *orig_buffer_pointer = *buffer;

    while (orig_buffer_pointer < buffer_end) {
        // ensure new_buffer capacity has enough for remaining bytes in original buffer. 
        int original_bytes_left = buffer_end - orig_buffer_pointer;
        if ((original_bytes_left + COLOR_L) > (new_buffer_cap - new_buffer_sz)) {
            new_buffer = realloc(new_buffer, new_buffer_cap + COLOR_L + 1);
            if (new_buffer == NULL) {
                return ERROR_FAILURE;
            }
            // new_buffer_cap += original_bytes_left;
            new_buffer_cap += COLOR_L;
            new_buffer_pointer = new_buffer + new_buffer_sz;
        }

        //  strstr for the next "<a " tag
        char *anchor = strstr(orig_buffer_pointer, ANCHOR_HTTPS_OPEN);
        if (anchor != NULL) {   // found an anchor tag; COLOR
            // copy the bytes up to the space in the anchor tag 
            char *stop = anchor + ANCHOR_OPEN_L;
            for (; orig_buffer_pointer != stop; 
                   (new_buffer_pointer)++, (orig_buffer_pointer)++) 
            {
                (*new_buffer_pointer) = (*orig_buffer_pointer);
                new_buffer_sz++;
            }

            // parse out the link, which is always in quotes "   " 
            char *start_of_link = strstr(orig_buffer_pointer, "\"");
            start_of_link += 1;
            char *end_of_link = strstr(start_of_link, "\"");
            int link_l = end_of_link - start_of_link;
            // char *link = malloc(link_l + 1);
            // memcpy(link, start_of_link, link_l);

            // check if link is a perfect prefix of any key in the array, pick color
            char *color_attribute = NULL;
            // if (foundKey(link2, cache_keys, num_keys)) {
            if (foundKey(start_of_link, link_l, cache_keys, num_keys)) {
                #if DEBUG
                print_debug("Found key: GREEN\n");
                #endif
                color_attribute = GREEN_STYLE;
            } else {
                #if DEBUG 
                print_debug("Found key: RED\n");
                #endif
                color_attribute = RED_STYLE;
            }

            // insert color attribute: 'style="color:#FF0000;" '
            char *color_pointer = color_attribute;
            char *color_end = color_attribute + COLOR_L;

            for (; color_pointer != color_end; 
                   (new_buffer_pointer)++, (color_pointer)++) 
            {
                (*new_buffer_pointer) = (*color_pointer);
                new_buffer_sz++;
            }

        } else { // no more anchor tags, copy bytes until the end of the buffer.
            for (; orig_buffer_pointer != buffer_end; 
                   (new_buffer_pointer)++, (orig_buffer_pointer)++) 
            {
                (*new_buffer_pointer) = (*orig_buffer_pointer);
                new_buffer_sz++;
            }

        }
    }

    free(*buffer);

    new_buffer = realloc(new_buffer, new_buffer_sz + 1);
    if (new_buffer == NULL) {
        // fprintf(stderr, "[color_links] realloc failed.\n");
        return ERROR_FAILURE;
    }
    new_buffer[new_buffer_sz] = '\0';

    *buffer_l = new_buffer_sz;
    *buffer = new_buffer;

    // write repsonse to out file 
    // int out_file = open("ResponseHTTP.html", O_WRONLY | O_CREAT, 0777); 
    // write(out_file, new_buffer, new_buffer_sz);
    // fprintf(stderr, "[HTTP]: Wrote to file\n");
    // close(out_file);

    return 0;


}

/**
    if target is a perfect url-prefix of a string (key) in the array, function returns 1 for true. 0 for false, o.w.
*/
// int foundKey(char *target, char *arr[], int arr_size)
int foundKey(char *target, int target_l, char **arr, int arr_size)
{
    // fprintf(stderr, "Arr size: %d\n", arr_size);
    int i = 0;
    for (i = 0; i < arr_size; i++) {
        // fprintf(stderr, "i: %d\n", i);
        if (perfectKeyPrefix(target, target_l, arr[i])) {
            // fprintf(stderr, "Perfect Key Prefix Match\n");
            return 1;
        }
    }
    return 0;
}

/**
 if pre is a prefix of str, then returns 1 for true. 0 for false, o.w.
*/
int perfectKeyPrefix(char *pre, int pre_l, char *str) 
{
    // if s is longer than t, return 0
    // int pre_sz = strlen(pre);
    // fprintf(stderr, "Response Target Link: ");
    // print_ascii(pre, pre_l);
    // fprintf(stderr, "\nKey: %s\n", str);

    int pre_sz = pre_l;
    int str_sz = strlen(str);

    if (pre_sz > str_sz) { 
        // fprintf(stderr, "Target string is longer than Key-candidate\n");
        return 0; 
    }

    // chekc if they match from the first http:// or https://
    if (strncmp(pre, str, 4) != 0) { 
        // fprintf(stderr, "Target and Candidate have different HTTP protocols\n");
        return 0; 
    }

    // start comparting on the first // 
    char *pre_ptr = strstr(pre, "//");
    char *str_ptr = strstr(str, "//");
    char *pre_end = pre + pre_sz;
    char *str_end = str + str_sz;

    // loop until you find a mismatch or until pre completes
    for (; pre_ptr != pre_end; (pre_ptr)++, (str_ptr)++) 
    {
        if (*pre_ptr != *str_ptr) { // Mismatch
            // fprintf(stderr, "Mismatch: %c vs. %c\n", *pre_ptr, *str_ptr);
            return 0;
        }
    }

    // check remainter of str; if it has more in it's path, return 0
    // but if its a port number thats part of a key, or nothing, return 1.
    if (str_ptr == str_end) { // both are "\0"
        // fprintf(stderr, "Match: Perfect\n");
        return 1;
    } else if (*str_ptr == ':' && isdigit(*(str_ptr + 1))) {
        // end of str is ":<portno>", just check for partial port
        // fprintf(stderr, "Match: Candidate only has a port\n");
        return 1;
    } 
    
    // port does not follow the prefix, str is not ended. There is more to the path in str
    // fprintf(stderr, "Mismatch: Candidate indicated longer URI\n");
    return 0;
}
