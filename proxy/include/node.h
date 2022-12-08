#ifndef _NODE_H_
#define _NODE_H_

#include "utility.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    void *data;
    struct Node *next;
    struct Node *prev;
} Node;

Node *Node_new(void *data);
void Node_free(Node *node, void (*free_data)(void *data));
void Node_print(Node *node, void (*print_data)(void *data));
void *Node_getData(Node *node);
Node *Node_getNext(Node *node);
Node *Node_getPrev(Node *node);

#endif /* _NODE_H_ */
