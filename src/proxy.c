#include "proxy.h"

// Ports -- 9055 to 9059
static char *get_key(Request *req);

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

        if (ret == HALT) {
            fprintf(stderr, "[DEBUG] proxy: halt\n");
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
            fprintf(stderr, "[!] proxy: handleListener failed\n");
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
                fprintf(stderr, "GOING TO EVENT HANDLING\n");
                goto event_handling;
            }
        }

        if (client != NULL && client->query != NULL &&
            FD_ISSET(client->query->socket, &proxy->readfds))
        {
            ret = Proxy_handleQuery(proxy, client->query);
            if (ret != 0) {
                goto event_handling;
            }
            // If response is finished cache and send to client
            if (client->query->res != NULL) {
                // TODO - add response to cache and send response to client
                Proxy_send(client->query->res->raw, client->query->res->raw_l,
                           client->socket);
                char *key = get_key(client->query->req);
                // Response_print(client->query->res);
                Response *cache_res = Response_copy(client->query->res);

                /**
                 * if Response isn't for a CONNECT request, may cache response
                 */
                // char *method = client->query->req->method;
                if (memcmp(client->query->req->method, GET, GET_L) == 0) {
                    fprintf(stderr, "%s[Proxy_handle] Caching response%s\n", BYEL, reset);
                    int debug = Cache_put(proxy->cache, key, cache_res, cache_res->max_age);
                    fprintf(stderr, "%s[Proxy_handle] Cache_put returned %d%s\n", BYEL, debug, reset);
                } else {
                    Response_free(cache_res);
                }

                free(key);
                ret = CLOSE_CLIENT;
            } else {
                // TODO - keep selecting
                fprintf(stderr, "[DEBUG] Full response not received...\n");
            }
        }

        event_handling:
        ret = Proxy_event_handle(proxy, client, ret);
        if (ret == HALT) {
            return HALT;
        }
        // /* check for events & errors */
        // if (ret == CLOSE_CLIENT) {
        //     fprintf(stderr, "[DEBUG] Closing client\n");
        //     Proxy_close(client->socket, &proxy->master_set, proxy->client_list,
        //                 client);
        // } else if (ret == HALT) {
        //     fprintf(stderr, "[DEBUG] proxy: halt\n");
        //     return HALT;
        // } else if (ret < 0) {
        //     fprintf(stderr, "[DEBUG] proxy: error\n");
        //     ret = Proxy_event_handle(proxy, client, ret);
        //     if (ret == HALT) {
        //         return HALT;
        //     }
        // } 

        // refresh(); // TODO
    }

    return EXIT_SUCCESS;
}

int Proxy_event_handle(Proxy *proxy, Client *client, int error_code)
{
    if (proxy == NULL || error_code == 0) {
        return ERROR_FAILURE;
    }
    
    switch(error_code) {
        case ERROR_FAILURE:
            fprintf(stderr, "[!] proxy: error\n");
            return HALT;
        case INVALID_REQUEST:
            fprintf(stderr, "[!] proxy: invalid request\n");
            Proxy_close(client->socket, &proxy->master_set, proxy->client_list,
                        client);
            break;
        case INVALID_RESPONSE:
            fprintf(stderr, "[!] proxy: invalid response\n");
            Proxy_close(client->socket, &proxy->master_set, proxy->client_list,
                        client);
            break;
        case CLOSE_CLIENT:
            fprintf(stderr, "[*] proxy: client %d\n", client->socket);
            Proxy_close(client->socket, &proxy->master_set, proxy->client_list,
                        client);
            break;
        case ERROR_CONNECT:
            fprintf(stderr, "[!] proxy: connect failed\n");
            Proxy_close(client->socket, &proxy->master_set, proxy->client_list,
                        client);
            break;
        default:
            fprintf(stderr, "[!] proxy: unknown error: %d\n", error_code);
            return HALT;
    }

    return EXIT_SUCCESS;
}

int Proxy_handleQuery(Proxy *proxy, Query *query)
{
    if (proxy == NULL || query == NULL) {
        return ERROR_FAILURE;
    }

    int ret;
    size_t buffer_l  = query->buffer_l;
    size_t buffer_sz = query->buffer_sz;

    /* read from socket */
    ret =
        recv(query->socket, query->buffer + buffer_l, buffer_sz - buffer_l, 0);
    if (ret == -1) {
        fprintf(stderr, "[!] proxy: recv failed\n");
        return ERROR_FAILURE;
    } else if (ret == 0) {
        fprintf(stderr, "[!] proxy: server connection closed\n");
        // ? does this mean we have the full response
        FD_CLR(query->socket, &proxy->master_set);
        query->res = Response_new(query->buffer, buffer_l);
    } else {
        query->buffer_l += ret;
        if (query->buffer_l == query->buffer_sz) {
            expand_buffer(&query->buffer, &query->buffer_l, &query->buffer_sz);
        }
    }

    return EXIT_SUCCESS;
}

