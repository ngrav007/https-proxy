#ifndef _NODE_H_
#define _NODE_H_

#include "entry.h"
#include "utility.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct Node {
    Entry entry;
    struct Node *next;
} * Node;

Node Node_new(Entry entry);
Entry Node_free(Node *node);
void Node_print(Node node, void (*foo)(void *));

#endif /* _NODE_H_ */
