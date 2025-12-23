/**
 * @file test_utils.h
 * @brief Header file for test_utils.c, containing utility functions for testing.
 */

/**
 * @brief Macro to run a test and automatically track total and failed counts.
 * 
 * Usage: RUN_TEST(test_function(args));
 * 
 * The test function should return 0 for pass, non-zero for fail.
 */
#define RUN_TEST(test_func) do { \
    total_tests++; \
    failed_tests += (test_func); \
} while(0)

/**
 * @brief Prints the start of a test suite with the test name.
 *
 * @param test_name The name of the test suite.
 */
void print_test_start(char* test_name);

/**
 * @brief Prints the end of a test suite with passed and failed test counts.
 *
 * @param test_name The name of the test suite.
 * @param passed_test_count The total number of passed tests.
 * @param failed_test_count The total number of failed tests.
 */
void print_test_end(char* test_name, int passed_test_count, int failed_test_count);

/**
 * @brief Prints a final summary of all test suites run.
 *
 * @param total_passed Total number of passed tests across all suites.
 * @param total_failed Total number of failed tests across all suites.
 * @param suite_count Number of test suites run.
 */
void print_test_summary(int total_passed, int total_failed, int suite_count);