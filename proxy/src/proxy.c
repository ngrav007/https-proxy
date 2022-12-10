#include "proxy.h"

#define SELECT_ERROR -1
#define SELECT_TIMEOUT 0

// Ports -- 9055 to 9059
static char *get_key(Request *req);
static int validate_request(Request *req);
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
int Proxy_run(short port, size_t cache_size)
{
    struct Proxy proxy;

    #if RUN_SSL
    if (!is_root()) {
        print_error("proxy: must be run as root");
        return ERROR_FAILURE;
    }

    SSL_library_init();    
    #endif

    /* initialize the proxy */
    Proxy_init(&proxy, port, cache_size);
    print_success("[+] HTTP Proxy -----------------------------------");
    fprintf(stderr, "%s[*]%s   Proxy Port = %d\n", YEL, reset, proxy.port);

    #if RUN_CACHE 
    fprintf(stderr, "%s[*]%s   Cache Size = %ld\n", YEL, reset, proxy.cache->capacity);
    #endif 

    /* bind and listen proxy socket */
    if (Proxy_listen(&proxy) < 0) {
        print_error("proxy: failed to listen");
        return ERROR_FAILURE;
    }

    /* Accept Loop ---------------------------------------------------------- */
    short ret = select_loop(&proxy);
    if (ret == HALT) {
        print_info("proxy: shutting down");
        Proxy_free(&proxy);
        return EXIT_SUCCESS;
    }

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
        proxy->cache = Cache_new(cache_size, Response_free, Response_print, Response_compare);
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

    /* set the proxy port */
    proxy->port = port;

    /* zero out fd_sets and initialize max fd of master_set */
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
        proxy->ctx = init_server_context();
        if (proxy->ctx == NULL) {
            print_error("proxy: failed to initialize SSL context");
            return ERROR_FAILURE;
        }
        

        /* load certificates */
        load_ca_certificates(proxy->ctx, PROXY_CERT, PROXY_KEY);
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
    /* accept connection request from new client */
    Client *cli = Client_new();
    cli->addr_l = sizeof(cli->addr);
    cli->socket = accept(proxy->listen_fd, (struct sockaddr *)&(cli->addr), &(cli->addr_l));
    if (cli->socket == -1) {
        print_error("proxy: accept failed");
        Client_free(cli);
        return ERROR_ACCEPT;
    }

    /* set socket to non-blocking */// ? why did we do this again?
    int flags = fcntl(cli->socket, F_GETFL, 0);
    if (flags == -1) {
        print_error("proxy: fcntl failed");
        Client_free(cli);
        return ERROR_FAILURE;
    }
    flags |= O_NONBLOCK;
    if (fcntl(cli->socket, F_SETFL, flags) == -1) {
        print_error("proxy: fcntl failed");
        Client_free(cli);
        return ERROR_FAILURE;
    }

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

    /* 5. Check if client wants to initiate a TLS/SSL handshake */
    #if RUN_SSL
        fprintf(stderr, "[Proxy_accept] Checking for a SSL/TLS connection...\n");

        cli->ssl = SSL_new(proxy->ctx);
        SSL_set_fd(cli->ssl, cli->socket);
        int ssl_acc_ret = SSL_accept(cli->ssl);
        if (ssl_acc_ret == 1) {
            fprintf(stderr, "[Proxy_accept] SSL/TLS connection established!\n");
        } else {
            fprintf(stderr, "[Proxy_accept] SSL/TLS connection failed!\n");
            return ERROR_SSL;
        }
    #endif 

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
        return ERROR_CONNECT;
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
        n = send(socket, buffer + written, to_write, 0);
        if (n == -1) {
            print_error("proxy: send failed");
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
    case INTERNAL_SERVER_ERROR_500:
        return Proxy_send(client->socket, STATUS_500, STATUS_500_L);
    case NOT_IMPLEMENTED_501:
        return Proxy_send(client->socket, STATUS_501, STATUS_501_L);
    case IM_A_TEAPOT_418:
        return Proxy_send(client->socket, STATUS_418, STATUS_418_L);
    default:
        fprintf(stderr, "%s[!]%s proxy: unknown error code: %d\n", RED, reset, msg_code);
        exit(EXIT_FAILURE); // TODO - handle this better
        return ERROR_FAILURE;
    }

    return EXIT_SUCCESS;
}

/* ---------------------------------------------------------------------------------------------- */
/* PROXY SSL FUNCTIONS -------------------------------------------------------------------------- */
/* ---------------------------------------------------------------------------------------------- */

/** Proxy_SSLconnect
 * makes s connection to destination host on behalf of a clien
 */
