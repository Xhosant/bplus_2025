//
// Created by User on 17/11/2025.
//
#include "sorted_linked_list.h"

#include <stdlib.h>
#include <tgmath.h>

int sll_insert(struct sll_node** head, int id, void* pointer) {
    struct sll_node* new_node = malloc(sizeof(struct sll_node));
    new_node->id = id;
    new_node->pointer = pointer;
    new_node->next = NULL;
    if (*head == NULL) {
        *head = new_node;
        return 0;
    }
    struct sll_node* temp = *head;
    struct sll_node* prev = NULL;
    while (temp->next != NULL) {
        if (temp->id == new_node->id) {
            break;
        }
        else if (temp->id < new_node->id) {
            prev = temp;
            temp = temp->next;
        }
        else if (temp->id > new_node->id) {
            prev->next = new_node;
            new_node->next = temp;
            return 0;
        }
    }
    free(new_node);
    return -1;
}

void sll_delete(struct sll_node** head, int id) {
    struct sll_node* temp = *head;
    struct sll_node* prev = NULL;
    if (temp->id == id) {
        *head = (*head)->next;
        free(temp);
        return;
    }
    while (temp != NULL && temp->id != id) {
        prev = temp;
        temp = temp->next;
    }
    if (temp != NULL) {
        prev->next = temp->next;
        free(temp);
    }
}

void* sll_get(struct sll_node** head, int id) {
    struct sll_node* current = *head;
    while (current != NULL) {
        if (current->id == id) {
            return current;
        }
        else if (current->id < id) {
            current = current->next;
        }
        else if (current->id > id) {
            break;
        }
    }
    return NULL;
}

void* sll_get_branch(struct sll_node** head, int id) {
    struct sll_node* current = *head;
    while (current != NULL) {
        if (current->id <= id) {
            current = current->next;
        }
        else return current->pointer;
    }
}

void sll_clear(struct sll_node** head) {
    struct sll_node* temp = *head;
    while (temp != NULL) {
        struct sll_node* next = temp->next;
        free(temp);
        temp = next;
    }
}

void sll_swap(struct sll_node** head, int targetID, int newID, void* newPointer) {
    struct sll_node* temp = *head;
    while (temp != NULL && temp->id != targetID) {
        temp = temp->next;
    }
    if (temp != NULL) {
        temp->pointer = newPointer;
        temp->id = newID;
    }
}

int sll_count(struct sll_node** head) {
    int count = 0;
    struct sll_node* current = *head;
    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}

void* sll_split(struct sll_node** head, int position) {
    struct sll_node* current = *head;
    while (position > 1 && current != NULL) {
        current = current->next;
        position--;
    }
    struct sll_node* output = current->next;
    current->next = NULL;
    return output;
}