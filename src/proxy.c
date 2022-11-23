#include "proxy.h"

// Ports -- 9055 to 9059
static char *get_key(Request *req);

int Proxy_run(short port, size_t cache_size)
{
    struct Proxy proxy;

    /* initialize the proxy */
    Proxy_init(&proxy, port, cache_size);
    fprintf(stderr, "%s[+] HTTP Proxy -----------------------------------%s\n", BCYN, reset);
    fprintf(stderr, "%s[*]%s   Proxy Port = %d\n", YEL, reset, proxy.port);
    fprintf(stderr, "%s[*]%s   Cache Size = %ld\n", YEL, reset, proxy.cache->capacity);

    /* open listening socket */
    proxy.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy.listen_fd == -1) {
        print_error("proxy: socket failed\n");
        Proxy_free(&proxy);
        return ERROR_FAILURE;
    }

    /* set socket options */
    int optval = 1;
    if (setsockopt(proxy.listen_fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(optval)) == -1)
    {
        print_error("proxy: setsockopt failed\n");
        Proxy_free(&proxy);
        return ERROR_FAILURE;
    }

    /* build proxy address (type is: struct sockaddr_in) */
    bzero((char *)&(proxy.addr), sizeof(proxy.addr));
    proxy.addr.sin_family      = AF_INET;
    proxy.addr.sin_addr.s_addr = htonl(INADDR_ANY);
    proxy.addr.sin_port        = htons(proxy.port);

    /* bind listening socket to proxy address and port */
    if (bind(proxy.listen_fd, (struct sockaddr *)&(proxy.addr), sizeof(proxy.addr)) == -1) {
        print_error("proxy: bind failed\n");
        Proxy_free(&proxy);
        return ERROR_FAILURE;
    }

    /* listen for connections */
    if (listen(proxy.listen_fd, 10) == -1) {
        print_error("proxy: listen failed\n");
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
        int select_value = select(proxy.fdmax + 1, &(proxy.readfds), NULL, NULL, proxy.timeout);
        if (select_value < 0) {
            print_error("proxy: select failed\n");
            Proxy_free(&proxy);
            return ERROR_FAILURE;
        } else if (select_value == 0) {
            // TODO: TIMEOUT
            Proxy_handleTimeout(&proxy);
        } else {
            ret = Proxy_handle(&proxy);
        }

        if (ret == HALT) {
            fprintf(stderr, "%s[+] Halt signal received.%s\n", BCYN, reset);
            break; // HALT signal
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
            print_error("proxy: listen failed:\n");
            return HALT;
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
            if (ret != 0) {
                goto event_handling;
            }
        }

        if (client != NULL && client->query != NULL &&
            FD_ISSET(client->query->socket, &proxy->readfds)) {
            fprintf(stderr, "[Proxy_handle] query socket is %d\n", client->query->socket);
            // Request_print(client->query->req); // TODO - remove debug
            ret = Proxy_handleQuery(proxy, client->query);
            if (ret != 0) {
                goto event_handling;
            }
            // If response is finished cache and send to client
            if (client->query->res != NULL) {
                fprintf(stderr, "[Proxy_handle] query response not null\n");
                // TODO - add response to cache and send response to client
                Proxy_send(client->query->res->raw, client->query->res->raw_l, client->socket);
                char *key = get_key(client->query->req);
                Response_print(client->query->res); // TODO - remove debug
                // Response *cache_res = Response_copy(client->query->res);

                /**
                 * if Response isn't for a CONNECT request, may cache response
                 */
                // char *method = client->query->req->method;
                if (memcmp(client->query->req->method, GET, GET_L) == 0) {
                    Response *cache_res = Response_copy(client->query->res);
                    fprintf(stderr, "%s[?] Caching response%s\n", YEL, reset);
                    int debug = Cache_put(proxy->cache, key, cache_res, cache_res->max_age);
                    fprintf(stderr, "%s[?] Cache_put returned %d%s\n", YEL, debug, reset);
                }

                free(key);
                ret = CLOSE_CLIENT;
            } else {
                // TODO - keep selecting on query socket
            }
        }

    event_handling:
        ret = Proxy_event_handle(proxy, client, ret);
        if (ret == HALT) {
            return HALT;
        }

        // refresh(); // TODO
    }

    return EXIT_SUCCESS;
}

int Proxy_event_handle(Proxy *proxy, Client *client, int error_code)
{
    if (proxy == NULL || error_code == 0) {
        return ERROR_FAILURE;
    }

    switch (error_code) {
    case ERROR_FAILURE:
        fprintf(stderr, "%s[!]%s proxy: error\n", RED, reset);
        print_error("proxy: failed\n");
        return HALT;
    case INVALID_REQUEST:
        print_error("proxy: invalid request\n");
        Proxy_close(client->socket, &proxy->master_set, proxy->client_list, client);
        break;
    case INVALID_RESPONSE:
        print_error("proxy: invalid response\n");
        Proxy_close(client->socket, &proxy->master_set, proxy->client_list, client);
        break;
    case CLOSE_CLIENT:
        fprintf(stderr, "%s[*] proxy: closing client %d%s\n", CYN, client->socket, reset);
        Proxy_close(client->socket, &proxy->master_set, proxy->client_list, client);
        break;
    case ERROR_CONNECT:
        print_error("proxy: connect failed\n");
        Proxy_close(client->socket, &proxy->master_set, proxy->client_list, client);
        break;
    default:
        fprintf(stderr, "%s[!]%s proxy: unknown error: %d\n", RED, reset, error_code);
        return HALT;
    }

    return EXIT_SUCCESS;
}