#if RUN_SSL
int Proxy_SSLconnect(Proxy *proxy, Query *query)
{
    if (proxy == NULL || query == NULL) {
        print_error("[Proxy_SSLconnect]: invalid arguments");
        return ERROR_FAILURE;
    }

    fprintf(stderr, "[Proxy_SSLconnect]: initializing SSL context...\n");

    query->ctx = init_ctx();

    load_ca_certificates(query->ctx, PROXY_CERT, PROXY_KEY);
    query->ssl = NULL;

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

    query->ssl = SSL_new(query->ctx);
    SSL_set_fd(query->ssl, query->socket);

    /* perform connection */
    if (SSL_connect(query->ssl) < 0) {
        fprintf(stderr, "[Proxy_SSLconnect]: SSL_connect() failed\n");
        ERR_print_errors_fp(stderr);
        return ERROR_CLOSE;
    }

    return EXIT_SUCCESS;
}
#endif 


/* ---------------------------------------------------------------------------------------------- */
/* --------------------------------------- PROXY HANDLERS --------------------------------------- */
/* ---------------------------------------------------------------------------------------------- */

/** Proxy_handle
 */
int Proxy_handle(Proxy *proxy)
{
    int ret              = EXIT_SUCCESS;
    char *key            = NULL;
    Client *client       = NULL;
    Response *cached_res = NULL;
    Node *curr = NULL, *next = NULL;
    for (curr = proxy->client_list->head; curr != NULL; curr = next) {
        next   = curr->next;
        client = (Client *)curr->data;

        switch (client->state) {
        case CLI_QUERY:
            if (FD_ISSET(client->socket, &proxy->readfds)) {
                fprintf(stderr, "[Proxy_handle] socket %d matched on CLI_QUERY\n", client->socket);
                ret = Proxy_handleClient(proxy, client);
            }
            break;
        case CLI_GET:
            fprintf(stderr, "[Proxy_handle] matched on CLI_GET\n");
            if (FD_ISSET(client->socket, &proxy->readfds)) {
                /* TODO: for persisting connection */
            }

            if (client != NULL && client->query != NULL && FD_ISSET(client->query->socket, &proxy->readfds)) {
                fprintf(stderr, "[Proxy_handle] query socket is %d\n", client->query->socket);
                ret = Proxy_handleQuery(proxy, client->query);
                fprintf(stderr, "[Proxy_handle] Proxy_handleQuery returned: %d\n", ret);
                if (client->query->res != NULL) { // Non-Persistent
                    fprintf(stderr, "proxy_handle: response received, size = %ld\n", client->query->res->raw_l);


                    /* Color links in the response */
                    #if (RUN_CACHE && RUN_COLOR)    // CACHING
                        print_info("PROXY CACHE AND PROXY COLOR IS ON");
                        char *response_buffer = calloc(client->query->res->raw_l + 1, sizeof(char));
                        size_t response_sz = client->query->res->raw_l;
                        memcpy(response_buffer, client->query->res->raw, response_sz);
                    
                        char **key_array = Cache_getKeyList(proxy->cache);
                        int num_keys = (int) proxy->cache->size;

                        if (color_links(&response_buffer, &response_sz, 
                            key_array, num_keys) != 0) {
                            fprintf(stderr, "%s[Proxy_handle]: couldn't color hyperlinks.%s\n", RED, reset);
                        } else {
                            fprintf(stderr, "%s[Proxy_handle]: colored hyperlinks.%s\n", RED, reset);
                        }

                        if (Proxy_send(client->socket, response_buffer, response_sz) < 0) {
                            print_error("proxy: failed to send response to client");
                            ret = ERROR_SEND;
                        }
                        free(response_buffer);

                    #else 
                        print_info("PROXY CACHE AND PROXY COLOR IS OFF");
                        if (Proxy_send(client->socket, client->query->res->raw, client->query->res->raw_l) < 0) {
                            print_error("proxy: failed to send response to client");
                            ret = ERROR_SEND;
                        } else {
                            ret = CLIENT_CLOSE;
                        }
                    #endif 

                    #if RUN_CACHE    // CACHING
                        key        = get_key(client->query->req);
                        cached_res = Response_copy(client->query->res);
                        ret        = Cache_put(proxy->cache, key, cached_res, cached_res->max_age);
                        print_debug("proxy_handle: response cached");
                        fprintf(stderr, "%s[?] Cache_put returned %d%s\n", YEL, ret, reset);
                        if (ret < 0) {
                            print_error("proxy: failed to cache response");
                            ret = ERROR_FAILURE;
                        } else {
                            ret = CLIENT_CLOSE; // TODO - check if client is persistent
                        }
                        free(key);
                    #else 
                        (void) key;
                        (void) cached_res;
                    #endif 
                }
            }
            break;
        case CLI_CONNECT: // CONNECT tunnelling
            print_info("proxy: client is in CONNECT state");
            if (FD_ISSET(client->socket, &proxy->readfds)) {
                ret = Proxy_handleConnect(client->socket, client->query->socket);
            }

            if (ret == EXIT_SUCCESS && client != NULL && client->query != NULL &&
                FD_ISSET(client->query->socket, &proxy->readfds)) {
                ret = Proxy_handleConnect(client->query->socket, client->socket);
            }
            break;
        case CLI_SSL:
            // print_info("proxy: client is in SSL state");
            // if (FD_ISSET(client->socket, &proxy->readfds)) {
            //     ret = Proxy_handleSSL(client->socket, client->query->socket);
            // }

            // if (ret == EXIT_SUCCESS && client != NULL && client->query != NULL &&
            //     FD_ISSET(client->query->socket, &proxy->readfds)) {
            //     ret = Proxy_handleSSL(client->query->socket, client->socket);
            // }
            // break;
        }

        /* checking for events/errors */
        if (ret != EXIT_SUCCESS) {
            ret = Proxy_handleEvent(proxy, client, ret);
            if (ret != EXIT_SUCCESS) {
                return ret;
            }
        }
    }

    /* handling query socket (connection with server) */
    // refresh(); // TODO - update staleness of cache entries
    return EXIT_SUCCESS;
}

