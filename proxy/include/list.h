#ifndef _LIST_H_
#define _LIST_H_

#include "node.h"
#include "utility.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct List {
    Node *head;
    Node *tail;
    void (*free_data)(void *);
    void (*print_data)(void *);
    int (*compare)(void *, void *);
    int size;
} List;

List *List_new(void (*free_data)(void *), void (*print_data)(void *),
               int (*compare)(void *, void *));
void List_free(List **list);
int List_clear(List *list);
bool List_is_empty(List *list);
void List_print(List *list);
int List_push_back(List *list, void *data);
int List_push_front(List *list, void *data);
int List_insert(List *list, void *data, int index);

int List_remove(List *list, void *data);
int List_remove_back(List *list);
int List_remove_front(List *list);
int List_remove_node(List *list, Node *node);

void *List_pop_front(List *list);
void *List_pop_back(List *list);

void *List_get(List *list, int index);
int List_move_to_back(List *list, void *data);
int List_size(List *list);
bool List_contains(List *list, void *data);
void *List_find(List *list, void *data);
Node *List_head(List *list);
Node *List_tail(List *list);
Node *List_next(Node *node);

#endif /* _LIST_H_ */
