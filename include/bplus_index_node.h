#ifndef BP_INDEX_NODE_H
#define BP_INDEX_NODE_H
#include <stdbool.h>
/* Στο αντίστοιχο αρχείο .h μπορείτε να δηλώσετε τις συναρτήσεις
 * και τις δομές δεδομένων που σχετίζονται με τους Κόμβους Δεδομένων.*/

#endif

struct indexNode {
    bool isRoot;
    int branchingFactor;
    //metadata, then:
    struct sll_node* head;
    void* tail; //indexNode or dataNode depending on what's under it
};