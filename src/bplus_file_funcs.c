#include "bplus_file_funcs.h"
#include "bf.h"
#include "bplus_file_structs.h"
#include "bplus_datanode.h"
#include "bplus_index_node.h"
#include "record.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CALL_BF(call)         \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK)        \
    {                         \
      BF_PrintError(code);    \
      return -1;              \
    }                         \
  }


/*                     ΒΟΗΘΗΤΙΚΕΣ ΣΥΝΑΡΤΗΣΕΙΣ                        */


/*
 * Επιστρέφει το κλειδί της εγγραφής (int) σύμφωνα με το δοσμένο schema
 */
static int bplus_get_key(const TableSchema *schema, const Record *rec) {
  return record_get_key(schema, rec);
}

/*
 * Αναδρομική εισαγωγή σε υποδέντρο με ρίζα στο block_id.
 * Επιστρεφόμενα μέσω pointer:
 *  - *promoted_key  : κλειδί που ανεβαίνει στον γονέα σε περίπτωση split
 *  - *new_block_id  : block id του νέου κόμβου (δεξιού) που δημιουργήθηκε
 *  - *leaf_block_id : block id του φύλλου στο οποίο τελικά κατέληξε η εγγραφή
 *
 * Τι επιστρέφει :
 *  -  0  : η εισαγωγή έγινε κανονικά, δεν έγινε split στο block_id
 *  -  1  : έγινε split στο block_id, *promoted_key και *new_block_id είναι έγκυρα
 *  - -1  : σφάλμα 
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
  /* Υπολογίζουμε το key της εγγραφής ανάλογα με το schema */
  int key = bplus_get_key(&meta->schema, record);

  /* Φορτώνουμε το block από το δίσκο */
  BF_Block *block;
  BF_Block_Init(&block);
  CALL_BF(BF_GetBlock(file_desc, block_id, block));
  char *data = BF_Block_GetData(block);

  /* Το πρώτο πεδίο σε κάθε κόμβο είναι is_leaf (int) */
  int is_leaf = *((int *) data);

  
  /* Περίπτωση 1: Φύλλο (BPlusDataNode) */

  if (is_leaf) {
    BPlusDataNode *leaf = (BPlusDataNode *) data;

    /* 1. Έλεγχος για διπλό κλειδί  */
    for (int i = 0; i < leaf->num_records; i++) {
      int k = bplus_get_key(&meta->schema, &leaf->records[i]);
      if (k == key) {
        /* Βρήκαμε ίδιο key ->  σφάλμα εισαγωγής. */
        CALL_BF(BF_UnpinBlock(block));
        BF_Block_Destroy(&block);
        return -1;
      }
    }

    /* 2. Αν υπάρχει χώρος στο φύλλο -> ταξινομημένη εισαγωγή in-place */
    if (leaf->num_records < BPLUS_MAX_DATA_RECORDS) {
      int pos = leaf->num_records;

      /*
       * Μετακινούμε προς τα δεξιά τα μεγαλύτερα κλειδιά,
       * ώστε να κάνουμε space για το νέο .
       */
      while (pos > 0) {
        int prev_key =
            bplus_get_key(&meta->schema, &leaf->records[pos - 1]);
        if (prev_key <= key)
          break;

        leaf->records[pos] = leaf->records[pos - 1];
        pos--;
      }

      /* Εισαγωγή της νέας εγγραφής στη σωστή θέση */
      leaf->records[pos] = *record;
      leaf->num_records++;

      BF_Block_SetDirty(block);
      CALL_BF(BF_UnpinBlock(block));
      BF_Block_Destroy(&block);

      *leaf_block_id = block_id;   
      return 0;                    /* δεν έγινε split */
    }

    
    /* 3. Δεν υπάρχει χώρος -> πρέπει να κάνουμε split του φύλλου  */
   

    /*
     * Δημιουργούμε έναν προσωρινό πίνακα με τις εγγραφές του φύλλου
     * και μαζι και την νέα εγγραφή, ώστε να κάνουμε split πάνω σε αυτόν.
     */
    Record temp[BPLUS_MAX_DATA_RECORDS + 1];
    int total = leaf->num_records;

    /* Βρίσκουμε τη θέση εισαγωγής της νέας εγγραφής στον temp */
    int pos = total;
    for (int i = 0; i < total; i++) {
      int k = bplus_get_key(&meta->schema, &leaf->records[i]);
      if (k > key) {
        pos = i;
        break;
      }
    }

    /* Αντιγράφουμε ότι προηγείται της θέσης εισαγωγής */
    for (int i = 0; i < pos; i++)
      temp[i] = leaf->records[i];

    /* Βάζουμε τη νέα εγγραφή στη θέση pos */
    temp[pos] = *record;

    /* Αντιγράφουμε ότι ακολουθεί */
    for (int i = pos; i < total; i++)
      temp[i + 1] = leaf->records[i];

    total++;  /* πλέον = BPLUS_MAX_DATA_RECORDS + 1 */

    /* Split στη μέση  */
    int left_count = total / 2;
    int right_count = total - left_count;

    /*
     * Αριστερό φύλλο: ο τωρινός κόμβος (block_id).
     * Γράφουμε σε αυτό τις left_count εγγραφές.
     */
    leaf->num_records = left_count;
    for (int i = 0; i < left_count; i++)
      leaf->records[i] = temp[i];

    /*
     * Δεξί φύλλο νέο block στο τέλος του αρχείου.
     */
    int blocks_num;
    CALL_BF(BF_GetBlockCounter(file_desc, &blocks_num));
    int new_leaf_id = blocks_num;

    BF_Block *new_block;
    BF_Block_Init(&new_block);
    CALL_BF(BF_AllocateBlock(file_desc, new_block));
    char *new_data = BF_Block_GetData(new_block);
    memset(new_data, 0, BF_BLOCK_SIZE);

    BPlusDataNode *new_leaf = (BPlusDataNode *) new_data;
    new_leaf->is_leaf     = 1;
    new_leaf->num_records = right_count;
    new_leaf->next_leaf   = leaf->next_leaf;  

    /* Αντιγράφουμε τις right_count εγγραφές στο νέο φύλλο */
    for (int i = 0; i < right_count; i++)
      new_leaf->records[i] = temp[left_count + i];

    /* Ενημερώνουμε τη λίστα των φύλλων: leaf -> new_leaf -> (παλιός next) */
    leaf->next_leaf = new_leaf_id;

    /*
     * Το κλειδί που ανεβαίνει στον γονέα είναι το πρώτο κλειδί του νέου φύλλου.
     * Έτσι, όλοι οι keys < up_key βρίσκονται στο αριστερό, και >= στο δεξί.
     */
    int up_key = bplus_get_key(&meta->schema, &new_leaf->records[0]);

    BF_Block_SetDirty(block);
    BF_Block_SetDirty(new_block);

    CALL_BF(BF_UnpinBlock(block));
    CALL_BF(BF_UnpinBlock(new_block));
    BF_Block_Destroy(&block);
    BF_Block_Destroy(&new_block);

    *promoted_key = up_key;
    *new_block_id = new_leaf_id;

    /* Προσδιορίζουμε σε ποιο φύλλο τελικά βρίσκεται η εγγραφή */
    if (key < up_key)
      *leaf_block_id = block_id;    /* στο αριστερό */
    else
      *leaf_block_id = new_leaf_id; /* στο δεξί */

    return 1;   /* split */
  }

  
  /* Περίπτωση 2: Εσωτερικός κόμβος (BPlusIndexNode)                       */
 

  BPlusIndexNode *node = (BPlusIndexNode *) data;

  /*
   * Βρίσκουμε το σωστό παιδί με βάση τα keys του κόμβου:
   *  children: [c0] key0 [c1] key1 [c2] ... key(n-1) [cn]
   *
   * Θέλουμε το πρώτο i όπου key < keys[i], αλλιώς πηγαίνουμε στο τελευταίο.
   */
  int i = 0;
  while (i < node->num_keys && key >= node->keys[i])
    i++;

  int child_id = node->children[i];

  /* Δεν χρειαζόμαστε άλλο τον τωρινό κόμβο κατά την κάθοδο */
  CALL_BF(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);

  /* Αναδρομική εισαγωγή στο παιδί */
  int child_promoted_key;
  int child_new_block_id;
  int res = bplus_insert_recursive(
      file_desc, meta, child_id, record,
      &child_promoted_key, &child_new_block_id, leaf_block_id);

  if (res <= 0) {
    /*
     * res ==  0 : split έγινε πιο κάτω, αλλά στον πατέρα χωρούσε ο νέος key
     * res == -1 : κάποιο σφάλμα
     */
    return res;
  }

  
  /* Split έγινε στο παιδί -> πρέπει να εισάγουμε νέο (key, child) εδώ     */
 

  BF_Block *iblk;
  BF_Block_Init(&iblk);
  CALL_BF(BF_GetBlock(file_desc, block_id, iblk));
  char *idata = BF_Block_GetData(iblk);
  BPlusIndexNode *inode = (BPlusIndexNode *) idata;

  /*
   * Βρίσκουμε τη θέση όπου θα μπει το child_promoted_key στον τωρινό κόμβο.
   */
  i = 0;
  while (i < inode->num_keys &&
         child_promoted_key >= inode->keys[i])
    i++;

 
  /* Αν ΧΩΡΑΕΙ κι άλλο key -> απλή παρεμβολή   */
 
  if (inode->num_keys < BPLUS_MAX_INDEX_KEYS) {

    /* Μετακινούμε keys και children προς τα δεξιά για να ανοίξουμε τρύπα */
    for (int j = inode->num_keys; j > i; j--) {
      inode->keys[j] = inode->keys[j - 1];
      inode->children[j + 1] = inode->children[j];
    }

    /* Γράφουμε τη νέα εγγραφή */
    inode->keys[i] = child_promoted_key;
    inode->children[i + 1] = child_new_block_id;
    inode->num_keys++;

    BF_Block_SetDirty(iblk);
    CALL_BF(BF_UnpinBlock(iblk));
    BF_Block_Destroy(&iblk);

    return 0;   /* split δεν φτάνει μέχρι πάνω */
  }

  
  /* Αλλιώς: Ο κόμβος είναι πλήρης -> split    */
  

  int total_keys = inode->num_keys;

  /* Προσωρινοί πίνακες με όλα τα keys & children + τη νέα εγγραφή */
  int temp_keys[BPLUS_MAX_INDEX_KEYS + 1];
  int temp_children[BPLUS_MAX_INDEX_KEYS + 2];

  for (int j = 0; j < total_keys; j++)
    temp_keys[j] = inode->keys[j];

  for (int j = 0; j < total_keys + 1; j++)
    temp_children[j] = inode->children[j];

  /* Βρίσκουμε σε ποια θέση θα εισαγάγουμε το child_promoted_key */
  int insert_pos = 0;
  while (insert_pos < total_keys &&
         child_promoted_key >= temp_keys[insert_pos])
    insert_pos++;

  /* Μετακινούμε προς τα δεξιά για να χωρέσει */
  for (int j = total_keys; j > insert_pos; j--) {
    temp_keys[j] = temp_keys[j - 1];
    temp_children[j + 1] = temp_children[j];
  }

  /* Βάζουμε το νέο key & child */
  temp_keys[insert_pos] = child_promoted_key;
  temp_children[insert_pos + 1] = child_new_block_id;

  total_keys++;

  /*
   * mid: δείκτης του κλειδιού που θα ανέβει στον γονέα.
   * - keys [0..mid-1]  -> μένουν στον αριστερό κόμβο (παλιός)
   * - keys [mid+1..]   -> πάνε στον νέο δεξιό κόμβο
   */
  int mid = total_keys / 2;
  int up_key = temp_keys[mid];

  /* Αριστερός κόμβος = inode (παλιός κόμβος) */
  inode->num_keys = mid;
  for (int j = 0; j < mid; j++)
    inode->keys[j] = temp_keys[j];

  for (int j = 0; j < mid + 1; j++)
    inode->children[j] = temp_children[j];

  /* Δημιουργία νέου δεξιού εσωτερικού κόμβου */
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

  /* keys & children για το νέο δεξί κόμβο */
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
  
  CALL_BF(BF_CreateFile(fileName));

  int fd;
  CALL_BF(BF_OpenFile(fileName, &fd));

  BF_Block *block;
  BF_Block_Init(&block);

  /*  Block 0: BPlusMeta */
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

  /*  Block 1: ρίζα  */
  BF_Block_Init(&block);
  CALL_BF(BF_AllocateBlock(fd, block));
  data = BF_Block_GetData(block);
  memset(data, 0, BF_BLOCK_SIZE);

  BPlusDataNode *leaf = (BPlusDataNode *) data;
  leaf->is_leaf     = 1;
  leaf->num_records = 0;
  leaf->next_leaf   = -1;   /* δεν υπάρχει επόμενο φύλλο */

  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);

  CALL_BF(BF_CloseFile(fd));
  return 0;
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
    /* Σε περίπτωση αποτυχίας allocation, πρέπει να ξεκαρφιτσώσουμε το block */
    BF_UnpinBlock(block);
    BF_Block_Destroy(&block);
    return -1;
  }

  memcpy(*metadata, data, sizeof(BPlusMeta));

  CALL_BF(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);

  return 0;
}


