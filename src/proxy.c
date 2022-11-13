#include "proxy.h"

// Ports -- 9055 to 9059

static void zero(void *p, size_t n);
static int expand_buffer(struct Proxy *proxy);
static int compact_buffer(struct Proxy *proxy);
static void clear_buffer(struct Proxy *proxy);

int Proxy_run(short port, size_t cache_size)
{
    struct Proxy proxy;

    /* initialize the proxy */
    Proxy_init(&proxy, port, cache_size);

    /* open listening socket */
    proxy.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy.listen_fd == -1) {
        fprintf(stderr, "[Error] Proxy_run: socket failed\n");
        Proxy_free(&proxy);
        return -1;
    }

    /* set socket options */
    int optval = 1;
    if (setsockopt(proxy.listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval,
                   sizeof(optval)) == -1)
    {
        fprintf(stderr, "[Error] Proxy_run: setsockopt failed\n");
        Proxy_free(&proxy);
        return -1;
    }

    /* build proxy address (type is: struct sockaddr_in) */
    zero(&proxy.addr, sizeof(proxy.addr));
    proxy.addr.sin_family      = AF_INET;
    proxy.addr.sin_addr.s_addr = htonl(INADDR_ANY);
    proxy.addr.sin_port        = htons(proxy.port);

    /* bind listening socket to proxy address; binding listent_fd==mastersocket
     */
    if (bind(proxy.listen_fd, (struct sockaddr *)&proxy.addr,
             sizeof(proxy.addr)) == -1)
    {
        fprintf(stderr, "[Error] Proxy_run: bind failed - %s\n",
                strerror(errno));
        Proxy_free(&proxy);
        return -1;
    }

    /* listen for connections */
    if (listen(proxy.listen_fd, 10) == -1) {
        fprintf(stderr, "[Error] Proxy_run: listen failed\n");
        Proxy_free(&proxy);
        return -1;
    }

    /******************* New accept loop *******************/
    /* add mastersocket to master set; make current maskersocket the fdmax */
    FD_SET(proxy.listen_fd, &(proxy.master_set));
    proxy.fdmax = proxy.listen_fd;

    short flag;
    while (true) {
        proxy.readfds    = proxy.master_set;
        int select_value = select(proxy.fdmax + 1, &(proxy.readfds), NULL, NULL,
                                  proxy.timeout);
        if (select_value < 0) { // ?? < 0 or  == -1
            fprintf(stderr, "[Error] Proxy_run: select failed\n");
            // ?? do we quit if a select fails?
            Proxy_free(&proxy);
            return -1;
        }
        /* timeout */
        else if (select_value == 0)
        {
            // TODO: TIMEOUT
            Proxy_handleTimeout(&proxy);
        }
        /* select_value > 0 */
        else
        {
            flag = Proxy_handle(&proxy);
        }

        if (flag == 666)
            break; // HALT signal
    }
    /******************* End of new accept loop *******************/

    Proxy_free(&proxy);

    return EXIT_SUCCESS;
}

// TODO
int Proxy_handle(struct Proxy *proxy)
{
    int ret;
    /* handle when a new client wants to connect */
    if (FD_ISSET(proxy->listen_fd, &(proxy->readfds))) {
        ret = Proxy_handleListener(proxy);
        if (ret < 0) {
            Proxy_errorHandle(proxy, ret);
        }
    }

    /* handle when clients are sending data */
    Node *curr = NULL;
    Client *client;
    for (curr = proxy->client_list->head; curr != NULL; curr = curr->next) {
        client             = (Client *)curr->data;
        int curr_client_fd = client->socket;
        if (FD_ISSET(curr_client_fd, &proxy->readfds)) {
            ret = Proxy_handleClient(proxy, client);
        }
        if (ret < 0) {
            Proxy_errorHandle(proxy, ret);
        } else {
            // refresh(); // TODO
        }
    }

    return 0;
}

