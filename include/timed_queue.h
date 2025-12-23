#ifndef TIMED_QUEUE_H
#define TIMED_QUEUE_H

#include "linked_list.h"

/**
 * @file timed_queue.h
 * @brief Wrapper around LinkedList that automatically tracks last interaction time.
 *
 * @note This queue automatically updates the last_interaction_time_us field
 *       whenever items are added or removed.
 */

typedef struct timed_queue {
    linked_list_t list;
    unsigned long last_interaction_time_us;
} timed_queue_t;

// --- Function Declarations ---

/**
 * @brief Initialize a TimedQueue structure.
 * @param tq Pointer to the TimedQueue to initialize.
 * @return 1 on success, 0 on failure.
 */
int timed_queue_init(timed_queue_t* tq);

/**
 * @brief Get the number of elements in the queue.
 * @param tq Pointer to the TimedQueue.
 * @return The number of elements in the queue.
 */
int timed_queue_length(timed_queue_t* tq);

/**
 * @brief Check if the queue is empty.
 * @param tq Pointer to the TimedQueue.
 * @return 1 if the queue is empty, 0 otherwise.
 */
int timed_queue_is_empty(timed_queue_t* tq);

/**
 * @brief Enqueue (append) an object to the end of the queue.
 * Automatically updates the last_interaction_time_us.
 * @param tq Pointer to the TimedQueue.
 * @param data Pointer to the object to enqueue.
 * @return 1 on success, 0 on failure (e.g., memory allocation failure).
 */
int timed_queue_enqueue(timed_queue_t* tq, void* data);

/**
 * @brief Enqueue (append) an object to the front of the queue.
 * Automatically updates the last_interaction_time_us.
 * @param tq Pointer to the TimedQueue.
 * @param data Pointer to the object to enqueue.
 * @return 1 on success, 0 on failure (e.g., memory allocation failure).
 */
int timed_queue_enqueue_front(timed_queue_t* tq, void* data);

/**
 * @brief Dequeue (remove) and return the last object from the queue.
 * Automatically updates the last_interaction_time_us.
 * @param tq Pointer to the TimedQueue.
 * @return Pointer to the removed ListNode, or NULL if the queue is empty.
 */
list_node_t* timed_queue_dequeue(timed_queue_t* tq);

/**
 * @brief Dequeue (remove) and return the first object from the queue.
 * Automatically updates the last_interaction_time_us.
 * @param tq Pointer to the TimedQueue.
 * @return Pointer to the removed ListNode, or NULL if the queue is empty.
 */
list_node_t* timed_queue_dequeue_front(timed_queue_t* tq);

/**
 * @brief Remove a specific node from the queue.
 * Automatically updates the last_interaction_time_us.
 * @param tq Pointer to the TimedQueue.
 * @param node Pointer to the ListNode to remove.
 */
void timed_queue_remove(timed_queue_t* tq, list_node_t* node);

/**
 * @brief Clear all elements from the queue.
 * Automatically updates the last_interaction_time_us.
 * @param tq Pointer to the TimedQueue.
 */
void timed_queue_clear(timed_queue_t* tq);

/**
 * @brief Get the first node in the queue without removing it.
 * Does NOT update the timestamp (read-only operation).
 * @param tq Pointer to the TimedQueue.
 * @return Pointer to the first ListNode, or NULL if the queue is empty.
 */
list_node_t* timed_queue_first(timed_queue_t* tq);

/**
 * @brief Get the last node in the queue without removing it.
 * Does NOT update the timestamp (read-only operation).
 * @param tq Pointer to the TimedQueue.
 * @return Pointer to the last ListNode, or NULL if the queue is empty.
 */
list_node_t* timed_queue_last(timed_queue_t* tq);

/**
 * @brief Find a node in the queue containing the specified data.
 * Does NOT update the timestamp (read-only operation).
 * @param tq Pointer to the TimedQueue.
 * @param data Pointer to the data to search for.
 * @return Pointer to the found ListNode, or NULL if not found.
 */
list_node_t* timed_queue_find(timed_queue_t* tq, void* data);

/**
 * @brief Get the next node in the queue.
 * Does NOT update the timestamp (read-only operation).
 * @param tq Pointer to the TimedQueue.
 * @param node Pointer to the current ListNode.
 * @return Pointer to the next ListNode, or NULL if at the end of the queue.
 */
list_node_t* timed_queue_next(timed_queue_t* tq, list_node_t* node);

/**
 * @brief Get the previous node in the queue.
 * Does NOT update the timestamp (read-only operation).
 * @param tq Pointer to the TimedQueue.
 * @param node Pointer to the current ListNode.
 * @return Pointer to the previous ListNode, or NULL if at the beginning of the queue.
 */
list_node_t* timed_queue_prev(timed_queue_t* tq, list_node_t* node);

#endif // TIMED_QUEUE_H