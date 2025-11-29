#ifndef BP_DATANODE_H
#define BP_DATANODE_H
#include <sorted_linked_list.h>
#include <record.h>
#include <stdbool.h>
/* Στο αντίστοιχο αρχείο .h μπορείτε να δηλώσετε τις συναρτήσεις
 * και τις δομές δεδομένων που σχετίζονται με τους Κόμβους Δεδομένων.*/

#define BPLUS_MAX_DATA_RECORDS \
  ((BF_BLOCK_SIZE - 3 * sizeof(int)) / sizeof(Record))

typedef struct {
    int is_leaf;                     
    int num_records;                 
    int next_leaf;                  
    Record records[BPLUS_MAX_DATA_RECORDS];  
} BPlusDataNode;

#endif /* BP_DATANODE_H */