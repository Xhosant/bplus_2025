#include "bplus_file_funcs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CALL_BF(call)         \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK)        \
    {                         \
      BF_PrintError(code);    \
      return bplus_ERROR;     \
    }                         \
  }


/*  Βοηθητικές static συναρτήσεις                                     */


/**
 * Επιστρέφει το κλειδί της εγγραφής σύμφωνα με το schema.
  */
static int bplus_get_key(const TableSchema *schema, const Record *rec) {
  return record_get_key(schema, rec);
}

/**
 * Αναδρομική εισαγωγή σε υποδέντρο με ρίζα στο block_id.
 *
 * Αν ΔΕΝ γίνει split στον κόμβο "block_id":
 *   επιστρέφει 0
 *
 * Αν γίνει split στον κόμβο "block_id":
 *   επιστρέφει 1
 *   και γεμίζει:
 *      *promoted_key  : το κλειδί που ανεβαίνει στον γονέα
 *      *new_block_id  : το block id του νέου κόμβου που δημιουργήθηκε
 *
 * Σε οποιοδήποτε σφάλμα:
 *   επιστρέφει bplus_ERROR
 *
 * Σε κάθε περίπτωση γεμίζει και το *leaf_block_id με το block id
 * του φύλλου όπου κατέληξε η εγγραφή.
 */
