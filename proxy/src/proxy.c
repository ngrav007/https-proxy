#include "proxy.h"

#define SELECT_ERROR      -1
#define SELECT_TIMEOUT    0
#define TIMEOUT_FALSE     0
#define TIMEOUT_TRUE      1
#define QUERY_TYPE        0
#define CLIENT_TYPE       1

// Ports -- 9055 to 9059
#if RUN_CACHE
static char *get_key(Request *req);
#endif
static short select_loop(Proxy *proxy);

/* ---------------------------------------------------------------------------------------------- */
/* PROXY DRIVER FUNCTION ------------------------------------------------------------------------ */
/* ---------------------------------------------------------------------------------------------- */

/** Proxy_run
 *    Purpose: Runs the proxy on the given port and cache size. Initializes the proxy, binds and
 *             listens on the proxy socket, and enters the accept loop. The accept loop calls
 *             select() to wait for connections on the proxy socket and client sockets.
 * Parameters: @port - port to listen on
 *             @cache_size - size of cache to initialize to
 *    Returns: EXIT_SUCCESS on success, or ERROR_FAILURE indicating type of failure
 */
int Proxy_run(short port)
{
    struct Proxy proxy;
    signal(SIGPIPE, SIG_IGN); // ignore SIGPIPE (broken pipe error)

    /* Initialize SSL --------------------------------------------------------------------------- */

#if RUN_SSL
    if (!is_root()) {
        print_error("proxy: must be run as root");
        return ERROR_FAILURE;
    }
    SSL_load_error_strings(); // ? - might not need
    SSL_library_init();       // ? - might not need
    OpenSSL_add_all_algorithms();
#endif

    /* Initialize Proxy ------------------------------------------------------------------------- */
    Proxy_init(&proxy, port);
    print_success("[+] HTTP Proxy -----------------------------------");
    fprintf(stderr, "%s[*]%s   Proxy Port = %d\n", YEL, reset, proxy.port);
    #if RUN_CACHE
    fprintf(stderr, "%s[*]%s   Cache Size = %ld\n", YEL, reset, proxy.cache->capacity);
    #endif

    /* Bind & Listen Proxy Socket --------------------------------------------------------------- */
    if (Proxy_listen(&proxy) < 0) {
        print_error("proxy: failed to listen");
        return ERROR_FAILURE;
    }

    /* Select Loop ------------------------------------------------------------------------------ */
    short ret = select_loop(&proxy);
    if (ret == HALT) {
        print_info("proxy: shutting down");
        Proxy_free(&proxy);
        return EXIT_SUCCESS;
    }

    /* Shutdown Proxy --------------------------------------------------------------------------- */
    print_error("proxy: failed and cannot recover");
    Proxy_free(&proxy);

    return EXIT_FAILURE;
}

/* ---------------------------------------------------------------------------------------------- */
/* PROXY GENERAL FUNCTIONS ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------------------------------- */

/** Proxy_init
 *   Purpose: Initializes a proxy struct with the given port and cache size. Allocates memory for
 *            the proxy struct and initializes the cache if RUN_CACHE is set to 1. Also initializes
 *            the proxy client list. If RUN_SSL is set to 1, initializes the SSL context and loads
 *            the certificate and private key. Proxy's master and read sets are initialized to
 *            zero.
 * Parameters: @proxy - pointer to proxy struct to initialize
 *             @port - port to listen on
 *             @cache_size - size of cache to initialize to
 *    Returns: EXIT_SUCCESS on success, or ERROR_FAILURE indicating type of failure
 */
int Proxy_init(struct Proxy *proxy, short port)
{
    if (proxy == NULL) {
        print_error("proxy: null parameter passed to init call");
        return ERROR_FAILURE;
    }

/* initialize the proxy cache */
#if RUN_CACHE
    proxy->cache = Cache_new(CACHE_SZ, Response_free, Response_print);
    if (proxy->cache == NULL) {
        print_error("proxy: cache init failed");
        return ERROR_FAILURE;
    }
#endif
#if RUN_FILTER
    Proxy_readFilterList(proxy);
#endif
    /* zero out the proxy addresses */
    zero(&(proxy->addr), sizeof(proxy->addr));

    /* initialize socket fds to -1 */
    proxy->listen_fd = -1;

    /* set the proxy port to given port */
    proxy->port = port;

    /* zero out fd_sets and initialize max fd of master_set and timeout */
    FD_ZERO(&(proxy->master_set));
    FD_ZERO(&(proxy->readfds));
    proxy->fdmax   = 0;
    proxy->timeout = NULL;

    /* initialize client list */
    proxy->client_list = List_new(Client_free, Client_print, Client_compare);
    if (proxy->client_list == NULL) {
        print_error("proxy: failed to initialize client list");
        return ERROR_FAILURE;
    }

/* initialize SSL Context */
#if RUN_SSL
    proxy->ctx = InitServerCTX();
    if (proxy->ctx == NULL) {
        print_error("proxy: failed to initialize SSL context");
        return ERROR_FAILURE;
    }

    /* load certificates */
    LoadCertificates(proxy->ctx, PROXY_CERT, PROXY_KEY);
#endif

    return EXIT_SUCCESS;
}

/** Proxy_free
 *     Purpose: Frees the memory allocated to the proxy struct. Closes the socket file descriptors
 *              and frees the cache and client list.
 *  Parameters: @proxy - pointer to proxy struct to free
 *     Returns: None
 */
void Proxy_free(void *proxy)
{
    if (proxy == NULL) {
        return;
    }

    struct Proxy *p = (struct Proxy *)proxy;

    /* free the SSL context */
#if RUN_SSL
    SSL_CTX_free(p->ctx);
#endif

    /* free the proxy */
#if RUN_CACHE
    Cache_free(&p->cache);
#endif

#if RUN_FILTER
    Proxy_freeFilters(p);
#endif

    List_free(&p->client_list);
    if (p->listen_fd != -1) {
        close(p->listen_fd);
        p->listen_fd = -1;
    }

    /* free the timeout */
    if (p->timeout != NULL) {
        free(p->timeout);
        p->timeout = NULL;
    }

}

/** Proxy_print
 *     Purpose: Prints the proxy struct to stderr
 *  Parameters: @proxy - pointer to proxy struct to print
 *     Returns: None
 */
void Proxy_print(struct Proxy *proxy)
{
    if (proxy == NULL) {
        return;
    }

    fprintf(stderr, "%s[Proxy]%s\n", BMAG, reset);
    fprintf(stderr, "  port = %d\n", proxy->port);
    fprintf(stderr, "  listen_fd = %d\n", proxy->listen_fd);
    List_print(proxy->client_list);
#if RUN_CACHE
    Cache_print(proxy->cache);
#endif
}

/** Proxy_close
 *     Purpose: Closes the given socket and removes it from the master fd_set, and removes the
 *              client from the client_list and master fd_set.
 * Parameters: @socket - the socket to close
 *             @master_set - the master fd_set
 *             @client_list - the list of clients
 *             @client - the client to remove
 *    Returns: None
 */
void Proxy_close(int socket, fd_set *master_set, List *client_list, Client *client)
{
    /* close this connection */
    fprintf(stderr, "%s[?] Closing Socket #%d%s\n", YEL, socket, reset);
    close(socket);

    fprintf(stderr, "%s[?] Removing Socket #%d from master_set%s\n", YEL, socket, reset);
    /* remove the client from master fd_set */
    FD_CLR(socket, master_set);
    if (client->query != NULL && client->query->socket != -1) {
        FD_CLR(client->query->socket, master_set);
    }

    /* remove the client from client_list */
    fprintf(stderr, "%s[?] Remove Socket #%d from client_list%s\n", YEL, socket, reset);
    List_remove(client_list, (void *)client);
}

