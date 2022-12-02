#include "request.h"

/* Request_new
 *    Purpose: Creates a new Request populated with the given values.
 * Parameters: @http_header - Pointer to a buffer containing the HTTP request
 *                            header, buffer must be null terminated.
 *    Returns: Pointer to a new Request, or NULL if memory allocation fails.
 */
Request *Request_new(char *http_header)
{
    Request *request = calloc(1, sizeof(struct Request));
    if (request == NULL) {
        return NULL;
    }

    HTTP_parse(&request->header, http_header);
    request->server_fd = -1;
    requst->timeout = NULL;

    return request;
}
/* Request_free
 *    Purpose: Frees a Request and all of the allocated memory in its fields
 * Parameters: @request - Pointer to a Request to free
 *    Returns: None
 */
void Request_free(void *request)
{
    if (request == NULL) {
        return;
    }

    Request *req = (Request *)request;
    HTTP_free(&req->header);
    if (req->timeout != NULL) {
        Timer_free(&req->timeout);
    }
    free(req);
}

/* Request_print
 *    Purpose: Print a Request to stderr for debugging purposes.
 * Parameters: @request - Pointer to a Request to print
 *    Returns: None
 */
void Request_print(void *request)
{
    if (request == NULL) {
        return;
    }

    Request *req = (Request *)request;
    HTTP_print(&req->header);
    Response_print(&req->response);
    fprintf(stderr, "server_fd: %d\n", req->server_fd);
}

int Request_compare(void *request1, void *request2)
{
    if (request1 == NULL || request2 == NULL) {
        return -1;
    }

    Request *req1 = (Request *)request1;
    Request *req2 = (Request *)request2;

    return HTTP_compare(&req1->header, &req2->header);
}
int Request_init(Request *request, char *request)
{
    if (request == NULL) {
        return -1;
    }

    HTTP_parse(&request->header, request);
    
    return 0;
}
