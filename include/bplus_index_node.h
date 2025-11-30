#ifndef BP_INDEX_NODE_H
#define BP_INDEX_NODE_H
#include "bf.h"

#define BPLUS_MAX_INDEX_KEYS \
  ((BF_BLOCK_SIZE - 5 * sizeof(int)) / (2 * sizeof(int)))

typedef struct {
    int is_leaf;                           
    int num_keys;                         
    int unused0;                           
    int unused1;                           
    int keys[BPLUS_MAX_INDEX_KEYS];        
    int children[BPLUS_MAX_INDEX_KEYS + 1];
} BPlusIndexNode;

#endif /* BP_INDEX_NODE_H */