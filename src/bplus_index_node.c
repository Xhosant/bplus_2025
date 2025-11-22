// Μπορείτε να προσθέσετε εδώ βοηθητικές συναρτήσεις για την επεξεργασία Κόμβων Δεδομένων.

#include "bplus_index_node.h"

#include <stddef.h>
#include <stdlib.h>


int search(int key, struct indexNode* self) {
    if (self->isAlmostLeaf) {
        struct dataNode* node = sll_get_branch(&self->head, key);
        return leaf_search(key, node);
    }
    else {
        struct indexNode* node = sll_get_branch(&self->head, key);
        return search(key, node);
    }
}

int insert(int key, struct indexNode* self, const Record *record) {
    int blockID;
    if (self->isAlmostLeaf) {
        struct dataNode* node = sll_get_branch(&self->head, key);
        blockID = leaf_insert(key, node, record);
        if (blockID < 0) {
            blockID *= -1; //return it to positive
            struct dataNode* newNode = leaf_split(node);
            sll_insert_index(&self->head, newNode->head->id, &newNode);
        }
    } else {
        struct indexNode* node = sll_get_branch(&self->head, key);
        blockID =insert(key, node, record);
        if (blockID <0) {
            blockID *= -1;
            struct indexNode* newNode = split(node);
            sll_insert_index(&self->head, newNode->head->id, &newNode);
        }
    }
    int entryCount = sll_count(&self->head);
    if (entryCount > (self->branchingFactor)) {
        blockID *= -1;
    }

    return blockID;
}

struct indexNode* split(struct indexNode* self) {
    struct sll_node* new_sll_node = sll_split(&self->head);

    struct indexNode* new_data_node = (struct indexNode*)malloc(sizeof(struct indexNode));
    new_data_node->head = new_sll_node;
    new_data_node->branchingFactor = self->branchingFactor;
    new_data_node->isRoot = false;
    self->isRoot = false;
    new_data_node->isAlmostLeaf = self->isAlmostLeaf;

    return new_data_node;
}