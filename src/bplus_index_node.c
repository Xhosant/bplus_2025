#include "bplus_index_node.h"
#include <string.h>

void bplus_indexnode_init(BPlusIndexNode *node)
{
    node->is_leaf = 0;
    node->num_keys = 0;
    node->unused0 = 0;
    node->unused1 = 0;
}
