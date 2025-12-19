#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "common.h"
#include "config.h"
#include "preprocessing.h"

int g_debug = 0;
int g_terminate_now = 0;

void usage() {
    fprintf(stderr, "usage: ./bin/cli [-debug] [-help] [-num num_jobs] [-q queue_capacity]\n");
    fprintf(stderr, "                 [-p_cap printer_paper_capacity]\n");
    fprintf(stderr, "                 [-s service_rate] [-ref refill_rate]\n");
    fprintf(stderr, "                 [-papers_lower papers_required_lower_bound]\n");
    fprintf(stderr, "                 [-papers_upper papers_required_upper_bound]\n");
    fprintf(stderr, "                 [-consumers consumer_count] [-auto_scale 0|1]\n");
    fprintf(stderr, "                 [-fixed_arrival 0|1] [-job_arr_time job_arrival_time_ms]\n");
    fprintf(stderr, "                 [-min_arr min_arrival_time] [-max_arr max_arrival_time]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Notes:\n");
    fprintf(stderr, "  - If fixed_arrival is 1, job_arr_time (ms) determines inter-arrival time\n");
    fprintf(stderr, "  - If fixed_arrival is 0, inter-arrival time is random between min_arr and max_arr\n");
}

int random_between(int lower, int upper) {
    return (rand() % (upper - lower + 1)) + lower;
}

void swap_bounds(int* lower, int* upper) {
    int temp = *lower;
    *lower = (int)fmin(*upper, *lower);
    *upper = (int)fmax(temp, *upper);
}

int is_positive_double(const char* str, double value) {
    if (value <= 0) {
        fprintf(stderr, "Error: %s must be a positive number.\n", str);
        return FALSE;
    }
    return TRUE;
}

int is_positive_integer(const char* str, int value) {
    if (value <= 0) {
        fprintf(stderr, "Error: %s must be a positive integer.\n", str);
        return FALSE;
    }
    return TRUE;
}

int is_in_range_double(const char* str, double value, double min, double max) {
    if (value < min || value > max) {
        fprintf(stderr, "Error: %s must be between %.2f and %.2f.\n", str, min, max);
        return FALSE;
    }
    return TRUE;
}

int is_in_range_int(const char* str, int value, int min, int max) {
    if (value < min || value > max) {
        fprintf(stderr, "Error: %s must be between %d and %d.\n", str, min, max);
        return FALSE;
    }
    return TRUE;
}

