#include "list.h"

/*
 *    Purpose:
 * Parameters:
 *    Returns: 
 */
List List_new() {
    List list = calloc(1, sizeof(struct List));
    if (list == NULL) {
        fprintf(stderr, "[Error] List_new: calloc failed\n");
        return NULL;
    }
    list->head = NULL;
    list->tail = NULL;

    return list;
}

/*
 *    Purpose:
 * Parameters:
 *    Returns: 
 */
void List_free(List *list) {
    if (list == NULL || *list == NULL) {
        return;
    }

    Node *node = (*list)->head;
    while (node != NULL) {
        Node *next = node->next;
        Node_free(&node);
        node = next;
    }

    free(*list);
    *list = NULL;
}

/*
 *    Purpose:
 * Parameters:
 *    Returns: 
 */
void List_print(List list, void (*foo)(void *)) {
    Node node = list->head;
    while (node != NULL) {
        Entry_print(node->entry, foo);

        node = node->next;
    }
}

/*
 *    Purpose:
 * Parameters:
 *    Returns: 
 */
int List_add_back(List list, Entry entry) {
    if (list == NULL) {
        fprintf(stderr, "[Error] List_add_to_back: list == NULL\n");
        return -1;
    }

    Node node = Node_new(entry);
    if (node == NULL) {
        return -1;
    }

    if (list->tail == NULL) {
        list->tail = node;
        list->head = node;
    } else {
        list->tail->next = node;
        list->tail       = node;
    }

    return 0;
}

/*
 *    Purpose:
 * Parameters:
 *    Returns: 
 */
int List_add_front(List list, Entry entry) {
    if (list == NULL) {
        fprintf(stderr, "[Error] List_add_to_front: list == NULL\n");
        return -1;
    }

    Node node = Node_new(entry);
    if (node == NULL) {
        return -1;
    }
    if (list->head == NULL) {
        list->head = node;
        if (list->tail == NULL) {
            list->tail = node;
            node->next = NULL;
        }
    } else {
        node->next = list->head;
        list->head = node;
    }

    return 0;
}

/*
 *    Purpose:
 * Parameters:
 *    Returns: 
 */
Entry List_remove_back(List list) {
    if (list == NULL) {
        fprintf(stderr, "[Error] List_get_back: list == NULL\n");
        return NULL;
    }

    if (list->head == NULL) {
        return NULL;
    } else if (list->head->next == NULL) {
        Entry entry = Node_free(&list->head);
        list->head  = NULL;
        list->tail  = NULL;
        return entry;
    } else {
        Node node = list->head;
        while (node->next->next != NULL) {
            node = node->next;
        }
        Entry entry = Node_free(&node->next);
        node->next  = NULL;
        return entry;
    }
}

/*
 *    Purpose:
 * Parameters:
 *    Returns: 
 */
Entry List_remove_front(List list) {
    if (list == NULL) {
        fprintf(stderr, "[Error] List_get_front: list == NULL\n");
        return NULL;
    }

    if (list->head == NULL) {
        return NULL;
    } else if (list->head->next == NULL) {
        Entry entry = list->head->entry;
        Node_free(&list->head);
        list->head = NULL;
        list->tail = NULL;
        return entry;
    } else {
        Node node   = list->head;
        list->head  = node->next;
        Entry entry = node->entry;
        Node_free(&node);
        return entry;
    }
}

/*
 *    Purpose:
 * Parameters:
 *    Returns: 
 */
Entry List_remove(List list, Entry entry) {
    if (list == NULL) {
        fprintf(stderr, "[Error] List_remove: list == NULL\n");
        return NULL;
    }

    Node node = list->head;
    Node prev = NULL;
    while (node != NULL) {
        if (Entry_is_equal(node->entry, entry->key)) {
            if (prev == NULL) {
                list->head = node->next;
            } else {
                prev->next = node->next;
            }
            if (Entry_is_equal(node->entry, list->tail->entry->key)) {
                list->tail = prev;
            }
            Entry e = Node_free(&node);
            return e;
        }
        prev = node;
        node = node->next;
    }
    return NULL;
}

/*
 *    Purpose:
 * Parameters:
 *    Returns: 
 */
Entry List_get(List list, char *key) {
    if (list == NULL) {
        fprintf(stderr, "[Error] List_get: list == NULL\n");
        return NULL;
    }
    Node node = list->head;
    while (node != NULL) {
        if (Entry_is_equal(node->entry, key)) {
            return node->entry;
        }
        node = node->next;
    }
    return NULL;
}
