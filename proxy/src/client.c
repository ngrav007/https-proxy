#include "client.h"

/* Client_new
 *    Purpose: Creates a new Client, and returns a pointer to it. Client
 *             socket is initialized to -1, all other fields are initialized to
 *             NULL or 0.
 * Parameters: None
 *    Returns: Pointer to a new Client, or NULL if memory allocation fails.
 */
Client *Client_new()
{
    Client *client = calloc(1, sizeof(struct Client));
    if (client == NULL) {
        return NULL;
    }

    client->state             = CLI_INIT;
    client->query             = NULL;

    #if RUN_SSL
    client->ssl               = NULL;
    client->isSSL             = false;

    #endif 

    client->buffer            = calloc(BUFFER_SZ, sizeof(char));
    client->buffer_l          = 0;
    client->buffer_sz         = BUFFER_SZ;
    client->socket            = -1;
    client->last_active.tv_sec  = 0;
    client->last_active.tv_usec = 0;

    return client;
}

/* Client_create
 *    Purpose: Creates a new Client and initializes it with the given socket
 *             and client id. Client is considered logged in.
 * Parameters: @socket - Client's socket file descriptor
 *             @id - Client id
 *    Returns: Pointer to a new Client, or NULL if memory allocation fails.
 */
Client *Client_create(int socket)
{
    Client *client = Client_new();
    if (client == NULL) {
        return NULL;
    }

    if (Client_init(client, socket) != 0) {
        Client_free(&client);
        return NULL;
    }

    return client;
}

/* Client_init
 *    Purpose: Initializes a Client with the given socket and client id.
 *             When a Client is initialized
 * Parameters: @client - Pointer to a Client to initialize
 *             @socket - Client's socket file descriptor
 *             @id - Client id
 *    Returns: 0 on success, -1 on failure.
 *
 * Note: If id is too long, it will be truncated to fit in the buffer
 *       including the null terminator.
 */
int Client_init(Client *client, int socket)
{
    if (client == NULL || socket == -1) {
        return -1;
    }

    Client_setSocket(client, socket);
    client->query             = NULL;

    #if RUN_SSL
        client->ssl               = NULL;
    #endif 

    client->buffer            = NULL;
    client->buffer_l          = 0;
    client->socket            = -1;
    client->last_active.tv_sec  = 0;
    client->last_active.tv_usec = 0;
    #if RUN_SSL
        client->isSSL             = false;
    #endif

    return 0;
}

/* Client_free
 *    Purpose: Frees a Client
 * Parameters: @client - Pointer to a pointer to a Client to free
 *    Returns: None
 */
void Client_free(void *client)
{
    if (client == NULL) {
        return;
    }

    Client *c = (Client *)client;

    Client_clearQuery(c);

    #if RUN_SSL
        Client_clearSSL(c);
    #endif 

    
    close(c->socket);
    c->socket = -1;

    free_buffer(&c->buffer, &c->buffer_l, &c->buffer_sz);
    free(c);
}

/* Client_print
 *    Purpose: Prints a Client to stdout
 * Parameters: @client - Pointer to a Client to print
 *    Returns: None
 */
void Client_print(void *client)
{
    Client *c = (Client *)client;
    if (c == NULL) {
        printf("[Client] (null)\n");
    } else {
        fprintf(stderr, "%s[Client]%s\n", CYN, CRESET);
        fprintf(stderr, "  socket = %d\n", c->socket);
        #if RUN_SSL
            fprintf(stderr, "  ssl = %p\n", c->ssl);
            fprintf(stderr, "  isSSL = %s\n", (c->isSSL) ? "true" : "false");
        #endif
        fprintf(stderr, "  last_active = %ld.%ld\n", c->last_active.tv_sec,
                c->last_active.tv_usec);
        fprintf(stderr, "  buffer_l = %zd\n", c->buffer_l);
        Query_print(c->query);
    }
}

/* Client_compare
 *    Purpose: Compares a Client and a client id string to see if they are
 *             equal.
 * Parameters: @client1 - Pointer to a Client
 *             @id - Pointer to a socket file descriptor
 *    Returns: 1 (true) if the clients are equal, 0  otherwise. If either
 *             parameter is NULL, -1 is returned.
 */
int Client_compare(void *client1, void *client2)
{
    if (client1 == NULL || client2 == NULL) {
        return -1;
    }
    Client *c1 = (Client *)client1;
    Client *c2 = (Client *)client2;

    if (c1->socket == c2->socket) {
        return 1;
    }

    return 0;
}

/* Client_timestamp
 *    Purpose: Gets the timestamp of the last time a message was received from
 *             the client.
 * Parameters: @client - Pointer to a Client to timestamp
 *    Returns: 0 on success, -1 on failure.
 */
int Client_timestamp(Client *client)
{
    if (client == NULL) {
        return -1;
    }

    return gettimeofday(&client->last_active, NULL);
}

/* Client_setSocket
 *    Purpose: Sets the socket file descriptor for a Client, if the client's
 *             socket is not -1, it will be closed first.
 * Parameters: @client - Pointer to a Client
 *             @socket - Socket file descriptor
 *    Returns: 0 on success, -1 on failure.
 */
int Client_setSocket(Client *client, int socket)
{
    if (client == NULL || socket == -1) {
        return -1;
    }

    if (client->socket != -1) {
        close(client->socket);
    }

    client->socket = socket;

    return 0;
}

/* Client_setAddr
 *     Purpose: Copies the address of the client into the client struct for
 *              later use.
 *  Parameters: @client - Pointer to a Client
 *              @addr - Pointer to a sockaddr_in struct containing the client's
 *                      address
 *     Returns: 0 on success, -1 on failure.
 */
int Client_setAddr(Client *client, struct sockaddr_in *addr)
{
    if (client == NULL || addr == NULL) {
        return EXIT_FAILURE;
    }

    memcpy(&client->addr, addr, sizeof(struct sockaddr_in));
    client->addr_l = sizeof(*addr);

    return EXIT_SUCCESS;
}

/* Client_getSocket
 *    Purpose: Returns the socket file descriptor for a Client
 * Parameters: @client - Pointer to a Client
 *    Returns: Socket file descriptor, or -1 on failure.
 */
int Client_getSocket(Client *client)
{
    if (client == NULL) {
        return EXIT_FAILURE;
    }

    return client->socket;
}

/* Client_clearQuery
 *    Purpose: Clears the query from a Client
 * Parameters: @client - Pointer to a Client to clear the query from
 *    Returns: 0 on success, -1 on failure.
 */
void Client_clearQuery(Client *client)
{
    if (client == NULL || client->query == NULL) {
        return;
    }

    Query_free(client->query);
    client->query = NULL;
}

/* Client_clearSSL
 *    Purpose: Clears the SSL struct from a Client and sets the pointer to NULL
 * Parameters: @client - Pointer to a Client to clear SLL from
 *    Returns: 0 on success, -1 on failure.
 */
#if RUN_SSL
void Client_clearSSL(Client *client)
{
    if (client == NULL || client->ssl == NULL) {
        return;
    }
    
    SSL_shutdown(client->ssl);
    SSL_free(client->ssl);
    client->ssl = NULL;
    client->isSSL = 0;
}
#endif 

/* Client_isisSlow
 *    Purpose: Returns the isSlow status for a Client
 * Parameters: @client - Pointer to a Client
 *    Returns: isSlow status (true or false)
 */
#if RUN_SSL
bool Client_isSSL(Client *client)
{
    if (client == NULL) {
        return false;
    }

    return client->isSSL;
}
#endif
