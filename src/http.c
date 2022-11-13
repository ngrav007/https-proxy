#include "http.h"

HTTP_Request *HTTP_parse_request(char *request, size_t len)
{
    HTTP_Request *req = calloc(1, sizeof(struct HTTP_Request));
    req->request_l    = len;

    /* store the request string */
    req->request = calloc(len + 1, sizeof(char));
    memcpy(req->request, request, len);
    req->request[len] = '\0';

    /* find the path */
    char *path = strstr(req->request, " "); // find the first space
    if (path == NULL) {
        fprintf(stderr, "[Error] invalid request - no path found\n");
        HTTP_free_request(req);
        return NULL;
    }
    path++;                             // skip the space
    char *path_end = strstr(path, " "); // find the second space
    if (path_end == NULL) {
        fprintf(stderr, "[Error] no path end found in request\n");
        HTTP_free_request(req);
        return NULL;
    }
    req->path_l = path_end - path;
    req->path   = calloc(path_end - path + 1, sizeof(char));
    memcpy(req->path, path, path_end - path);
    req->path[path_end - path] = '\0';

    /* find the host */
    char *host = strstr(req->request, "Host: ");
    if (host == NULL) {
        fprintf(stderr, "[Error] invalid request - no host found\n");
        HTTP_free_request(req);
        return NULL;
    }
    host += 6; // skip "Host: "
    while (*host == ' ') {
        host++;
    }
    char *host_end = strchr(host, '\r');
    if (host_end == NULL) {
        fprintf(stderr, "[Error] no host end found in request\n");
        HTTP_free_request(req);
        return NULL;
    }
    req->host_l = host_end - host;
    req->host   = calloc(host_end - host + 1, sizeof(char));
    memcpy(req->host, host, host_end - host);
    req->host[host_end - host] = '\0';

    /* check for a port */
    char *port = strrchr(req->host, ':');
    if (port != NULL) {
        port++; // skip the colon
        while (!isdigit(*port)) {
            port++;
        }

        /*find the end of the port */
        char *port_end = port;
        while (isdigit(*port_end)) {
            port_end++;
        }

        /* store the port */
        req->port_l = port_end - port;
        req->port   = calloc(req->port_l + 1, sizeof(char));
        memcpy(req->port, port, req->port_l);
        req->port[req->port_l] = '\0';

        /* remove the port from the host */
        req->host_l            = port - req->host - 1;
        req->host              = realloc(req->host, req->host_l + 1);
        req->host[req->host_l] = '\0';
    } else {
        req->port = calloc(DEFAULT_PORT_L + 1, sizeof(char));
        memcpy(req->port, DEFAULT_PORT_S, DEFAULT_PORT_L);
        req->port[DEFAULT_PORT_L] = '\0';
        req->port_l               = DEFAULT_PORT_L;
    }

    return req;
}

void HTTP_free_request(void *request)
{
    if (request == NULL) {
        return;
    }
    HTTP_Request *req = (HTTP_Request *)request;
    free(req->request);
    free(req->path);
    free(req->host);
    free(req->port);
    free(req);
}

HTTP_Response *HTTP_parse_response(char *response, size_t len)
{
    HTTP_Response *res = calloc(1, sizeof(struct HTTP_Response));
    res->response_l    = len;

    /* store the header string */
    ssize_t header_len = HTTP_header_len(response);
    if (header_len == -1) {
        fprintf(stderr, "[Error] invalid response - no header found\n");
        HTTP_free_response(res);
        return NULL;
    }
    res->header_l = header_len;
    res->header   = calloc(header_len + 1, sizeof(char));
    memcpy(res->header, response, header_len);

    /* store the body string */
    size_t body_len = HTTP_body_len(response, len);
    res->body_l     = body_len;
    res->body       = calloc(body_len + 1, sizeof(char));
    memcpy(res->body, response + header_len + CRLF_L + CRLF_L, body_len);

    /* find the max-age */
    char *max_age = strstr(res->header, "max-age");
    if (max_age != NULL) {
        while (!isdigit(*max_age)) { // find the first digit
            max_age++;
        }
        char *max_age_end = max_age;
        while (isdigit(*max_age_end)) { // find the end of the number
            max_age_end++;
        }
        size_t max_age_l = max_age_end - max_age; // store the length

        char max_age_str[max_age_l + 1];
        memcpy(max_age_str, max_age, max_age_l);
        max_age_str[max_age_l] = '\0';
        res->max_age           = atoi(max_age_str);
    } else {
        res->max_age = DEFAULT_MAX_AGE;
    }

    return res;
}

void HTTP_free_response(void *response)
{
    if (response == NULL) {
        return;
    }
    HTTP_Response *res = (HTTP_Response *)response;
    free(res->header);
    free(res->body);
    free(res);
}

void HTTP_print_request(void *request)
{
    if (request == NULL) {
        fprintf(stderr, "[Error] invalid request\n");
        return;
    }

    HTTP_Request *req = (HTTP_Request *)request;
    printf("%sRequest:%s\n", YEL, reset);
    printf("%s\n(%ld)\n", req->request, req->request_l);
    printf("  path: %s (%ld)\n", req->path, req->path_l);
    printf("  host: %s (%ld)\n", req->host, req->host_l);
    printf("  port: %s (%ld)\n", req->port, req->port_l);
}

void HTTP_print_response(void *response)
{
    if (response == NULL) {
        fprintf(stderr, "[Error] invalid response\n");
        return;
    }

    HTTP_Response *res = (HTTP_Response *)response;
    printf("%sResponse:%s\n", YEL, reset);
    printf("  header:\n%s (%ld)\n", res->header, res->header_l);
    printf("  max-age: %ld\n", res->max_age);
}

short HTTP_get_port(HTTP_Request *request)
{
    if (request == NULL) {
        fprintf(stderr, "[Error] invalid request\n");
        return -1;
    }
    return atoi(request->port);
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

char *HTTP_response_to_string(HTTP_Response *response, long age, size_t *len,
                              bool is_from_cache)
{
    if (response == NULL) {
        fprintf(stderr, "[Error] invalid response\n");
        return NULL;
    }

    size_t age_header_l  = 0;
    char age_header[100] = {0};
    if (is_from_cache) {
        sprintf(age_header, "Age: %ld", age);
        age_header_l = strlen(age_header);
    }

    if (is_from_cache) {
        *len = response->header_l + CRLF_L + age_header_l + CRLF_L + CRLF_L +
               response->body_l;
    } else {
        *len = response->header_l + CRLF_L + CRLF_L + response->body_l;
    }

    char *res     = calloc(*len + 1, sizeof(char));
    size_t offset = 0;
    memcpy(res, response->header, response->header_l);
    offset += response->header_l;
    memcpy(res + offset, "\r\n", CRLF_L);
    offset += CRLF_L;
    if (is_from_cache) {
        memcpy(res + offset, age_header, age_header_l);
        offset += age_header_l;
        memcpy(res + offset, "\r\n", CRLF_L);
        offset += CRLF_L;
    }
    memcpy(res + offset, "\r\n", CRLF_L);
    offset += CRLF_L;
    memcpy(res + offset, response->body, response->body_l);
    offset += response->body_l;

    return res;
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

size_t HTTP_body_len(char *httpstr, size_t len)
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
