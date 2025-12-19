#ifndef PREPROCESSING_H
#define PREPROCESSING_H

/**
 * @file preprocessing.h
 * @brief Header file for preprocessing.c, containing function declarations
 *        and shared variables for command line argument processing and thread management.
 */

typedef struct simulation_parameters {
    double job_arrival_time_us;
    int papers_required_lower_bound;
    int papers_required_upper_bound;
    int queue_capacity;
    double printing_rate;
    int printer_paper_capacity;
    double refill_rate;
    int num_jobs;
    int consumer_count;
    int auto_scaling;
    int fixed_arrival;
    int min_arrival_time;
    int max_arrival_time;
} simulation_parameters_t;

/**
 * job_arrival_time_us: 500,000 us = 1 job every 0.5 sec (jobArrivalTime: 500ms)
 * papers_required_lower_bound: 5 pages (minPapers)
 * papers_required_upper_bound: 15 pages (maxPapers)
 * queue_capacity: -1 (unlimited, maxQueue)
 * printing_rate: 5 papers/sec (printRate)
 * printer_paper_capacity: 150 pages (paperCapacity)
 * refill_rate: 25 papers/sec (refillRate)
 * num_jobs: 10 jobs (jobCount)
 * consumer_count: 2 printers (consumerCount)
 * auto_scaling: 0 (false)
 * fixed_arrival: 1 (true, fixedArrival)
 * min_arrival_time: 300 ms (minArrivalTime)
 * max_arrival_time: 600 ms (maxArrivalTime)
 */
#define SIMULATION_DEFAULT_PARAMS {500000, 5, 15, -1, 5, 150, 25, 10, 2, 0, 1, 300, 600}
#define SIMULATION_DEFAULT_PARAMS_HIGH_LOAD {200000, 10, 30, -1, 5, 90, 25, 20, 2, 1, 1, 300, 600}

/**
 * @brief Print usage information for the program.
 */
void usage();

/**
 * @brief Generate a random integer between lower and upper (inclusive).
 * @param lower The lower bound inclusive.
 * @param upper The upper bound inclusive.
 * @return A random integer between lower and upper.
 */
int random_between(int lower, int upper);

/**
 * @brief Swap the values of lower and upper bounds if lower is greater than upper.
 * @param lower Pointer to the lower bound.
 * @param upper Pointer to the upper bound.
 */
void swap_bounds(int* lower, int* upper);

/**
 * @brief Check if a double value is positive.
 * @param str The name of the parameter being checked (for error messages).
 * @param value The double value to check.
 * @return 1 if the value is positive, 0 otherwise.
 */
int is_positive_double(const char* str, double value);
/**
 * @brief Check if an integer value is positive.
 * @param str The name of the parameter being checked (for error messages).
 * @param value The integer value to check.
 * @return 1 if the value is positive, 0 otherwise.
 */
int is_positive_integer(const char* str, int value);

/**
 * @brief Check if a double value is within a specified range.
 * @param str The name of the parameter being checked (for error messages).
 * @param value The double value to check.
 * @param min The minimum allowed value.
 * @param max The maximum allowed value.
 * @return 1 if the value is within range, 0 otherwise.
 */
int is_in_range_double(const char* str, double value, double min, double max);

/**
 * @brief Check if an integer value is within a specified range.
 * @param str The name of the parameter being checked (for error messages).
 * @param value The integer value to check.
 * @param min The minimum allowed value.
 * @param max The maximum allowed value.
 * @return 1 if the value is within range, 0 otherwise.
 */
int is_in_range_int(const char* str, int value, int min, int max);

/**
 * @brief Process command line arguments
 * @param argc Argument count
 * @param argv Argument vector
 * @param params Pointer to SimulationParameters struct to populate
 * @return 0 on failure, 1 on success
 */
int process_args(int argc, char *argv[], simulation_parameters_t* params);

#endif // PREPROCESSING_H