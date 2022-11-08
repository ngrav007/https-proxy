#include "node.h"

Node Node_new(Entry entry) {
    Node node = calloc(1, sizeof(struct Node));
    if (node == NULL) {
        return NULL;
    }

    node->entry = entry;
    node->next  = NULL;

    return node;
}

Entry Node_free(Node *node) {
    if (node == NULL || *node == NULL) {
        return NULL;
    }

    Entry e        = (*node)->entry;
    (*node)->entry = NULL;
    free((*node));
    (*node) = NULL;

    return e;
}

void Node_print(Node node, void (*foo)(void *)) {
    if (node == NULL) {
        fprintf(stderr, "Node: (null)\n");
        return;
    }

    fprintf(stderr, "Node {\n");
    foo(node->entry);
    fprintf(stderr, "    next = %p\n", (void *)node->next);
    fprintf(stderr, "}\n");
}