int Proxy_handleQuery(Proxy *proxy, Query *query)
{
    if (proxy == NULL || query == NULL) {
        return ERROR_FAILURE;
    }

    int n;
    size_t buffer_l  = query->buffer_l;
    size_t buffer_sz = query->buffer_sz;
    fprintf(stderr, "%s[?] proxy: buffer_l: %ld%s\n", YEL, buffer_l, reset);

    /* read from socket */
    n = recv(query->socket, query->buffer + buffer_l, buffer_sz - buffer_l, 0);
    if (n == -1) {
        print_error("prxy: recv failed: ");
        perror("recv");
        return ERROR_FAILURE;
    } else if (n == 0 || (size_t)n < buffer_sz - buffer_l) {
        fprintf(stderr, "%s[*] proxy: server finished sending response%s\n", CYN, reset);
        // ? does this mean we have the full response
        query->buffer_l += n;
        FD_CLR(query->socket, &proxy->master_set); // TODO - don't parse responses from CONNECT
        query->res = Response_new(query->req->method, query->req->method_l, query->req->path,
                                  query->req->path_l, query->buffer, query->buffer_l);
    } else {
        fprintf(stderr, "%s[*] proxy: read %d bytes from server%s\n", CYN, n, reset);
        query->buffer_l += n;
        if (query->buffer_l == query->buffer_sz) {
            expand_buffer(&query->buffer, &query->buffer_l, &query->buffer_sz);
        }
    }

    return EXIT_SUCCESS;
}

