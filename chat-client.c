#include "ChatClient.h"

/* ChatClient_new
 *    Purpose: Creates a new ChatClient, and returns a pointer to it. Client
 *             socket is initialized to -1, all other fields are initialized to
 *             NULL or 0.
 * Parameters: None
 *    Returns: Pointer to a new ChatClient, or NULL if memory allocation fails.
 */
ChatClient *ChatClient_new()
{
    ChatClient *client = calloc(1, sizeof(struct ChatClient));
    if (client == NULL) {
        return NULL;
    }

    client->socket            = -1;
    client->loggedIn          = false;
    client->slowMofo          = false;
    client->buffer_l          = 0;
    client->last_recv.tv_sec  = 0;
    client->last_recv.tv_usec = 0;

    return client;
}

/* ChatClient_create
 *    Purpose: Creates a new ChatClient and initializes it with the given socket
 *             and client id. Client is considered logged in.
 * Parameters: @socket - Client's socket file descriptor
 *             @id - Client id
 *    Returns: Pointer to a new ChatClient, or NULL if memory allocation fails.
 */
ChatClient *ChatClient_create(int socket, char *id)
{
    ChatClient *client = ChatClient_new();
    if (client == NULL) {
        return NULL;
    }

    if (ChatClient_init(client, id, socket) != 0) {
        ChatClient_free(&client);
        return NULL;
    }

    return client;
}

/* ChatClient_init
 *    Purpose: Initializes a ChatClient with the given socket and client id.
 *             When a ChatClient is initialized
 * Parameters: @client - Pointer to a ChatClient to initialize
 *             @socket - Client's socket file descriptor
 *             @id - Client id
 *    Returns: 0 on success, -1 on failure.
 *
 * Note: If id is too long, it will be truncated to fit in the buffer
 *       including the null terminator.
 */
int ChatClient_init(ChatClient *client, char *id, int socket)
{
    if (client == NULL || socket == -1 || id == NULL) {
        return -1;
    }

    ChatClient_setSocket(client, socket);
    ChatClient_setId(client, id);
    client->loggedIn = false;
    client->buffer_l = 0;

    return 0;
}

/* ChatClient_free
 *    Purpose: Frees a ChatClient
 * Parameters: @client - Pointer to a pointer to a ChatClient to free
 *    Returns: None
 */
void ChatClient_free(void *client)
{
    if (client == NULL) {
        return;
    }

    ChatClient *c = (ChatClient *)client;
    if (c->socket != -1) {
        close(c->socket);
        c->socket = -1;
    }

    free(c);
}

/* ChatClient_print
 *    Purpose: Prints a ChatClient to stdout
 * Parameters: @client - Pointer to a ChatClient to print
 *    Returns: None
 */
void ChatClient_print(void *client)
{
    ChatClient *c = (ChatClient *)client;
    if (c == NULL) {
        printf("[ChatClient] (null)\n");
    } else {
        fprintf(stderr, "%s[ChatClient]%s\n", CYN, CRESET);
        fprintf(stderr, "  id = %s\n", c->id);
        fprintf(stderr, "  socket = %d\n", c->socket);
        fprintf(stderr, "  loggedIn = %s\n", (c->loggedIn) ? "true" : "false");
        fprintf(stderr, "  partialRead = %s\n",
                (c->slowMofo) ? "true" : "false");
        fprintf(stderr, "   last_recv = %ld.%06ld\n", c->last_recv.tv_sec,
                c->last_recv.tv_usec);
        fprintf(stderr, "  buffer_l = %d\n", c->buffer_l);
        if (c->buffer_l > 0) {
            printAscii(c->buffer, c->buffer_l);
        } else {
            fprintf(stderr, "  buffer = \"\"\n");
        }
    }
}

/* ChatClient_compare
 *    Purpose: Compares a ChatClient and a client id string to see if they are
 *             equal.
 * Parameters: @client1 - Pointer to a ChatClient
 *             @id - Pointer to a socket file descriptor
 *    Returns: 1 (true) if the clients are equal, 0  otherwise. If either
 *             parameter is NULL, -1 is returned.
 */
int ChatClient_compare(void *client1, void *client2)
{
    if (client1 == NULL || client2 == NULL) {
        return -1;
    }
    ChatClient *c1 = (ChatClient *)client1;
    ChatClient *c2 = (ChatClient *)client2;

    if (c1->socket == c2->socket) {
        return 1;
    }

    return 0;
}

