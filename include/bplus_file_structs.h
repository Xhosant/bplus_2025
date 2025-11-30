//
// Created by theofilos on 11/4/25.
//

#ifndef BPLUS_BPLUS_FILE_STRUCTS_H
#define BPLUS_BPLUS_FILE_STRUCTS_H

#include "bf.h"
#include "record.h"

/**
 * Metadata του B+ αρχείου.
 * Αποθηκεύεται στο block 0.
 */
typedef struct {
    TableSchema schema;   
    int root_block;       
} BPlusMeta;


#define BPLUS_MAX_DATA_RECORDS \
  ((BF_BLOCK_SIZE - 3 * (int)sizeof(int)) / (int)sizeof(Record))

/*
 * Συνολικά bytes: (2K + 5) * sizeof(int) <= BF_BLOCK_SIZE
 *  => K <= (BF_BLOCK_SIZE/sizeof(int) - 5)/2
 */
#define BPLUS_MAX_INDEX_KEYS \
  (((int)(BF_BLOCK_SIZE / sizeof(int)) - 5) / 2)

/**
 * Φύλλο (data node) του B+.
 * is_leaf = 1
 */
typedef struct {
    int is_leaf;                     // πάντα 1 για φύλλο
    int num_records;                 // πόσες εγγραφές είναι έγκυρες
    int next_leaf;                   // block id επόμενου φύλλου ή -1
    Record records[BPLUS_MAX_DATA_RECORDS];
} BPlusDataNode;

/**
 * Εσωτερικός κόμβος (index node) του B+.
 * is_leaf = 0
 */
typedef struct {
    int is_leaf;                               // πάντα 0 για index node
    int num_keys;                              // πόσα keys είναι έγκυρα
    int unused0;                               // reserved / alignment
    int unused1;                               // reserved / alignment
    int keys[BPLUS_MAX_INDEX_KEYS];           // sorted keys
    int children[BPLUS_MAX_INDEX_KEYS + 1];   // block ids παιδιών
} BPlusIndexNode;

#endif // BPLUS_BPLUS_FILE_STRUCTS_H