int process_args(int argc, char *argv[], simulation_parameters_t* params) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-help") == 0) {
            usage();
            exit(0); // successful early exit for help
        }

        // Number of jobs to simulate
        if (strcmp(argv[i], "-num") == 0) {
            params->num_jobs = atoi(argv[++i]);
            if (!is_positive_integer("num_jobs", params->num_jobs)) return FALSE;
        }
        // Queue capacity
        else if (strcmp(argv[i], "-q") == 0) {
            params->queue_capacity = atoi(argv[++i]);
            // -1 means unlimited, otherwise must be positive
            if (params->queue_capacity != -1 
                && !is_positive_integer("queue_capacity", params->queue_capacity)) return FALSE;
        }
        // Min papers required
        else if (strcmp(argv[i], "-papers_lower") == 0) {
            params->papers_required_lower_bound = atoi(argv[++i]);
            if (!is_in_range_int(
                "papers_required_lower_bound",
                params->papers_required_lower_bound,
                CONFIG_RANGE_MIN_PAPERS_MIN,
                CONFIG_RANGE_MIN_PAPERS_MAX)
            ) return FALSE;
        }
        // Max papers required
        else if (strcmp(argv[i], "-papers_upper") == 0) {
            params->papers_required_upper_bound = atoi(argv[++i]);
            if (!is_in_range_int(
                "papers_required_upper_bound",
                params->papers_required_upper_bound,
                CONFIG_RANGE_MAX_PAPERS_MIN,
                CONFIG_RANGE_MAX_PAPERS_MAX)
            ) return FALSE;
        }
        // Printer paper capacity
        else if (strcmp(argv[i], "-p_cap") == 0) {
            params->printer_paper_capacity = atoi(argv[++i]);
            if (!is_in_range_int(
                "printer_paper_capacity",
                params->printer_paper_capacity,
                CONFIG_RANGE_PAPER_CAPACITY_MIN,
                CONFIG_RANGE_PAPER_CAPACITY_MAX)
            ) return FALSE;
        }
        // Service rate
        else if (strcmp(argv[i], "-s") == 0) {
            double service_rate = atof(argv[++i]);
            if (!is_in_range_double(
                "service_rate",
                service_rate,
                CONFIG_RANGE_PRINT_RATE_MIN,
                CONFIG_RANGE_PRINT_RATE_MAX)
            ) return FALSE;
            params->printing_rate = service_rate;
        }
        // Refill rate
        else if (strcmp(argv[i], "-ref") == 0) {
            params->refill_rate = atof(argv[++i]);
            if (!is_in_range_double(
                "refill_rate",
                params->refill_rate,
                CONFIG_RANGE_REFILL_RATE_MIN,
                CONFIG_RANGE_REFILL_RATE_MAX)
            ) return FALSE;
        }
        // Consumer count
        else if (strcmp(argv[i], "-consumers") == 0) {
            params->consumer_count = atoi(argv[++i]);
            if (!is_in_range_int(
                "consumer_count",
                params->consumer_count,
                CONFIG_RANGE_CONSUMER_COUNT_MIN,
                CONFIG_RANGE_CONSUMER_COUNT_MAX)
            ) return FALSE;
        }
        // Auto-scaling
        else if (strcmp(argv[i], "-auto_scale") == 0) {
            params->auto_scaling = atoi(argv[++i]);
            if (params->auto_scaling != 0 && params->auto_scaling != 1) {
                fprintf(stderr, "Error: auto_scaling must be 0 or 1.\n");
                return FALSE;
            }
        }
        // Fixed arrival time
        else if (strcmp(argv[i], "-fixed_arrival") == 0) {
            params->fixed_arrival = atoi(argv[++i]);
            if (params->fixed_arrival != 0 && params->fixed_arrival != 1) {
                fprintf(stderr, "Error: fixed_arrival must be 0 or 1.\n");
                return FALSE;
            }
        }
        // Job arrival time (ms)
        else if (strcmp(argv[i], "-job_arr_time") == 0) {
            int job_arrival_time_ms = atoi(argv[++i]);
            if (!is_in_range_int("job_arrival_time",
                job_arrival_time_ms,
                CONFIG_RANGE_JOB_ARRIVAL_TIME_MIN,
                CONFIG_RANGE_JOB_ARRIVAL_TIME_MAX)
            ) return FALSE;
            // Convert milliseconds to microseconds
            params->job_arrival_time_us = job_arrival_time_ms * 1000;
        }
        // Min arrival time (ms)
        else if (strcmp(argv[i], "-min_arr") == 0) {
            params->min_arrival_time = atoi(argv[++i]);
            if (!is_in_range_int("min_arrival_time",
                params->min_arrival_time,
                CONFIG_RANGE_MIN_ARRIVAL_TIME_MIN,
                CONFIG_RANGE_MIN_ARRIVAL_TIME_MAX)
            ) return FALSE;
        }
        // Max arrival time (ms)
        else if (strcmp(argv[i], "-max_arr") == 0) {
            params->max_arrival_time = atoi(argv[++i]);
            if (!is_in_range_int("max_arrival_time",
                params->max_arrival_time,
                CONFIG_RANGE_MAX_ARRIVAL_TIME_MIN,
                CONFIG_RANGE_MAX_ARRIVAL_TIME_MAX)
            ) return FALSE;
        }
        // Debug mode
        else if (strcmp(argv[i], "-debug") == 0) {
            g_debug = 1;
        }
        // Unrecognized argument
        else {
            fprintf(stderr, "Error: unrecognized argument %s.\n", argv[i]);
            usage();
            return FALSE;
        }
        swap_bounds(&params->papers_required_lower_bound, &params->papers_required_upper_bound);
    }
    return TRUE;
}