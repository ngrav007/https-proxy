#include "http.h"

/* HTTP Functions ----------------------------------------------------------- */

/* HTTP_Parse
 *    Purpose: Parse an HTTP request header and store the information in the
 *             given HTTP_Header struct.
 * Parameters: @header - Pointer to an HTTP_Header struct to store the parsed
 *                      information
 *             @buffer - Pointer to a buffer containing the HTTP request
 *             @len    - Length of the buffer
 *    Returns: 0 on success, -1 on failure.
 *
 *   Note: This function does not check for a valid HTTP request, it only
 *         parses the header.
 * 
 *   Note: This function allocates memory for the method, host, and path fields
 *         of the HTTP_Header struct. It is the responsibility of the caller to
 *         free this memory.
 */
int HTTP_parse(HTTP_Header *header, char *buffer)
{
    // parse out just header,
    size_t header_len = 0; 
    char *hd = parse_header_lower(buffer, &header_len);
    header->method = parse_method(hd, &(header->method_l));
    header->path = parse_path(hd, &(header->path_l));
    header->host = parse_host(hd, &(header->host_l), &(header->port), &(header->port_l));
    free(hd);

    return 0;
}

void HTTP_free_header(void *header)
{
    if (header == NULL) {
        return;
    }

    HTTP_Header *hdr = (HTTP_Header *)header;
    free(hdr->method);
    free(hdr->path);
    free(hdr->host);
    free(hdr->port);
}

long HTTP_get_max_age(char *httpstr)
{
    if (httpstr == NULL) {
        fprintf(stderr, "[Error] invalid http string\n");
        return -1;
    }

    /* copy the header */
    size_t header_len = HTTP_header_len(httpstr);
    char header[header_len + 1];
    memcpy(header, httpstr, header_len);
    header[header_len] = '\0';

    /* lowercase the header */
    size_t i;
    for (i = 0; i < header_len; i++) {
        header[i] = tolower(header[i]);
    }

    /* find the max-age */
    char *max_age_field = strstr(header, "max-age");
    if (max_age_field != NULL) {
        while (!isdigit(*max_age_field)) {
            max_age_field++;
        }
        char *max_age_end = max_age_field;
        while (isdigit(*max_age_end)) {
            max_age_end++;
        }
        char max_age_val[max_age_end - max_age_field + 1];
        memcpy(max_age_val, max_age_field, max_age_end - max_age_field);
        max_age_val[max_age_end - max_age_field] = '\0';

        return atoi(max_age_val);
    } else {
        return DEFAULT_MAX_AGE;
    }
}

ssize_t HTTP_get_content_length(char *httpstr)
{
    if (httpstr == NULL) {
        return -1;
    }

    /* copy the header into a new string */
    size_t header_len = HTTP_header_len(httpstr);
    char header[header_len + 1];
    memcpy(header, httpstr, header_len);
    header[header_len] = '\0';

    /* lowercase the header */
    size_t i;
    for (i = 0; i < header_len; i++) {
        header[i] = tolower(header[i]);
    }

    /* find the content-length */
    char *content_length = strstr(header, "content-length");
    if (content_length == NULL) {
        return -1;
    }

    /* find the start of the content-length value */
    while (!isdigit(*content_length)) {
        content_length++;
    }

    /* find the end of the content-length value */
    char *content_length_end = content_length;
    while (isdigit(*content_length_end)) {
        content_length_end++;
    }

    /* copy the content-length value into a new string */
    char content_length_value[content_length_end - content_length + 1];
    memcpy(content_length_value, content_length,
           content_length_end - content_length);
    content_length_value[content_length_end - content_length] = '\0';

    return atol(content_length_value);
}

ssize_t HTTP_header_len(char *httpstr)
{
    if (httpstr == NULL) {
        fprintf(stderr, "[Error] invalid http string\n");
        return -1;
    }

    char *header_end = strstr(httpstr, "\r\n\r\n");
    if (header_end == NULL) {
        fprintf(stderr, "[Error] no header end found in http string\n");
        return -1;
    }
    return header_end - httpstr;
}

ssize_t HTTP_body_len(char *httpstr, size_t len)
{
    if (httpstr == NULL) {
        fprintf(stderr, "[Error] invalid http string\n");
        return -1;
    }

    char *body = strstr(httpstr, "\r\n\r\n");
    if (body == NULL) {
        fprintf(stderr, "[Error] no body found in http string\n");
        return -1;
    }
    body += 4; // skip the "\r\n\r\n"
    return len - (body - httpstr);
}

