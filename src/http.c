#include "http.h"

// assume we have a header
int HTTP_parse(HTTP_Header *header, char *buffer, size_t len)
{
    // parse out just header,
    size_t header_len = 0; 
    char *hd = parse_header_lower(buffer, &header_len); /* malloc'd */
    // fprintf(stderr, "Header: (%ld) %s\n", header_len, hd); // TODO - debug
    // parse out the method 
    header->method = parse_method(hd, &(header->method_l)); /* malloc'd */
    // fprintf(stderr, "Method: (%ld) %s\n", header->method_l, header->method); // TODO - debug


    // parse path 
    header->path = parse_path(hd, &(header->path_l)); /* malloc'd */
    // fprintf(stderr, "Path: (%ld) %s\n", header->path_l, header->path); // TODO - debug

    
    // parse out host & port
    header->host = parse_host(hd, &(header->host_l), &(header->port), &(header->port_l)); /* malloc'd*/
    // fprintf(stderr, "Host: (%ld) %s\n", header->host_l, header->host);// TODO - debug
    // fprintf(stderr, "Port: (%ld) %s\n", header->port_l, header->port);// TODO - debug

    free(hd);

    return 0;
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

// assume header is the to_lower'd version of the buffer
char *parse_host(char *header, size_t *host_len, char **port, size_t *port_len)
{
    char *start = strstr(header, HOST);
    char *end = strstr(start, CRLF);

    char *field = get_buffer(start + HOST_SIZE, end); /* malloc'd */

    end = field;

    /* find end of host string */
    while(*end != '\0') {
        end++;
    }

    field = removeSpaces(field, strlen(field));
    *host_len = strlen(field);

    // get port 
    *port = NULL;
    *port_len = 0;
    char *hostname;
    char *colon = strstr(field, ":");
    if (colon != NULL) {
        *port = get_buffer(colon + 1, end); /* malloc'd */
        *port_len = end - colon - 1; 
        *host_len = colon - field;
        hostname = malloc(*host_len + 1);  /* malloc'd */
        hostname[*host_len] = '\0';

        memcpy(hostname, field, *host_len); 
    } else {
        
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
    char *max_age = strstr(header, "max-age");
    if (max_age != NULL) {
        while (!isdigit(*max_age)) {
            max_age++;
        }
        char *max_age_end = max_age;
        while (isdigit(*max_age_end)) {
            max_age_end++;
        }
        char max_age_val[max_age_end - max_age + 1];
        memcpy(max_age_val, max_age, max_age_end - max_age);
        max_age_val[max_age_end - max_age] = '\0';
        return atoi(max_age_val);
    } else {
        return MAX_AGE;
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
    if (flag == NULL) return 0;
    else return 1;
}

/* Given an HTTP response message, return just the header parsed out as a 
   new heap allocated string with \0 at end; inlcudes last \r\n\r\n */
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

    char *header = to_lower(message, end + 4); /* malloc'd */
    *len = (end + HEADER_END_L) - message;
    return header;
}


/* returns content-length from given header; header must have all lowercase; 
   -1 if it did not exist */
long parse_contentLength(char *header)
{
    char *startContLenField = strstr(header, CONTLEN);
    if (startContLenField == NULL) return -1;

    char *endContLenField = strstr(startContLenField, HDR_LN_END);

    char *value = get_buffer(startContLenField + CONTLEN_SIZE, 
                                endContLenField); /* malloc'd */

    value = removeSpaces(value, strlen(value));
 
    unsigned long contentLength = strtoul(value, NULL, 10);

    free(value);
    return contentLength;
}


/**
 * Removes space characters in a given string, and returns the modified string.
 */
char *removeSpaces(char *str, int size)
{
    unsigned int strIdx = 0;
    int i = 0;
    for (i = 0; i < size; i++) {
        if (str[i] != ' ') {
            str[strIdx] = str[i];
            strIdx++;
        }
    }
    str[strIdx] = '\0';
    return str;
}

/* parses out the max age if any from a lowercase'd header. Default age
   if field isn't present. */
unsigned int parse_maxAge(char *header)
{
    char *startCCField = strstr(header, CACHECONTROL);
    if (startCCField == NULL) {
        return MAX_AGE;
    }

    char *endCCField = strstr(startCCField, HDR_LN_END);

    char *value = get_buffer(startCCField + CACHECONTROL_SIZE, 
                                endCCField + 2); /* malloc'd */
    value = removeSpaces(value, strlen(value));

    char *startMAField = strstr(value, MAXAGE);
    if (startMAField == NULL) {
        return MAX_AGE;
    }

    char *endMAField = strstr(startMAField, HDR_LN_END);

    char *age = get_buffer(startMAField + MAXAGE_SIZE, 
                                endMAField); /* malloc'd */
    age = removeSpaces(age, strlen(age));
    unsigned int maxAge = strtoul(age, NULL, 10);

    free(value);
    free(age);
    return maxAge;
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

