#ifndef BP_DATANODE_H
#define BP_DATANODE_H
#include <sorted_linked_list.h>
#include <record.h>
#include <stdbool.h>
/* Στο αντίστοιχο αρχείο .h μπορείτε να δηλώσετε τις συναρτήσεις
 * και τις δομές δεδομένων που σχετίζονται με τους Κόμβους Δεδομένων.*/

#endif

struct dataNode {
    int branchingFactor; //TODO ideally move to overall tree metadata
    //metadata, then:
    struct sll_node* head;
    struct dataNode* tail;
};

int leaf_search(int key, struct dataNode* self);
int leaf_insert(int key, struct dataNode* self, const Record *record);
struct dataNode* leaf_split(struct dataNode* self);