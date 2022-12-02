#include "list.h"

static Node *get_node(List *list, int index);
static Node *get_nodeByVal(List *list, void *data);

/* List_new
 *    Purpose: Create a new linked list, free, print, and compare functions for
 *             the data in the list are set to the given function pointers. If
 *             the given free function pointer is NULL, the data will not be
 *             freed, printed, or have the ability to be compared.
 * Parameters: @free_data - Function pointer to a function that frees the data
 *                          in the list
 *             @print_data - Function pointer to a function that prints the data
 *                           in the list
 *             @compare_data - Function pointer to a function that compares the
 *                             data in the list
 *    Returns: A pointer to a new List on success, NULL on failure.
 */
List *List_new(void (*free_data)(void *), void (*print_data)(void *),
               int (*compare)(void *, void *))
{
    List *list = calloc(1, sizeof(struct List));
    if (list == NULL) {
        return NULL;
    }

    list->head       = NULL;
    list->tail       = NULL;
    list->size       = 0;
    list->free_data  = free_data;
    list->print_data = print_data;
    list->compare    = compare;

    return list;
}

/* List_free
 *    Purpose: Frees a list and all of its nodes, the data in the list is freed
 *             if the free function pointer is not NULL. The list pointer is set
 *             to NULL.
 * Parameters: @list - Pointer to a pointer to a list to free
 *    Returns: None
 */
void List_free(List **list)
{
    List_clear(*list);
    free(*list);
    *list = NULL;
}

/* List_clear
 *    Purpose: Clears a list and all of its nodes, the data in the list is freed
 *             if the free function pointer is not NULL.
 * Parameters: @list - Pointer to a list to clear
 *    Returns: 0 on success, -1 on failure.
 */
int List_clear(List *list)
{
    if (list == NULL) {
        return -1;
    }

    Node *node = list->head;
    while (node != NULL) {
        Node *next = node->next;
        Node_free(node, list->free_data);
        node = next;
    }

    list->head = NULL;
    list->tail = NULL;
    list->size = 0;

    return 0;
}

/* List_print
 *    Purpose: Prints all of the nodes in the list. If the print function
 *             pointer is NULL, the data is not printed. Instead, the address
 *             of the data in memory is printed.
 * Parameters: @list - Pointer to a list to print
 *    Returns: None
 */
void List_print(List *list)
{
    if (list == NULL) {
        fprintf(stderr, "%s[List]%s (null)\n", RED, CRESET);
    } else {
        fprintf(stderr, "%s[List]%s\n", CYN, CRESET);
        fprintf(stderr, "    size = %d\n", List_size(list));
        fprintf(stderr, "    head = %p\n", (void *)list->head);
        fprintf(stderr, "    tail = %p\n", (void *)list->tail);
        Node *node = list->head;
        while (node != NULL) {
            fprintf(stderr, "    node = %p\n", (void *)node);
            fprintf(stderr, "    node->next = %p\n", (void *)node->next);
            Node_print(node, list->print_data);
            node = node->next;
        }
    }
}

/* List_size
 *    Purpose: Returns the number of nodes in the list.
 * Parameters: @list - Pointer to a list
 *    Returns: The number of nodes in the list
 */
int List_size(List *list)
{
    if (list == NULL) {
        return -1;
    }

    return list->size;
}

/* List_insert
 *    Purpose: Inserts a new node into the list at the given index. The data in
 *             the new node is set to the given data. If the index is out of
 *             bounds, the node is not inserted and the function returns -1.
 * Parameters: @list - Pointer to a list to insert into
 *             @index - Index to insert the new node at
 *    Returns: 0 on success, -1 on failure.
 */
int List_insert(List *list, void *data, int index)
{
    if (index < 0 || index > List_size(list)) {
        return -1;
    }

    if (index == 0) {
        return List_push_front(list, data);
    } else if (index == List_size(list)) {
        return List_push_back(list, data);
    } else {
        Node *node = Node_new(data);
        if (node == NULL) {
            return -1;
        }

        Node *prev = get_node(list, index - 1);
        node->next = prev->next;
        node->prev = prev;
        prev->next = node;
        list->size++;
    }

    return 0;
}

/* List_push_back
 *    Purpose: Adds a new node to the end of the list with the given data.
 * Parameters: @list - Pointer to a list to add a node to
 *             @data - Pointer to the data to add to the list
 *    Returns:  0 on success, -1 on failure
 */
