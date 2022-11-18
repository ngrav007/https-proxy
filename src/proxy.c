#include "proxy.h"

// Ports -- 9055 to 9059

static void zero(void *p, size_t n);
static int expand_buffer(struct Proxy *proxy);
static void clear_buffer(char *buffer, size_t *buffer_l);
static void free_buffer(char **buffer, size_t *buffer_l, size_t *buffer_sz);

int Proxy_run(short port, size_t cache_size)
{
    struct Proxy proxy;

    /* initialize the proxy */
    fprintf(stderr, "[DEBUG] port = %d cache_size = %ld\n", port, cache_size);
    Proxy_init(&proxy, port, cache_size);
    fprintf(stderr, "[DEBUG] proxy.port = %d\n", proxy.port);

    /* open listening socket */
    proxy.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy.listen_fd == -1) {
        fprintf(stderr, "[!] proxy: socket failed\n");
        Proxy_free(&proxy);
        return ERROR_FAILURE;
    }

    /* set socket options */
    int optval = 1;
    if (setsockopt(proxy.listen_fd, SOL_SOCKET, SO_REUSEADDR,
                   (const void *)&optval, sizeof(optval)) == -1)
    {
        fprintf(stderr, "[!] proxy: setsockopt failed\n");
        Proxy_free(&proxy);
        return ERROR_FAILURE;
    }

    /* build proxy address (type is: struct sockaddr_in) */
    bzero((char *)&(proxy.addr), sizeof(proxy.addr));
    proxy.addr.sin_family      = AF_INET;
    proxy.addr.sin_addr.s_addr = htonl(INADDR_ANY);
    proxy.addr.sin_port        = htons(proxy.port);

    /* bind listening socket to proxy address and port */
    fprintf(stderr, "[DEBUG] listen_fd = %d\n", proxy.listen_fd);
    if (bind(proxy.listen_fd, (struct sockaddr *)&(proxy.addr),
             sizeof(proxy.addr)) == -1)
    {
        fprintf(stderr, "[!] proxy: bind failed - %s\n", strerror(errno));
        Proxy_free(&proxy);
        return ERROR_FAILURE;
    }

    /* listen for connections */
    if (listen(proxy.listen_fd, 10) == -1) {
        fprintf(stderr, "[!] proxy: listen failed\n");
        Proxy_free(&proxy);
        return -1;
    }

    /* add listening socket to master set */
    FD_SET(proxy.listen_fd, &(proxy.master_set));
    proxy.fdmax = proxy.listen_fd;

    /* Accept Loop ---------------------------------------------------------- */
    short ret;
    while (true) {
        proxy.readfds    = proxy.master_set;
        int select_value = select(proxy.fdmax + 1, &(proxy.readfds), NULL, NULL,
                                  proxy.timeout);
        if (select_value < 0) {
            fprintf(stderr, "[!] proxy: select failed\n"); // ? do we quit
            Proxy_free(&proxy);
            return -1;
        } else if (select_value == 0) {
            // TODO: TIMEOUT
            Proxy_handleTimeout(&proxy);
        } else {
            ret = Proxy_handle(&proxy);
        }

        if (ret == 666) {
            break; // HALT signal
        } else if (ret < 0) {
            // Handle error
        } else if (ret > 0) {
            // Handle success
        }
    }

    Proxy_free(&proxy);

    return EXIT_SUCCESS;
}

// TODO
int Proxy_handle(struct Proxy *proxy)
{
    int ret;

    /* check listening socket for new clients */
    if (FD_ISSET(proxy->listen_fd, &(proxy->readfds))) {
        ret = Proxy_handleListener(proxy);
        if (ret < 0) {
            Proxy_errorHandle(proxy, ret); // TODO - handle error 1 layer up
        }
    }

    /* check client sockets for requests */
    Node *curr = proxy->client_list->head;
    Node *next = curr->next;
    Client *client;
    for (curr = proxy->client_list->head; curr != NULL; curr = next) {
        next   = curr->next;
        client = (Client *)curr->data;
        if (FD_ISSET(client->socket, &proxy->readfds)) {
            ret = Proxy_handleClient(proxy, client);
        }

        /* check for errors */
        if (ret < 0) {
            Proxy_errorHandle(proxy, ret); // TODO -- handle error 1 layer up
        } else {
            // refresh(); // TODO
        }
    }

    /* check server sockets for responses */
    // TODO

    return EXIT_SUCCESS;
}

