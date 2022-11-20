#include "query.h"

Query *Query_new(char *buffer, size_t buffer_l)
{
    Query *q = calloc(1, sizeof(struct Query));
    if (q == NULL) {
        return NULL;
    }

    q->req = Request_new(buffer, buffer_l);
    if (q->req == NULL) {
        Query_free(q);
        return NULL;
    }

    q->buffer = calloc(QUERY_BUFFER_SZ, sizeof(char));
    if (q->buffer == NULL) {
        Query_free(q);
        return NULL;
    }
    q->buffer_l = 0;
    q->buffer_sz = QUERY_BUFFER_SZ;

    q->socket = socket(PF_INET, SOCK_STREAM, 0);
    if (q->socket < 0) {
        fprintf(stderr, "[!] proxy: client socket call failed\n");
        exit(1);
    }

    q->host_info = gethostbyname(q->req->host);
    if (q->host_info == NULL) {
        fprintf(stderr, "[!] proxy: unknown host\n");
        return NULL;
    }

    bzero(&q->server_addr, sizeof(q->server_addr));
    q->server_addr.sin_family = AF_INET;
    memcpy((char *)&q->server_addr.sin_addr.s_addr, q->host_info->h_addr_list[0], q->host_info->h_length);
    q->server_addr.sin_port = htons(atoi(q->req->port));

    q->res = NULL;

    return q;
}

Query *Query_create(Request *req, Response *res, struct sockaddr_in *server_addr, socklen_t server_addr_l, int server_fd);

void Query_free(Query *query)
{
    if (query == NULL) {
        return;
    } 

    Request_free(query->req);
    Response_free(query->res);
    close(query->socket);
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