int List_push_back(List *list, void *data)
{
    Node *node = Node_new(data);
    if (node == NULL) {
        return 0;
    }

    if (list->head == NULL) {
        list->head = node;
        list->tail = node;
    } else {
        list->tail->next = node;
        node->prev       = list->tail;
        list->tail       = node;
    }

    list->size++;

    return 0;
}

/* List_push_front
 *    Purpose: Adds a new node to the front of the list with the given data.
 * Parameters: @list - Pointer to a list to add a node to
 *             @data - Pointer to the data to add to the list
 *    Returns: 0 on success, -1 on failure
 */
int List_push_front(List *list, void *data)
{
    Node *node = Node_new(data);
    if (node == NULL) {
        return 0;
    }

    if (list->head == NULL) {
        list->tail = node;
    } else {
        node->next = list->head;
    }
    list->head = node;
    list->size++;

    return 0;
}

/* List_remove
 *    Purpose: Removes the node given a key. The data in the node is
 *             freed if the free function pointer is not NULL. The node is freed
 *             regardless. If free_data is NULL, the data is not freed and the
 *             caller is responsible for freeing the data. If the key is not
 *             found, the function returns -1.
 * Parameters: @list - Pointer to a list to remove a node from
 *             @data - Pointer to the data to remove from the list
 *    Returns: 0 on success, -1 on failure
 *
 * Note: The compare() function pointer must be set before this function can be
 *       used.
 */
int List_remove(List *list, void *data)
{
    if (list == NULL || list->compare == NULL) {
        return -1;
    }

    Node *node = list->head;
    while (node != NULL) {
        if (list->compare(node->data, data)) {
            if (node->prev == NULL) { // head
                list->head = node->next;
                if (node->next != NULL) {
                    node->next->prev = NULL;
                } else {
                    list->tail = NULL;
                }
            } else if (node->next == NULL) { // tail
                list->tail = node->prev;
                if (node->prev != NULL) {
                    node->prev->next = NULL;
                } else {
                    list->head = NULL;
                }
            } else { // middle
                node->prev->next = node->next;
                node->next->prev = node->prev;
            }

            Node_free(node, list->free_data);
            node = NULL;
            list->size--;
            
            return 0;
        }

        node = node->next;
    }

    return -1;
}

/* List_remove_node
 *    Purpose: Removes the given node from the list. The data in the node is
 *             freed if the free function pointer is not NULL. The node is freed
 *             regardless. If free_data is NULL, the data is not freed and the
 *             caller is responsible for freeing the data.
 * Parameters:
 *    Returns:
 */
int List_remove_node(List *list, Node *node)
{
    if (list == NULL || node == NULL) {
        return -1;
    }

    if (node->prev == NULL) {
        return List_pop_front(list);
    } else if (node->next == NULL) {
        return List_pop_back(list);
    } else {
        node->prev->next = node->next;
        node->next->prev = node->prev;
        Node_free(node, list->free_data);
        list->size--;
    }

    return 0;
}

/* List_pop_front
 *    Purpose: Removes the first node from the list. The data in the node is
 *             freed if the free function pointer is not NULL. The node is freed
 *             regardless. If free_data is NULL, the data is not freed and the
 *             caller is responsible for freeing the data. If the list is empty,
 *             the function returns -1 and does nothing.
 * Parameters: @list - Pointer to a list to remove a node from
 *    Returns: 0 on success, -1 on failure
 */
int List_pop_front(List *list)
{
    if (List_is_empty(list)) {
        return -1;
    }

    Node *node = list->head;
    if (List_size(list) == 1) {
        list->head = NULL;
        list->tail = NULL;
    } else {
        list->head       = node->next;
        list->head->prev = NULL;
    }

    Node_free(node, list->free_data);
    list->size--;

    return 0;
}

/* List_pop_back
 *    Purpose: Removes the last node from the list. The data in the node is
 *             freed if the free function pointer is not NULL. The node is freed
 *             regardless. If free_data is NULL, the data is not freed and the
 *             caller is responsible for freeing the data. If the list is empty,
 *             the function returns -1 and does nothing.
 * Parameters: @list - Pointer to a list to remove a node from
 *    Returns: 0 on success, -1 on failure
 */
int List_pop_back(List *list)
{
    if (List_is_empty(list)) {
        return -1;
    }

    Node *node = list->tail;
    if (List_size(list) == 1) {
        list->head = NULL;
        list->tail = NULL;
    } else {
        list->tail       = node->prev;
        list->tail->next = NULL;
    }

    Node_free(node, list->free_data);
    list->size--;

    return 0;
}