/* Proxy_handleListener
 *    Purpose: handles the listener socket and accepts new connections
 * Parameters: @proxy - the proxy with listener socket ready to accept
 *    Returns: 0 on success, -1 on failure
 */
int Proxy_handleListener(struct Proxy *proxy)
{
    int ret;
    if ((ret = Proxy_accept(proxy)) < 0) {
        Proxy_handleEvent(proxy, NULL, ret);
    }

    return EXIT_SUCCESS;
}

/** Proxy_handleClient
 *    Purpose: Handles a client socket. Receives request from the client and stores it in the
 *             client's buffer. If the client has sent a complete request, a new Query object is
 *             created and stored in the client's query field. The request is parsed for the type
 *             of request and the appropriate handler is called.
 */
int Proxy_handleClient(struct Proxy *proxy, Client *client)
{
    int ret;
    char recv_buffer[BUFFER_SZ + 1];
    memset(recv_buffer, 0, BUFFER_SZ + 1);

    int num_bytes = recv(client->socket, recv_buffer, BUFFER_SZ, 0);
    if (num_bytes < 0) { // error recv 
        print_error("proxy: recv failed");
        perror("recv");
        return ERROR_RECV; // TODO - if recv fails, just close the socket
    } else if (num_bytes == 0) {
        print_info("proxy: client disconnected");
        return CLIENT_CLOSE;
    }

    /* resize client buffer, include null-terminator, and copy recv'd bytes */
    client->buffer = realloc(client->buffer, client->buffer_l + num_bytes + 1);
    if (client->buffer == NULL) {
        return ERROR_FAILURE;
    }
    client->buffer[client->buffer_l + num_bytes] = '\0';

    /* copy data from recv_buffer to client buffer */
    memcpy(client->buffer + client->buffer_l, recv_buffer, num_bytes);
    client->buffer_l += num_bytes;
    fprintf(stderr, "[Proxy_handleClient]: client->buffer: \n%s\n", client->buffer);

    /* update last receive time to now */
    gettimeofday(&client->last_recv, NULL);

    if (HTTP_got_header(client->buffer)) {
        print_debug("proxy: got header");
        ret = Query_new(&client->query, client->buffer, client->buffer_l);
        if (ret != EXIT_SUCCESS) {
            print_error("proxy: failed to parse request");
            return ret;
        }

        char *key = get_key(client->query->req);
        if (client->query->req == NULL) {
            print_error("proxy: memory allocation failed request");
            free(key);
            return ERROR_FAILURE;
        }

        if (memcmp(client->query->req->method, GET, GET_L) == 0) {
            print_info("proxy: got GET request");
            /* check if we have the file in cache */
            Response *response = NULL;
            #if RUN_CACHE 
                response = Cache_get(proxy->cache, key);
            #endif 

            // if Entry Not null & fresh, serve from Cache (add Age field) // TODO - add 'freshness'
            #if RUN_CACHE
            if (response != NULL) {
                print_debug("proxy: serving from cache");

                /* get age of cache item */
                long a = Cache_get_age(proxy->cache, key);
                char age[MAX_DIGITS_LONG + 1];
                zero(age, MAX_DIGITS_LONG + 1);
                snprintf(age, MAX_DIGITS_LONG, "%ld", a);

                /* add 'Age' field to HTTP header */
                size_t response_size = Response_size(response);
                char *raw_response   = Response_get(response); // original in cache

                char *response_dup = calloc(response_size + 1, sizeof(char));
                memcpy(response_dup, raw_response, response_size);

                // if (HTTP_add_field(&raw_response, &response_size, "Age", age) < 0) {
                if (HTTP_add_field(&response_dup, &response_size, "Age", age) < 0) {
                    print_error("proxy: failed to add Age field to response");
                    return ERROR_FAILURE;
                }

                /* Color links in the response */
                #if (RUN_CACHE && RUN_COLOR)
                    char **key_array = Cache_getKeyList(proxy->cache);
                    int num_keys = (int) proxy->cache->size;
                    if (color_links(&response_dup, &response_size, 
                        key_array, num_keys) != 0) {
                        fprintf(stderr, "[Proxy_handleClient]: couldn't color hyperlinks.\n");
                    } else {
                        fprintf(stderr, "[Proxy_handleClient]: colored hyperlinks.\n");
                    }
                #endif

                // if (Proxy_send(client->socket, raw_response, response_size) < 0) {
                if (Proxy_send(client->socket, response_dup, response_size) < 0) {
                    print_error("proxy: send failed");
                    free(key);
                    return ERROR_SEND;
                }
                free(key);
                free(response_dup);
                return CLIENT_CLOSE; // non-persistent connection
            } else {
            #endif 

                print_debug("proxy: serving from server");
                Query_print(client->query);
                ssize_t n = Proxy_fetch(proxy, client->query);
                if (n < 0) {
                    print_error("proxy: fetch failed");
                    free(key);
                    
                    return n;
                }
                (void) response;

            #if RUN_CACHE
            }
            #endif 
            client->state = CLI_GET;
        } else if (memcmp(client->query->req->method, CONNECT, CONNECT_L) == 0) { // CONNECT
            print_info("proxy: got CONNECT request");

            /* 5. Check if client wants to initiate a TLS/SSL handshake */
            #if RUN_SSL
                fprintf(stderr, "[Proxy_handleClient] Checking for a SSL/TLS connection...\n");
                client->ssl = SSL_new(proxy->ctx);
                SSL_set_fd(client->ssl, client->socket);
                int ssl_acc_ret = SSL_accept(client->ssl);
                if (ssl_acc_ret <= 0) {
                    fprintf(stderr, "[Proxy_handleClient] Not an SSL/TLS connection. Shutting down SSL\n");
                    // fprintf(stderr, "[Proxy_handleClient] SSL_get_error() = %d; ssl_acc_ret = %d\n",
                    // SSL_get_error(client->ssl, ssl_acc_ret), ssl_acc_ret); fprintf(stderr, "[Proxy_handleClient]
                    // ERR_print_errors_fp():\n"); ERR_print_errors_fp(stderr);
                    char err[256];
                    memset(err, 0, 256);
                    ERR_error_string((unsigned long)SSL_get_error(client->ssl, ssl_acc_ret), err);
                    fprintf(stderr, "[Proxy_handleClient] err: %s\n", err);

                    Client_clearSSL(client);
                } else {
                    fprintf(stderr, "[Proxy_handleClient] Client requested a SSL/TLS connection\n");
                }
            #endif

            int n;
            #if RUN_SSL
            if (client->ssl == NULL) { // tunneling
            #endif 
                n             = Proxy_fetch(proxy, client->query);
                client->state = CLI_CONNECT;
                fprintf(stderr, "[Proxy_handleClient] set Client state to CLI_CONNECT\n"); 
            #if RUN_SSL
            } else { // client->ssl != NULL,
               
                    n             = Proxy_SSLconnect(proxy, client->query);
                    client->state = CLI_SSL;
                    fprintf(stderr, "[Proxy_handleClient] set Client state to CLI_SSL\n");
                
            }
            #endif 

            if (n < 0) {
                print_error("proxy: fetch/ssl_connect fail");
                free(key);
                return n;
            } else {
                size_t response_l = 0;
                char response[BUFFER_SZ];
                zero(response, BUFFER_SZ);
                memcpy(response + response_l, STATUS_200CE, STATUS_200CE_L);
                response_l += STATUS_200CE_L;
                if (Proxy_send(client->socket, response, response_l) < 0) {
                    print_error("proxy: send failed");
                    free(key);
                    return ERROR_SEND;
                }

                print_success("proxy: sent 200 Connection established");
            }

            clear_buffer(client->buffer, &client->buffer_l);

        } else {
            print_error("Unknown method : unimplemented method");
            free(key);
            return HALT;
        }
        free(key);
    }

    // Check for Body

    // assume all GETs for now, GETs don't have a body
    // If we have both call HandleRequest -- determine what to do depending on
    // HTTP Method

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
int Proxy_handleQuery(Proxy *proxy, Query *query)
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

    ssize_t n;
    size_t buffer_l  = query->buffer_l;
    size_t buffer_sz = query->buffer_sz;
    fprintf(stderr, "%s[?] proxy: buffer_l: %ld%s\n", YEL, buffer_l, reset);

    /* read from socket */
    fprintf(stderr, "BUFFER_SZ = %ld BUFFER_L = %ld\n", buffer_sz, buffer_l);
    fprintf(stderr, "Receiving %ld bytes\n", buffer_sz - buffer_l);
    n = recv(query->socket, query->buffer + buffer_l, buffer_sz - buffer_l, 0);
    fprintf(stderr, "n = %ld\n", n);
    if (n == -1) {
        print_error("prxy: recv failed");
        perror("recv");
        return ERROR_RECV;
    // } else if (n == 0 || (size_t)n < (buffer_sz - buffer_l)) {
    } else if (n == 0) {
        fprintf(stderr, "[handleQuery] n: %ld; buffer_sz - buffer_l: %ld\n", n, buffer_sz - buffer_l);
        // ? does this mean we have the full response
        query->buffer_l += n;
        query->res = Response_new(query->req->method, query->req->method_l, query->req->path, query->req->path_l,
                                  query->buffer, query->buffer_l);
        if (query->res == NULL) {
            print_error("proxy: failed to create response");
            return INVALID_RESPONSE;
        }
    } else {
        fprintf(stderr, "%s[*] proxy: read %ld bytes from server not done!!!!%s\n", CYN, n, reset);
        query->buffer_l += n;
        if (query->buffer_l == query->buffer_sz) {
            expand_buffer(&query->buffer, &query->buffer_l, &query->buffer_sz);
        }
    }

    return EXIT_SUCCESS;
}

