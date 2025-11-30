#include "bplus_datanode.h"

void bplus_datanode_init(BPlusDataNode *node)
{
    node->is_leaf = 1;
    node->num_records = 0;
    node->next_leaf = -1;
}