int bplus_close_file(const int file_desc, BPlusMeta* metadata)
{
  /* Block 0: γράφουμε πίσω τα metadata  */
  BF_Block *block;
  BF_Block_Init(&block);
  CALL_BF(BF_GetBlock(file_desc, 0, block));
  char *data = BF_Block_GetData(block);

  memcpy(data, metadata, sizeof(BPlusMeta));
  BF_Block_SetDirty(block);

  CALL_BF(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);

  /* free της δομής BPlusMeta που είχαμε κάνει malloc στο open */
  free(metadata);

  CALL_BF(BF_CloseFile(file_desc));
  return 0;
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

  if (res == -1)
    return -1;

  /* Αν δεν έγινε split στη ρίζα, απλά επιστρέφουμε το block του φύλλου */
  if (res == 0)
    return leaf_block_id;

  
  /* Έγινε split στη ρίζα -> δημιουργία νέας εσωτερικής ρίζας              */
  

  int blocks_num;
  CALL_BF(BF_GetBlockCounter(file_desc, &blocks_num));
  int new_root_id = blocks_num;

  BF_Block *block;
  BF_Block_Init(&block);
  CALL_BF(BF_AllocateBlock(file_desc, block));
  char *data = BF_Block_GetData(block);
  memset(data, 0, BF_BLOCK_SIZE);

  BPlusIndexNode *root = (BPlusIndexNode *) data;
  root->is_leaf  = 0;
  root->num_keys = 1;
  root->unused0  = 0;
  root->unused1  = 0;

  /* Η νέα ρίζα θα έχει:
   *  keys[0]      = promoted_key
   *  children[0]  = παλιά ρίζα
   *  children[1]  = νέος κόμβος που προέκυψε από split της παλιάς ρίζας
   */
  root->keys[0]      = promoted_key;
  root->children[0]  = metadata->root_block;  /* παλιά ρίζα */
  root->children[1]  = new_block_id;          /* νέος κόμβος */

  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);

  /* Ενημερώνουμε το root_block στα metadata */
  metadata->root_block = new_root_id;

  return leaf_block_id;
}


