#ifndef LINKED_LIST_H
#define LINKED_LIST_H

/** 
 * @file linked_list.h
 * @brief Header file for linked_list.c, containing function declarations
 *        and the LinkedList structure definition.
 *
 * @note This is a doubly linked list implementation.
 *
 * @note The list does not manage the memory of the objects it contains;
 *       it is the caller's responsibility to free the objects if needed.
 */

// --- Data Structures ---
typedef struct list_node {
    void* data;
    struct list_node* next;
    struct list_node* prev;
} list_node_t;

typedef struct linked_list {
    int members_count;
    list_node_t head;
    list_node_t tail;
} linked_list_t;

// --- Function Declarations ---
/**
 * @brief Get the number of elements in the list.
 * @param list Pointer to the LinkedList.
 * @return The number of elements in the list.
 */
int  list_length(linked_list_t* list);

/**
 * @brief Check if the list is empty.
 * @param list Pointer to the LinkedList.
 * @return 1 if the list is empty, 0 otherwise.
 */
int  list_is_empty(linked_list_t* list);

/**
 * @brief Append an object to the end of the list.
 * @param list Pointer to the LinkedList.
 * @param data Pointer to the object to append.
 * @return 1 on success, 0 on failure (e.g., memory allocation failure).
 */
int  list_append(linked_list_t* list, void* data);

/**
 * @brief Append an object to the beginning of the list.
 * @param list Pointer to the LinkedList.
 * @param data Pointer to the object to append.
 * @return 1 on success, 0 on failure (e.g., memory allocation failure).
 */
int  list_append_left(linked_list_t* list, void* data);

/**
 * @brief Remove and return the last object from the list.
 * @param list Pointer to the LinkedList.
 * @return Pointer to the removed object, or NULL if the list is empty.
 */
list_node_t* list_pop(linked_list_t* list);

/**
 * @brief Remove and return the first object from the list.
 * @param list Pointer to the LinkedList.
 * @return Pointer to the removed object, or NULL if the list is empty.
 */
list_node_t* list_pop_left(linked_list_t* list);

/**
 * @brief Remove a specific node from the list.
 * @param list Pointer to the LinkedList.
 * @param node Pointer to the ListNode to remove.
 */
void list_remove(linked_list_t* list, list_node_t* node);

/**
 * @brief Clear all elements from the list.
 * @param list Pointer to the LinkedList.
 */
void list_clear(linked_list_t* list);

/**
 * @brief Get the first node in the list.
 * @param list Pointer to the LinkedList.
 * @return Pointer to the first ListNode, or NULL if the list is empty.
 */
list_node_t* list_first(linked_list_t* list);

/**
 * @brief Get the last node in the list.
 * @param list Pointer to the LinkedList.
 * @return Pointer to the last ListNode, or NULL if the list is empty.
 */
list_node_t* list_last(linked_list_t* list);

/**
 * @brief Get the next node in the list.
 * @param list Pointer to the LinkedList.
 * @param node Pointer to the current ListNode.
 * @return Pointer to the next ListNode, or NULL if at the end of the list.
 */
list_node_t* list_next(linked_list_t* list, list_node_t* node);

/**
 * @brief Get the previous node in the list.
 * @param list Pointer to the LinkedList.
 * @param node Pointer to the current ListNode.
 * @return Pointer to the previous ListNode, or NULL if at the beginning of the list.
 */
list_node_t* list_prev(linked_list_t* list, list_node_t* node);

/** * @brief Find a node in the list containing the specified data.
 * @param list Pointer to the LinkedList.
 * @param data Pointer to the data to search for.
 * @return Pointer to the found ListNode, or NULL if not found.
 */
list_node_t* list_find(linked_list_t* list, void* data);

/**
 * @brief Initialize a LinkedList structure.
 * @param list Pointer to the LinkedList to initialize.
 * @return 1 on success, 0 on failure (e.g., memory allocation failure).
 */
int list_init(linked_list_t* list);

#endif // LINKED_LIST_H