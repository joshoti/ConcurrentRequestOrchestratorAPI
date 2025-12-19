#ifndef SIMULATION_STATS_H
#define SIMULATION_STATS_H

#include "config.h"

#define MAX_PRINTERS CONFIG_RANGE_CONSUMER_COUNT_MAX

typedef struct simulation_statistics {
    // --- General Simulation Metrics ---
    unsigned long simulation_start_time_us;     // Start time of the simulation
    unsigned long simulation_duration_us;       // Total simulation time in microseconds

    // --- Job Arrival & Flow Metrics ---
    double total_jobs_arrived;                  // Count of all jobs that entered the system
    double total_jobs_served;                   // Count of jobs that successfully printed
    double total_jobs_dropped;                  // Count of jobs dropped (e.g., queue full)
    double total_jobs_removed;                  // Count of jobs removed due to premature termination
    unsigned long total_inter_arrival_time_us;  // Sum of time between arrivals for calculating the average

    // --- System & Queue Performance Metrics ---
    unsigned long total_system_time_us;         // Sum of time each SERVED job spent in the system (wait + service)
    double sum_of_system_time_squared_us2;      // Sum of (system_time)^2 for calculating standard deviation
    unsigned long total_queue_wait_time_us;     // Sum of time each SERVED job spent waiting in the queue
    unsigned long area_num_in_job_queue_us;     // Integral of queue length over time, for avg queue length
    unsigned int max_job_queue_length;          // Peak number of jobs ever in the queue

    // --- Printer 1 (S1) Metrics (kept for backwards compatibility) ---
    double jobs_served_by_printer1;             // Total jobs completed by printer 1
    int printer1_paper_used;                     // Total paper used by printer 1
    unsigned long total_service_time_p1_us;     // Sum of service times for jobs on printer 1
    unsigned long printer1_paper_empty_time_us; // Total time printer 1 was idle due to no paper

    // --- Printer 2 (S2) Metrics (kept for backwards compatibility) ---
    double jobs_served_by_printer2;             // Total jobs completed by printer 2
    int printer2_paper_used;                     // Total paper used by printer 2
    unsigned long total_service_time_p2_us;     // Sum of service times for jobs on printer 2
    unsigned long printer2_paper_empty_time_us; // Total time printer 2 was idle due to no paper

    // --- Per-Printer Metrics (arrays for all 5 printers) ---
    double jobs_served_by_printer[MAX_PRINTERS];        // Jobs completed by each printer [0-4]
    int printer_paper_used[MAX_PRINTERS];                // Paper used by each printer [0-4]
    unsigned long total_service_time_printer_us[MAX_PRINTERS];  // Service times by each printer [0-4]
    unsigned long printer_paper_empty_time_us[MAX_PRINTERS];    // Idle time due to no paper [0-4]
    int max_printers_used;                               // Track how many printers were actually used (1-5)

    // --- Paper Refill Metrics ---
    double paper_refill_events;                 // Number of times the paper was refilled
    unsigned long total_refill_service_time_us; // Total time spent actively refilling paper
    int papers_refilled;                        // Total number of papers refilled during the simulation

} simulation_statistics_t;

/**
 * @brief Calculates all relevant simulation statistics and formats them as a JSON string to the provided buffer.
 *
 * @param stats A simulation statistics struct.
 * @param buf A character buffer to hold the JSON statistics message.
 * @param buf_size The size of the provided buffer.
 * @return The number of bytes written to the buffer, or -1 on error.
 */
int write_statistics_to_buffer(simulation_statistics_t* stats, char* buf, int buf_size);

/**
 * @brief Calculates and logs all relevant simulation statistics to stdout.
 *
 * @param stats A simulation statistics struct.
 */
void log_statistics(simulation_statistics_t* stats);

/**
 * @brief Prints all raw statistics for debugging purposes.
 *
 * @param stats A simulation statistics struct.
 */
void debug_statistics(const simulation_statistics_t* stats);

#endif // SIMULATION_STATS_H