bool HTTP_got_header(char *buffer)
{
    char *flag = strstr(buffer, HEADER_END);
    return flag != NULL;
}

/* Parser Functions --------------------------------------------------------- */

/* Given an HTTP response message, return just the header parsed out as a 
   new heap allocated string with \0 at end; includes last \r\n\r\n */
char *parse_header_raw(char *message, size_t *len)
{
    char *end = strstr(message, HEADER_END);
    if (end == NULL) return NULL;

    char *header = get_buffer(message, end + 4); /* malloc'd */
    *len = (end + HEADER_END_L) - message;
    return header;
}

/* Given an HTTP response message, return just the header parsed out as a 
   new heap allocated string with \0 at end, all letters lowercase.
   header goes until the last \r\n\r\n */
char *parse_header_lower(char *message, size_t *len)
{
    char *end = strstr(message, HEADER_END);
    if (end == NULL) return NULL;

    char *header = get_buffer_lc(message, end + 4); /* malloc'd */
    *len = (end + HEADER_END_L) - message;

    return header;
}


// Get the method from an HTTP header
char *parse_method(char *header, size_t *len)
{
    char *method = NULL;
    char *end = NULL;
    char *start = NULL;

    end = strchr(header, ' ');
    if (end == NULL) {
        return NULL;
    }

    start = header;

    method = malloc(end - start + 1);
    if (method == NULL) {
        return NULL;
    }

    memcpy(method, start, end - start);
    method[end - start] = '\0';

    *len = end - start;

    return method;
}

// assume header is the get_buffer_lc'd version of the buffer
char *parse_host(char *header, size_t *host_len, char **port, size_t *port_len)
{
    char *start = strstr(header, HOST);
    char *end = strstr(start, CRLF);
    char *field = get_buffer(start + HOST_L, end);

    end = field;

    /* find end of host string */
    while(*end != '\0') {
        end++;
    }

    field = remove_whitespace(field, strlen(field));
    *host_len = strlen(field);

    /* get the port if there is one */
    *port = NULL;
    *port_len = 0;
    char *hostname;
    char *colon = strstr(field, ":");
    if (colon != NULL) {    // port specified
        *port = get_buffer(colon + 1, end); /* malloc'd */
        *port_len = end - colon - 1; 
        *host_len = colon - field;
        hostname = malloc(*host_len + 1);  /* malloc'd */
        hostname[*host_len] = '\0';
        memcpy(hostname, field, *host_len); 
    } else {    // no port specified
        *port = calloc(HTTP_PORT_L + 1, sizeof(char));
        memcpy(*port, HTTP_PORT, HTTP_PORT_L); 
        *port_len = HTTP_PORT_L;
        hostname = malloc(*host_len + 1);  /* malloc'd */
        hostname[*host_len] = '\0';
        memcpy(hostname, field, *host_len);
    }

    free(field);

    return hostname;
}

char *parse_path(char *header, size_t *len)
{
    char *start = strstr(header, " ");
    char *end = strstr(start + 1, " ");
    char *resource = get_buffer(start + 1, end); /* malloc'd */

    *len = end - start;
    
    return resource; 
}

/* returns content-length from given header; header must have all lowercase; 
   -1 if it did not exist */
long parse_contentLength(char *header)
{
    char *start = strstr(header, CONTENT_LEN);
    if (start == NULL) {
        return -1;
    }

    char *end = strstr(start, CRLF);

    char *value = get_buffer(start + CONTENT_LEN_L, end); // malloc'd

    value = remove_whitespace(value, strlen(value));
 
    unsigned long contentLength = strtoul(value, NULL, 10);

    free(value);

    return contentLength;
}

/* parses out the max age if any from a lowercase'd header. Default age
   if field isn't present. */