ssize_t Proxy_send(struct Proxy *proxy, int socket)
{
    if (proxy == NULL) {
        fprintf(stderr, "[!] proxy: null parameter passed to send call\n");
        return ERROR_FAILURE;
    }

    /* write request to server */
    ssize_t written  = 0;
    ssize_t to_write = proxy->buffer_l;
    while (written < (ssize_t)proxy->buffer_l) {
        ssize_t n = send(socket, proxy->buffer + written, to_write, 0);
        if (n == -1) {
            fprintf(stderr, "[!] proxy: write failed: %s\n", strerror(errno));
            return ERROR_FAILURE;
        }
        written += n;
        to_write -= n;
    }

    return written;
}

ssize_t Proxy_recv(struct Proxy *proxy, int socket)
{
    if (proxy == NULL) {
        fprintf(stderr, "[!] proxy: null parameter passed to receive call\n");
        return -1;
    }

    ssize_t recvd     = 0;
    size_t to_recv    = proxy->buffer_sz - proxy->buffer_l - 1;
    size_t body_recvd = 0;
    clear_buffer(proxy->buffer, &proxy->buffer_l);
    while ((recvd = read(socket, proxy->buffer + proxy->buffer_l, to_recv)) > 0)
    {
        /* update buffer length */
        proxy->buffer_l += recvd;

        /* check if buffer is full */
        if (proxy->buffer_l == (proxy->buffer_sz - 1)) {
            if (expand_buffer(proxy) == -1) {
                fprintf(stderr, "[!] proxy: failed to expand buffer\n");
                return ERROR_FAILURE;
            }
        }

        /* update number of bytes to receive */
        to_recv = proxy->buffer_sz - proxy->buffer_l - 1;

        /* look for end of header */
        char *end_of_header = strstr(proxy->buffer, "\r\n\r\n");
        if (end_of_header != NULL) {
            char *body = end_of_header + 4;
            body_recvd += proxy->buffer_l - (body - proxy->buffer);
            break;
        }
    }

    if (recvd == -1) {
        fprintf(stderr, "[!] proxy: recv failed\n");
        return ERROR_FAILURE;
    }

    /* get content length */
    size_t body_l            = 0;
    char *content_length_str = strstr(proxy->buffer, "Content-Length: ");
    if (content_length_str != NULL) {
        char content_length[11];
        zero(content_length, 11);
        content_length_str += 16;
        char *end = strstr(content_length_str, "\r\n");
        if (end == NULL) {
            fprintf(stderr, "[!] Proxy_recv: invalid Content-Length header\n");
            return -1;
        }
        if (end - content_length_str > 10) {
            fprintf(stderr, "[!] Proxy_recv: Content-Length too large\n");
            return -1;
        }
        memcpy(content_length, content_length_str, end - content_length_str);
        body_l = atoi(content_length);
    } else {
        body_l = 0;
    }

    /* read body */
    while (body_recvd < body_l) {
        recvd = read(socket, proxy->buffer + proxy->buffer_l, to_recv);
        proxy->buffer_l += recvd;
        body_recvd += recvd;
        if (recvd == -1) {
            fprintf(stderr, "[!] Proxy_recv: read failed\n");
            return -1;
        }

        /* check if buffer is full */
        if (proxy->buffer_l == (proxy->buffer_sz - 1)) {
            if (expand_buffer(proxy) == -1) {
                fprintf(stderr, "[!] proxy: failed to expand buffer\n");
                return ERROR_FAILURE;
            }
        }

        to_recv = proxy->buffer_sz - proxy->buffer_l - 1;
    }

    return proxy->buffer_l;
}