ssize_t Proxy_write(struct Proxy *proxy, int socket)
{
    if (proxy == NULL) {
        fprintf(stderr, "[Error] Proxy_write: NULL parameter not allowed\n");
        return -1;
    }

    /* write request to server */
    size_t bytes_written  = 0;
    size_t bytes_to_write = proxy->buffer_l;
    while (bytes_written < proxy->buffer_l) {
        ssize_t n =
            write(socket, proxy->buffer + bytes_written, bytes_to_write);
        if (n == -1) {
            fprintf(stderr, "%s[Error]%s write failed: %s\n", RED, reset,
                    strerror(errno));
            return -1;
        }
        bytes_written += (size_t)n;
        bytes_to_write -= (size_t)n;
    }

    return bytes_written;
}

ssize_t Proxy_read(struct Proxy *proxy, int socket)
{
    if (proxy == NULL) {
        fprintf(stderr, "[Error] Proxy_read: NULL parameter not allowed\n");
        return -1;
    }

    ssize_t bytes_read     = 0;
    size_t bytes_to_read   = proxy->buffer_sz - proxy->buffer_l - 1;
    size_t body_bytes_read = 0;
    clear_buffer(proxy);
    while ((bytes_read = read(socket, proxy->buffer + proxy->buffer_l,
                              bytes_to_read)) > 0)
    {
        /* update buffer length */
        proxy->buffer_l += bytes_read;

        /* check if buffer is full */
        if (proxy->buffer_l == (proxy->buffer_sz - 1)) {
            if (expand_buffer(proxy) == -1) {
                fprintf(stderr, "[Error] Proxy_read: expand_buffer failed\n");
                return -1;
            }
            proxy->buffer[proxy->buffer_l] = '\0';
        }

        bytes_to_read = proxy->buffer_sz - proxy->buffer_l - 1;

        /* look for end of header */
        char *end_of_header = strstr(proxy->buffer, "\r\n\r\n");
        if (end_of_header != NULL) {
            char *body = end_of_header + 4;
            body_bytes_read += proxy->buffer_l - (body - proxy->buffer);
            break;
        }
    }

    if (bytes_read == -1) {
        fprintf(stderr, "[Error] Proxy_read: read failed\n");
        return -1;
    }

    /* get content length */
    size_t body_length       = 0;
    char *content_length_str = strstr(proxy->buffer, "Content-Length: ");
    if (content_length_str != NULL) {
        char content_length[11];
        zero(content_length, 11);
        content_length_str += 16;
        char *end = strstr(content_length_str, "\r\n");
        if (end == NULL) {
            fprintf(stderr,
                    "[Error] Proxy_read: invalid Content-Length header\n");
            return -1;
        }
        if (end - content_length_str > 10) {
            fprintf(stderr, "[Error] Proxy_read: Content-Length too large\n");
            return -1;
        }
        memcpy(content_length, content_length_str, end - content_length_str);
        body_length = atoi(content_length);
    } else {
        body_length = 0;
    }

    /* read body */
    while (body_bytes_read < body_length) {
        bytes_read =
            read(socket, proxy->buffer + proxy->buffer_l, bytes_to_read);
        proxy->buffer_l += bytes_read;
        body_bytes_read += bytes_read;
        if (bytes_read == -1) {
            fprintf(stderr, "[Error] Proxy_read: read failed\n");
            return -1;
        }

        /* check if buffer is full */
        if (proxy->buffer_l == (proxy->buffer_sz - 1)) {
            if (expand_buffer(proxy) == -1) {
                fprintf(stderr, "[Error] Proxy_read: expand_buffer failed\n");
                return -1;
            }
            proxy->buffer[proxy->buffer_l] = '\0';
        }

        bytes_to_read = proxy->buffer_sz - proxy->buffer_l - 1;
    }

    return proxy->buffer_l;
}

