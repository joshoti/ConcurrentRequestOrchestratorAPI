#ifndef PRINTER_H
#define PRINTER_H

#include <pthread.h>
#include "config.h"

struct linked_list;
struct timed_queue;
struct simulation_parameters;
struct simulation_statistics;

// --- Printer structure ---
typedef struct printer {
    int id; // Unique identifier for the printer
    int current_paper_count; // Current number of papers in the printer
    int total_papers_used; // Total number of papers used by this printer
    int capacity; // Maximum paper capacity of the printer
    int jobs_printed_count; // Total number of jobs printed by this printer
    unsigned long last_job_completion_time_us; // Last time this printer completed a job (for idle tracking)
    int is_idle; // 1 if idle, 0 if serving
} printer_t;

// --- Utility functions ---
/**
 * @brief Prints printer details for debugging purposes.
 * @param printer Pointer to the Printer struct.
 */
void debug_printer(const printer_t* printer);

// --- Printer Thread Arguments ---
/**
 * @brief Arguments for the printer thread.
 */
typedef struct printer_thread_args {
    pthread_mutex_t* paper_refill_queue_mutex;
    pthread_mutex_t* job_queue_mutex;
    pthread_mutex_t* stats_mutex;
    pthread_mutex_t* simulation_state_mutex; // protects g_terminate_now
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
    printer_t* printer;
} printer_thread_args_t;

// --- Printer Instance (for array management) ---
typedef struct printer_instance {
    pthread_t thread;
    printer_t printer;
    printer_thread_args_t args;
    int active; // 1 if thread is running, 0 if not yet spawned
} printer_instance_t;

// --- Printer Pool (manages all printers) ---
typedef struct printer_pool {
    printer_instance_t printers[CONFIG_RANGE_CONSUMER_COUNT_MAX]; // Array sized by config constant
    int active_count; // Number of currently active printers
    int min_count; // Minimum printers (from config consumer_count)
    pthread_mutex_t pool_mutex; // Protects the pool during scaling operations
    unsigned long last_scale_time_us; // Last time we scaled up or down (for cooldown)
    unsigned long low_queue_start_time_us; // When queue first went below scale-down threshold
} printer_pool_t;

// --- Thread function ---
/**
 * @brief The main function for the printer thread.
 *
 * @param arg Pointer to the PrinterThreadArgs struct.
 * @return NULL
 */
void* printer_thread_func(void* arg);

// --- Printer Pool Management ---
/**
 * @brief Initialize the printer pool with base configuration.
 * @param pool Pointer to the printer pool.
 * @param min_printers Minimum number of printers to maintain.
 * @param paper_capacity Initial paper capacity for each printer.
 */
void printer_pool_init(printer_pool_t* pool, int min_printers, int paper_capacity);

/**
 * @brief Start a new printer in the pool.
 * @param pool Pointer to the printer pool.
 * @param printer_id The ID for the new printer.
 * @param shared_args Template args to copy from (contains all mutexes, queues, etc).
 * @return 1 on success, 0 on failure.
 */
int printer_pool_start_printer(printer_pool_t* pool, int printer_id, const printer_thread_args_t* shared_args);

/**
 * @brief Join all active printer threads.
 * @param pool Pointer to the printer pool.
 */
void printer_pool_join_all(printer_pool_t* pool);

/**
 * @brief Cleanup the printer pool.
 * @param pool Pointer to the printer pool.
 */
void printer_pool_destroy(printer_pool_t* pool);

#endif // PRINTER_H