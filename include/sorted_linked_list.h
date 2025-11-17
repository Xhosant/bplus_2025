//
// Created by User on 17/11/2025.
//

#ifndef SORTED_LINKED_LIST_H
#define SORTED_LINKED_LIST_H

#endif //SORTED_LINKED_LIST_H

struct Node {
  int id;
  void* pointer;
  struct Node* next;
};

int sll_insert(struct Node** head, int id, void* pointer);
void sll_delete(struct Node** head, int id);
void* sll_get(struct Node** head, int id);
void sll_clear(struct Node** head);
void sll_swap(struct Node** head, int targetID, int newID, void* newPointer);
int sll_count(struct Node** head);
void* sll_split(struct Node** head);