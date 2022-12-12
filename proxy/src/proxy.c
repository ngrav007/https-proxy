#include "proxy.h"

#define SELECT_ERROR   -1
#define SELECT_TIMEOUT 0
#define TIMEOUT_FALSE  0
#define TIMEOUT_TRUE   1   
#define RESPONSE_RECEIVED 202
#define QUERY_TYPE 0
#define CLIENT_TYPE 1

// Ports -- 9055 to 9059
static char *get_key(Request *req);
static short select_loop(Proxy *proxy);
static char *get_certificate(char *host, size_t host_l);

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
int Proxy_run(short port, size_t cache_size)
{
    struct Proxy proxy;

    if (!is_root()) {
        print_error("proxy: must be run as root");
        return ERROR_FAILURE;
    }

    /* Initialize SSL --------------------------------------------------------------------------- */
    SSL_load_error_strings();   // ? - might not need
    SSL_library_init();         // ? - might not need

    /* Initialize Proxy ------------------------------------------------------------------------- */
    Proxy_init(&proxy, port, cache_size);
    print_success("[+] HTTP Proxy -----------------------------------");
    fprintf(stderr, "%s[*]%s   Proxy Port = %d\n", YEL, reset, proxy.port);
    fprintf(stderr, "%s[*]%s   Cache Size = %ld\n", YEL, reset, proxy.cache->capacity);

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
int Proxy_init(struct Proxy *proxy, short port, size_t cache_size)
{
    if (proxy == NULL) {
        print_error("proxy: null parameter passed to init call");
        return ERROR_FAILURE;
    }

/* initialize the proxy cache */
#if RUN_CACHE
    proxy->cache = Cache_new(cache_size, Response_free, Response_print);
    if (proxy->cache == NULL) {
        print_error("proxy: cache init failed");
        return ERROR_FAILURE;
    }
#endif

    /* zero out the proxy addresses */
    zero(&(proxy->addr), sizeof(proxy->addr));
    zero(&(proxy->client), sizeof(proxy->client));
    zero(&(proxy->server), sizeof(proxy->server));
    zero(&(proxy->client_ip), sizeof(proxy->client_ip));
    zero(&(proxy->server_ip), sizeof(proxy->server_ip));

    /* initialize socket fds to -1 */
    proxy->listen_fd = -1;
    proxy->server_fd = -1;
    proxy->client_fd = -1;

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

    fprintf(stderr, "FREESING Proxy\n");

    struct Proxy *p = (struct Proxy *)proxy;

/* free the proxy */
#if RUN_CACHE
    Cache_free(&p->cache);
#endif

    List_free(&p->client_list);
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

    /* free the timeout */
    if (p->timeout != NULL) {
        free(p->timeout);
        p->timeout = NULL;
    }

/* free the SSL context */
#if RUN_SSL
    SSL_CTX_free(p->ctx);
#endif

    p->port = 0;
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
    fprintf(stderr, "  server_fd = %d\n", proxy->server_fd);
    fprintf(stderr, "  client_fd = %d\n", proxy->client_fd);

#if RUN_CACHE
    Cache_print(proxy->cache);
#endif

    List_print(proxy->client_list);
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
    fprintf(stderr, "%s[?] Closing Socket # = %d%s\n", YEL, socket, reset);
    close(socket);

    fprintf(stderr, "%s[?] Remove Socket # from master_set = %d%s\n", YEL, socket, reset);

    /* remove the client from master fd_set */
    FD_CLR(socket, master_set);
    if (client->query != NULL && client->query->socket != -1) {
        FD_CLR(client->query->socket, master_set);
    }

    /* remove the client from client_list */
    fprintf(stderr, "%s[?] Remove Socket # from client_list = %d%s\n", YEL, socket, reset);
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

    /* build proxy address (type is: struct sockaddr_in) */
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
    Client *cli = Client_new();
    cli->addr_l = sizeof(cli->addr);
    cli->socket = accept(proxy->listen_fd, (struct sockaddr *)&(cli->addr), &(cli->addr_l));
    if (cli->socket == -1) {
        print_error("proxy: accept failed");
        Client_free(cli);
        return ERROR_ACCEPT;
    }

    /* timestamp the client */
    Client_timestamp(cli);

    /* set socket to non-blocking */ // ? why did we do this again?
    // int flags = fcntl(cli->socket, F_GETFL, 0);
    // if (flags == -1) {
    //     print_error("proxy: fcntl failed");
    //     Client_free(cli);
    //     return ERROR_FAILURE;
    // }
    // flags |= O_NONBLOCK;
    // if (fcntl(cli->socket, F_SETFL, flags) == -1) {
    //     print_error("proxy: fcntl failed");
    //     Client_free(cli);
    //     return ERROR_FAILURE;
    // }

    /* add client to the proxy's client list */
    int ret = List_push_back(proxy->client_list, (void *)cli);
    if (ret < 0) {
        Client_free(cli);
        print_error("proxy: failed to add client to client list");
        return ERROR_FAILURE;
    }

    /* put new clients's socket in master set */
    FD_SET(cli->socket, &(proxy->master_set));

    /* 4. updates the max fd */
    proxy->fdmax = cli->socket > proxy->fdmax ? cli->socket : proxy->fdmax;

    return EXIT_SUCCESS;
}

/** Proxy_fetch
 *    Purpose: Connects to the client-requested server and forwards client's request.
 * Parameters: @proxy - the proxy struct
 *             @qry - the query containing the server information we are fetcher from
 *    Returns: The number of bytes sent if successful, otherwise, ERROR_CONNECT is returned
 *             indicating an error connecting to the server has occurred, ERROR_FETCH is an error
 *             occurred sending the request to server, else ERROR_FAILURE
 */
ssize_t Proxy_fetch(Proxy *proxy, Query *qry)
{
    if (proxy == NULL || qry == NULL) {
        print_error("proxy_fetch: fetch: invalid arguments");
        return ERROR_FAILURE;
    }

    /* make connection to server */
    print_debug("proxy_fetch: making connection to server");
    if (connect(qry->socket, (struct sockaddr *)&qry->server_addr, sizeof(qry->server_addr)) < 0) {
        print_error("proxy_fetch: failed to connect to server");
        perror("connect");
        return ERROR_BAD_GATEWAY;
    }
    print_debug("proxy_fetch: connected to server");

    /* add server socket to master_set */
    FD_SET(qry->socket, &proxy->master_set);
    proxy->fdmax = (qry->socket > proxy->fdmax) ? qry->socket : proxy->fdmax;

    /* send message to server */
    ssize_t msgsize = 0;
    if (strncmp(qry->req->method, GET, GET_L) == 0) {
        print_info("proxy_fetch: sending GET request to server");
        print_ascii(qry->req->raw, qry->req->raw_l);
        if (Proxy_send(qry->socket, qry->req->raw, qry->req->raw_l) < 0) {
            print_error("proxy_fetch: send failed");
            return ERROR_FETCH;
        }
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
    ssize_t written  = 0;
    ssize_t to_write = buffer_l;
    ssize_t n        = 0;
    while (written < (ssize_t)buffer_l) {
        n = send(socket, 
        buffer + written, 
        to_write, 0);
        if (n == -1) {
            print_error("proxy: send failed");
            perror("send");
            return ERROR_SEND;
        }
        written += n;
        to_write -= n;

        fprintf(stderr, "[Proxy_send] Bytes sent: %ld\n", n);
    }
    fprintf(stderr, "Bytes To Write: %ld\n", to_write);
    fprintf(stderr, "Bytes Written: %ld\n", written);
    fprintf(stderr, "%s[END PROXY SEND]%s\n", GRN, reset);

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
    switch(sender_type) {
    case QUERY_TYPE:
        q = (Query *)sender;
        n = recv(q->socket, q->buffer + q->buffer_l, q->buffer_sz - q->buffer_l, 0);
        if (n == 0) {
            q->res = Response_new(q->req->method, q->req->method_l, q->req->path, q->req->path_l,
                                q->buffer, q->buffer_l);
            if (q->res == NULL) {
                print_error("proxy: failed to create response");
                return INVALID_RESPONSE;
            }
            q->state = QRY_RECVD_RESPONSE;
            return RESPONSE_RECEIVED;
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
        fprintf(stderr, "[Proxy_recv] Receiving from client: %d\n", c->socket);
        fprintf(stderr, "[Proxy_recv] Client buffer size: %ld\n", c->buffer_sz);
        fprintf(stderr, "[Proxy_recv] Client buffer length: %ld\n", c->buffer_l);
        fprintf(stderr, "Bytes To Read: %ld\n", c->buffer_sz - c->buffer_l);
        n = recv(c->socket, c->buffer + c->buffer_l, c->buffer_sz - c->buffer_l, 0);
        if (n == 0) {
            print_info("proxy: client closed connection");
            return CLIENT_CLOSE;
        } else if (n < 0) {
            print_error("proxy: recv failed");
            perror("recv");
            return ERROR_RECV;
        } else {
            c->buffer_l += n;
            if (c->buffer_l == c->buffer_sz) {
                expand_buffer(&c->buffer, &c->buffer_l, &c->buffer_sz);
            }
        }

        /* update last active time to now */
        Client_timestamp(c);
        break;
    }

    return n;
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

#if RUN_SSL
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
    if (Proxy_updateExtFile(proxy, hostname) != EXIT_SUCCESS) {
        return ERROR_FAILURE;
    }

    /* create new SSL object for client */
    fprintf(stderr, "[ProxySSL_accept] Checking for a SSL/TLS connection...\n");
    client->ssl = SSL_new(proxy->ctx);
    SSL_set_fd(client->ssl, client->socket);

    /* accept */
    if (SSL_accept(client->ssl) == -1) {
        ERR_print_errors_fp(stderr);
        fprintf(stderr, "[ProxySSL_accept] SSL/TLS connection failed!\n");
        /* stop SSL */
        SSL_shutdown(client->ssl);
        SSL_free(client->ssl);
        client->ssl = NULL;
        client->isSSL = 0;
        return ERROR_SSL;
    } else {
        fprintf(stderr, "[ProxySSL_accept] SSL/TLS connection established!\n");
        client->isSSL = 1;
        client->state = CLI_QUERY;
        Query_free(client->query);
        client->query = NULL;
        clear_buffer(client->buffer, &client->buffer_l);
    }

    fprintf(stderr, "[ProxySSL_accept] SSL/TLS connection established!\n");
    /* update last active time to now */
    Client_timestamp(client);

    return EXIT_SUCCESS;
}
#endif

int Proxy_updateExtFile(Proxy *proxy, char *hostname)
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
        if (Proxy_updateContext(proxy) == ERROR_FAILURE) {
            print_error("proxy: updateExtFile failed, updateContext failed");
            return ERROR_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

int Proxy_updateContext(Proxy *proxy)
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

int Proxy_handleGET(Proxy *proxy, Client *client)
{
    int ret = EXIT_SUCCESS;
    if (client->query->state != QRY_SENT_REQUEST) { // check cache, if sent request, already checked cache
        fprintf(stderr, "[Proxy_handleGET] CLIENT matched on CLI_GET\n");
        char *key = get_key(client->query->req);
        if (key == NULL) {
            print_error("[Proxy_handleGET]: failed to get key");
            return ERROR_FAILURE;
        }

   
        long cache_res_age = Cache_get_age(proxy->cache, key);
        if (cache_res_age >= 0) {   // cache hit
            fprintf(stderr, "[Proxy_handleGET]: cache hit!\n");
            ret = Proxy_serveFromCache(proxy, client, cache_res_age, key);
            if (ret < 0) {
                free(key);
                print_error("[Proxy_handleGET]: failed to serve from cache");
                return ret;
            }
            free(key);
            client->query->state = QRY_DONE;
        } else {                    // cache miss
            free(key);
            fprintf(stderr, "[Proxy_handleGET]: cache miss!\n");
            /* if in HTTPS mode use TLS/SSL otherwise do a regular fetch */
            ret = (client->isSSL) ? ProxySSL_connect(proxy, client->query) : Proxy_fetch(proxy, client->query);
            if (ret < 0) {
                print_error("[Proxy_handleGET]:fetch failed");
                return ret; 
            }

            client->query->state = QRY_SENT_REQUEST;
        }
    } else if (FD_ISSET(client->query->socket, &proxy->readfds)) {      // query server
        fprintf(stderr, "[Proxy_handleGET] QUERY matched on CLI_GET\n");
        if (client->query->state < QRY_RECVD_RESPONSE) {
            ret = Proxy_handleQuery(proxy, client->query, client->isSSL);
            if (ret < 0) {
                print_error("[Proxy_handleGET]: handle server query failed");
                return ret;
            }
        }

        fprintf(stderr, "[Proxy_handleGET] QUERY state: %d\n", client->query->state);
        fprintf(stderr, "[Proxy_handleGET] handleQUERY ret: %d\n", ret);
        
        if (client->query->state == QRY_RECVD_RESPONSE) { // Non-Persistent
            fprintf(stderr, "[Proxy_handleGET]: response received...\n");
            ret = Proxy_sendServerResp(proxy, client);
        }
    }

    return (ret < 0) ? ret : EXIT_SUCCESS;
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
    int n;
    switch(sender_type) {
    case QUERY_TYPE:
        fprintf(stderr, "[ProxySSL_read] QUERY matched on QUERY_TYPE\n");
        q = (Query *)sender;
        n = SSL_read(q->ssl, q->buffer + q->buffer_l, q->buffer_sz - q->buffer_l);
        fprintf(stderr, "[ProxySSL_read] QUERY SSL_read returned %d\n", n);
        print_ascii(q->buffer + q->buffer_l, n);
        if (n <= 0) {
            print_error("proxy: recv failed");
            perror("recv");
            return ERROR_SSL;
            
        } else if (n < q->buffer_sz - q->buffer_l) {
            q->buffer_l += n;
            q->res = Response_new(q->req->method, q->req->method_l, q->req->path, q->req->path_l,
                                q->buffer, q->buffer_l);
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

/** Proxy_SSLconnect
 * makes s connection to destination host on behalf of a clien
 */
#if RUN_SSL
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
#endif

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
            ret = Proxy_handleGET(proxy, client);
            break;
        case CLI_CONNECT: // CONNECT tunneling
            print_info("proxy: client is in CONNECT state");
            ret = Proxy_handleCONNECT(proxy, client);
            break;
        case CLI_SSL:
            print_info("proxy: client is in SSL state");
            if (FD_ISSET(client->socket, &proxy->readfds)) {
                ret = ProxySSL_handshake(proxy, client);
            }
            break;
        case CLI_TUNNEL:
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
    Cache_refresh(proxy->cache);
    // refresh(); // TODO - update staleness of cache entries
    return EXIT_SUCCESS;
}

int Proxy_sendServerResp(Proxy *proxy, Client *client)
{
    if (proxy == NULL || client == NULL) {
        return ERROR_FAILURE;
    }

    /* Color links in the response */
    char *response_buffer = calloc(client->query->res->raw_l + 1, sizeof(char));
    size_t response_sz = client->query->res->raw_l;
    memcpy(response_buffer, client->query->res->raw, response_sz);

#if (RUN_COLOR)
    char **key_array = Cache_getKeyList(proxy->cache);
    int num_keys = (int) proxy->cache->size;
    if (color_links(&response_buffer, &response_sz, 
        key_array, num_keys) != 0) {
        fprintf(stderr, "%s[Proxy_handle]: couldn't color hyperlinks.%s\n", RED, reset);
    } else {
        fprintf(stderr, "%s[Proxy_handle]: colored hyperlinks.%s\n", RED, reset);
    }
#endif

    if (client->isSSL) {
        fprintf(stderr, "%s[Proxy_handle]: sending SSL response to client %d%s\n", YEL, client->socket, reset);
        if (ProxySSL_write(proxy, client, response_buffer, response_sz) <= 0) {
            print_error("proxy: failed to send response to client");
            return ERROR_SSL;
        }
    } else {
        fprintf(stderr, "%s[Proxy_handle]: sending response to client %d%s\n", YEL, client->socket, reset);
        if (Proxy_send(client->socket, response_buffer, response_sz) < 0) {
            print_error("proxy: failed to send response to client");
            return ERROR_SEND;
        }
    }

    fprintf(stderr, "%s[Proxy_handle]: sent response to client %d%s\n", YEL, client->socket, reset);
    client->state = (client->isSSL) ? CLI_QUERY : CLI_TUNNEL;
    free(response_buffer);

    /* store copy the response in cache so it is available even after client closes */
    char *key        = get_key(client->query->req);
    Response *cached_res = Response_copy(client->query->res);
    int ret        = Cache_put(proxy->cache, key, cached_res, cached_res->max_age);
    print_debug("proxy_handle: response cached");
    fprintf(stderr, "%s[?] Cache_put returned %d%s\n", YEL, ret, reset);
    if (ret < 0) {
        print_error("proxy: failed to cache response");
        ret = ERROR_FAILURE;
    } else {
        fprintf(stderr, "%s[Proxy_handle]: cached response 1%s\n", YEL, reset);
        client->state = CLI_QUERY; // todo this might be slightly too early
        client->query->state = QRY_INIT;
        FD_CLR(client->query->socket, &proxy->readfds);
        FD_CLR(client->query->socket, &proxy->master_set);
        // Query_free(client->query);
        // client->query = NULL;
        // ret = CLIENT_CLOSE; // TODO - check if client is persistent
    }
    free(key);

    return ret;
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

    if (client->isSSL == 1) {
        fprintf(stderr, "[Proxy_handle] ssl client %d receiving\n", client->socket);
        ret = ProxySSL_read(client, CLIENT_TYPE);
        if (ret <= 0) {
            fprintf(stderr, "[Proxy_handle] ssl client %d closed connection\n", client->socket);
            return CLIENT_CLOSE;
        }
    } else {
        fprintf(stderr, "[Proxy_handle] client %d receiving\n", client->socket);
        ret = Proxy_recv(client, CLIENT_TYPE);
        if (ret < 0) {
            fprintf(stderr, "[Proxy_handle] client %d closed connection\n", client->socket);
            return CLIENT_CLOSE;
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

    // 1. Check if response so far has a header -- header flag

    // 1a. if yes, switch-case the transfer encoding type (normal, or chunked)
    // 1a.i. if chunk'd:
    // Do a recv
    // Parse the hexidecimal chunk size for next chunk, (ex: 6\r\n)
    // loop until the chunk following the size is that many bytes. A chunk is \r\n terminated
    // 1a.ii. if normal, and has a content-length
    // do one recv of BUFFER_SZ (like default)
    // stop when you have content-length total bytes in the buffer.
    // 1a.iii. if no transfer type is set (no content-length, no TE)
    // default to recv'ing BUFFER_SZ bytes
    //

    // 1b. if no,
    // default to recv'ing BUFFER_SZ bytes
    // check if we have a header so far, and set any header flag if yes, and set flag for transfer type, if any (ie. has
    // content length or a transfer-encoding field) if chunk encoding:
    //     - check if any bytes after end of header (this is partial chunk)

    /* if connection hasn't been made with server, make the connection */
    ssize_t n;
    size_t buffer_l  = query->buffer_l;
    size_t buffer_sz = query->buffer_sz;

    fprintf(stderr, "%s[?] proxy: buffer_l: %ld%s\n", YEL, buffer_l, reset);
    fprintf(stderr, "BUFFER_SZ = %ld BUFFER_L = %ld\n", query->buffer_sz, query->buffer_l);
    fprintf(stderr, "BUFFER_SZ = %ld BUFFER_L = %ld\n", buffer_sz, buffer_l);
    fprintf(stderr, "Receiving %ld bytes\n", buffer_sz - buffer_l);

    /* read from socket */
    if (isSSL) {
        n = ProxySSL_read(query, QUERY_TYPE);
    } else {
        n = Proxy_recv(query, QUERY_TYPE);
    }
    if (n <= 0) {
        print_error("proxy: failed to receive bytes from server");
        return ERROR_CLOSE;
    }
    fprintf(stderr, "Received %ld bytes\n", n);
    return (n < 0) ? ERROR_RECV : EXIT_SUCCESS;
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
    
    int ret = Proxy_send(client->socket, STATUS_200, STATUS_200_L);
    if (ret != STATUS_200_L) {
        print_error("proxy: failed to send 200 OK");
        return ret;
    }

    if (RUN_SSL) {
        client->state = CLI_SSL;
    } else {
        client->state = CLI_TUNNEL;
    }

    fprintf(stderr, "client->state = %d\n", client->state);

    return EXIT_SUCCESS;
}

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
        Proxy_close(client->socket, &proxy->master_set, proxy->client_list, client); // ? - close for persistent connections?
        break;
    case PROXY_AUTH_REQUIRED:
        print_error("proxy: invalid method in request");
        Proxy_sendError(client, PROXY_AUTH_REQUIRED_407);
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
int Proxy_handleTimeout(struct Proxy *proxy) {
    if (proxy == NULL) {
        return ERROR_FAILURE;
    }

    
    double age, time_til_timeout, now = get_current_time(), timeout = TIMEOUT_THRESHOLD;
    Node *client_node = List_head(proxy->client_list);
    while (client_node != NULL) {
        Client *client = (Client *)client_node->data;
        client_node = List_next(client_node);
        // if (client->state == CLI_PERSISTENT) {  // ? skip persistent clients
        //     continue;
        // }
        fprintf(stderr, "%s[+] proxy: client %d now: %f%s\n", GRN, client->socket, now, reset);
        age = now - timeval_to_double(client->last_active);
        fprintf(stderr, "%s[+] proxy: client %d age: %f%s\n", GRN, client->socket, age, reset);
        if (age > TIMEOUT_THRESHOLD) {  // client timed out
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
    fprintf(stderr, "%s[+] proxy: timeout set to %ld.%ld%s\n", GRN, proxy->timeout->tv_sec, proxy->timeout->tv_usec, reset);

    return TIMEOUT_TRUE;

}

/* Static Functions --------------------------------------------------------- */
static char *get_key(Request *req)
{
    if (req == NULL) {
        return NULL;
    }

    char *key = calloc(req->host_l + req->path_l + 1, sizeof(char));
    memcpy(key, req->path, req->path_l);
    memcpy(key + req->path_l, req->host, req->host_l);

    return key;
}

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
        ret = select(proxy->fdmax + 1, &(proxy->readfds), NULL, NULL, proxy->timeout);
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
/* serve from cache */
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
#if (RUN_CACHE && RUN_COLOR)
    char **key_array = Cache_getKeyList(proxy->cache);
    int num_keys     = (int)proxy->cache->size;
    if (color_links(&response_dup, &response_size, key_array, num_keys) != 0) {
        fprintf(stderr, "[Proxy_serveFromCache]: couldn't color hyperlinks.\n");
    } else {
        fprintf(stderr, "[Proxy_serveFromCache]: colored hyperlinks.\n");
    }
#endif

    if (client->isSSL) {
        if (ProxySSL_write(proxy, client, response_dup, response_size) < 0) {
            print_error("proxy: send failed");
            free(key);
            return ERROR_SEND;
        }
    } else {
        if (Proxy_send(client->socket, response_dup, response_size) < 0) {
            print_error("proxy: send failed");
            free(key);
            return ERROR_SEND;
        }
    }

    free(response_dup);

    return CLIENT_CLOSE; // non-persistent c
}

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


static char *get_certificate(char *host, size_t host_l)
{
    if (host == NULL) {
        return NULL;
    }

    char *cert_dir = CA_CERT_DIR;
    char *cert_path = calloc(CA_CERT_L + 1, sizeof(char));
    memcpy(cert_path, cert_dir, CA_CERT_L);

    char cert_name[host_l + 4];
    zero(cert_name, host_l + 4);
    memcpy(cert_name, host, host_l);
    memcpy(cert_name + host_l, ".crt", 4);

    memcpy(cert_path + CA_CERT_L, cert_name, host_l + 4);

    return cert_path;
}