int Proxy_init(struct Proxy *proxy, short port, size_t cache_size)
{
    if (proxy == NULL) {
        return -1;
    }

    /* initialize the proxy cache */
    proxy->cache = Cache_new(cache_size, Connection_free, Connection_print);
    if (proxy->cache == NULL) {
        return -1;
    }

    /* zero out the proxy addresses */
    zero(&proxy->addr, sizeof(proxy->addr));
    // zero(&proxy->client_addr, sizeof(proxy->client_addr)); // TODO - Remove
    zero(&proxy->server_addr, sizeof(proxy->server_addr));
    zero(&proxy->client, sizeof(proxy->client));
    zero(&proxy->server, sizeof(proxy->server));
    zero(&proxy->client_ip, sizeof(proxy->client_ip));
    zero(&proxy->server_ip, sizeof(proxy->server_ip));

    /* initialize socket fds to -1 */
    proxy->listen_fd = -1;
    proxy->server_fd = -1;
    proxy->client_fd = -1;

    /* set the proxy port */
    proxy->port = port;

    /* set buffer to NULL */
    proxy->buffer_sz = BUFSIZE;
    proxy->buffer_l  = 0;
    proxy->buffer    = calloc(proxy->buffer_sz, sizeof(char));

    /* zero out fd_sets and initialize fdmax of muliclient set */
    FD_ZERO(&proxy->master_set); /* clear the sets */
    FD_ZERO(&proxy->readfds);
    proxy->fdmax   = 0;
    proxy->timeout = NULL;

    return 0;
}

void Proxy_free(void *proxy)
{
    if (proxy == NULL) {
        return;
    }

    struct Proxy *p = (struct Proxy *)proxy;

    /* free the proxy */
    Cache_free(&p->cache);
    if (p->listen_fd != -1) {
        close(p->listen_fd);
        p->listen_fd = -1;
    }
    if (p->server_fd != -1) {
        close(p->server_fd);
        p->server_fd = -1;
    }
    if (p->client_fd != -1) {
        close(p->client_fd);
        p->client_fd = -1;
    }
    if (p->buffer != NULL) {
        free(p->buffer);
        p->buffer = NULL;
    }

    p->buffer_sz = 0;
    p->buffer_l  = 0;
    p->port      = 0;
}

void Proxy_print(struct Proxy *proxy)
{
    if (proxy == NULL) {
        return;
    }

    fprintf(stderr, "Proxy {\n");
    fprintf(stderr, "    port = %d\n", proxy->port);
    fprintf(stderr, "    listen_fd = %d\n", proxy->listen_fd);
    fprintf(stderr, "    server_fd = %d\n", proxy->server_fd);
    fprintf(stderr, "    client_fd = %d\n", proxy->client_fd);
    Cache_print(proxy->cache);
    fprintf(stderr, "}\n");
}

static int expand_buffer(struct Proxy *proxy)
{
    if (proxy == NULL) {
        return -1;
    }

    /* expand the buffer */
    proxy->buffer_sz *= 2;
    char *new_buffer = calloc(proxy->buffer_sz, sizeof(char));
    if (new_buffer == NULL) {
        return -1;
    }

    /* copy the old buffer to the new buffer */
    memcpy(new_buffer, proxy->buffer, proxy->buffer_l);

    /* free the old buffer */
    free(proxy->buffer);

    /* set the new buffer */
    proxy->buffer = new_buffer;

    return 0;
}

void Connection_init(struct Connection *conn, struct HTTP_Request *request,
                     struct HTTP_Response *response)
{
    if (conn == NULL) {
        return;
    }

    if (conn->request != NULL) {
        HTTP_free_request(conn->request);
    }

    if (conn->response != NULL) {
        HTTP_free_response(conn->response);
    }

    conn->request  = request;
    conn->response = response;

    conn->is_from_cache = false;
}

struct Connection *Connection_new(struct HTTP_Request *request,
                                  struct HTTP_Response *response)
{
    struct Connection *conn = calloc(1, sizeof(struct Connection));
    if (conn == NULL) {
        return NULL;
    }

    Connection_init(conn, request, response);

    return conn;
}

void Connection_free(void *conn)
{
    if (conn == NULL) {
        return;
    }

    struct Connection *c = (struct Connection *)conn;

    if (c->request != NULL) {
        HTTP_free_request(c->request);
    }

    if (c->response != NULL) {
        HTTP_free_response(c->response);
    }

    free(c);
}

