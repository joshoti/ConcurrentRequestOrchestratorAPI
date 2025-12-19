#include <stdio.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

#include "simulation_stats.h"

// --- Private Helper Functions ---
/**
 * @brief Calculates the average inter-arrival time in seconds.
 * @param stats Pointer to simulation_statistics_t struct.
 * @return Average inter-arrival time in seconds.
 */
static double calculate_average_inter_arrival_time(simulation_statistics_t* stats) {
    if (stats->total_jobs_arrived <= 1) {
        return 0.0;
    }
    return ((double)stats->total_inter_arrival_time_us / 1000000.0) / (stats->total_jobs_arrived - 1);
}

/**
 * @brief Calculates the average system time in seconds.
 * @param stats Pointer to simulation_statistics_t struct.
 * @return Average system time in seconds.
 */
static double calculate_average_system_time(simulation_statistics_t* stats) {
    if (stats->total_jobs_served == 0) {
        return 0.0;
    }
    return ((double)stats->total_system_time_us / 1000000.0) / stats->total_jobs_served;
}

/**
 * @brief Calculates the average queue wait time in seconds.
 * @param stats Pointer to simulation_statistics_t struct.
 * @return Average queue wait time in seconds.
 */
static double calculate_average_queue_wait_time(simulation_statistics_t* stats) {
    if (stats->total_jobs_served == 0) {
        return 0.0;
    }
    return ((double)stats->total_queue_wait_time_us / 1000000.0) / stats->total_jobs_served;
}

/**
 * @brief Calculates the average service time for a specific printer in seconds.
 * @param stats Pointer to simulation_statistics_t struct.
 * @param printer_index Zero-based printer index (0 - MAX_PRINTERS-1 for printers 1 - MAX_PRINTERS).
 * @return Average service time in seconds.
 */
static double calculate_average_service_time(simulation_statistics_t* stats, int printer_index) {
    if (printer_index < 0 || printer_index >= MAX_PRINTERS) {
        return 0.0;
    }
    double jobs_served = stats->jobs_served_by_printer[printer_index];
    if (jobs_served == 0) {
        return 0.0;
    }
    return ((double)stats->total_service_time_printer_us[printer_index] / 1000000.0) / jobs_served;
}

/**
 * @brief Calculates the average number of jobs in the queue.
 * @param stats Pointer to simulation_statistics_t struct.
 * @return Average number of jobs in the queue.
 */
static double calculate_average_queue_length(simulation_statistics_t* stats) {
    if (stats->simulation_duration_us == 0) {
        return 0.0;
    }
    return ((double)stats->area_num_in_job_queue_us) / stats->simulation_duration_us;
}

/**
 * @brief Calculates the standard deviation of system time in seconds.
 * @param stats Pointer to simulation_statistics_t struct.
 * @return Standard deviation of system time in seconds.
 */
static double calculate_system_time_std_dev(simulation_statistics_t* stats) {
    if (stats->total_jobs_served <= 1) {
        return 0.0;
    }
    double avg_system_time = calculate_average_system_time(stats);
    double avg_system_time_us = avg_system_time * 1000000.0;

    double avg_time_sq = stats->sum_of_system_time_squared_us2 / stats->total_jobs_served;

    double variance = avg_time_sq - (avg_system_time_us * avg_system_time_us);
    return sqrt(variance > 0 ? variance : 0) / 1000000.0; // Convert back to seconds for display consistency
}

/**
 * @brief Calculates the system utilization for a specific printer.
 * @param stats Pointer to simulation_statistics_t struct.
 * @param printer_index Zero-based printer index (0 - MAX_PRINTERS-1 for printers 1 - MAX_PRINTERS).
 * @return Utilization (a value between 0 and 1).
 */
static double calculate_system_utilization(simulation_statistics_t* stats, int printer_index) {
    if (printer_index < 0 || printer_index >= MAX_PRINTERS) {
        return 0.0;
    }
    if (stats->simulation_duration_us == 0) {
        return 0.0;
    }
    return ((double)stats->total_service_time_printer_us[printer_index]) / stats->simulation_duration_us;
}

/**
 * @brief Calculates the job arrival rate (jobs per second).
 * @param stats Pointer to simulation_statistics_t struct.
 * @return Job arrival rate in jobs per second.
 */
static double calculate_job_arrival_rate(simulation_statistics_t* stats) {
    if (stats->simulation_duration_us == 0) {
        return 0.0;
    }
    // Convert simulation duration from microseconds to seconds (multiply by 1.0e-6)
    double simulation_duration_sec = stats->simulation_duration_us * 1.0e-6;
    return stats->total_jobs_arrived / simulation_duration_sec;
}

/**
 * @brief Calculates the job drop probability.
 * @param stats Pointer to simulation_statistics_t struct.
 * @return Job drop probability (a value between 0 and 1).
 */
