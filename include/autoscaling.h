#ifndef AUTOSCALING_H
#define AUTOSCALING_H

#include <pthread.h>

struct printer_pool;
struct timed_queue;
struct simulation_parameters;
struct simulation_statistics;

// --- Autoscaling Thread Arguments ---
typedef struct autoscaling_thread_args {
    pthread_mutex_t* job_queue_mutex;
    pthread_mutex_t* stats_mutex;
    pthread_mutex_t* simulation_state_mutex;
    pthread_cond_t* job_queue_not_empty_cv;
    pthread_cond_t* refill_needed_cv;
    pthread_cond_t* refill_supplier_cv;
    pthread_t* paper_refill_thread;
    struct timed_queue* job_queue;
    struct linked_list* paper_refill_queue;
    struct simulation_parameters* params;
    struct simulation_statistics* stats;
    int* all_jobs_served;
    int* all_jobs_arrived;
    struct printer_pool* pool;
} autoscaling_thread_args_t;

// --- Autoscaling Functions ---

/**
 * @brief Get the scale-up threshold based on current active printer count.
 * Uses stepped thresholds to prevent thrashing:
 * - 2 printers: threshold = 10
 * - 3 printers: threshold = 15
 * - 4 printers: threshold = 20
 * - 5 printers: no scaling (max capacity)
 * 
 * @param active_printers Current number of active printers.
 * @return Queue length threshold for scaling up.
 */
int get_scale_up_threshold(int active_printers);

/**
 * @brief Check if conditions are met to scale up.
 * @param pool Pointer to printer pool.
 * @param queue_length Current job queue length.
 * @param current_time_us Current time in microseconds.
 * @return 1 if should scale up, 0 otherwise.
 */
int should_scale_up(struct printer_pool* pool, int queue_length, unsigned long current_time_us);

/**
 * @brief Check if conditions are met to scale down.
 * Conditions:
 * - Current printers > minimum configured
 * - Queue length < 5 for sustained period (5 seconds)
 * - At least one printer has been idle for 5+ seconds
 * 
 * @param pool Pointer to printer pool.
 * @param queue_length Current job queue length.
 * @param current_time_us Current time in microseconds.
 * @return 1 if should scale down, 0 otherwise.
 */
int should_scale_down(struct printer_pool* pool, int queue_length, unsigned long current_time_us);

/**
 * @brief Scale up by adding one printer to the pool.
 * @param args Autoscaling thread arguments containing all shared resources.
 * @return 1 on success, 0 on failure.
 */
int scale_up(autoscaling_thread_args_t* args);

/**
 * @brief Scale down by removing the most recently added printer.
 * @param args Autoscaling thread arguments containing all shared resources.
 * @return 1 on success, 0 on failure.
 */
int scale_down(autoscaling_thread_args_t* args);

/**
 * @brief Main autoscaling monitoring thread function.
 * Periodically checks queue length and scaling conditions.
 * 
 * @param arg Pointer to autoscaling_thread_args_t.
 * @return NULL
 */
void* autoscaling_thread_func(void* arg);

#endif // AUTOSCALING_H
