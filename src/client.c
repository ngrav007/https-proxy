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

    client->socket = -1;
    // client->loggedIn          = false; // TODO - remove
    client->slowMofo          = false;
    client->buffer_l          = 0;
    client->last_recv.tv_sec  = 0;
    client->last_recv.tv_usec = 0;
    client->key               = NULL;

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
    // Client_setId(client, id);    // TODO - remove
    // client->loggedIn = false;    // TODO - Remove
    client->buffer_l = 0;

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
    if (c->socket != -1) {
        close(c->socket);
        c->socket = -1;
    }

    free(c->key);
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
        fprintf(stderr, "  partialRead = %s\n",
                (c->slowMofo) ? "true" : "false");
        fprintf(stderr, "  last_recv = %ld.%d\n", c->last_recv.tv_sec,
                c->last_recv.tv_usec);
        fprintf(stderr, "  buffer_l = %zd\n", c->buffer_l);
        if (c->buffer_l > 0) {
            print_ascii(c->buffer, c->buffer_l);
        } else {
            fprintf(stderr, "  buffer = \"\"\n");
        }
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

    return gettimeofday(&client->last_recv, NULL);
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

int Client_setKey(Client *client, HTTP_Header *header)
{
    if (client == NULL) {
        return -1;
    }

    client->key = calloc(header->host_l + header->path_l + 1, sizeof(char));
    memcpy(client->key, header->host, header->host_l);
    memcpy(client->key + header->host_l, header->path, header->path_l);

    return 0;
}

/* Client_setHeader
 *    Purpose: Sets the header for a Client
 * Parameters: @client - Pointer to a Client
 *             @header - Pointer to a header string
 *    Returns: 0 on success, -1 on failure.
 */
// int Client_setHeader(Client *client, char *buffer, int length)
// {
//     if (client == NULL || buffer == NULL || length < 0) {
//         return -1;
//     }

//     int ret = ChatHdr_fromNetString(&client->header, buffer, length);

//     return ret;
// }

/* Client_setId
 *    Purpose: Sets the client id for a Client, if the client id is too long
 *             it is deemed an error and the client id is not set, and -1 is
 *             returned.
 * Parameters: @client - Pointer to a Client
 *             @id - Client id string, must be null terminated
 *    Returns: 0 on success, -1 on failure.
 */
// int Client_setId(Client *client, char *id)
// {
//     if (client == NULL || id == NULL) {
//         return -1;
//     }

//     /* clear the id if it's already set */
//     if (client->id[0] != '\0') {
//         memset(client->id, 0, MAX_ID_SZ);
//     }

//     size_t idLength = strlen(id);
//     if (idLength > MAX_ID_SZ) {
//         return -1;
//     } else {
//         strncpy(client->id, id, idLength);
//     }

//     return 0;
// }

/* Client_setLoggedIn
 *    Purpose: Sets the logged in status for a Client to given value.
 * Parameters: @client - Pointer to a Client
 *             @loggedIn - Logged in status (true or false)
 *    Returns: 0 on success, -1 on failure.
 */
// int Client_setLoggedIn(Client *client, bool loggedIn)
// {
//     if (client == NULL) {
//         return -1;
//     }

//     client->loggedIn = loggedIn;

//     return 0;
// }

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
        return -1;
    }

    memcpy(&client->addr, addr, sizeof(struct sockaddr_in));
    client->addr_l = sizeof(*addr);

    return 0;
}

/* Client_getSocket
 *    Purpose: Returns the socket file descriptor for a Client
 * Parameters: @client - Pointer to a Client
 *    Returns: Socket file descriptor, or -1 on failure.
 */
int Client_getSocket(Client *client)
{
    if (client == NULL) {
        return -1;
    }

    return client->socket;
}

/* Client_getId
 *    Purpose: Returns the client id for a Client
 * Parameters: @client - Pointer to a Client
 *    Returns: A constant pointer to the client id string, or NULL on failure.
 */
// const char *Client_getId(Client *client)
// {
//     if (client == NULL) {
//         return NULL;
//     }

//     return client->id;
// }

/* Client_isLoggedIn
 *    Purpose: Returns the logged in status for a Client
 * Parameters: @client - Pointer to a Client
 *    Returns: Logged in status (true or false)
 */
// bool Client_isLoggedIn(Client *client)
// {
//     if (client == NULL) {
//         return false;
//     }

//     return client->loggedIn;
// }

/* Client_isSlowMofo
 *    Purpose: Returns the slowMofo status for a Client
 * Parameters: @client - Pointer to a Client
 *    Returns: slowMofo status (true or false)
 */
bool Client_isSlowMofo(Client *client)
{
    if (client == NULL) {
        return false;
    }

    return client->slowMofo;
}