int Proxy_init(struct Proxy *proxy, short port, size_t cache_size)
{
    if (proxy == NULL) {
        fprintf(stderr, "[!] proxy: init was passed a NULL proxy\n");
        return -1;
    }

    /* initialize the proxy cache */
    proxy->cache = Cache_new(cache_size, Response_free, Response_print);
    if (proxy->cache == NULL) {
        fprintf(stderr, "[!] proxy: failed to initialize cache\n");
        return -1;
    }

    /* zero out the proxy addresses */
    zero((char *)&(proxy->addr), sizeof(proxy->addr));
    zero(&(proxy->client), sizeof(proxy->client));
    zero(&(proxy->server), sizeof(proxy->server));
    zero(&(proxy->client_ip), sizeof(proxy->client_ip));
    zero(&(proxy->server_ip), sizeof(proxy->server_ip));

    /* initialize socket fds to -1 */
    proxy->listen_fd = -1;
    proxy->server_fd = -1;
    proxy->client_fd = -1;

    /* set the proxy port */
    proxy->port = port;

    /* set buffer to NULL */
    proxy->buffer_sz = BUFFER_SZ;
    proxy->buffer_l  = 0;
    proxy->buffer    = calloc(proxy->buffer_sz, sizeof(char));

    /* zero out fd_sets and initialize max fd of master_set */
    FD_ZERO(&(proxy->master_set)); /* clear the sets */
    FD_ZERO(&(proxy->readfds));
    proxy->fdmax   = 0;
    proxy->timeout = NULL;

    /* initialize client list */
    proxy->client_list = List_new(Client_free, Client_print, Client_compare);
    if (proxy->client_list == NULL) {
        fprintf(stderr, "[!] proxy: failed to initialize client list\n");
        return -1;
    }
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
    List_print(proxy->client_list);
    fprintf(stderr, "}\n");
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
        fprintf(stderr, "[!] Proxy_accept: accept failed\n");
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

    Client_print(client);

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
// Note:  special handling for HALT
int Proxy_handleClient(struct Proxy *proxy, Client *client)
{
    /* receive data from client */
    char tmp_buffer[BUFFER_SZ];
    memset(tmp_buffer, 0, BUFFER_SZ);
    int num_bytes = recv(client->socket, tmp_buffer, BUFFER_SZ, 0);
    if (num_bytes < 0) {
        fprintf(stderr, "[!] proxy: recv failed\n");
        perror("recv");
        return ERROR_FAILURE;
    } else if (num_bytes == 0) {
        fprintf(stderr, "[*] proxy: client closed connection\n");
        Proxy_close(client->socket, &(proxy->master_set),
                           proxy->client_list, client);
        return CLIENT_CLOSED;
    }

    /* resize client buffer, include null-terminator for parsing (strstr) */
    client->buffer = realloc(client->buffer, client->buffer_l + num_bytes + 1);
    if (client->buffer == NULL) {
        return ERROR_FAILURE;
    }
    client->buffer[client->buffer_l + num_bytes] = '\0';

    /* copy data from tmp_buffer to client buffer */
    memcpy(client->buffer + client->buffer_l, tmp_buffer, num_bytes);
    client->buffer_l += num_bytes;

    print_ascii(client->buffer, client->buffer_l); // DEBUG

    /* update last receive time to now */
    gettimeofday(&client->last_recv, NULL);

    /* check for http header, parse header if we have it */
    if (HTTP_got_header(client->buffer)) {
        HTTP_Header header;
        int parse = HTTP_parse(&header, client->buffer);
        if (parse < 0) {
            fprintf(stderr, "[!] proxy: http parser failed\n");
            return ERROR_FAILURE;
        }

        fprintf(stderr, "[DEBUG] header.method: %s\n", header.method);

        /* store the key of current request in client */
        Client_setKey(
            client, &header); // TODO - we may need to calculate key every time

        if (strncmp(header.method, HTTP_GET, HTTP_GET_L) == 0) { // GET
            fprintf(stderr, "[Proxy_handleClient] GET request received\n");

            /* check if we have the file in cache */
            Response *response =
                (Response *)Cache_get(proxy->cache, client->key);
            Response_print(response);

            // if Entry Not null & fresh, serve from Cache (add Age field)
            if (response != NULL) {
                fprintf(stderr, "[DEBUG] Entry in cache\n");
                int response_size  = Response_size(response);
                proxy->buffer      = calloc(response_size, sizeof(char));
                char *raw_response = Response_get(response);
                memcpy(proxy->buffer, raw_response, response_size);
                proxy->buffer_l = response_size;
                if (Proxy_send(proxy, client->socket) == -1) {
                    fprintf(stderr, "[!] Error: write()\n");
                    return ERROR_FAILURE;
                }
            } else {
                fprintf(stderr, "[DEBUG] Entry NOT in cache\n");
                // request resource from server
                ssize_t n = Proxy_fetch(proxy, header.host, atoi(header.port),
                                        client->buffer, client->buffer_l);
                if (n < 0) {
                    fprintf(stderr, "[!] proxy: Proxy_fetch() failed\n");
                    return ERROR_FAILURE;
                }
                // cache response that was put in proxy->buffer
                Response *r = Response_new(proxy->buffer, proxy->buffer_l);
                Cache_put(proxy->cache, client->key, (void *)r,
                          DEFAULT_MAX_AGE);

                // serve response to client
                if (Proxy_send(proxy, client->socket) == -1) {
                    fprintf(stderr, "[!] Error: write()\n");
                    return ERROR_FAILURE;
                }
            }

            /* clean up */
            free_buffer(&client->buffer, &client->buffer_l, NULL);
            free_buffer(&proxy->buffer, &proxy->buffer_l, &proxy->buffer_sz);

            // TODO - might need to close socket after confirming fullresponse
            // was sent
            Proxy_close(client->socket, &(proxy->master_set),
                               proxy->client_list, client);
        } else if (strncmp(header.method, "connect", 7) == 0) { // CONNECT
            fprintf(stderr, "[DEBUG] CONNECT : unimplemented.\n");
        }
    }

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

/**
 * Proxy_close
 * Closes the given socket and removes it from the master fd_set, and marks
 * the node tracking the client associated to the client for removal from
 * the clients_status and registered_users tracking lists that belong to
 * the server.
 */
void Proxy_close(int socket, fd_set *master_set, List *client_list, Client *client)
{
    /* close this connection */
    fprintf(stderr, "[DEBUG] Closing Socket # = %d\n", socket);
    close(socket);

    fprintf(stderr, "[DEBUG] Remove Socket # from master_set = %d\n", socket);
    /* remove the client from master fd_set */
    FD_CLR(socket, master_set);

    /* mark the node for removal from trackers */
    fprintf(stderr, "[DEBUG] Remove Socket # from client_list = %d\n", socket);
    List_remove(client_list, (void *)client);
}

/**
 * makes ONE request to a server for a resource in the given request.
 * Writes to a server and then closes conenction.
 */
ssize_t Proxy_fetch(Proxy *proxy, char *hostname, int port, char *request,
                    int request_len)
{
    struct sockaddr_in saddr;
    struct hostent *h;
    int sockfd, connfd;

    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "[!] proxy: client socket call failed\n");
        exit(1);
    }

    h = gethostbyname(hostname);
    if (h == NULL) {
        fprintf(stderr, "[!] proxy: unknown host\n");
        return -1;
    }

    memset(&saddr, '\0', sizeof(saddr));
    saddr.sin_family = AF_INET;
    memcpy((char *)&saddr.sin_addr.s_addr, h->h_addr_list[0], h->h_length);
    saddr.sin_port = htons(port);

    /*
     * make connection request
     */
    connfd = connect(sockfd, (struct sockaddr *)&saddr, sizeof(saddr));
    if (connfd < 0) {
        fprintf(stderr, "[!] proxy: error connecting to host\n");
        return -1;
    }

    /*
     * send (write) message to server
     */
    long msgsize = send(sockfd, request, request_len, 0);
    if (msgsize < 0) {
        fprintf(stderr, "[!] proxy: error sending to host\n");
        return -1;
    }

    /*
     * receive message from server
     */
    int r = Proxy_recv(proxy, sockfd); // puts response in proxy->buffer
    if (r < 0) {
        fprintf(stderr, "[!] proxy: error receiving from host\n");
        return -1;
    }

    /* close the socket */
    close(sockfd);

    return r;
}

/* Static Functions --------------------------------------------------------- */
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

static void clear_buffer(char *buffer, size_t *buffer_l)
{
    if (buffer == NULL || buffer_l == 0) {
        return;
    }

    /* clear the buffer */
    zero(buffer, *buffer_l);
    *buffer_l = 0;
}

static void free_buffer(char **buffer, size_t *buffer_l, size_t *buffer_sz)
{
    if (buffer == NULL || *buffer == NULL) {
        return;
    }

    /* free the buffer */
    free(*buffer);

    /* set the buffer to NULL */
    *buffer = NULL;

    if (buffer_l != NULL) {
        *buffer_l = 0;
    }

    if (buffer_sz != NULL) {
        *buffer_sz = 0;
    }
}

static void zero(void *p, size_t n) { memset(p, 0, n); }
