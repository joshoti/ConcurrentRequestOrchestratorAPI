#include <stdio.h>
#include <string.h>

#include "test_utils.h"
#include "job_receiver.h"

int test_job_init(job_t* job) {
    int failed = 0;
    if (!init_job(job, 1, 1000000, 10)) {
        printf("Test failed: init_job returned FALSE\n");
        return 1;
    }
    if (job->id != 1 || job->inter_arrival_time_us != 1000000 || job->papers_required != 10) {
        printf("Test failed: init_job did not set fields correctly\n");
        failed = 1;
    } else {
        printf("Test passed: init_job set fields correctly\n");
    }
    return failed;
}

int test_debug_job(job_t* job) {
    printf("Testing debug_job output:\n");
    debug_job(job);
    // Manual verification needed for debug output
    return 0;
}

int main() {
    char test_name[] = "JOB";
    print_test_start(test_name);
    
    int total_tests = 0;
    int failed_tests = 0;

    job_t job;
    RUN_TEST(test_job_init(&job));
    RUN_TEST(test_debug_job(&job));

    int passed_tests = total_tests - failed_tests;
    print_test_end(test_name, passed_tests, failed_tests);
    return failed_tests > 0 ? 1 : 0;
}