/* ChatClient_timestamp
 *    Purpose: Gets the timestamp of the last time a message was received from
 *             the client.
 * Parameters: @client - Pointer to a ChatClient to timestamp
 *    Returns: 0 on success, -1 on failure.
 */
int ChatClient_timestamp(ChatClient *client)
{
    if (client == NULL) {
        return -1;
    }

    return gettimeofday(&client->last_recv, NULL);
}

/* ChatClient_setSocket
 *    Purpose: Sets the socket file descriptor for a ChatClient, if the client's
 *             socket is not -1, it will be closed first.
 * Parameters: @client - Pointer to a ChatClient
 *             @socket - Socket file descriptor
 *    Returns: 0 on success, -1 on failure.
 */
int ChatClient_setSocket(ChatClient *client, int socket)
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

/* ChatClient_setHeader
 *    Purpose: Sets the header for a ChatClient
 * Parameters: @client - Pointer to a ChatClient
 *             @header - Pointer to a header string
 *    Returns: 0 on success, -1 on failure.
 */
int ChatClient_setHeader(ChatClient *client, char *buffer, int length)
{
    if (client == NULL || buffer == NULL || length < 0) {
        return -1;
    }

    int ret = ChatHdr_fromNetString(&client->header, buffer, length);

    return ret;
}

/* ChatClient_setId
 *    Purpose: Sets the client id for a ChatClient, if the client id is too long
 *             it is deemed an error and the client id is not set, and -1 is
 *             returned.
 * Parameters: @client - Pointer to a ChatClient
 *             @id - Client id string, must be null terminated
 *    Returns: 0 on success, -1 on failure.
 */
int ChatClient_setId(ChatClient *client, char *id)
{
    if (client == NULL || id == NULL) {
        return -1;
    }

    /* clear the id if it's already set */
    if (client->id[0] != '\0') {
        memset(client->id, 0, MAX_ID_SZ);
    }

    size_t idLength = strlen(id);
    if (idLength > MAX_ID_SZ) {
        return -1;
    } else {
        strncpy(client->id, id, idLength);
    }

    return 0;
}

/* ChatClient_setLoggedIn
 *    Purpose: Sets the logged in status for a ChatClient to given value.
 * Parameters: @client - Pointer to a ChatClient
 *             @loggedIn - Logged in status (true or false)
 *    Returns: 0 on success, -1 on failure.
 */
int ChatClient_setLoggedIn(ChatClient *client, bool loggedIn)
{
    if (client == NULL) {
        return -1;
    }

    client->loggedIn = loggedIn;

    return 0;
}

/* ChatClient_setAddr
 *     Purpose: Copies the address of the client into the client struct for
 *              later use.
 *  Parameters: @client - Pointer to a ChatClient
 *              @addr - Pointer to a sockaddr_in struct containing the client's
 *                      address
 *     Returns: 0 on success, -1 on failure.
 */
int ChatClient_setAddr(ChatClient *client, struct sockaddr_in *addr)
{
    if (client == NULL || addr == NULL) {
        return -1;
    }

    memcpy(&client->addr, addr, sizeof(struct sockaddr_in));
    client->addr_l = sizeof(*addr);

    return 0;
}

/* ChatClient_getSocket
 *    Purpose: Returns the socket file descriptor for a ChatClient
 * Parameters: @client - Pointer to a ChatClient
 *    Returns: Socket file descriptor, or -1 on failure.
 */
int ChatClient_getSocket(ChatClient *client)
{
    if (client == NULL) {
        return -1;
    }

    return client->socket;
}

/* ChatClient_getId
 *    Purpose: Returns the client id for a ChatClient
 * Parameters: @client - Pointer to a ChatClient
 *    Returns: A constant pointer to the client id string, or NULL on failure.
 */
const char *ChatClient_getId(ChatClient *client)
{
    if (client == NULL) {
        return NULL;
    }

    return client->id;
}

/* ChatClient_isLoggedIn
 *    Purpose: Returns the logged in status for a ChatClient
 * Parameters: @client - Pointer to a ChatClient
 *    Returns: Logged in status (true or false)
 */
bool ChatClient_isLoggedIn(ChatClient *client)
{
    if (client == NULL) {
        return false;
    }

    return client->loggedIn;
}

/* ChatClient_isSlowMofo
 *    Purpose: Returns the slowMofo status for a ChatClient
 * Parameters: @client - Pointer to a ChatClient
 *    Returns: slowMofo status (true or false)
 */
bool ChatClient_isSlowMofo(ChatClient *client)
{
    if (client == NULL) {
        return false;
    }

    return client->slowMofo;
}