unsigned int parse_maxage(char *header)
{
    char *cc_start = strstr(header, CACHECONTROL);
    if (cc_start == NULL) {
        return DEFAULT_MAX_AGE;
    }

    char *cc_end = strstr(cc_start, CRLF);

    char *value = get_buffer(cc_start + CACHECONTROL_L, cc_end + 2);
    value = remove_whitespace(value, strlen(value));

    char *ma_start = strstr(value, MAXAGE);
    if (ma_start == NULL) {
        return DEFAULT_MAX_AGE;
    }

    char *ma_end = strstr(ma_start, CRLF);

    char *age = get_buffer(ma_start + MAXAGE_L, ma_end); /* malloc'd */
    age = remove_whitespace(age, strlen(age));
    unsigned int max_age_value = strtoul(age, NULL, 10);

    free(value);
    free(age);

    return max_age_value;
}

/* returns the string for "Age: <age>\r\n" */
char *make_ageField(unsigned int age)
{
    char *field = calloc(18, sizeof(char));
    int size = snprintf(field, 18, "Age: %u\r\n", age);
    // printf("Age field: \"%s\"\t\tSize: %d\t\t\"size\": %d\n\n", field, strlen(field), size);
    field = realloc(field, size + 1);
    // printf("Age field: \"%s\"\t\tSize: %d\t\t\"size\": %d\n\n", field, strlen(field), size);
    return field;
}

/* HTTP_compare
 *    Purpose: Compare two HTTP request headers (HTTP_Header
 *             the same
 * Parameters: @header1: first header to compare
 *             @header2: second header to compare
 *   Returns: 1 if the same, 0 if not
 */
int HTTP_compare(HTTP_Header *header1, HTTP_Header *header2)
{
    if (header1 == NULL || header2 == NULL) {
        return 0;
    }

    if (strcmp(header1->method, header2->method) != 0) {
        return 0;
    }

    if (strcmp(header1->host, header2->host) != 0) {
        return 0;
    }

    if (strcmp(header1->path, header2->path) != 0) {
        return 0;
    }

    if (strcmp(header1->port, header2->port) != 0) {
        return 0;
    }

    return 1;
}


/* Response Functions ------------------------------------------------------- */
/**
 * Response_new
 *    Purpose: Allocates memory for a Response and initializes it's size
 *             and original raw bytes of the response
 * Parameters: The number of bytes of the response to store, and the 
 *             original response message
 *   Returns: Pointer to a Response containing the response data
 * 
 * Note: It is the responsibility of the caller to deallocate the memory of 
 *       the message argument, if need be.
 */
Response *Response_new(char *message, size_t message_l)
{
    Response *response = malloc(sizeof(struct Response));
    if (response == NULL) {
        fprintf(stderr, "Unable to allocate memory for Response structure.\n");
        exit(1);
    }

    /* create calloc'd copy of message buffer */
    response->size = message_l;
    response->raw = calloc(message_l, sizeof(char));
    if (response->raw == NULL) {
        fprintf(stderr, "Unable to allocate memory for response data.\n");
        exit(1);
    }
    memcpy(response->raw, message, message_l);

    // Response_print(response); // DEBUG

    return response;
}

/**
 * Response_free
 *    Purpose: Deallocates the memory pointed to by content
 * Parameters: Pointer to the FileContent to deallocate
 *    Returns: None
 */
void Response_free(void *response)
{
    if (response == NULL) {
        return;
    }

    Response *r = response; 
    free(r->raw);
    free(r);
}

/**
 * Response_get
 *    Purpose: Returns the raw response message
 * Parameters: A response pointer
 *    Returns: Char array representing the raw bytes of the original store
 *             response message
 */
char *Response_get(Response *response) 
{ 
    if (response == NULL) { return NULL; }
    return response->raw; 
}

/**
 * Response_size
 *   Purpose: Returns the size of the response in bytes
 * Parameter: A response pointer
 *    Returns: Size of response message in bytes
 */
unsigned long Response_size(Response *response) 
{ 
    if (response == NULL) { return 0; }
    return response->size; 
}

/**
 * Response_print
 *    Purpose: Prints the file response message contained in given Response
 * Parameters: A Response called response
 *    Returns: None
 */
void Response_print(void *response)
{
    if (response == NULL) { return; }
    Response *r = (Response *)response;
    print_ascii(r->raw, r->size);
}

int Response_compare(void *response1, void *response2)
{
    if (response1 == NULL || response2 == NULL) {
        return 0;
    }

    Response *r1 = (Response *)response1;
    Response *r2 = (Response *)response2;

    if (r1->size != r2->size) {
        return 0;
    }

    return memcmp(r1->raw, r2->raw, r1->size);
}
