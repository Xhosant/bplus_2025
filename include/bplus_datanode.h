#ifndef BP_DATANODE_H
#define BP_DATANODE_H
/* Στο αντίστοιχο αρχείο .h μπορείτε να δηλώσετε τις συναρτήσεις
 * και τις δομές δεδομένων που σχετίζονται με τους Κόμβους Δεδομένων.*/

#endif

struct dataNode {
    int branchingFactor;
    //metadata, then:
    struct sll_node* head;
    struct dataNode* tail;
};