void Connection_print(void *conn)
{
    if (conn == NULL) {
        return;
    }

    struct Connection *c = (struct Connection *)conn;

    fprintf(stderr, "Connection {\n");
    fprintf(stderr, "    %sRequest {%s\n", YEL, reset);
    HTTP_print_request(c->request);
    fprintf(stderr, "    }\n");
    fprintf(stderr, "    %sResponse {%s\n", YEL, reset);
    HTTP_print_response(c->response);
    fprintf(stderr, "    }\n");
    fprintf(stderr, "}\n");
}

static int compact_buffer(struct Proxy *proxy)
{
    if (proxy == NULL) {
        return -1;
    }

    /* compact the buffer */
    proxy->buffer_sz = proxy->buffer_l;
    proxy->buffer    = realloc(proxy->buffer, proxy->buffer_sz);
    if (proxy->buffer == NULL) {
        return -1;
    }

    return 0;
}

static void zero(void *p, size_t n) { memset(p, 0, n); }

static void clear_buffer(struct Proxy *proxy)
{
    if (proxy == NULL) {
        return;
    }

    /* clear the buffer */
    zero(proxy->buffer, proxy->buffer_sz);
    proxy->buffer_l = 0;
}

/**
 * 1. accepts connection request from new client
 * 2. creates a client object for new client
 * 3. puts new client's fd in master set
 * 4. update the value of fdmax
 */
int Proxy_accept(struct Proxy *proxy)
{
    /* 1. accepts connection request from new client */
    // Make a new client struct
    Client *client = Client_new();
    client->addr_l = sizeof(client->addr);
    client->socket =
        accept(proxy->listen_fd, (struct sockaddr *)&(client->addr),
               &(client->addr_l));
    if (client->socket == -1) {
        fprintf(stderr, "[Error] Proxy_accept: accept failed\n");
        // Proxy_free(proxy);
        return ERROR_FAILURE;
    }

    /**
     * 2. Create new client object here ...
     */
    List_push_back(proxy->client_list, (void *)client);

    /* 3. puts new client's fd in master set */
    FD_SET(client->socket, &(proxy->master_set));

    /* 4. update the value of fdmax */
    proxy->fdmax =
        client->socket > proxy->fdmax ? client->socket : proxy->fdmax;

    return EXIT_SUCCESS; // success
}

// TODO
int Proxy_handleListener(struct Proxy *proxy)
{
    if (Proxy_accept(proxy) < 0) {
        /* handle when accept fails */
        // Proxy_free(proxy);
        return ERROR_FAILURE;
    }

    return EXIT_SUCCESS;
}

// TODO
// Note:  special handlign for HALT
int Proxy_handleClient(struct Proxy *proxy, Client *client)
{
    // recv() from the client, assume client is ready to send
    char tmp_buffer[BUFFER_SZ];
    memset(tmp_buffer, 0, BUFFER_SZ);
    int num_bytes = recv(client->socket, tmp_buffer, BUFFER_SZ, 0);
    if (num_bytes < 0) {
        perror("recv");
        return ERROR_FAILURE;
    } else if (num_bytes == 0) {
        // client closed
        return CLIENT_CLOSED;
    }

    client->buffer = realloc(
        client->buffer, client->buffer_l + num_bytes); // Not null terminated
    if (client->buffer == NULL) {
        return ERROR_FAILURE;
    }

    // get client buffer
    char *buffer = client->buffer;
    char *buffer_rest =
        buffer +
        client->buffer_l; /* point to where we need to write the bytes. */
    // write recv'd bytes to client's current buffer
    memcpy(buffer_rest, tmp_buffer, num_bytes);

    // update buffer length
    client->buffer_l += num_bytes;

    // reset timer since we received from client
    gettimeofday(&client->last_recv, NULL);

    // Check for Header

    // Check for Body

    // assume all GETs for now, GETs don't have a body
    // If we have both call HandleRequest -- determine what to do depnding on
    // HTTP Method

    return 0;
}

// TODO
int Proxy_handleTimeout(struct Proxy *proxy) { return 0; }

// TODO
int Proxy_errorHandle(struct Proxy *proxy, int error_code) { return 0; }