int bplus_record_find(const int file_desc,
                      const BPlusMeta *metadata,
                      const int key,
                      Record** out_record)
{
  *out_record = NULL;

  /* Ξεκινάμε από τη ρίζα */
  int current = metadata->root_block;
  BF_Block *block;
  BF_Block_Init(&block);

  while (1) {
    CALL_BF(BF_GetBlock(file_desc, current, block));
    char *data = BF_Block_GetData(block);
    int is_leaf = *((int *) data);

    if (is_leaf) {
      /*  Γραμμική αναζήτηση μέσα στις εγγραφές  */
      BPlusDataNode *leaf = (BPlusDataNode *) data;

      for (int i = 0; i < leaf->num_records; i++) {
        int k = bplus_get_key(&metadata->schema, &leaf->records[i]);
        if (k == key) {
          /* Βρήκαμε την εγγραφή με το ζητούμενο key */

          if (*out_record == NULL) {
            *out_record = malloc(sizeof(Record));
            if (*out_record == NULL) {
              CALL_BF(BF_UnpinBlock(block));
              BF_Block_Destroy(&block);
              return -1;
            }
          }

          **out_record = leaf->records[i];

          CALL_BF(BF_UnpinBlock(block));
          BF_Block_Destroy(&block);
          return 0;
        }
      }

      /* Αν φτάσαμε εδώ, δεν βρέθηκε η εγγραφή στο φύλλο */
      CALL_BF(BF_UnpinBlock(block));
      BF_Block_Destroy(&block);
      return -1;
    }

    /* Εσωτερικος κομβος κατεβαίνουμε στο σωστό παιδί  */
    BPlusIndexNode *node = (BPlusIndexNode *) data;
    int i = 0;
    while (i < node->num_keys && key >= node->keys[i])
      i++;

    int next = node->children[i];
    CALL_BF(BF_UnpinBlock(block));
    current = next;   /* συνεχίζουμε τη while στο παιδί */
  }
}
