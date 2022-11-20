#include "node.h"

/* Node_new
 *    Purpose: Creates a new Node, and returns a pointer to it. Node's next and
 *             previous pointers are initialized to NULL.
 * Parameters: The data to store in the Node.
 *    Returns: Pointer to a new Node, or NULL if memory allocation fails.
 */
Node *Node_new(void *data)
{
    Node *node = malloc(sizeof(*node));
    if (node == NULL) {
        return NULL;
    }

    node->data = data;
    node->next = NULL;
    node->prev = NULL;

    return node;
}

/* Node_free
 *    Purpose: Frees a Node and its data.
 * Parameters: @node - Pointer to a Node to free
 *             @free_data - Function to free the data stored in the Node
 *    Returns: None
 */
void Node_free(Node *node, void (*free_data)(void *))
{
    if (node == NULL) {
        return;
    }

    if (free_data != NULL) {
        free_data(node->data);
    }
    node->data = NULL;
    node->next = NULL;
    node->prev = NULL;
    free(node);
}

/* Node_print
 *    Purpose: Prints a Node's data and next and previous pointers.
 * Parameters: @node - Pointer to a Node to print
 *             @print_data - Function to print the data stored in the Node
 *    Returns: None
 */
void Node_print(Node *node, void (*print_data)(void *))
{
    if (node == NULL) {
        fprintf(stderr, "%s[Node]%s (null)\n", BWHT, CRESET);
    } else {
        fprintf(stderr, "%s[Node]%s\n", BWHT, CRESET);
        fprintf(stderr, "    next = %p\n", (void *)node->next);
        fprintf(stderr, "    prev = %p\n", (void *)node->prev);
        if (print_data != NULL) {
            fprintf(stderr, "  data = %p\n", (void *)node->data);
            print_data(node->data);
        } else {
            fprintf(stderr, "  data = %p\n", node->data);
        }
    }
}

/* Node_getData
 *    Purpose: Returns the data stored in a Node.
 * Parameters: @node - Pointer to the Node to get data from
 *    Returns: The data stored in the Node
 */
void *Node_getData(Node *node)
{
    if (node == NULL) {
        return NULL;
    }

    return node->data;
}

/* Node_getNext
 *    Purpose: Returns the next Node in a linked list given a Node.
 * Parameters: @node - Pointer to the Node to get the next Node from
 *    Returns: The next Node in the linked list
 */
Node *Node_getNext(Node *node)
{
    if (node == NULL) {
        return NULL;
    }

    return node->next;
}

/* Node_getPrev
 *    Purpose: Returns the previous Node in a linked list given a Node.
 * Parameters: @node - Pointer to the Node to get the previous Node from
 *    Returns: The previous Node in the linked list
 */
Node *Node_getPrev(Node *node)
{
    if (node == NULL) {
        return NULL;
    }

    return node->prev;
}