/* ---------------------------------------------------------------------------------------------- */
/* PROXY SOCKET FUNCTIONS ----------------------------------------------------------------------- */
/* ---------------------------------------------------------------------------------------------- */

/** Proxy_listen
 *    Purpose: Sets the proxy's listening socket. Binds and listens on the port given at the command
 *             line. If successful, the listening socket will be added to the proxy's master set.
 * Parameters: A pointer to the proxy struct
 *    Returns: EXIT_SUCCESS when successful, otherwise ERROR_FAILURE
 */
int Proxy_listen(Proxy *proxy)
{
    if (proxy == NULL) {
        print_error("proxy_listen: listen failed, invalid args");
        return ERROR_FAILURE;
    }

    /* open listening socket */
    proxy->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy->listen_fd == -1) {
        print_error("proxy_listen: socket failed");
        Proxy_free(&proxy);
        return ERROR_FAILURE;
    }

    /* set socket options */
    int optval = 1;
    if (setsockopt(proxy->listen_fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(optval)) == -1) {
        print_error("proxy_listen: setsockopt failed");
        Proxy_free(&proxy);
        return ERROR_FAILURE;
    }

    /* build proxy address */
    bzero((char *)&(proxy->addr), sizeof(proxy->addr));
    proxy->addr.sin_family      = AF_INET;
    proxy->addr.sin_addr.s_addr = htonl(INADDR_ANY);
    proxy->addr.sin_port        = htons(proxy->port);

    /* bind listening socket to proxy address and port */
    if (bind(proxy->listen_fd, (struct sockaddr *)&(proxy->addr), sizeof(proxy->addr)) == -1) {
        print_error("proxy_listen: bind failed");
        Proxy_free(&proxy);
        return ERROR_FAILURE;
    }

    /* listen for connections */
    if (listen(proxy->listen_fd, 10) == -1) {
        print_error("proxy_listen: listen failed");
        Proxy_free(&proxy);
        return ERROR_FAILURE;
    }

    /* add listening socket to master set */
    FD_SET(proxy->listen_fd, &(proxy->master_set));
    proxy->fdmax = proxy->listen_fd;

    return EXIT_SUCCESS;
}

/** Proxy_accept
 *     Purpose: Accepts a connection request from a new client. Creates a new client object and
 *              adds the client's socket file descriptor to the master set. Updates the value of
 *              fdmax. The client socket is set to non-blocking. If RUN_SSL is set to 1, the client
 *              SSL object is created and the client is added to the client list, otherwise the
 *              client is only added to the client list.
 *  Parameters: @proxy - pointer to proxy struct
 *    Returns: EXIT_SUCCESS on success, ERROR_ACCEPT indicating failure to accept connection,
 *             ERROR_SSL indicating failure to create client SSL object, or ERROR_FAILURE indicating
 *             failure to set socket to non-blocking, or failure to add client to client list
 *
 * Notes:
 *      1. Accepts connection request from new client
 *      2. creates a client object for new client, make socket non-blocking
 *      3. puts new client's fd in master set
 *      4. update the value of fdmax
 */
int Proxy_accept(struct Proxy *proxy)
{
    if (proxy == NULL) {
        print_error("proxy: accept failed, invalid args");
        return ERROR_FAILURE;
    }
    /* accept connection request from new client */
    Client *c = Client_new();
    c->addr_l = sizeof(c->addr);
    c->socket = accept(proxy->listen_fd, (struct sockaddr *)&(c->addr), &(c->addr_l));
    if (c->socket == -1) {
        print_error("proxy: accept failed");
        Client_free(c);
        return ERROR_ACCEPT;
    }

    /* timestamp the client */
    Client_timestamp(c);

    /* set socket to non-blocking */ // ? set to non-blocking - why?
    // int flags = fcntl(c->socket, F_GETFL, 0);
    // if (flags == -1) {
    //     print_error("proxy: fcntl failed");
    //     Client_free(c);
    //     return ERROR_FAILURE;
    // }
    // flags |= O_NONBLOCK;
    // if (fcntl(c->socket, F_SETFL, flags) == -1) {
    //     print_error("proxy: fcntl failed");
    //     Client_free(c);
    //     return ERROR_FAILURE;
    // }

    /* add client to the proxy's client list */
    int ret = List_push_back(proxy->client_list, (void *)c);
    if (ret < 0) {
        Client_free(c);
        print_error("proxy: failed to add client to client list");
        return ERROR_FAILURE;
    }

    /* put new client's socket in master set */
    FD_SET(c->socket, &(proxy->master_set));

    /* 4. updates the max fd */
    proxy->fdmax = (c->socket > proxy->fdmax) ? c->socket : proxy->fdmax;

    return EXIT_SUCCESS;
}

/** Proxy_fetch
 *    Purpose: Connects to the client-requested server and forwards client's request.
 * Parameters: @proxy - the proxy struct
 *             @q - the query containing the server information we are fetcher from
 *    Returns: The number of bytes sent if successful, otherwise, ERROR_CONNECT is returned
 *             indicating an error connecting to the server has occurred, ERROR_FETCH is an error
 *             occurred sending the request to server, else ERROR_FAILURE
 */
ssize_t Proxy_fetch(Proxy *proxy, Query *q)
{
    if (proxy == NULL || q == NULL) {
        print_error("proxy_fetch: fetch: invalid arguments");
        return ERROR_FAILURE;
    }

    /* make connection to server */
    print_debug("proxy_fetch: making connection to server");
    if (connect(q->socket, (struct sockaddr *)&q->server_addr, sizeof(q->server_addr)) < 0) {
        print_error("proxy_fetch: failed to connect to server");
        perror("connect");
        return ERROR_BAD_GATEWAY;
    }
    print_debug("proxy_fetch: connected to server");

    /* add server socket to master_set */
    FD_SET(q->socket, &proxy->master_set);
    proxy->fdmax = (q->socket > proxy->fdmax) ? q->socket : proxy->fdmax;

    /* send message to server */
    ssize_t msgsize = 0;
    print_info("proxy_fetch: sending GET request to server");
    print_ascii(q->req->raw, q->req->raw_l);
    if (Proxy_send(q->socket, q->req->raw, q->req->raw_l) < 0) {
        print_error("proxy_fetch: send failed");
        return ERROR_FETCH;
    }


    return msgsize; // number of bytes sent
}

/** Proxy_send
 *    Purpose: Sends bytes from buffer to socket. Returns number of bytes sent.
 * Parameters: @socket - socket to send to
 *             @buffer - pointer to buffer to send
 *             @buffer_l - length of buffer
 *   Returns: number of bytes sent on success, or error code indicating type of failure
 */
