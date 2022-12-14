#include "query.h"

int Query_new(Query **q, char *buffer, size_t buffer_l)
{
    if (q == NULL || buffer == NULL) {
        return ERROR_FAILURE;
    } 

    /* allocate memory for the query */
    if (*q != NULL) { Query_free(*q); }
    *q = calloc(1, sizeof(Query));
    if (*q == NULL) {
        return ERROR_FAILURE;
    }

    /* create new request from buffer */
    (*q)->req = Request_new(buffer, buffer_l);
    if ((*q)->req == NULL) {
        print_error("query: failed to create request");
        Query_free(*q);
        *q = NULL;
        return ERROR_FAILURE;
    }

    /* check if the request is a halt request */
    if (strncmp((*q)->req->method, PROXY_HALT, (*q)->req->method_l) == 0) {
        Query_free(*q);
        *q = NULL;
        return HALT; // TODO make halt in body of POST request
    }

    /* initialize query buffer */
    (*q)->buffer = calloc(QUERY_BUFFER_SZ + 1, sizeof(char));
    if ((*q)->buffer == NULL) {
        Query_free(*q);
        *q = NULL;
        return ERROR_FAILURE;
    }
    (*q)->buffer_l = 0;
    (*q)->buffer_sz = QUERY_BUFFER_SZ;

    /* open query socket */
    (*q)->socket = socket(AF_INET, SOCK_STREAM, 0);
    if ((*q)->socket < 0) {
        print_error("client socket call failed");
        Query_free(*q);
        *q = NULL;
        return ERROR_FAILURE;
    }

    fprintf(stderr, "%s[DEBUG]%s query_new: host: %s\n", BYEL, reset, (*q)->req->host);
    if ((*q)->req->host_l == 0) {
        print_warning("query_new: host is empty");
        return HOST_UNKNOWN;
    }
    
    (*q)->host_info = gethostbyname((*q)->req->host);
    if ((*q)->host_info == NULL) {
        fprintf(stderr, "[!] proxy: unknown host\n");
        return HOST_UNKNOWN;
    }

    bzero(&(*q)->server_addr, sizeof((*q)->server_addr));
    (*q)->server_addr.sin_family = AF_INET;
    memcpy((char *)&(*q)->server_addr.sin_addr.s_addr, (*q)->host_info->h_addr_list[0], ((*q)->host_info->h_length));
    (*q)->server_addr.sin_port = htons(atoi((*q)->req->port)); // ! - caller needs to change port 443 for HTTPS

    (*q)->res = NULL;

    (*q)->state = QRY_INIT;

    return EXIT_SUCCESS;
}


void Query_free(Query *query)
{
    if (query == NULL) {
        return;
    } 

    Request_free(query->req);
    Response_free(query->res);

    #if RUN_SSL
    Query_clearSSL(query);
    #endif 

    close(query->socket);
    query->socket = -1;

    #if RUN_SSL
    Query_clearSSLCtx(query);
    #endif 

    free(query->buffer);
    free(query);
}

void Query_print(Query *query)
{
    if (query == NULL) {
        return;
    }

    printf("Query:\n");
    Request_print(query->req);
    Response_print(query->res);
    fprintf(stderr, "Buffer Length: %zu\n", query->buffer_l);
    fprintf(stderr, "Buffer Size: %zu\n", query->buffer_sz);
    fprintf(stderr, "Socket: %d\n", query->socket);
    print_ascii(query->buffer, query->buffer_l);
}

int Query_compare(Query *query1, Query *query2)
{
    if (query1 == NULL || query2 == NULL) {
        return -1;
    }

    return Request_compare(query1->req, query2->req);
}

#if RUN_SSL
void Query_clearSSL(Query *query)
{
    if (query == NULL) {
        return;
    }

    SSL_free(query->ssl);
    query->ssl = NULL;
}

void Query_clearSSLCtx(Query *query)
{
    if (query == NULL) {
        return;
    }

    SSL_CTX_free(query->ctx);
    query->ctx = NULL;
}
#endif 