static double calculate_job_drop_probability(simulation_statistics_t* stats) {
    if (stats->total_jobs_arrived == 0) {
        return 0.0;
    }
    return stats->total_jobs_dropped / stats->total_jobs_arrived;
}

// --- Public API Function Implementations ---
int write_statistics_to_buffer(simulation_statistics_t* stats, char* buf, int buf_size) {
    if (stats == NULL || buf == NULL || buf_size <= 0) return -1;

    // Calculate derived statistics
    double avg_inter_arrival_time = calculate_average_inter_arrival_time(stats);
    double avg_system_time = calculate_average_system_time(stats);
    double avg_queue_wait_time = calculate_average_queue_wait_time(stats);
    double avg_queue_length = calculate_average_queue_length(stats);
    double system_time_std_dev = calculate_system_time_std_dev(stats);
    double job_arrival_rate = calculate_job_arrival_rate(stats);
    double job_drop_probability = calculate_job_drop_probability(stats);
    double simulation_time_sec = stats->simulation_duration_us / 1000000.0;
    
    // Start building JSON
    int offset = snprintf(buf, buf_size,
        "{\"type\":\"statistics\", \"data\":{"
        "\"simulation_duration_sec\":%.3g,"
        "\"total_jobs_arrived\":%.0f,"
        "\"total_jobs_served\":%.0f,"
        "\"total_jobs_dropped\":%.0f,"
        "\"total_jobs_removed\":%.0f,"
        "\"job_arrival_rate_per_sec\":%.3g,"
        "\"job_drop_probability\":%.3g,"
        "\"avg_inter_arrival_time_sec\":%.3g,"
        "\"avg_system_time_sec\":%.3g,"
        "\"system_time_std_dev_sec\":%.3g,"
        "\"avg_queue_wait_time_sec\":%.3g,"
        "\"avg_queue_length\":%.3g,"
        "\"max_queue_length\":%u,",
        simulation_time_sec,
        stats->total_jobs_arrived,
        stats->total_jobs_served,
        stats->total_jobs_dropped,
        stats->total_jobs_removed,
        job_arrival_rate,
        job_drop_probability,
        avg_inter_arrival_time,
        avg_system_time,
        system_time_std_dev,
        avg_queue_wait_time,
        avg_queue_length,
        stats->max_job_queue_length
    );
    
    // Add dynamic printer statistics array
    offset += snprintf(buf + offset, buf_size - offset, "\"printers\":[");
    
    int printers_to_report = stats->max_printers_used > 0 ? stats->max_printers_used : 2;
    for (int i = 0; i < printers_to_report; i++) {
        double avg_service_time = calculate_average_service_time(stats, i);
        double utilization = calculate_system_utilization(stats, i);
        
        offset += snprintf(buf + offset, buf_size - offset,
            "{\"id\":%d,\"jobs_served\":%.0f,\"paper_used\":%d,"
            "\"avg_service_time_sec\":%.3g,\"utilization\":%.3g}%s",
            i + 1,
            stats->jobs_served_by_printer[i],
            stats->printer_paper_used[i],
            avg_service_time,
            utilization,
            (i < printers_to_report - 1) ? "," : ""
        );
    }
    
    // Close printers array and add paper refill stats
    offset += snprintf(buf + offset, buf_size - offset,
        "],\"paper_refill_events\":%.0f,"
        "\"total_refill_service_time_sec\":%.3g,"
        "\"papers_refilled\":%d}}",
        stats->paper_refill_events,
        stats->total_refill_service_time_us / 1000000.0,
        stats->papers_refilled
    );

    return offset;
}

