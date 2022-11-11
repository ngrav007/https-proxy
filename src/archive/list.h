#ifndef _LIST_H_
#define _LIST_H_

#include "entry.h"
#include "node.h"
#include "utility.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct List {
    Node head;
    Node tail;
} * List;

List List_new();
void List_free(List *list);
void List_print(List list, void (*foo)(void *));
int List_add_back(List list, Entry entry);
int List_add_front(List list, Entry entry);
Entry List_get(List list, char *key);
Entry List_remove_back(List list);
Entry List_remove_front(List list);
Entry List_remove(List list, Entry entry);

#endif /* _LIST_H_ */