ssize_t Proxy_send(int socket, char *buffer, size_t buffer_l)
{
    if (buffer == NULL) {
        print_error("proxy_send: buffer is null ");
        return ERROR_FAILURE;
    }

    // fprintf(stderr, "[Proxy_Send] we are sending: \n%s\n", buffer);
    fprintf(stderr, "[Proxy_Send] sending non-null buffer of size: %ld.\n", buffer_l);

    /* write request to server */
    ssize_t written = 0, n = 0, to_write = buffer_l;
    while ((size_t)written < buffer_l) {
        n = send(socket, buffer + written, to_write, MSG_NOSIGNAL);
        if (n <= 0) {
            print_error("proxy_send: connection reset by peer");
            return ERROR_CLOSE;
        } else {
            written += n;
            to_write -= n;
            fprintf(stderr, "[Proxy_send] Bytes sent: %ld\n", n);
            fprintf(stderr, "Bytes To Write: %ld\n", to_write);
            fprintf(stderr, "Bytes Written: %ld\n", written);
            fprintf(stderr, "%s[END PROXY SEND]%s\n", GRN, reset);
        }
    }

    return written;
}

ssize_t Proxy_recv(void *sender, int sender_type)
{
    if (sender == NULL) {
        print_error("proxy_recv: buffer is null ");
        return ERROR_FAILURE;
    }

    Query *q;
    Client *c;
    int n;
    switch (sender_type) {
    case QUERY_TYPE:
        q = (Query *)sender;
        n = recv(q->socket, q->buffer + q->buffer_l, q->buffer_sz - q->buffer_l, 0);
        if (n == 0) {
            q->res = Response_new(q->req->method, q->req->method_l, q->req->path, q->req->path_l, q->buffer, q->buffer_l);
            if (q->res == NULL) {
                print_error("proxy: failed to create response");
                return INVALID_RESPONSE;
            }
            q->state = QRY_RECVD_RESPONSE;
            return SERVER_RESP_RECVD;
        } else if (n < 0) {
            print_error("proxy: recv failed");
            perror("recv");
            return ERROR_RECV;
        } else {
            q->buffer_l += n;
            if (q->buffer_l == q->buffer_sz) {
                expand_buffer(&q->buffer, &q->buffer_l, &q->buffer_sz);
            }
        }
        break;
    case CLIENT_TYPE:
        c = (Client *)sender;
        fprintf(stderr, "[Proxy-recv] Receiving from client: %d\n", c->socket);
        fprintf(stderr, "[Proxy-recv] Client buffer size: %ld\n", c->buffer_sz);
        fprintf(stderr, "[Proxy-recv] Client buffer length: %ld\n", c->buffer_l);
        fprintf(stderr, "Bytes To Read: %ld\n", c->buffer_sz - c->buffer_l);
        n = recv(c->socket, c->buffer + c->buffer_l, c->buffer_sz - c->buffer_l, 0);
        if (n == 0) {
            print_info("proxy: client closed connection");
            return EXIT_SUCCESS;
        } else if (n < 0) {
            print_error("proxy: recv failed");
            perror("recv");
            return ERROR_RECV;
        } else {
            c->buffer_l += n;
            if (c->buffer_l == c->buffer_sz) {
                if (expand_buffer(&c->buffer, &c->buffer_l, &c->buffer_sz) < 0) {
                    print_error("proxy: failed to expand buffer");
                    return ERROR_FAILURE;
                }
            }
        }

        /* update last active time to now */
        Client_timestamp(c);
        break;
    }

    return EXIT_SUCCESS;
}

/* Proxy_sendError
 *    Purpose: Send error message to client
 * Parameters: @client - client to send error message to
 *             @msg_code - error code indicating which message to send
 *   Returns: EXIT_SUCCESS on success, ERROR_FAILURE when encountering an unexpected error code
 */
int Proxy_sendError(Client *client, int msg_code)
{
    if (client == NULL) {
        return EXIT_SUCCESS;
    }

    switch (msg_code) {
    case BAD_REQUEST_400:
        return Proxy_send(client->socket, STATUS_400, STATUS_400_L);
    case NOT_FOUND_404:
        return Proxy_send(client->socket, STATUS_404, STATUS_404_L);
    case METHOD_NOT_ALLOWED_405:
        return Proxy_send(client->socket, STATUS_405, STATUS_405_L);
    case PROXY_AUTH_REQUIRED_407:
        return Proxy_send(client->socket, STATUS_407, STATUS_407_L);
    case INTERNAL_SERVER_ERROR_500:
        return Proxy_send(client->socket, STATUS_500, STATUS_500_L);
    case NOT_IMPLEMENTED_501:
        return Proxy_send(client->socket, STATUS_501, STATUS_501_L);
    case BAD_GATEWAY_502:
        return Proxy_send(client->socket, STATUS_502, STATUS_502_L);
    case FORBIDDEN_403:
        return Proxy_send(client->socket, STATUS_403, STATUS_403_L);
    case IM_A_TEAPOT_418:
        return Proxy_send(client->socket, STATUS_418, STATUS_418_L);
    default: // Internal Server Error
        fprintf(stderr, "%s[!]%s proxy: unknown error code: %d\n", RED, reset, msg_code);
        return Proxy_send(client->socket, STATUS_500, STATUS_500_L);
    }

    return EXIT_SUCCESS;
}

/* ---------------------------------------------------------------------------------------------- */
/* PROXY SSL FUNCTIONS -------------------------------------------------------------------------- */
/* ---------------------------------------------------------------------------------------------- */
#if RUN_SSL

int ProxySSL_shutdown(Proxy *proxy, Client *client)
{
    if (proxy == NULL || client == NULL) {
        return ERROR_FAILURE;
    }

    int ret = SSL_shutdown(client->ssl);
    if (ret == 0) {
        ret = SSL_shutdown(client->ssl);
    }
    if (ret == -1) {
        print_error("proxy: failed to shutdown ssl");
        return ERROR_SSL;
    }

    return EXIT_SUCCESS;
}

int ProxySSL_connect(Proxy *proxy, Query *query)
{
    if (proxy == NULL || query == NULL) {
        print_error("[Proxy_SSLconnect]: invalid arguments");
        return ERROR_FAILURE;
    }

    fprintf(stderr, "[Proxy_SSLconnect]: initializing SSL context...\n");
    query->server_addr.sin_port = htons(atoi(query->req->port)); // tODO - need to change port 443
    if (ntohs(query->server_addr.sin_port) != 443) {
        query->server_addr.sin_port = htons(443);
    }

    /* TCP Connect */
    print_debug("[Proxy_SSLconnect]: connecting to server...");
    int conn = connect(query->socket, (struct sockaddr *)&query->server_addr, sizeof(query->server_addr));
    if (conn < 0) {
        print_error("[Proxy_SSLconnect]: failed to connect to server");
        perror("connect");
        return ERROR_CONNECT;
    }
    print_debug("[Proxy_SSLconnect]: connected to server");
    FD_SET(query->socket, &proxy->master_set);
    proxy->fdmax = (query->socket > proxy->fdmax) ? query->socket : proxy->fdmax;

    /* Initialize SSL Context */
    query->ctx = InitCTX();
    LoadCertificates(query->ctx, PROXY_CERT, PROXY_KEY);
    query->ssl = NULL;
    query->ssl = SSL_new(query->ctx);
    SSL_set_fd(query->ssl, query->socket);

    /* SSL/TLS Handshake with Server */
    /* perform connection */
    if (SSL_connect(query->ssl) < 0) {
        fprintf(stderr, "[Proxy_SSLconnect]: SSL_connect() failed\n");
        ERR_print_errors_fp(stderr);
        return ERROR_CLOSE;
    } else {
        fprintf(stderr, "[Proxy_SSLconnect]: SSL/TLS connection established!\n");
    }

    /* send request */
    int ret = SSL_write(query->ssl, query->req->raw, query->req->raw_l);
    if (ret < 0) {
        print_error("[Proxy_SSLconnect]: SSL_write failed");
        return ERROR_SSL;
    } else if (ret == 0) {
        print_error("[Proxy_SSLconnect]: SSL_write failed, connection closed");
        return ERROR_SSL;
    }

    /* sent request successfully */
    print_debug("[Proxy_SSLconnect]: SSL_write successful");
    fprintf(stderr, "[Proxy_SSLconnect]: sent request to server of %d bytes\n", ret);

    return (ret > 0) ? EXIT_SUCCESS : ERROR_FAILURE;
}

