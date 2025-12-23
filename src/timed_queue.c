#include <stdlib.h>
#include "timed_queue.h"
#include "linked_list.h"
#include "timeutils.h"
#include "common.h"

int timed_queue_init(timed_queue_t* tq) {
    if (tq == NULL) {
        return FALSE;
    }
    
    int result = list_init(&tq->list);
    if (result) {
        tq->last_interaction_time_us = get_time_in_us();
    }
    return result;
}

int timed_queue_length(timed_queue_t* tq) {
    if (tq == NULL) {
        return 0;
    }
    return list_length(&tq->list);
}

int timed_queue_is_empty(timed_queue_t* tq) {
    if (tq == NULL) {
        return TRUE;
    }
    return list_is_empty(&tq->list);
}

int timed_queue_enqueue(timed_queue_t* tq, void* data) {
    if (tq == NULL) {
        return FALSE;
    }
    
    int result = list_append(&tq->list, data);
    if (result) {
        tq->last_interaction_time_us = get_time_in_us();
    }
    return result;
}

int timed_queue_enqueue_front(timed_queue_t* tq, void* data) {
    if (tq == NULL) {
        return FALSE;
    }
    
    int result = list_append_left(&tq->list, data);
    if (result) {
        tq->last_interaction_time_us = get_time_in_us();
    }
    return result;
}

list_node_t* timed_queue_dequeue(timed_queue_t* tq) {
    if (tq == NULL) {
        return NULL;
    }
    
    list_node_t* node = list_pop(&tq->list);
    if (node != NULL) {
        tq->last_interaction_time_us = get_time_in_us();
    }
    return node;
}

list_node_t* timed_queue_dequeue_front(timed_queue_t* tq) {
    if (tq == NULL) {
        return NULL;
    }
    
    list_node_t* node = list_pop_left(&tq->list);
    if (node != NULL) {
        tq->last_interaction_time_us = get_time_in_us();
    }
    return node;
}

void timed_queue_remove(timed_queue_t* tq, list_node_t* node) {
    if (tq == NULL || node == NULL) {
        return;
    }
    
    list_remove(&tq->list, node);
    tq->last_interaction_time_us = get_time_in_us();
}

void timed_queue_clear(timed_queue_t* tq) {
    if (tq == NULL) {
        return;
    }
    
    list_clear(&tq->list);
    tq->last_interaction_time_us = get_time_in_us();
}

list_node_t* timed_queue_first(timed_queue_t* tq) {
    if (tq == NULL) {
        return NULL;
    }
    // Read-only operation - does NOT update timestamp
    return list_first(&tq->list);
}

list_node_t* timed_queue_last(timed_queue_t* tq) {
    if (tq == NULL) {
        return NULL;
    }
    // Read-only operation - does NOT update timestamp
    return list_last(&tq->list);
}

list_node_t* timed_queue_find(timed_queue_t* tq, void* data) {
    if (tq == NULL) {
        return NULL;
    }
    // Read-only operation - does NOT update timestamp
    return list_find(&tq->list, data);
}

list_node_t* timed_queue_next(timed_queue_t* tq, list_node_t* node) {
    if (tq == NULL) {
        return NULL;
    }
    // Read-only operation - does NOT update timestamp
    return list_next(&tq->list, node);
}

list_node_t* timed_queue_prev(timed_queue_t* tq, list_node_t* node) {
    if (tq == NULL) {
        return NULL;
    }
    // Read-only operation - does NOT update timestamp
    return list_prev(&tq->list, node);
}