static int bplus_insert_recursive(
    int file_desc,
    const BPlusMeta *meta,
    int block_id,
    const Record *record,
    int *promoted_key,
    int *new_block_id,
    int *leaf_block_id)
{
  int key = bplus_get_key(&meta->schema, record);

  BF_Block *block;
  BF_Block_Init(&block);
  CALL_BF(BF_GetBlock(file_desc, block_id, block));
  char *data = BF_Block_GetData(block);

  int is_leaf = *((int *) data);

  /*  Περίπτωση 1: Φύλλο (BPlusDataNode) */
  if (is_leaf) {
    BPlusDataNode *leaf = (BPlusDataNode *) data;

    /* Έλεγχος για διπλό key  */
    for (int i = 0; i < leaf->num_records; i++) {
      int k = bplus_get_key(&meta->schema, &leaf->records[i]);
      if (k == key) {
        /* Διπλό κλειδί – απορρίπτουμε την εισαγωγή */
        CALL_BF(BF_UnpinBlock(block));
        BF_Block_Destroy(&block);
        return bplus_ERROR;
      }
    }

    /* Αν υπάρχει χώρος στο φύλλο -> απλή ταξινομημένη εισαγωγή */
    if (leaf->num_records < BPLUS_MAX_DATA_RECORDS) {
      int pos = leaf->num_records;

      /* βρίσκουμε τη σωστή θέση  */
      while (pos > 0) {
        int prev_key =
            bplus_get_key(&meta->schema, &leaf->records[pos - 1]);
        if (prev_key <= key)
          break;

        leaf->records[pos] = leaf->records[pos - 1];
        pos--;
      }

      leaf->records[pos] = *record;
      leaf->num_records++;

      BF_Block_SetDirty(block);
      CALL_BF(BF_UnpinBlock(block));
      BF_Block_Destroy(&block);

      *leaf_block_id = block_id;
      return 0;   /* δεν έγινε split */
    }

    /* Δεν υπάρχει χώρος -> split φύλλου*/

    /* Φτιάχνουμε προσωρινό πίνακα με όλες τις εγγραφές και μαζι και την νέα */
    Record temp[BPLUS_MAX_DATA_RECORDS + 1];
    int total = leaf->num_records;

    /* βρίσκουμε θέση εισαγωγής της νέας εγγραφής */
    int pos = total;
    for (int i = 0; i < total; i++) {
      int k = bplus_get_key(&meta->schema, &leaf->records[i]);
      if (k > key) {
        pos = i;
        break;
      }
    }

    /*  παρεμβολή στη σωστή θέση */
    for (int i = 0; i < pos; i++)
      temp[i] = leaf->records[i];

    temp[pos] = *record;

    for (int i = pos; i < total; i++)
      temp[i + 1] = leaf->records[i];

    total++;  /* BPLUS_MAX_DATA_RECORDS + 1 */

    /* split στη μέση */
    int left_count = total / 2;
    int right_count = total - left_count;

    /* Αριστερό φύλλο (ο τωρινός κόμβος) */
    leaf->num_records = left_count;
    for (int i = 0; i < left_count; i++)
      leaf->records[i] = temp[i];

    /* Δημιουργία νέου φύλλου */
    int blocks_num;
    CALL_BF(BF_GetBlockCounter(file_desc, &blocks_num));
    int new_leaf_id = blocks_num;

    BF_Block *new_block;
    BF_Block_Init(&new_block);
    CALL_BF(BF_AllocateBlock(file_desc, new_block));
    char *new_data = BF_Block_GetData(new_block);
    memset(new_data, 0, BF_BLOCK_SIZE);

    BPlusDataNode *new_leaf = (BPlusDataNode *) new_data;
    new_leaf->is_leaf = 1;
    new_leaf->num_records = right_count;
    new_leaf->next_leaf = leaf->next_leaf;

    for (int i = 0; i < right_count; i++)
      new_leaf->records[i] = temp[left_count + i];

    /* Ενημέρωση  φύλλων */
    leaf->next_leaf = new_leaf_id;

    /* το κλειδί που ανεβαίνει είναι το πρώτο του νέου φύλλου */
    int up_key = bplus_get_key(&meta->schema, &new_leaf->records[0]);

    BF_Block_SetDirty(block);
    BF_Block_SetDirty(new_block);

    CALL_BF(BF_UnpinBlock(block));
    CALL_BF(BF_UnpinBlock(new_block));
    BF_Block_Destroy(&block);
    BF_Block_Destroy(&new_block);

    *promoted_key = up_key;
    *new_block_id = new_leaf_id;

    /* η εγγραφή είναι  σε ένα από τα δύο φύλλα 
     * αν key < up_key -> στο αριστερό, αλλιώς στο δεξί */
    if (key < up_key)
      *leaf_block_id = block_id;
    else
      *leaf_block_id = new_leaf_id;

    return 1;   /* split */
  }

  /*  Περίπτωση 2: Εσωτερικός κόμβος (BPlusIndexNode)*/
  BPlusIndexNode *node = (BPlusIndexNode *) data;

  /* Βρίσκουμε σε ποιο παιδί θα κατέβουμε */
  int i = 0;
  while (i < node->num_keys && key >= node->keys[i])
    i++;

  int child_id = node->children[i];

  /* Δεν χρειάζεται άλλο τον τωρινό κόμβο κατά την κάθοδο */
  CALL_BF(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);

  int child_promoted_key;
  int child_new_block_id;
  int res = bplus_insert_recursive(
      file_desc, meta, child_id, record,
      &child_promoted_key, &child_new_block_id, leaf_block_id);

  if (res <= 0) {
    /* res == 0 : split μόνο πιο κάτω
     * res == bplus_ERROR : λάθος */
    return res;
  }

  /* Έγινε split στο παιδί -> πρέπει να βάλουμε νέο (key,child)
   *  στον τωρινό εσωτερικό κόμβο */
  BF_Block *iblk;
  BF_Block_Init(&iblk);
  CALL_BF(BF_GetBlock(file_desc, block_id, iblk));
  char *idata = BF_Block_GetData(iblk);
  BPlusIndexNode *inode = (BPlusIndexNode *) idata;

  /* ξαναβρίσκουμε τη θέση i ως προς child_promoted_key */
  i = 0;
  while (i < inode->num_keys &&
         child_promoted_key >= inode->keys[i])
    i++;

  /* Αν χωράει άλλο ένα key -> απλή παρεμβολή */
  if (inode->num_keys < BPLUS_MAX_INDEX_KEYS) {
    for (int j = inode->num_keys; j > i; j--) {
      inode->keys[j] = inode->keys[j - 1];
      inode->children[j + 1] = inode->children[j];
    }

    inode->keys[i] = child_promoted_key;
    inode->children[i + 1] = child_new_block_id;
    inode->num_keys++;

    BF_Block_SetDirty(iblk);
    CALL_BF(BF_UnpinBlock(iblk));
    BF_Block_Destroy(&iblk);

    return 0;  
  }

  /*   Δεν χωράει -> split και του εσωτερικού κόμβου*/

  int total_keys = inode->num_keys;
  int temp_keys[BPLUS_MAX_INDEX_KEYS + 1];
  int temp_children[BPLUS_MAX_INDEX_KEYS + 2];

  for (int j = 0; j < total_keys; j++)
    temp_keys[j] = inode->keys[j];

  for (int j = 0; j < total_keys + 1; j++)
    temp_children[j] = inode->children[j];

  
  int insert_pos = 0;
  while (insert_pos < total_keys &&
         child_promoted_key >= temp_keys[insert_pos])
    insert_pos++;

  for (int j = total_keys; j > insert_pos; j--) {
    temp_keys[j] = temp_keys[j - 1];
    temp_children[j + 1] = temp_children[j];
  }

  temp_keys[insert_pos] = child_promoted_key;
  temp_children[insert_pos + 1] = child_new_block_id;

  total_keys++;

  /* mid: το κλειδί που θα ανέβει  στον parent*/
  int mid = total_keys / 2;
  int up_key = temp_keys[mid];

  /* Αριστερός κόμβος = ο τωρινός */
  inode->num_keys = mid;
  for (int j = 0; j < mid; j++)
    inode->keys[j] = temp_keys[j];

  for (int j = 0; j < mid + 1; j++)
    inode->children[j] = temp_children[j];

  /* Δεξιός νέος κόμβος */
  int blocks_num;
  CALL_BF(BF_GetBlockCounter(file_desc, &blocks_num));
  int new_index_id = blocks_num;

  BF_Block *new_iblk;
  BF_Block_Init(&new_iblk);
  CALL_BF(BF_AllocateBlock(file_desc, new_iblk));
  char *new_idata = BF_Block_GetData(new_iblk);
  memset(new_idata, 0, BF_BLOCK_SIZE);

  BPlusIndexNode *new_inode = (BPlusIndexNode *) new_idata;
  new_inode->is_leaf = 0;
  new_inode->num_keys = total_keys - mid - 1;
  new_inode->unused0 = 0;
  new_inode->unused1 = 0;

  for (int j = 0; j < new_inode->num_keys; j++)
    new_inode->keys[j] = temp_keys[mid + 1 + j];

  for (int j = 0; j < new_inode->num_keys + 1; j++)
    new_inode->children[j] = temp_children[mid + 1 + j];

  BF_Block_SetDirty(iblk);
  BF_Block_SetDirty(new_iblk);

  CALL_BF(BF_UnpinBlock(iblk));
  CALL_BF(BF_UnpinBlock(new_iblk));
  BF_Block_Destroy(&iblk);
  BF_Block_Destroy(&new_iblk);

  *promoted_key = up_key;
  *new_block_id = new_index_id;

  return 1;  /* split */
}



