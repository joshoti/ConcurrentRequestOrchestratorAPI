#include <stdlib.h>
#include "common.h"
#include "linked_list.h"


int list_length(linked_list_t* list) {
    return list->members_count;
}
int list_is_empty(linked_list_t* list) {
    return list->members_count == 0;
}

int list_append(linked_list_t* list, void* obj) {
    list_node_t* newNode = (list_node_t*) malloc(sizeof(list_node_t));
    if (newNode == NULL) {
        return FALSE; // Memory allocation failure
    }
    newNode->data = obj;
    newNode->next = &list->tail;
    newNode->prev = list->tail.prev;

    list->tail.prev->next = newNode;
    list->tail.prev = newNode;
    list->members_count++;

    return TRUE;
}

int list_append_left(linked_list_t* list, void* obj) {
    list_node_t* newNode = (list_node_t*) malloc(sizeof(list_node_t));
    if (newNode == NULL) {
        return FALSE; // Memory allocation failure
    }
    newNode->data = obj;
    newNode->next = list->head.next;
    newNode->prev = &list->head;

    list->head.next->prev = newNode;
    list->head.next = newNode;
    list->members_count++;

    return TRUE;
}
list_node_t* list_pop(linked_list_t* list) {
    if (list_is_empty(list)) {
        return NULL;
    }
    list_node_t* last = list->tail.prev;
    last->prev->next = &list->tail;
    list->tail.prev = last->prev;
    list->members_count--;
    return last;
}

list_node_t* list_pop_left(linked_list_t* list) {
    if (list_is_empty(list)) {
        return NULL;
    }
    list_node_t* first = list->head.next;
    first->next->prev = &list->head;
    list->head.next = first->next;
    list->members_count--;
    return first;
}

void list_remove(linked_list_t* list, list_node_t* node) {
    if (node == NULL || list_is_empty(list)) {
        return;
    }
    node->prev->next = node->next;
    node->next->prev = node->prev;
    list->members_count--;
    free(node);
}

void list_clear(linked_list_t* list) {
    while (!list_is_empty(list)) {
    list_node_t* node = list_pop_left(list);
        free(node);
    }
}

list_node_t* list_first(linked_list_t* list) {
    if (list_is_empty(list)) {
        return NULL;
    }
    return list->head.next;
}

list_node_t* list_last(linked_list_t* list) {
    if (list_is_empty(list)) {
        return NULL;
    }
    return list->tail.prev;
}

list_node_t* list_next(linked_list_t* list, list_node_t* node) {
    if (list == NULL || node == NULL || node->next == NULL) {
        return NULL;
    }
    // Check if next is the tail sentinel
    if (node->next == &list->tail) {
        return NULL;
    }
    return node->next;
}

list_node_t* list_prev(linked_list_t* list, list_node_t* node) {
    if (list == NULL || node == NULL || node->prev == NULL) {
        return NULL;
    }
    // Check if prev is the head sentinel
    if (node->prev == &list->head) {
        return NULL;
    }
    return node->prev;
}

list_node_t* list_find(linked_list_t* list, void* data) {
    list_node_t* curr = NULL;
    for (curr = list_first(list); curr != NULL; curr = curr->next) {
        if (curr->data == data) {
            return curr;
        }
    }
    return NULL;
}

int list_init(linked_list_t* list) {
    if (list == NULL) {
        return FALSE; // Invalid list pointer
    }
    list->members_count = 0;
    list->head.next = &list->tail;
    list->head.prev = NULL;
    list->head.data = NULL;

    list->tail.prev = &list->head;
    list->tail.next = NULL;
    list->tail.data = NULL;

    return TRUE;
}