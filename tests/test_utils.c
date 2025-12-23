#include <stdio.h>
#include <string.h>

#include "test_utils.h"

// ANSI color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_RED     "\033[31m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_BOLD    "\033[1m"

void print_test_start(char* test_name) {
    printf("\n%s%s=== %s ===%s\n", COLOR_BOLD, COLOR_BLUE, test_name, COLOR_RESET);
}

void print_test_end(char* test_name, int passed_test_count, int failed_test_count) {
    if (failed_test_count == 0) {
        printf("%s✓ %s: %s%d passed%s, %d failed\n",
               COLOR_GREEN, test_name, COLOR_BOLD, passed_test_count, COLOR_RESET, failed_test_count);
    } else {
        printf("%s✗ %s: %s%d passed, %s%d failed%s\n",
               COLOR_RED, test_name, COLOR_GREEN, passed_test_count, COLOR_RED, failed_test_count, COLOR_RESET);
    }
}

void print_test_summary(int total_passed, int total_failed, int suite_count) {
    printf("\n%s%s", COLOR_BOLD, "=");
    for (int i = 0; i < 60; i++) printf("=");
    printf("%s\n", COLOR_RESET);
    
    printf("%s%sTEST SUMMARY%s\n", COLOR_BOLD, COLOR_BLUE, COLOR_RESET);
    printf("%sTest Suites: %s%d%s\n", COLOR_BOLD, COLOR_BLUE, suite_count, COLOR_RESET);
    
    if (total_failed == 0) {
        printf("%s%s✓ ALL TESTS PASSED%s\n", COLOR_BOLD, COLOR_GREEN, COLOR_RESET);
        printf("%sTotal: %s%d passed%s\n", COLOR_BOLD, COLOR_GREEN, total_passed, COLOR_RESET);
    } else {
        printf("%s%s✗ SOME TESTS FAILED%s\n", COLOR_BOLD, COLOR_RED, COLOR_RESET);
        printf("%sTotal: %s%d passed%s, %s%d failed%s\n",
               COLOR_BOLD, COLOR_GREEN, total_passed, COLOR_RESET,
               COLOR_RED, total_failed, COLOR_RESET);
    }
    
    printf("%s", COLOR_BOLD);
    for (int i = 0; i < 61; i++) printf("=");
    printf("%s\n\n", COLOR_RESET);
}