int bplus_create_file(const TableSchema *schema, const char *fileName)
{
  /* Δημιουργία αρχείου BF */
  CALL_BF(BF_CreateFile(fileName));

  int fd;
  CALL_BF(BF_OpenFile(fileName, &fd));

  BF_Block *block;
  BF_Block_Init(&block);

  /* Block 0: μεταδεδομένα BPlusMeta */
  CALL_BF(BF_AllocateBlock(fd, block));
  char *data = BF_Block_GetData(block);
  memset(data, 0, BF_BLOCK_SIZE);

  BPlusMeta meta;
  memset(&meta, 0, sizeof(BPlusMeta));
  meta.schema     = *schema;
  meta.root_block = 1;   /* η ρίζα θα είναι στο block 1 (φύλλο) */

  memcpy(data, &meta, sizeof(BPlusMeta));

  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);

  /* Block 1: αρχικό φύλλο = και ρίζα */
  BF_Block_Init(&block);
  CALL_BF(BF_AllocateBlock(fd, block));
  data = BF_Block_GetData(block);
  memset(data, 0, BF_BLOCK_SIZE);

  BPlusDataNode *leaf = (BPlusDataNode *) data;
  leaf->is_leaf     = 1;
  leaf->num_records = 0;
  leaf->next_leaf   = -1;

  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);

  CALL_BF(BF_CloseFile(fd));
  return bplus_OK;
}

