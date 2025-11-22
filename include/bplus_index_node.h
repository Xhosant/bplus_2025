#ifndef BP_INDEX_NODE_H
#define BP_INDEX_NODE_H
#include <stdbool.h>
#include <bplus_datanode.h> //this also includes sorted_linked_list.h, record.h
/* Στο αντίστοιχο αρχείο .h μπορείτε να δηλώσετε τις συναρτήσεις
 * και τις δομές δεδομένων που σχετίζονται με τους Κόμβους Δεδομένων.*/

#endif

struct indexNode {
    bool isRoot;
    bool isAlmostLeaf; //true if it points to leaf, false otherwise. Only initial root and its splits are true.
    int branchingFactor;  //TODO ideally move to overall tree metadata
    //metadata, then:
    struct sll_node* head;
    //void* tail; //indexNode or dataNode depending on what's under it
};

int search(int key, struct indexNode* index);
bool insert(int key, struct indexNode* index, const Record *record);
struct indexNode* split(struct indexNode* self);