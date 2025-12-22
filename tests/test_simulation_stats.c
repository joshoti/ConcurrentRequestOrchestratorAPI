#include <stdio.h>
#include <string.h>

#include "test_utils.h"
#include "simulation_stats.h"

int test_create_simulation_stats(simulation_statistics_t* stats) {
    *stats = (simulation_statistics_t){0};

    // Simulate some statistics
    stats->simulation_start_time_us = 0;
    stats->simulation_duration_us = 1000000; // 1 second
    stats->total_jobs_arrived = 10;
    stats->total_jobs_served = 8;
    stats->total_jobs_dropped = 1;
    stats->total_jobs_removed = 1;
    stats->total_inter_arrival_time_us = 900000; // total time between arrivals
    stats->total_system_time_us = 800000; // total time in system for served jobs
    stats->sum_of_system_time_squared_us2 = 640000000000; // sum of squares
    stats->total_queue_wait_time_us = 400000; // total wait time in queue
    stats->area_num_in_job_queue_us = 2000000; // integral of queue length over time
    stats->max_job_queue_length = 5;
    // total jobs served by printer [1-5]
    // total service time for printer [1-5]
    // paper empty time for printer [1-5]
    stats->paper_refill_events = 2;
    stats->total_refill_service_time_us = 20000; // total refill service time
    stats->papers_refilled = 15;
    
    // Test debugging statistics (output raw values)
    debug_statistics(stats);

    return 0;
}

int test_write_statistics_to_buffer(simulation_statistics_t* stats) {
    int failed = 0;

    char buffer[1024];
    int result;
    memset(buffer, 0, sizeof(buffer));

    // Test writing statistics to buffer
    result = write_statistics_to_buffer(stats, buffer, sizeof(buffer));
    if (result < 0) {
        printf("Error writing statistics to buffer\n");
        failed = 1;
    } else {
        printf("Successfully wrote %d bytes to buffer\n", result);
        printf("Statistics JSON:\n%s\n", buffer);
    }

    return failed;
}

int test_log_statistics(simulation_statistics_t* stats) {
    // Test logging statistics (output to stdout)
    log_statistics(stats);
    return 0;
}

int main() {
    char test_name[] = "SIMULATION STATS";
    print_test_start(test_name);
    int failed_tests = 0;
    
    simulation_statistics_t stats;
    failed_tests += test_create_simulation_stats(&stats);
    failed_tests += test_write_statistics_to_buffer(&stats);
    failed_tests += test_log_statistics(&stats);

    print_test_end(test_name, failed_tests);
    return 0;
}