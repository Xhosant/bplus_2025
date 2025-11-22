//
// Created by User on 17/11/2025.
//

#ifndef SORTED_LINKED_LIST_H
#define SORTED_LINKED_LIST_H

#endif //SORTED_LINKED_LIST_H

struct sll_node {
  int id;
  int blockID; //TODO this should be a blockID;
  struct sll_node* next;
};

int sll_insert_data(struct sll_node** head, int id, void* pointer);
int sll_insert_index(struct sll_node** head, int id, void* pointer);
void sll_delete(struct sll_node** head, int id); //unneeded
int sll_get(struct sll_node** head, int id);
void* sll_get_branch(struct sll_node** head, int id); //TODO this should return a blockID
void sll_clear(struct sll_node** head);
void sll_swap(struct sll_node** head, int targetID, int newID, int newBlockID);
int sll_count(struct sll_node** head);
struct sll_node* sll_split(struct sll_node** head);