int bplus_open_file(const char *fileName, int *file_desc, BPlusMeta **metadata)
{
  CALL_BF(BF_OpenFile(fileName, file_desc));

  BF_Block *block;
  BF_Block_Init(&block);
  CALL_BF(BF_GetBlock(*file_desc, 0, block));
  char *data = BF_Block_GetData(block);

  *metadata = malloc(sizeof(BPlusMeta));
  if (*metadata == NULL) {
    BF_UnpinBlock(block);
    BF_Block_Destroy(&block);
    return bplus_ERROR;
  }

  memcpy(*metadata, data, sizeof(BPlusMeta));

  CALL_BF(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);

  return bplus_OK;
}

int bplus_close_file(const int file_desc, BPlusMeta* metadata)
{
  /* Ξαναγράφουμε τα metadata στο block 0 */
  BF_Block *block;
  BF_Block_Init(&block);
  CALL_BF(BF_GetBlock(file_desc, 0, block));
  char *data = BF_Block_GetData(block);

  memcpy(data, metadata, sizeof(BPlusMeta));
  BF_Block_SetDirty(block);

  CALL_BF(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);

  free(metadata);

  CALL_BF(BF_CloseFile(file_desc));
  return bplus_OK;
}

int bplus_record_insert(const int file_desc, BPlusMeta *metadata, const Record *record)
{
  int promoted_key;
  int new_block_id;
  int leaf_block_id = -1;

  int res = bplus_insert_recursive(
      file_desc,
      metadata,
      metadata->root_block,
      record,
      &promoted_key,
      &new_block_id,
      &leaf_block_id);

  if (res == bplus_ERROR)
    return bplus_ERROR;

  /* Αν δεν έγινε split στη ρίζα, απλά επιστρέφουμε το block του φύλλου */
  if (res == 0)
    return leaf_block_id;

  /*  Έγινε split στη ρίζα -> δημιουργία νέας ρίζας */

  int blocks_num;
  CALL_BF(BF_GetBlockCounter(file_desc, &blocks_num));
  int new_root_id = blocks_num;

  BF_Block *block;
  BF_Block_Init(&block);
  CALL_BF(BF_AllocateBlock(file_desc, block));
  char *data = BF_Block_GetData(block);
  memset(data, 0, BF_BLOCK_SIZE);

  BPlusIndexNode *root = (BPlusIndexNode *) data;
  root->is_leaf = 0;
  root->num_keys = 1;
  root->unused0 = 0;
  root->unused1 = 0;

  root->keys[0] = promoted_key;
  root->children[0] = metadata->root_block;  /* παλιά ρίζα */
  root->children[1] = new_block_id;          /* νέος κόμβος */

  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);

  metadata->root_block = new_root_id;

  return leaf_block_id;
}

int bplus_record_find(const int file_desc,
                      const BPlusMeta *metadata,
                      const int key,
                      Record** out_record)
{
  *out_record = NULL;

  int current = metadata->root_block;
  BF_Block *block;
  BF_Block_Init(&block);

  while (1) {
    CALL_BF(BF_GetBlock(file_desc, current, block));
    char *data = BF_Block_GetData(block);
    int is_leaf = *((int *) data);

    if (is_leaf) {
      /*  γραμμική αναζήτηση μέσα στις εγγραφές
       */
      BPlusDataNode *leaf = (BPlusDataNode *) data;

      for (int i = 0; i < leaf->num_records; i++) {
        int k = bplus_get_key(&metadata->schema, &leaf->records[i]);
        if (k == key) {
          if (*out_record == NULL) {
            *out_record = malloc(sizeof(Record));
            if (*out_record == NULL) {
              CALL_BF(BF_UnpinBlock(block));
              BF_Block_Destroy(&block);
              return bplus_ERROR;
            }
          }
          **out_record = leaf->records[i];

          CALL_BF(BF_UnpinBlock(block));
          BF_Block_Destroy(&block);
          return bplus_OK;
        }
      }

      /* Δεν βρέθηκε στο φύλλο */
      CALL_BF(BF_UnpinBlock(block));
      BF_Block_Destroy(&block);
      return bplus_ERROR;
    }

    /* Εσωτερικός κόμβος: κατεβαίνουμε στο σωστό παιδι
     */
    BPlusIndexNode *node = (BPlusIndexNode *) data;
    int i = 0;
    while (i < node->num_keys && key >= node->keys[i])
      i++;

    int next = node->children[i];
    CALL_BF(BF_UnpinBlock(block));
    current = next;
  }
}
