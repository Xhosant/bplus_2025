// Μπορείτε να προσθέσετε εδώ βοηθητικές συναρτήσεις για την επεξεργασία Κόμβων toy Ευρετηρίου.
#include <bplus_datanode.h>
#include <stdlib.h>

int leaf_search(int key, struct dataNode* self) {
    return sll_get(&self->head,key);
}

int leaf_insert(int key, struct dataNode* self, const Record *record) {
    if (sll_get(&self->head,key)==-1) {return false;} //already exists

    int blockID = sll_get_branch(&self->head,key);
    //TODO check if there's enough space in that block
    //If not, create new blocks and change blockID to it
    sll_insert_data(&self->head,key, blockID);
    int entryCount = sll_count(&self->head);
    if (entryCount > (self->branchingFactor-1)) {
        blockID *=-1;
    }
    return blockID;
}

struct dataNode* leaf_split(struct dataNode* self) {
    struct sll_node* new_sll_node = sll_split(&self->head);

    struct dataNode* new_data_node = (struct dataNode*)malloc(sizeof(struct dataNode));
    new_data_node->head = new_sll_node;
    new_data_node->tail = self->tail;
    new_data_node->branchingFactor = self->branchingFactor;
    self->tail = new_data_node;

    return new_data_node;
}