/*
 */
int ProxySSL_read(void *sender, int sender_type)
{
    if (sender == NULL) {
        print_error("proxy_sslread: invalid args");
        return ERROR_FAILURE;
    }

    Query *q;
    Client *c;
    ssize_t n;
    switch (sender_type) {
    case QUERY_TYPE:
        fprintf(stderr, "[ProxySSL_read] QUERY matched on QUERY_TYPE\n");
        q = (Query *)sender;
        n = SSL_read(q->ssl, q->buffer + q->buffer_l, q->buffer_sz - q->buffer_l);
        fprintf(stderr, "[ProxySSL_read] QUERY SSL_read returned %ld\n", n);
        print_ascii(q->buffer + q->buffer_l, n);
        if (n <= 0) {
            print_error("proxy: recv failed");
            perror("recv");
            return ERROR_SSL;

        } else if (n < (ssize_t)(q->buffer_sz - q->buffer_l)) {
            q->buffer_l += n;
            q->res =
                Response_new(q->req->method, q->req->method_l, q->req->path, q->req->path_l, q->buffer, q->buffer_l);
            if (q->res == NULL) {
                print_error("proxy: failed to create response");
                return INVALID_RESPONSE;
            }
            q->state = QRY_RECVD_RESPONSE;
            return RESPONSE_RECEIVED;
        } else {
            q->buffer_l += n;
            if (q->buffer_l == q->buffer_sz) {
                expand_buffer(&q->buffer, &q->buffer_l, &q->buffer_sz);
            }
        }
        break;
    case CLIENT_TYPE:
        c = (Client *)sender;
        fprintf(stderr, "[ProxySSL_read] CLIENT matched on CLI_GET\n");
        fprintf(stderr, "buffer_l: %ld\n", c->buffer_l);
        fprintf(stderr, "buffer_sz: %ld\n", c->buffer_sz);
        n = SSL_read(c->ssl, c->buffer + c->buffer_l, c->buffer_sz - c->buffer_l);
        if (n == 0) {
            print_info("proxy: client closed connection");
            return CLIENT_CLOSE;
        } else if (n < 0) {
            print_error("proxy: recv failed");
            perror("recv");
            return ERROR_SSL;
        } else {
            c->buffer_l += n;
            if (c->buffer_l == c->buffer_sz) {
                expand_buffer(&c->buffer, &c->buffer_l, &c->buffer_sz);
            }
        }

        /* add null terminator */
        c->buffer[c->buffer_l] = '\0';

        /* update last active time to now */
        Client_timestamp(c);
        break;
    default:
        print_error("proxy_sslread: invalid sender type");
        return ERROR_FAILURE;
    }

    return n;

    // char recv_buffer[BUFFER_SZ + 1];
    // zero(recv_buffer, BUFFER_SZ + 1);

    // int ret, read  = 0;
    // ret = SSL_read(client->ssl, recv_buffer, BUFFER_SZ);
    // if (ret < 0) {
    //     print_error("proxy_sslread: SSL_read failed");
    //     return ERROR_SSL;
    // } else if (ret == 0) {
    //     print_error("proxy_sslread: SSL_read failed, connection closed");
    //     return ERROR_SSL;
    // } else {
    //     read += ret;
    // }

    // client->buffer = realloc(client->buffer, client->buffer_l + read + 1);
    // if (client->buffer == NULL) {
    //     print_error("proxy_sslread: realloc failed");
    //     return ERROR_FAILURE;
    // }
    // memcpy(client->buffer + client->buffer_l, recv_buffer, read);
    // client->buffer_l += read;
    // client->buffer[client->buffer_l] = '\0';
    // return read;
}

int ProxySSL_write(Proxy *proxy, Client *client, char *buf, int len)
{
    if (proxy == NULL || client == NULL || buf == NULL || len < 0) {
        print_error("proxy_sslwrite: invalid args");
        return ERROR_FAILURE;
    }
    fprintf(stderr, "len: %d\n", len);
    print_ascii(buf, len);
    fprintf(stderr, "[ProxySSL_write] Writing to client: %d\n", client->socket);
    ssize_t written  = 0;
    ssize_t to_write = len;
    int ret;
    while (to_write > 0) {
        ret = SSL_write(client->ssl, buf + written, to_write);
        if (ret <= 0) {
            int err = SSL_get_error(client->ssl, ret);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                continue;
            }
            print_error("proxy_sslwrite: SSL_write failed");
            return ERROR_FAILURE;
        } else {
            to_write -= ret;
        }
    }

    return written;
}

int ProxySSL_handshake(Proxy *proxy, Client *client)
{
    if (proxy == NULL) {
        print_error("proxy: accept failed, invalid args");
        return ERROR_FAILURE;
    }

    /* configure context */
    char *hostname = client->query->req->host;
    if (hostname == NULL) {
        return INVALID_REQUEST;
    }

    /* update proxy ext file ad context if needed */
    if (ProxySSL_updateExtFile(proxy, hostname) != EXIT_SUCCESS) {
        return ERROR_FAILURE;
    }

    /* create new SSL object for client */
    fprintf(stderr, "[ProxySSL-accept] Checking for a SSL/TLS connection...\n");
    client->ssl = SSL_new(proxy->ctx);
    SSL_set_fd(client->ssl, client->socket);

    /* accept */
    if (SSL_accept(client->ssl) == -1) {
        ERR_print_errors_fp(stderr);
        fprintf(stderr, "[ProxySSL-accept] SSL/TLS connection failed!\n");
        /* stop SSL */
        SSL_shutdown(client->ssl);
        SSL_free(client->ssl);
        client->ssl   = NULL;
        client->isSSL = 0;
        return ERROR_SSL;
    } else {
        fprintf(stderr, "[ProxySSL-accept] SSL/TLS connection established!\n");
        client->isSSL = 1;
        client->state = CLI_QUERY;
        Query_free(client->query);
        client->query = NULL;
        clear_buffer(client->buffer, &client->buffer_l);
    }

    fprintf(stderr, "[ProxySSL-accept] SSL/TLS connection established!\n");
    /* update last active time to now */
    Client_timestamp(client);

    return EXIT_SUCCESS;
}