ssize_t Proxy_send(char *buffer, size_t buffer_l, int socket)
{
    if (buffer == NULL) {
        print_error("proxy_send: buffer is null");
        return ERROR_FAILURE;
    }

    /* write request to server */
    ssize_t written  = 0;
    ssize_t to_write = buffer_l;
    while (written < (ssize_t)buffer_l) {
        ssize_t n = send(socket, buffer + written, to_write, 0);
        if (n == -1) {
            print_error("proxy: send failed\n");
            return ERROR_FAILURE;
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

int Proxy_init(struct Proxy *proxy, short port, size_t cache_size)
{
    if (proxy == NULL) {
        print_error("proxy: null parameter passed to init call\n");
        return ERROR_FAILURE;
    }

    /* initialize the proxy cache */
    proxy->cache = Cache_new(cache_size, Response_free, Response_print, Response_compare);
    if (proxy->cache == NULL) {
        print_error("proxy: cache init failed\n");
        return ERROR_FAILURE;
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

    /* zero out fd_sets and initialize max fd of master_set */
    FD_ZERO(&(proxy->master_set)); /* clear the sets */
    FD_ZERO(&(proxy->readfds));
    proxy->fdmax   = 0;
    proxy->timeout = NULL;

    /* initialize client list */
    proxy->client_list = List_new(Client_free, Client_print, Client_compare);
    if (proxy->client_list == NULL) {
        print_error("proxy: failed to initialize client list\n");
        return ERROR_FAILURE;
    }

    return EXIT_SUCCESS;
}

void Proxy_free(void *proxy)
{
    if (proxy == NULL) {
        return;
    }

    struct Proxy *p = (struct Proxy *)proxy;

    /* free the proxy */
    Cache_free(&p->cache);
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

    p->port = 0;
}

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
    Cache_print(proxy->cache);
    List_print(proxy->client_list);
}

/**
 * 1. accepts connection request from new client
 * 2. creates a client object for new client
 * 3. puts new client's fd in master set
 * 4. update the value of fdmax
 */
int Proxy_accept(struct Proxy *proxy)
{
    /* accept connection request from new client */
    Client *cli = Client_new();
    cli->addr_l = sizeof(cli->addr);
    cli->socket = accept(proxy->listen_fd, (struct sockaddr *)&(cli->addr), &(cli->addr_l));
    if (cli->socket == -1) {
        print_error("proxy: accept failed\n");
        return ERROR_FAILURE;
    }

    /* add client to the proxy's client list */
    List_push_back(proxy->client_list, (void *)cli);

    /* put new clients's socket in master set */
    FD_SET(cli->socket, &(proxy->master_set));

    /* 4. updates the max fd */
    proxy->fdmax = cli->socket > proxy->fdmax ? cli->socket : proxy->fdmax;

    return EXIT_SUCCESS;
}

/* Proxy_handleListener
 *    Purpose: handles the listener socket and accepts new connections
 * Parameters: @proxy - the proxy with listener socket ready to accept
 *    Returns: 0 on success, -1 on failure
 */
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
        print_error("proxy: recv failed: ");
        perror("recv");
        return ERROR_FAILURE;
    } else if (num_bytes == 0) {
        fprintf(stderr, "%s[*] proxy: client closed connection%s\n", YEL, reset);

        return CLOSE_CLIENT;
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

    /* update last receive time to now */
    gettimeofday(&client->last_recv, NULL);

    /* check for http header, parse header if we have it */
    if (HTTP_got_header(client->buffer)) {
        client->query = Query_new(client->buffer, client->buffer_l);
        if (client->query == NULL) {
            print_error("proxy: failed to parse query\n");
            return INVALID_REQUEST;
        }
        char *key = get_key(client->query->req);
        if (client->query->req == NULL) {
            print_error("proxy: memory allocation failed request\n");
            free(key);
            return ERROR_FAILURE;
        }

        if (memcmp(client->query->req->method, GET, GET_L) == 0) { // GET
            fprintf(stderr, "%s[+] GET Request Received%s\n", GRN, reset);

            /* check if we have the file in cache */
            Response *response = Cache_get(proxy->cache, key);

            // if Entry Not null & fresh, serve from Cache (add Age field)
            if (response != NULL) {
                fprintf(stderr, "%s[?] Entry in cache%s\n", YEL, reset);
                int response_size  = Response_size(response);
                char *raw_response = Response_get(response);
                if (Proxy_send(raw_response, response_size, client->socket) < 0) {
                    print_error("proxy: send failed\n");
                    free(key);
                    return ERROR_FAILURE;
                }
                free(key);
                return CLOSE_CLIENT; // non-persistent
            } else {
                fprintf(stderr, "%s[?] Entry NOT in cache%s\n", YEL, reset);
                Query_print(client->query);
                ssize_t n = Proxy_fetch(proxy, client->query);
                if (n < 0) {
                    print_error("proxy: fetch failed\n");
                    free(key);
                    return n;
                }
            }
        } else if (memcmp(client->query->req->method, CONNECT, CONNECT_L) == 0) { // CONNECT
            fprintf(stderr, "%s[+] CONNECT Request Received%s\n", GRN, reset);

            // 1. establish connection on client-proxy side
            // --> already done when we accepted a client_fd

            // 2. establish connection on proxy-server side (Proxy_fetch?)
            // --> forward CONNECT request to server

            ssize_t n = Proxy_fetch(proxy, client->query);
            if (n < 0) {
                print_error("proxy: fetch failed");
                free(key);
                return n;
            }

            // free buffer here?

        } else {
            print_error("Unknown method : unimplemented (halt)");
            free(key);
            return HALT;
        }
        free(key);
    }

    // Check for Body

    // assume all GETs for now, GETs don't have a body
    // If we have both call HandleRequest -- determine what to do depnding on
    // HTTP Method

    return 0;
}

// TODO
int Proxy_handleTimeout(struct Proxy *proxy) { return 0; }

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
    fprintf(stderr, "%s[?] Closing Socket # = %d%s\n", YEL, socket, reset);
    close(socket);

    fprintf(stderr, "%s[?] Remove Socket # from master_set = %d%s\n", YEL, socket, reset);
    /* remove the client from master fd_set */
    FD_CLR(socket, master_set);

    /* mark the node for removal from trackers */
    fprintf(stderr, "%s[?] Remove Socket # from client_list = %d%s\n", YEL, socket, reset);
    List_remove(client_list, (void *)client);
}

/**
 * makes ONE request to a server for a resource in the given request.
 * Writes to a server and then closes conenction.
 */
ssize_t Proxy_fetch(Proxy *proxy, Query *qry)
{
    if (proxy == NULL || qry == NULL) {
        print_error("proxy: fetch failed, invalid args\n");
        return ERROR_FAILURE;
    }

    /* make connection to server */
    if (connect(qry->socket, (struct sockaddr *)&qry->server_addr, sizeof(qry->server_addr)) < 0) {
        print_error("proxy: failed to connect to server: ");
        perror("connect");
        return ERROR_CONNECT;
    }

    /* add server socket to master_set */
    FD_SET(qry->socket, &proxy->master_set);
    proxy->fdmax = (qry->socket > proxy->fdmax) ? qry->socket : proxy->fdmax;

    /* send message to server */
    print_ascii(qry->req->raw, qry->req->raw_l);
    ssize_t msgsize = send(qry->socket, qry->req->raw, qry->req->raw_l, 0);
    if ((size_t)msgsize != qry->req->raw_l) {
        print_error("proxy: sending to host failed\n");
        return ERROR_FAILURE;
    }

    /* wait for response from server */
    // ssize_t n = recv(qry->socket, qry->buffer + qry->buffer_l, qry->buffer_sz - qry->buffer_l,
    // 0); fprintf(stderr, "Fetch recv is : %ld\n", n);

    return msgsize; // number of bytes sent
}

/* Static Functions --------------------------------------------------------- */
static char *get_key(Request *req)
{
    if (req == NULL) {
        return NULL;
    }

    char *key = calloc(req->host_l + req->path_l + 1, sizeof(char));
    memcpy(key, req->host, req->host_l);
    memcpy(key + req->host_l, req->path, req->path_l);

    return key;
}