ssize_t Proxy_send(char *buffer, size_t buffer_l, int socket)
{
    fprintf(stderr, "%s[BEGIN PROXY SEND]%s\n", GRN, reset);
    if (buffer == NULL) {
        fprintf(stderr, "[!] proxy: null parameter passed to send call\n");
        return ERROR_FAILURE;
    }

    /* write request to server */
    ssize_t written  = 0;
    ssize_t to_write = buffer_l;
    while (written < (ssize_t)buffer_l) {
        ssize_t n = send(socket, buffer + written, to_write, 0);
        if (n == -1) {
            fprintf(stderr, "[!] proxy: write failed: %s\n", strerror(errno));
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

// ssize_t Proxy_recv(struct Proxy *proxy, int socket)
// {
//     if (proxy == NULL) {
//         fprintf(stderr, "[!] proxy: null parameter passed to receive
//         call\n"); return -1;
//     }

//     ssize_t recvd     = 0;
//     size_t to_recv    = proxy->buffer_sz - proxy->buffer_l - 1;
//     size_t body_recvd = 0;
//     clear_buffer(proxy->buffer, &proxy->buffer_l);
//     while ((recvd = read(socket, proxy->buffer + proxy->buffer_l, to_recv)) >
//     0)
//     {
//         /* update buffer length */
//         proxy->buffer_l += recvd;

//         /* check if buffer is full */
//         if (proxy->buffer_l == (proxy->buffer_sz - 1)) {
//             if (expand_buffer(&proxy->buffer, &proxy->buffer_l,
//             &proxy->buffer_sz) == -1) {
//                 fprintf(stderr, "[!] proxy: failed to expand buffer\n");
//                 return ERROR_FAILURE;
//             }
//         }

//         /* update number of bytes to receive */
//         to_recv = proxy->buffer_sz - proxy->buffer_l - 1;

//         /* look for end of header */
//         char *end_of_header = strstr(proxy->buffer, "\r\n\r\n");
//         if (end_of_header != NULL) {
//             char *body = end_of_header + 4;
//             body_recvd += proxy->buffer_l - (body - proxy->buffer);
//             break;
//         }
//     }

//     if (recvd == -1) {
//         fprintf(stderr, "[!] proxy: recv failed\n");
//         return ERROR_FAILURE;
//     }

//     /* get content length */
//     size_t body_l            = 0;
//     char *content_length_str = strstr(proxy->buffer, "Content-Length: ");
//     if (content_length_str != NULL) {
//         char content_length[11];
//         zero(content_length, 11);
//         content_length_str += 16;
//         char *end = strstr(content_length_str, "\r\n");
//         if (end == NULL) {
//             fprintf(stderr, "[!] proxy : invalid Content-Length header\n");
//             return -1;
//         }
//         if (end - content_length_str > 10) {
//             fprintf(stderr, "[!] proxy: Content-Length too large\n");
//             return -1;
//         }
//         memcpy(content_length, content_length_str, end - content_length_str);
//         body_l = atoi(content_length);
//     } else {
//         body_l = 0;
//     }

//     /* read body */
//     while (body_recvd < body_l) {
//         recvd = read(socket, proxy->buffer + proxy->buffer_l, to_recv);
//         proxy->buffer_l += recvd;
//         body_recvd += recvd;
//         if (recvd == -1) {
//             fprintf(stderr, "[!] proxy: read failed\n");
//             return -1;
//         }

//         /* check if buffer is full */
//         if (proxy->buffer_l == (proxy->buffer_sz - 1)) {
//             if (expand_buffer(&proxy->buffer, &proxy->buffer_l,
//             &proxy->buffer_sz) == -1) {
//                 fprintf(stderr, "[!] proxy: failed to expand buffer\n");
//                 return ERROR_FAILURE;
//             }
//         }

//         to_recv = proxy->buffer_sz - proxy->buffer_l - 1;
//     }

//     return proxy->buffer_l;
// }

int Proxy_init(struct Proxy *proxy, short port, size_t cache_size)
{
    if (proxy == NULL) {
        fprintf(stderr, "[!] proxy: init was passed a NULL proxy\n");
        return ERROR_FAILURE;
    }

    /* initialize the proxy cache */
    proxy->cache =
        Cache_new(cache_size, Response_free, Response_print, Response_compare);
    if (proxy->cache == NULL) {
        fprintf(stderr, "[!] proxy: failed to initialize cache\n");
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

    fprintf(stderr, "[Proxy]\n");
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
    cli->socket = accept(proxy->listen_fd, (struct sockaddr *)&(cli->addr),
               &(cli->addr_l));
    if (cli->socket == -1) {
        fprintf(stderr, "[!] Proxy_accept: accept failed\n");
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
        fprintf(stderr, "[!] proxy: recv failed\n");
        perror("recv");
        return ERROR_FAILURE;
    } else if (num_bytes == 0) {
        fprintf(stderr, "[*] proxy: client closed connection\n");

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
            fprintf(stderr, "[!] proxy: failed to parse query\n");
            return INVALID_REQUEST;
        }
        char *key     = get_key(client->query->req);
        fprintf(stderr, "[DEBUG] QUERY KEY: %s\n", key);
        if (client->query->req == NULL) {
            fprintf(stderr, "[!] proxy: memory allocation failed request\n");
            free(key);
            return ERROR_FAILURE;
        }

        if (memcmp(client->query->req->method, GET, GET_L) == 0) { // GET
            fprintf(stderr, "[*] GET\n");

            /* check if we have the file in cache */
            Response *response = Cache_get(proxy->cache, key);

            // if Entry Not null & fresh, serve from Cache (add Age field)
            if (response != NULL) {
                fprintf(stderr, "[DEBUG] Entry in cache\n");
                int response_size  = Response_size(response);
                proxy->buffer      = calloc(response_size, sizeof(char));
                char *raw_response = Response_get(response);
                memcpy(proxy->buffer, raw_response, response_size);
                proxy->buffer_l = response_size;
                if (Proxy_send(proxy->buffer, proxy->buffer_l,
                               client->socket) == -1)
                {
                    fprintf(stderr, "[!] Error: proxy: send\n");
                    free(key);
                    return ERROR_FAILURE;
                }
                free(key);
                return CLOSE_CLIENT;    // non-persistent
            } else {
                fprintf(stderr, "[DEBUG] Entry NOT in cache\n");
                ssize_t n = Proxy_fetch(proxy, client->query);
                if (n < 0) {
                    fprintf(stderr, "[!] proxy: fetch failed\n");
                    free(key);
                    return n;
                }
            }

            /* clean up */
            free_buffer(&proxy->buffer, &proxy->buffer_l, &proxy->buffer_sz);
        } else if (memcmp(client->query->req->method, CONNECT, CONNECT_L) == 0) { // CONNECT
            fprintf(stderr, "[*] CONNECT : unimplemented\n");

            // 1. establish connection on client-proxy side 
            // --> already done when we accepted a client_fd 

            // 2. establish connection on proxy-server side (Proxy_fetch?)
            // --> forward CONNECT request to server 
            ssize_t n = Proxy_fetch(proxy, client->query);
            if (n < 0) {
                fprintf(stderr, "[!] proxy: fetch failed\n");
                free(key);
                return n;
            }

            // free buffer here?

        } else {
            fprintf(stderr, "[DEBUG] Unknown method : unimplemented (halt)\n");
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
void Proxy_close(int socket, fd_set *master_set, List *client_list,
                 Client *client)
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
ssize_t Proxy_fetch(Proxy *proxy, Query *qry)
{
    if (proxy == NULL || qry == NULL) {
        fprintf(stderr, "[!] proxy: fetch failed, invalid args\n");
        return ERROR_FAILURE;
    }


    /* make connection to server */ 
    if (connect(qry->socket, (struct sockaddr *)&qry->server_addr,
                sizeof(qry->server_addr)) < 0)
    {
        perror("connect");
        fprintf(stderr, "[!] proxy: fetch failed: %s\n", strerror(errno));
        return ERROR_CONNECT;
    }

    /* add server socket to master_set */
    FD_SET(qry->socket, &proxy->master_set);
    proxy->fdmax = (qry->socket > proxy->fdmax) ? qry->socket : proxy->fdmax;


    /* send message to server */
    ssize_t msgsize = send(qry->socket, qry->req->raw, qry->req->raw_l, 0);
    if ((size_t)msgsize != qry->req->raw_l) {
        fprintf(stderr, "[!] proxy: error sending to host\n");
        return ERROR_FAILURE;
    }

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