/* List_get
 *    Purpose: Returns the data at the given index. If the index is out of
 *             bounds, the function returns NULL.
 * Parameters: @list - Pointer to a list to get data from
 *             @index - Index of the data to get
 *    Returns: Pointer to the data at the given index, NULL on failure
 */
void *List_get(List *list, int index)
{
    if (list == NULL) {
        return NULL;
    }

    if (index < 0 || index >= List_size(list)) {
        return NULL;
    }

    Node *curr = NULL;
    int i;
    if (index < List_size(list) / 2) {
        curr = list->head;
        for (i = 0; i < index; i++) {
            curr = curr->next;
        }
    } else {
        curr = list->tail;
        for (i = List_size(list) - 1; i > index; i--) {
            curr = curr->prev;
        }
    }

    return curr->data;
}

/* List_moveBack
 *    Purpose: Moves the given node to the back of the list. If the node is
 *             already at the back of the list, the function does nothing.
 * Parameters: @list - Pointer to a list to move a node in
 *             @node - Pointer to the node to move to the back of the list
 *    Returns: 0 on success, -1 on failure
 */
int List_move_to_back(List *list, void *data)
{
    if (list == NULL || data == NULL) {
        return -1;
    }

    if (list->compare == NULL) {
        return -1;
    }

    if (list->head == NULL || list->tail == NULL) { // empty list
        return 0;
    }

    Node *node = get_nodeByVal(list, data);
    if (node == NULL) {
        return -1;
    }

    if (node == NULL) {
        return -1;
    }

    if (node->next == NULL) { // back
        return 0;
    } else if (node->prev == NULL) { // front
        list->head       = node->next;
        list->head->prev = NULL;
    } else { // middle
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }

    list->tail->next = node;
    node->prev       = list->tail;
    node->next       = NULL;
    list->tail       = node;

    return 0;
}

/* List_find
 *    Purpose: Returns the data that matches the given key. If the key is not
 *             found, the function returns NULL.
 * Parameters: @list - Pointer to a list to find data in
 *             @key - Key to find in the list
 *    Returns: Pointer to the data that matches the given key, NULL on failure
 *
 * Note: The compare() function pointer must be set before this function can be
 *       used.
 */
void *List_find(List *list, void *data)
{
    if (list == NULL || list->compare == NULL) {
        return NULL;
    }

    Node *node = list->head;
    while (node != NULL) {
        if (list->compare(node->data, data) == 0) {
            return node->data;
        }
        node = node->next;
    }

    return NULL;
}

/* List_contains
 *    Purpose: Returns true if the list contains the given data, otherwise this
 *             function returns false.
 * Parameters: @list - Pointer to a list to check
 *             @data - Pointer to the data to check for
 *    Returns: True (1) if the list contains the data, false (0) otherwise
 *
 * Note: The compare() function pointer must be set for this function to work.
 *
 */
bool List_contains(List *list, void *data)
{
    if (list == NULL || list->compare == NULL) {
        return false;
    }

    Node *node = list->head;
    while (node != NULL) {
        if (list->compare(node->data, data) == 0) {
            return true;
        }
        node = node->next;
    }

    return false;
}

/* List_is_empty
 *    Purpose: Returns true if the list is empty, otherwise this function
 *             returns false.
 * Parameters: @list - Pointer to a list to check for emptiness
 *    Returns: True (1) if the list is empty or NULL, false (0) otherwise
 */
bool List_is_empty(List *list)
{
    if (list == NULL) {
        return true;
    }

    return list->head == NULL && list->tail == NULL && List_size(list) == 0;
}

/* get_node
 *    Purpose: Returns the node at the given index. If the index is out of
 *             bounds, the function returns NULL.
 * Parameters: @list - Pointer to a list to get data from
 *             @index - Index of the data to get
 *    Returns: Pointer to the node at the given index, NULL on failure
 */
static Node *get_node(List *list, int index)
{
    if (list == NULL) {
        return NULL;
    }

    if (index < 0 || index >= List_size(list)) {
        return NULL;
    }

    Node *curr = NULL;
    int i;
    if (index < List_size(list) / 2) {
        curr = list->head;
        for (i = 0; i < index; i++) {
            curr = curr->next;
        }
    } else {
        curr = list->tail;
        for (i = List_size(list) - 1; i > index; i--) {
            curr = curr->prev;
        }
    }

    return curr;
}

static Node *get_nodeByVal(List *list, void *data)
{
    if (list == NULL) {
        return NULL;
    }

    Node *curr = list->head;
    while (curr != NULL) {
        if (list->compare(curr->data, data)) {
            return curr;
        }
        curr = curr->next;
    }

    return NULL;
}