void log_statistics(simulation_statistics_t* stats) {
    if (stats == NULL) return;
    
    // Calculate derived statistics (same calculations as publish_statistics)
    double avg_inter_arrival_time = calculate_average_inter_arrival_time(stats);
    double avg_system_time = calculate_average_system_time(stats);
    double avg_queue_wait_time = calculate_average_queue_wait_time(stats);
    double avg_queue_length = calculate_average_queue_length(stats);
    double system_time_std_dev = calculate_system_time_std_dev(stats);
    double job_arrival_rate = calculate_job_arrival_rate(stats);
    double job_drop_probability = calculate_job_drop_probability(stats);
    double simulation_time_sec = stats->simulation_duration_us / 1000000.0;
    
    // Print formatted statistics to stdout
    flockfile(stdout);
    
    printf("\n");
    printf("================= SIMULATION STATISTICS =================\n");
    printf("Simulation Duration:               %.3g sec\n", simulation_time_sec);
    printf("\n");
    printf("--- Job Flow Statistics ---\n");
    printf("Total Jobs Arrived:                %.0f\n", stats->total_jobs_arrived);
    printf("Total Jobs Served:                 %.0f\n", stats->total_jobs_served);
    printf("Total Jobs Dropped:                %.0f\n", stats->total_jobs_dropped);
    printf("Total Jobs Removed:                %.0f\n", stats->total_jobs_removed);
    printf("Job Arrival Rate (λ):              %.3g jobs/sec\n", job_arrival_rate);
    printf("Job Drop Probability:              %.3g (%.2f%%)\n", job_drop_probability, job_drop_probability * 100);
    printf("\n");
    printf("--- Timing Statistics ---\n");
    printf("Average Inter-arrival Time:        %.3g sec\n", avg_inter_arrival_time);
    printf("Average System Time:               %.3g sec\n", avg_system_time);
    printf("System Time Standard Deviation:    %.3g sec\n", system_time_std_dev);
    printf("Average Queue Wait Time:           %.3g sec\n", avg_queue_wait_time);
    printf("\n");
    printf("--- Queue Statistics ---\n");
    printf("Average Queue Length:              %.3g jobs\n", avg_queue_length);
    printf("Maximum Queue Length:              %u jobs\n", stats->max_job_queue_length);
    printf("\n");
    printf("--- Printer Statistics ---\n");
    
    // Dynamically report on printers that were actually used
    int printers_to_report = stats->max_printers_used > 0 ? stats->max_printers_used : 2;
    for (int i = 0; i < printers_to_report; i++) {
        int printer_id = i + 1;
        double jobs_served = stats->jobs_served_by_printer[i];
        int paper_used = stats->printer_paper_used[i];
        double avg_service_time = calculate_average_service_time(stats, i);
        double utilization = calculate_system_utilization(stats, i);
        
        printf("Jobs Served by Printer %d:          %.0f\n", printer_id, jobs_served);
        printf("Total Paper Used by Printer %d:     %d\n", printer_id, paper_used);
        printf("Avg Service Time (Printer %d):      %.3g sec\n", printer_id, avg_service_time);
        printf("Utilization (Printer %d):           %.3g%%\n", printer_id, utilization * 100);
        if (i < printers_to_report - 1) printf("\n");
    }
    
    printf("\n");
    printf("--- Paper Management ---\n");
    printf("Paper Refill Events:               %.0f\n", stats->paper_refill_events);
    printf("Total Refill Service Time:         %.3g sec\n", stats->total_refill_service_time_us / 1000000.0);
    printf("Papers Refilled:                   %d\n", stats->papers_refilled);
    printf("=========================================================\n");
    
    funlockfile(stdout);
}

void debug_statistics(const simulation_statistics_t* stats) {
    if (stats == NULL) {
        printf("Statistics struct is NULL\n");
        return;
    }
    
    flockfile(stdout);
    printf("\n=== RAW STATISTICS DEBUG ===\n");
    printf("simulation_start_time_us: %lu\n", stats->simulation_start_time_us);
    printf("simulation_duration_us: %lu\n", stats->simulation_duration_us);
    printf("total_jobs_arrived: %.0f\n", stats->total_jobs_arrived);
    printf("total_jobs_served: %.0f\n", stats->total_jobs_served);
    printf("total_jobs_dropped: %.0f\n", stats->total_jobs_dropped);
    printf("total_jobs_removed: %.0f\n", stats->total_jobs_removed);
    printf("total_inter_arrival_time_us: %lu\n", stats->total_inter_arrival_time_us);
    printf("total_system_time_us: %lu\n", stats->total_system_time_us);
    printf("sum_of_system_time_squared_us2: %.0f\n", stats->sum_of_system_time_squared_us2);
    printf("total_queue_wait_time_us: %lu\n", stats->total_queue_wait_time_us);
    printf("area_num_in_job_queue_us: %lu\n", stats->area_num_in_job_queue_us);
    printf("max_job_queue_length: %u\n", stats->max_job_queue_length);
    printf("jobs_served_by_printer1: %.0f\n", stats->jobs_served_by_printer1);
    printf("total_service_time_p1_us: %lu\n", stats->total_service_time_p1_us);
    printf("printer1_paper_empty_time_us: %lu\n", stats->printer1_paper_empty_time_us);
    printf("jobs_served_by_printer2: %.0f\n", stats->jobs_served_by_printer2);
    printf("total_service_time_p2_us: %lu\n", stats->total_service_time_p2_us);
    printf("printer2_paper_empty_time_us: %lu\n", stats->printer2_paper_empty_time_us);
    printf("paper_refill_events: %.0f\n", stats->paper_refill_events);
    printf("total_refill_service_time_us: %lu\n", stats->total_refill_service_time_us);
    printf("papers_refilled: %d\n", stats->papers_refilled);
    printf("==============================\n");
    funlockfile(stdout);
}