int ProxySSL_updateExtFile(Proxy *proxy, char *hostname)
{
    if (proxy == NULL || hostname == NULL) {
        print_error("proxy: updateExtFile failed, invalid args");
        return ERROR_FAILURE;
    }

    int ret;

    /* update proxy ext file */
    char command[BUFFER_SZ + 1];
    zero(command, BUFFER_SZ + 1);
    snprintf(command, BUFFER_SZ, "%s %s", UPDATE_PROXY_CERT, hostname);
    if ((ret = system(command)) < 0) {
        print_error("proxy: updateExtFile failed, system call failed");
        return ERROR_FAILURE;
    } else if (ret > 0) {
        /* update proxy context */
        if (ProxySSL_updateContext(proxy) == ERROR_FAILURE) {
            print_error("proxy: updateExtFile failed, updateContext failed");
            return ERROR_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

int ProxySSL_updateContext(Proxy *proxy)
{
    if (proxy == NULL) {
        print_error("proxy: updateContext failed, invalid args");
        return ERROR_FAILURE;
    }

    /* update proxy context */
    SSL_CTX_free(proxy->ctx);
    proxy->ctx = InitServerCTX();
    if (proxy->ctx == NULL) {
        print_error("proxy: updateContext failed, InitServerCTX failed");
        return ERROR_FAILURE;
    }

    if (LoadCertificates(proxy->ctx, PROXY_CERT, PROXY_KEY) == ERROR_FAILURE) {
        print_error("proxy: updateContext failed, LoadCertificates failed");
        return ERROR_FAILURE;
    }

    return EXIT_SUCCESS;
}
#endif

int Proxy_handleGET(Proxy *proxy, Client *client)
{
    int ret = EXIT_SUCCESS;
    if (client->query->state == QRY_INIT) { // check cache, if sent request, already checked cache
        fprintf(stderr, "[Proxy-handleGET] CLIENT matched on CLI_GET\n");
#if RUN_CACHE
        char *key = get_key(client->query->req);
        if (key == NULL) {
            print_error("[Proxy-handleGET]: failed to get key");
            return ERROR_FAILURE;
        }

        long cache_res_age = Cache_get_age(proxy->cache, key);
        if (cache_res_age >= 0) { // cache hit
            fprintf(stderr, "[Proxy-handleGET]: cache hit!\n");
            ret = Proxy_serveFromCache(proxy, client, cache_res_age, key);
            if (ret < 0) {
                free(key);
                print_error("[Proxy-handleGET]: failed to serve from cache");
                return ret;
            }
            free(key);
            client->query->state = QRY_DONE;
        } 
        else
        { // cache miss
            free(key);
#endif
            fprintf(stderr, "[Proxy-handleGET]: cache miss!\n");
            /* if in HTTPS mode use TLS/SSL otherwise do a regular fetch */
#if RUN_SSL
            fprintf(stderr, "client->isSSL = %d\n", client->isSSL);
            ret = (client->isSSL) ? ProxySSL_connect(proxy, client->query) : Proxy_fetch(proxy, client->query);
#else
            ret = Proxy_fetch(proxy, client->query);
#endif
            if (ret < 0) {
                print_error("[Proxy-handleGET]: fetch failed");
#if RUN_SSL
                if (client->isSSL) {
                    /* stop SSL */
                    SSL_shutdown(client->ssl);
                    SSL_free(client->ssl);
                    client->ssl   = NULL;
                    client->isSSL = 0;
                }
#endif
                return ret;
            }

            client->query->state = QRY_SENT_REQUEST;
#if RUN_CACHE
        }
#endif
    } else if (FD_ISSET(client->query->socket, &proxy->readfds)) { // query server
        fprintf(stderr, "[Proxy-handleGET] QUERY matched on CLI_GET\n");
        if (client->query->state == QRY_SENT_REQUEST) {
#if RUN_SSL
            ret = Proxy_handleQuery(proxy, client->query, client->isSSL);
#else
            ret = Proxy_handleQuery(proxy, client->query, 0);
#endif
            if (ret < 0) {
                print_error("[Proxy-handleGET]: handle server query failed");
                return ret;
            }
        }

        if (client->query->state == QRY_RECVD_RESPONSE) { // Non-Persistent
            fprintf(stderr, "[Proxy-handleGET]: response received...\n");
            ret = Proxy_sendServerResp(proxy, client);
            fprintf(stderr, "proxy-sendServerRsp ret: %d\n", ret);
        }
    }


    fprintf(stderr, "[Proxy-handleGET] QUERY state: %d\n", client->query->state);
    fprintf(stderr, "[Proxy-handleGET] handleQUERY ret: %d\n", ret);

    return ret;
}



/* ---------------------------------------------------------------------------------------------- */
/* --------------------------------------- PROXY HANDLERS --------------------------------------- */
/* ---------------------------------------------------------------------------------------------- */

/** Proxy_handle
 */
int Proxy_handle(Proxy *proxy)
{
    int ret        = EXIT_SUCCESS;
    Client *client = NULL;
    Node *curr = NULL, *next = NULL;
    for (curr = proxy->client_list->head; curr != NULL; curr = next) {
        next   = curr->next;
        client = (Client *)curr->data;

        if (client->state == CLI_QUERY) {
            if (FD_ISSET(client->socket, &proxy->readfds)) {
                fprintf(stderr, "[Proxy_handle] socket %d matched on CLI_QUERY\n", client->socket);
                ret = Proxy_handleClient(proxy, client);
                if (ret < 0) {
                    fprintf(stderr, "[Proxy_handle] client %d failed\n", client->socket);
                    goto event_handler;
                }
            }
        }

        switch (client->state) {
        case CLI_GET:
            print_info("proxy: client is in GET state");
            ret = Proxy_handleGET(proxy, client);
            break;
        case CLI_CONNECT: // CONNECT tunneling
            print_info("proxy: client is in CONNECT state");
            ret = Proxy_handleCONNECT(proxy, client);
            break;
#if RUN_SSL
        case CLI_SSL:
            print_info("proxy: client is in SSL state");
            if (FD_ISSET(client->socket, &proxy->readfds)) {
                ret = ProxySSL_handshake(proxy, client);
            }
            break;
#endif
        case CLI_TUNNEL:
            if (client->query->state == QRY_INIT) {
                /* change server address socket port to 443 */
                client->query->server_addr.sin_port = htons(443);
                /* connect to server */
                ret = connect(client->query->socket, (struct sockaddr *)&client->query->server_addr, sizeof(client->query->server_addr));
                if (ret < 0) {
                    print_error("[Proxy_handle] failed to connect to server");
                    return ERROR_FAILURE;
                } else {
                    /* add query socket to master set */
                    FD_SET(client->query->socket, &proxy->master_set);
                    print_info("[Proxy_handle] connected to server");
                    /* set client and query state to tunneling */
                    client->query->state = QRY_TUNNEL;
                    client->state        = CLI_TUNNEL;
                }
            }
            print_info("proxy: client is in TUNNEL state");
            if (FD_ISSET(client->socket, &proxy->readfds)) {
                fprintf(stderr, "[Proxy_handle] client %d sending\n", client->socket);
                ret = Proxy_handleTunnel(client->socket, client->query->socket);
                /* update last active time to now */
                Client_timestamp(client);
            }

            if (FD_ISSET(client->query->socket, &proxy->readfds)) {
                fprintf(stderr, "[Proxy_handle] server %d sending\n", client->socket);
                ret = Proxy_handleTunnel(client->query->socket, client->socket);
                /* update last active time to now */
                Client_timestamp(client); // ? TODO - is this necessary?
            }
            break;
        }

    event_handler:
        /* checking for events/errors */
        if (ret != EXIT_SUCCESS) {
            ret = Proxy_handleEvent(proxy, client, ret);
            if (ret != EXIT_SUCCESS) {
                return ret;
            }
        }
    }

    /* handling query socket (connection with server) */
#if RUN_CACHE
    Cache_refresh(proxy->cache);
#endif
    return EXIT_SUCCESS;
}

int Proxy_sendServerResp(Proxy *proxy, Client *client)
{
    if (proxy == NULL || client == NULL) {
        return ERROR_FAILURE;
    }

    /* Color links in the response */
    char *response_buffer = calloc(client->query->res->raw_l + 1, sizeof(char));
    size_t response_sz    = client->query->res->raw_l;
    memcpy(response_buffer, client->query->res->raw, response_sz);

#if (RUN_COLOR)
    char **key_array = Cache_getKeyList(proxy->cache);
    int num_keys     = (int)proxy->cache->size;
    if (color_links(&response_buffer, &response_sz, key_array, num_keys) != 0) {
        fprintf(stderr, "%s[Proxy_handle]: couldn't color hyperlinks.%s\n", RED, reset);
    } else {
        fprintf(stderr, "%s[Proxy_handle]: colored hyperlinks.%s\n", RED, reset);
    }
#endif
#if RUN_SSL
    if (client->isSSL) {
        fprintf(stderr, "%s[Proxy_handle]: sending SSL response to client %d%s\n", YEL, client->socket, reset);
        if (ProxySSL_write(proxy, client, response_buffer, response_sz) <= 0) {
            print_error("proxy: failed to send response to client");
            free(response_buffer);
            return ERROR_SSL;
        } else {
            free(response_buffer);
        }
    } 
    else 
#endif
    {
        fprintf(stderr, "%s[Proxy_handle]: sending response to client %d%s\n", YEL, client->socket, reset);
        if (Proxy_send(client->socket, response_buffer, response_sz) < 0) {
            print_error("proxy: failed to send response to client");
            free(response_buffer);
            return ERROR_SEND;
        } else {
            free(response_buffer);
        }
    }

    fprintf(stderr, "%s[Proxy_handle]: sent response to client %d%s\n", YEL, client->socket, reset);

#if RUN_SSL
    client->state = (client->isSSL) ? CLI_QUERY : CLI_TUNNEL;
#else
    client->state = CLI_TUNNEL;
#endif

    /* store copy the response in cache so it is available even after client closes */
#if RUN_CACHE
    char *key            = get_key(client->query->req);
    Response *cached_res = Response_copy(client->query->res);
    int ret              = Cache_put(proxy->cache, key, cached_res, cached_res->max_age);
    print_debug("proxy_handle: response cached");
    fprintf(stderr, "%s[?] Cache_put returned %d%s\n", YEL, ret, reset);
    if (ret < 0) {
        print_error("proxy: failed to cache response");
        free(key);
        return ERROR_FAILURE;
    } else {
        fprintf(stderr, "%s[Proxy_handle]: cached response 1%s\n", YEL, reset);
        client->state        = CLI_QUERY; // todo this might be slightly too early
        client->query->state = QRY_INIT;
        FD_CLR(client->query->socket, &proxy->readfds);
        FD_CLR(client->query->socket, &proxy->master_set);
        // Query_free(client->query);
        // client->query = NULL;
        // ret = CLIENT_CLOSE; // TODO - check if client is persistent
        free(key);
    }
#endif

    return EXIT_SUCCESS;
}

/* Proxy_handleListener
 *    Purpose: handles the listener socket and accepts new connections
 * Parameters: @proxy - the proxy with listener socket ready to accept
 *    Returns: 0 on success, -1 on failure
 */
int Proxy_handleListener(struct Proxy *proxy)
{
    Proxy_accept(proxy);
    return EXIT_SUCCESS;
}

/** Proxy_handleClient
 *    Purpose: Handles a client socket. Receives request from the client and stores it in the
 *             client's buffer. If the client has sent a complete request, a new Query object is
 *             created and stored in the client's query field. The request is parsed for the type
 *             of request and the appropriate handler is called.
 */
int Proxy_handleClient(Proxy *proxy, Client *client)
{
    (void)proxy;
    if (client == NULL) {
        return ERROR_FAILURE;
    }

    /* Receive request from client -------------------------------------------------------------- */
    int ret;
#if RUN_SSL
    if (client->isSSL == 1) 
    {
        fprintf(stderr, "[Proxy_handle] ssl client %d receiving\n", client->socket);
        ret = ProxySSL_read(client, CLIENT_TYPE);
        if (ret <= 0) {
            fprintf(stderr, "[Proxy_handle] ssl client %d closed connection\n", client->socket);
            return ERROR_CLOSE;
        }
    } 
    else
#endif 
    {
        fprintf(stderr, "[Proxy_handle] client %d receiving\n", client->socket);
        ret = Proxy_recv(client, CLIENT_TYPE);
        if (ret < 0) {
            fprintf(stderr, "[Proxy_handle] client %d closed connection\n", client->socket);
            return ERROR_CLOSE;
        }
    }

    /* Check for header  ------------------------------------------------------------------------ */
    if (HTTP_got_header(client->buffer)) {
        print_debug("proxy: got header");
        ret = Query_new(&(client->query), client->buffer, client->buffer_l);
        if (ret != EXIT_SUCCESS) {
            print_error("proxy: failed to parse request");
            return ret;
        }

        #if RUN_FILTER
        if (Proxy_isFiltered(proxy, client->query->req->host)) {
            print_error("proxy: request blocked by filter");
            return ERROR_FILTERED;
        }
        #endif

        /* assign state to client */
        if (memcmp(client->query->req->method, CONNECT, CONNECT_L) == 0) {
            client->state = CLI_CONNECT;
            print_info("proxy: got CONNECT request -- sending 200 OK response");
        } else if (memcmp(client->query->req->method, GET, GET_L) == 0) {
            client->state = CLI_GET;
            print_info("proxy: got GET request");
        } else if (memcmp(client->query->req->method, POST, POST_L) == 0) {
            client->state = CLI_POST;
            print_info("proxy: got POST request");
        } else {
            print_error("proxy: unsupported request method");
            return INVALID_REQUEST;
        }
    }

    return EXIT_SUCCESS;
}

/** Proxy_handleQuery
 * Purpose: Handles communication when a client made a GET request. Receives bytes from
 *          query->socket. Creates a Response object onces recv'd all bytes from sender. Caller's
 *          responsibility to handle Response, i.e. caching and forwarding.
 * Parameters: @proxy - pointer to Proxy object
 *             @query - pointer to Query object
 * Returns: EXIT_SUCCESS on success, ERROR_FAILURE on failure
 *
 * Note: Caller is only Proxy_handle
 */
int Proxy_handleQuery(Proxy *proxy, Query *query, int isSSL)
{
    if (proxy == NULL || query == NULL) {
        return ERROR_FAILURE;
    }

    fprintf(stderr, "BUFFER_SZ = %ld BUFFER_L = %ld\n", query->buffer_sz, query->buffer_l);
    ssize_t n = 0;
    /* read from socket */
    if (isSSL) {
#if RUN_SSL
        n = ProxySSL_read(query, QUERY_TYPE);
        if (n <= 0) {
            print_error("proxy: failed to receive bytes from server");
            return ERROR_CLOSE;
        }
#endif 
    } else {
        n = Proxy_recv(query, QUERY_TYPE);
        if (n < 0) {
            print_error("proxy: failed to receive bytes from server");
            return ERROR_CLOSE;
        }
    }
    
    fprintf(stderr, "Received %ld bytes\n", n);
    return EXIT_SUCCESS;
}

/** Proxy_handleCONNECT
 *    Purpose: Handles CONNECT tunneling - takes byte stream from sender socket and forwards to
 *             receiver socket without parsing or inspecting bytes.
 * Parameters: @sender - socket to read bytes from
 *             @receiver - socket to write bytes to
 *    Returns: EXIT_SUCCESS on success, or error code indicating type of failure
 */
int Proxy_handleCONNECT(Proxy *proxy, Client *client)
{
    if (proxy == NULL || client == NULL || client->query == NULL) {
        return ERROR_FAILURE;
    }

    int ret;
    if (RUN_SSL) {
        ret = Proxy_send(client->socket, STATUS_200, STATUS_200_L);
        if (ret != STATUS_200_L) {
            print_error("proxy: failed to send 200 OK");
            return ERROR_SEND;
        }
        client->state = CLI_SSL;
    } else {
        /* make connection with server */
        ret = Proxy_send(client->socket, STATUS_200CE, STATUS_200CE_L);
        if (ret != STATUS_200CE_L) {
            print_error("proxy: failed to send 200 OK");
            return ERROR_SEND;
        }
        client->state = CLI_TUNNEL;
    }

    fprintf(stderr, "client->state = %d\n", client->state);

    return EXIT_SUCCESS;
}


int Proxy_handleEvent(Proxy *proxy, Client *client, int error_code)
{
    fprintf(stderr, "[Event_Handle] ErrorCode = %d\n", error_code); // TODO - remove
    if (proxy == NULL) {
        return ERROR_FAILURE;
    }

    switch (error_code) {
    case ERROR_FAILURE:
        print_error("proxy: failed");
        Proxy_sendError(client, INTERNAL_SERVER_ERROR_500);
        return ERROR_FAILURE;
    case HALT:
        print_info("proxy: halt signal received");
        Proxy_sendError(client, IM_A_TEAPOT_418);
        return HALT;
    case ERROR_SEND:
        print_error("proxy: failed to send message, closing connection");
        Proxy_close(client->socket, &proxy->master_set, proxy->client_list, client);
        break;
    case ERROR_SSL:
        print_error("proxy: failed to establish SSL connection");
        Proxy_sendError(client, INTERNAL_SERVER_ERROR_500);
        Proxy_close(client->socket, &proxy->master_set, proxy->client_list, client);
        break;
    case INVALID_REQUEST:
        print_error("proxy: invalid request");
        Proxy_sendError(client, BAD_REQUEST_400);
        Proxy_close(client->socket, &proxy->master_set, proxy->client_list,
                    client); // ? - close for persistent connections?
        break;
    case PROXY_AUTH_REQUIRED:
        print_error("proxy: invalid method in request");
        Proxy_sendError(client, PROXY_AUTH_REQUIRED_407);
        Proxy_close(client->socket, &proxy->master_set, proxy->client_list,
                    client); // ? - close for persistent connections?
        break;
    case ERROR_FILTERED:
        print_error("proxy: forbidden request");
        Proxy_sendError(client, FORBIDDEN_403);
        Proxy_close(client->socket, &proxy->master_set, proxy->client_list,
                    client); // ? - close for persistent connections?
        break;
    case HOST_UNKNOWN:
        print_error("proxy: invalid request");
        Proxy_sendError(client, BAD_REQUEST_400);
        Proxy_close(client->socket, &proxy->master_set, proxy->client_list,
                    client); // ? - close for persistent connections?
        break;
    case ERROR_BAD_METHOD:
        print_error("proxy: invalid method in request");
        Proxy_sendError(client, METHOD_NOT_ALLOWED_405);
        Proxy_close(client->socket, &proxy->master_set, proxy->client_list,
                    client); // ? - close for persistent connections?
        break;
    case ERROR_BAD_GATEWAY:
        print_error("proxy: invalid method in request");
        Proxy_sendError(client, BAD_GATEWAY_502);
        Proxy_close(client->socket, &proxy->master_set, proxy->client_list,
                    client); // ? - close for persistent connections?
        break;
    case CLIENT_CLOSE:
        fprintf(stderr, "%s[+] proxy: closing client %d%s\n", GRN, client->socket, reset);
        Proxy_close(client->socket, &proxy->master_set, proxy->client_list, client);
        break;
    case ERROR_CLOSE:
        print_error("proxy: [-] closing client due to error");
        Proxy_close(client->socket, &proxy->master_set, proxy->client_list, client);
        break;
    case ERROR_CONNECT:
        print_warning("proxy: failed to accept new connection");
        break;
    default:
        fprintf(stderr, "%s[!]%s proxy: unknown error: %d\n", RED, reset, error_code);
        return ERROR_FAILURE;
    }

    return EXIT_SUCCESS;
}

/** Proxy_handleTimeout
 *      Purpose: Sets the select timeout to the time until the next client times out, or NULL if no
 *               clients are present. If the timeout is already set to the correct value, it is not
 *               changed. If the timeout is set to a different value, it is updated. If the timeout
 *               is NULL, it timeval is allocated and set to the correct value. If the timeout is
 *               greater than the threshold, the client is removed from the list - this is to
 *               prevent a client from being in the list forever.
 *   Parameters: @proxy - a pointer to the proxy object
 *     Returns: TIMEOUT_FALSE if the timeout is NULL, TIMEOUT_TRUE if the timeout is set, or
 *              ERROR_FAILURE if an error occurs
 */
int Proxy_handleTimeout(struct Proxy *proxy)
{
    if (proxy == NULL) {
        return ERROR_FAILURE;
    }

    double age, time_til_timeout, now = get_current_time(), timeout = TIMEOUT_THRESHOLD;
    Node *client_node = List_head(proxy->client_list);
    while (client_node != NULL) {
        Client *client = (Client *)client_node->data;
        client_node    = List_next(client_node);
        // if (client->state == CLI_PERSISTENT) {  // ? skip persistent clients
        //     continue;
        // }
        fprintf(stderr, "%s[+] proxy: client %d now: %f%s\n", GRN, client->socket, now, reset);
        age = now - timeval_to_double(client->last_active);
        fprintf(stderr, "%s[+] proxy: client %d age: %f%s\n", GRN, client->socket, age, reset);
        if (age > TIMEOUT_THRESHOLD) { // client timed out
            Proxy_close(client->socket, &proxy->master_set, proxy->client_list, client);
        } else {
            time_til_timeout = TIMEOUT_THRESHOLD - age;
            if (time_til_timeout < timeout) {
                timeout = time_til_timeout;
            }
        }
    }

    if (timeout == TIMEOUT_THRESHOLD) { // no clients - timeout is NULL
        fprintf(stderr, "%s[+] proxy: min timeout equal to threshold - timeout is NULL%s\n", GRN, reset);
        if (proxy->timeout != NULL) {
            free(proxy->timeout);
            proxy->timeout = NULL;
        }
        return TIMEOUT_FALSE;
    }

    /* we have a timeout value */
    fprintf(stderr, "%s[+] proxy: timeout set to %f%s\n", GRN, timeout, reset);
    if (proxy->timeout == NULL) {
        proxy->timeout = calloc(1, sizeof(struct timeval));
    }
    double_to_timeval(proxy->timeout, timeout);
    fprintf(stderr, "%s[+] proxy: timeout set to %ld.%ld%s\n", GRN, proxy->timeout->tv_sec, proxy->timeout->tv_usec,
            reset);

    return TIMEOUT_TRUE;
}

/* serve from cache */
#if RUN_CACHE
int Proxy_serveFromCache(Proxy *proxy, Client *client, long age, char *key)
{
    if (proxy == NULL || client == NULL || age < 0) {
        return ERROR_FAILURE;
    }

    char age_str[MAX_DIGITS_LONG + 1];
    zero(age_str, MAX_DIGITS_LONG + 1);
    snprintf(age_str, MAX_DIGITS_LONG, "%ld", age);
    Response *response = Cache_get(proxy->cache, key);
    print_debug("proxy: serving from cache");

    /* add 'Age' field to HTTP header */
    size_t response_size = Response_size(response);
    char *raw_response   = Response_get(response); // original in cache

    char *response_dup = calloc(response_size + 1, sizeof(char));
    memcpy(response_dup, raw_response, response_size);

    if (HTTP_add_field(&response_dup, &response_size, "Age", age_str) < 0) {
        print_error("proxy: failed to add Age field to response");
        return ERROR_FAILURE;
    }

/* Color links in the response */
#if RUN_COLOR
    char **key_array = Cache_getKeyList(proxy->cache);
    int num_keys     = (int)proxy->cache->size;
    if (color_links(&response_dup, &response_size, key_array, num_keys) != 0) {
        fprintf(stderr, "[Proxy_serveFromCache]: couldn't color hyperlinks.\n");
    } else {
        fprintf(stderr, "[Proxy_serveFromCache]: colored hyperlinks.\n");
    }
#endif
#if RUN_SSL
    if (client->isSSL) {
        if (ProxySSL_write(proxy, client, response_dup, response_size) < 0) {
            print_error("proxy: send failed");
            free(key);
            return ERROR_SEND;
        }
    } else {
#endif
        if (Proxy_send(client->socket, response_dup, response_size) < 0) {
            print_error("proxy: send failed");
            free(key);
            return ERROR_SEND;
        }
#if RUN_SSL 
    }
#endif

    free(response_dup);

    return CLIENT_CLOSE; // non-persistent c
}
#endif

int Proxy_handleTunnel(int sender, int receiver)
{
    if (sender == -1 || receiver == -1) {
        return ERROR_FAILURE;
    }

    char buffer[BUFFER_SZ + 1];
    zero(buffer, BUFFER_SZ + 1);
    ssize_t buf_l = 0;

    buf_l = recv(sender, buffer, BUFFER_SZ, 0);
    if (buf_l < 0) {
        print_error("handle connect: recv failed");
        perror("recv");
        return ERROR_RECV;
    } else if (buf_l == 0) {
        fprintf(stderr, "Client Query Socket Closed Connection.\n");
        return CLIENT_CLOSE;
    } else {
        fprintf(stderr, "handle connect: received response from client query socket\n");
        if (Proxy_send(receiver, buffer, buf_l) != buf_l) {
            return ERROR_SEND;
        }
    }

    return EXIT_SUCCESS;
}

#if RUN_FILTER
int Proxy_readFilterList(Proxy *proxy) 
{
    if (proxy == NULL) {
        return ERROR_FAILURE;
    }

    FILE *fp = fopen(FILTER_LIST_PATH, "r");
    if (fp == NULL) {
        print_error("proxy: failed to open filter list");
        fprintf(stderr, "proxy: filter list path: %s\n", FILTER_LIST_PATH);
        return ERROR_FAILURE;
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    proxy->num_filters = 0;

    while ((read = getline(&line, &len, fp)) != -1) {
        fprintf(stderr, "proxy: read line: %s", line);
        if (read > 1) {
            line[read - 1] = '\0';
            if (Proxy_addFilter(proxy, line) != EXIT_SUCCESS) {
                print_error("proxy: failed to add filter");
                fprintf(stderr, "proxy: filter: %s\n", line);
                fclose(fp);
                return FILTER_LIST_TOO_BIG;
            }
        }
    }

    for (int i = 0; i < proxy->num_filters; i++) {
        fprintf(stderr, "proxy: filter %d: %s\n", i, proxy->filters[i]);
    }


    fclose(fp);
    if (line) {
        free(line);
    }

    return EXIT_SUCCESS;
}


int Proxy_addFilter(Proxy *proxy, char *filter) 
{
    if (proxy == NULL || filter == NULL) {
        return ERROR_FAILURE;
    }

    if (proxy->num_filters == MAX_FILTERS) {
        print_error("proxy: max filters reached");
        return ERROR_FAILURE;
    }

    proxy->filters[proxy->num_filters] = calloc(strlen(filter) + 1, sizeof(char));
    memcpy(proxy->filters[proxy->num_filters], filter, strlen(filter));
    proxy->num_filters++;

    return EXIT_SUCCESS;
}

bool Proxy_isFiltered(Proxy *proxy, char *host)
{
    if (proxy == NULL || host == NULL) {
        return false;
    }
    /* both url and host must be filtered */
    for (int i = 0; i < proxy->num_filters; i++) {
        if (strstr(host, proxy->filters[i]) != NULL) {
            return true;
        }
    }

    return false;
}

void Proxy_freeFilters(Proxy *proxy)
{
    if (proxy == NULL) {
        return;
    }

    for (int i = 0; i < proxy->num_filters; i++) {
        free(proxy->filters[i]);
    }
}
#endif


/* Static Functions --------------------------------------------------------- */
#if RUN_CACHE
static char *get_key(Request *req)
{
    if (req == NULL) {
        return NULL;
    }
    Request_print(req);
    fprintf(stderr, "%s[get-key]: host_l: %ld, port_l: %ld, path_l: %ld%s\n", BYEL, req->host_l, req->port_l, req->path_l, reset);
    int key_l = req->host_l + req->port_l + req->path_l + COLON_L;
    fprintf(stderr, "%s[get-key]: key_l: %d%s\n", BYEL, key_l, reset);
    int offset = 0;
    char *key = calloc(key_l + 1, sizeof(char));
    memcpy(key + offset, req->path, req->path_l);
    offset += req->path_l;
    memcpy(key + offset, COLON, COLON_L);
    offset += COLON_L;
    memcpy(key + offset, req->port, req->port_l);
    offset += req->port_l;
    memcpy(key + offset, req->host, req->host_l);
    offset += req->host_l;

    key[key_l] = '\0';

    print_ascii(key, key_l);

    fprintf(stderr, "%s[get-key]: key: %s%s\n", BYEL, key, reset);
    fprintf(stderr, "%s[get-key]: key: %s%s\n", BYEL, key, reset);

    return key;
}
#endif

static short select_loop(Proxy *proxy)
{
    if (proxy == NULL) {
        return ERROR_FAILURE;
    }

    short ret;
    while (true) {
        print_debug("proxy: waiting for connections");
        Proxy_handleTimeout(proxy);
        proxy->readfds = proxy->master_set;
        ret            = select(proxy->fdmax + 1, &(proxy->readfds), NULL, NULL, NULL);
        fprintf(stderr, "[Proxy_run] select returned: %d\n", ret);
        switch (ret) {
        case SELECT_ERROR:
            print_error("proxy: select failed");
            perror("select");
            return ERROR_FAILURE;
        case SELECT_TIMEOUT:
            print_warning("proxy: select timed out");
            continue;
            break;
        default:
            /* check listening socket and accept new clients */
            if (FD_ISSET(proxy->listen_fd, &(proxy->readfds))) {
                Proxy_handleListener(proxy);
                // if ((ret = Proxy_handleListener(proxy)) != EXIT_SUCCESS) {
                //     ret = Proxy_handleEvent(proxy, NULL, ret);
                //     if (ret != EXIT_SUCCESS) {
                //         return ret;
                //     }
                // }
            }
            /* check client sockets for requests and serve responses */
            fprintf(stderr, "[Proxy_run] calling Proxy_handle\n");
            ret = Proxy_handle(proxy);
        }

        if (ret != EXIT_SUCCESS) {
            break;
        }
    }

    return ret;
}