/** Proxy_handleConnect
 *    Purpose: Handles CONNECT tunneling - takes byte stream from sender socket and forwards to
 *             receiver socket without parsing or inspecting bytes.
 * Parameters: @sender - socket to read bytes from
 *             @receiver - socket to write bytes to
 *    Returns: EXIT_SUCCESS on success, or error code indicating type of failure
 */
int Proxy_handleConnect(int sender, int receiver)
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


// int Proxy_handleSSL(int sender, int receiver)
// {
//     char buffer[BUF_SIZE];
//     int ret = EXIT_SUCCESS;

//     memset(buffer, 0, BUF_SIZE);
//     ret = recv(socket1, buffer, BUF_SIZE, 0);
//     if (ret < 0) {
//         print_error("proxy: failed to receive data from socket");
//         return ERROR_FAILURE;
//     }

//     ret = send(socket2, buffer, ret, 0);
//     if (ret < 0) {
//         print_error("proxy: failed to send data to socket");
//         return ERROR_FAILURE;
//     }

//     return EXIT_SUCCESS;
// }

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
    case HOST_UNKNOWN:
        print_error("proxy: invalid request");
        Proxy_sendError(client, BAD_REQUEST_400);
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



// TODO
// int Proxy_handleTimeout(struct Proxy *proxy) {
//     (void) proxy;
//     return 0;
// }

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
        proxy->readfds = proxy->master_set;
        print_debug("proxy: waiting for connections");
        ret = select(proxy->fdmax + 1, &(proxy->readfds), NULL, NULL, proxy->timeout);
        fprintf(stderr, "[Proxy_run] select returned: %d\n", ret);
        switch (ret) {
        case SELECT_ERROR:
            print_error("proxy: select failed");
            return ERROR_FAILURE;
        case SELECT_TIMEOUT:                // TODO: TIMEOUT HANDLER
            print_warning("proxy: select timed out");
            ret = EXIT_SUCCESS;
            // Proxy_handleTimeout(&proxy);
            break;
        default:
            /* check listening socket and accept new clients */
            if (FD_ISSET(proxy->listen_fd, &(proxy->readfds))) {
                if ((ret = Proxy_handleListener(proxy)) != EXIT_SUCCESS) {
                    ret = Proxy_handleEvent(proxy, NULL, ret);
                    if (ret != EXIT_SUCCESS) {
                        return ret;
                    }
                }
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

