//
// Created by User on 17/11/2025.
//

#ifndef SORTED_LINKED_LIST_H
#define SORTED_LINKED_LIST_H

#endif //SORTED_LINKED_LIST_H

struct sll_node {
  int id;
  void* pointer;
  struct sll_node* next;
};

int sll_insert(struct sll_node** head, int id, void* pointer);
void sll_delete(struct sll_node** head, int id);
void* sll_get(struct sll_node** head, int id);
void* sll_get_branch(struct sll_node** head, int id);
void sll_clear(struct sll_node** head);
void sll_swap(struct sll_node** head, int targetID, int newID, void* newPointer);
int sll_count(struct sll_node** head);
void* sll_split(struct sll_node** head, int position);