#include <stdio.h>
#include <string.h>

#include "common.h"
#include "preprocessing.h"
#include "test_utils.h"

int test_process_args() {
    int failed = 0;
    char *argv[] = {
        "program_name",
        "-num", "5",
        "-q", "10",
        "-p_cap", "100",
        "-job_arr_time", "200",
        "-s", "5",
        "-ref", "15",
        "-papers_lower", "10",
        "-papers_upper", "30"
    };
    int argc = sizeof(argv) / sizeof(argv[0]);
    simulation_parameters_t params;

    if (process_args(argc, argv, &params)) {
        printf("Test passed: num_jobs=%d, queue_capacity=%d,"
               " printer_paper_capacity=%d, arrival_time=%gus,"
               " service_rate=%gpapers/sec, refill_rate=%gpapers/sec,"
               " papers_required_lower_bound=%d, papers_required_upper_bound=%d\n",
               params.num_jobs,
               params.queue_capacity,
               params.printer_paper_capacity,
               params.job_arrival_time_us,
               params.printing_rate,
               params.refill_rate,
               params.papers_required_lower_bound,
               params.papers_required_upper_bound);
    } else {
        printf("Test failed\n");
        failed = 1;
    }
    return failed;
}

int test_bad_args() {
    int failed = 0;
    char *argv[] = {
        "program_name",
        "-num", "-5",  // Invalid negative number
        "-q", "10",
        "-p_cap", "100",
        "-job_arr_time", "200",
        "-s", "5",
        "-ref", "30"
    };
    int argc = sizeof(argv) / sizeof(argv[0]);
    simulation_parameters_t params;

    if (!process_args(argc, argv, &params)) {
        printf("Test passed: Detected invalid argument\n");
    } else {
        printf("Test failed: Did not detect invalid argument\n");
        failed = 1;
    }
    return failed;
}

int test_random_between() {
    int failed = 0;
    int lower = 10;
    int upper = 20;
    for (int i = 0; i < 100; i++) {
        int rand_val = random_between(lower, upper);
        if (rand_val < lower || rand_val > upper) {
            printf("Test failed: random_between(%d, %d) returned %d\n", lower, upper, rand_val);
            failed = 1;
            break;
        }
    }
    if (!failed) {
        printf("Test passed: random_between(%d, %d) returned values within range\n", lower, upper);
    }
    return failed;
}

int test_swap_bounds() {
    int failed = 0;
    int lower = 30;
    int upper = 20;
    swap_bounds(&lower, &upper);
    if (lower != 20 || upper != 30) {
        printf("Test failed: swap_bounds did not swap correctly. Got lower=%d, upper=%d\n", lower, upper);
        failed = 1;
    } else {
        printf("Test passed: swap_bounds swapped correctly to lower=%d, upper=%d\n", lower, upper);
    }
    return failed;
}

int test_swap_bounds_with_correct_values() {
    int failed = 0;
    int lower = 20;
    int upper = 30;
    swap_bounds(&lower, &upper);
    if (lower != 20 || upper != 30) {
        printf("Test failed: swap_bounds_with_correct_values did not swap correctly. Got lower=%d, upper=%d\n", lower, upper);
        failed = 1;
    } else {
        printf("Test passed: swap_bounds_with_correct_values swapped correctly to lower=%d, upper=%d\n", lower, upper);
    }
    return failed;
}

int main() {
    char test_name[] = "PREPROCESSING";
    print_test_start(test_name);
    
    int total_tests = 0;
    int failed_tests = 0;
    
    RUN_TEST(test_process_args());
    RUN_TEST(test_bad_args());
    RUN_TEST(test_random_between());
    RUN_TEST(test_swap_bounds());
    RUN_TEST(test_swap_bounds_with_correct_values());
    
    int passed_tests = total_tests - failed_tests;
    print_test_end(test_name, passed_tests, failed_tests);
    return failed_tests > 0 ? 1 : 0;
}