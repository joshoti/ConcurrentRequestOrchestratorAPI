#ifndef CONFIG_H
#define CONFIG_H

/**
 * @file config.h
 * @brief Single source of truth for all simulation configuration values and ranges
 * 
 * This header defines default values and valid ranges for all simulation parameters.
 * These constants are used for:
 * - Validation in preprocessing.c
 * - Default values in simulation_parameters_t
 * - Building JSON responses for /api/config endpoint
 */

// ============================================================================
// DEFAULT CONFIGURATION VALUES
// ============================================================================

// Printer configuration
#define CONFIG_DEFAULT_PRINT_RATE           5.0     // pages/second
#define CONFIG_DEFAULT_CONSUMER_COUNT       2       // number of printers
#define CONFIG_DEFAULT_AUTO_SCALING         0       // false (0) or true (1)
#define CONFIG_DEFAULT_REFILL_RATE          25.0    // papers/second
#define CONFIG_DEFAULT_PAPER_CAPACITY       150     // maximum papers per printer

// Job configuration
#define CONFIG_DEFAULT_JOB_ARRIVAL_TIME     500     // milliseconds between jobs
#define CONFIG_DEFAULT_JOB_COUNT            10      // number of jobs to process
#define CONFIG_DEFAULT_FIXED_ARRIVAL        1       // true (1) = fixed, false (0) = random
#define CONFIG_DEFAULT_MIN_ARRIVAL_TIME     300     // milliseconds (if random)
#define CONFIG_DEFAULT_MAX_ARRIVAL_TIME     600     // milliseconds (if random)
#define CONFIG_DEFAULT_MAX_QUEUE            -1      // -1 = unlimited queue size
#define CONFIG_DEFAULT_MIN_PAPERS           5       // minimum pages per job
#define CONFIG_DEFAULT_MAX_PAPERS           15      // maximum pages per job

// UI display flags
#define CONFIG_DEFAULT_SHOW_TIME            1       // true
#define CONFIG_DEFAULT_SHOW_STATS           1       // true
#define CONFIG_DEFAULT_SHOW_LOGS            1       // true
#define CONFIG_DEFAULT_SHOW_COMPONENTS      1       // true

// ============================================================================
// VALID RANGES FOR PARAMETERS
// ============================================================================

// Print rate range (pages/second)
#define CONFIG_RANGE_PRINT_RATE_MIN         4.0
#define CONFIG_RANGE_PRINT_RATE_MAX         10.0

// Consumer count range (number of printers)
#define CONFIG_RANGE_CONSUMER_COUNT_MIN     1
#define CONFIG_RANGE_CONSUMER_COUNT_MAX     5

// Refill rate range (papers/second)
#define CONFIG_RANGE_REFILL_RATE_MIN        15.0
#define CONFIG_RANGE_REFILL_RATE_MAX        30.0

// Paper capacity range (maximum papers per printer)
#define CONFIG_RANGE_PAPER_CAPACITY_MIN     50
#define CONFIG_RANGE_PAPER_CAPACITY_MAX     200

// Job arrival time range (milliseconds)
#define CONFIG_RANGE_JOB_ARRIVAL_TIME_MIN   200
#define CONFIG_RANGE_JOB_ARRIVAL_TIME_MAX   800

// Minimum arrival time range (milliseconds, for random arrival)
#define CONFIG_RANGE_MIN_ARRIVAL_TIME_MIN   200
#define CONFIG_RANGE_MIN_ARRIVAL_TIME_MAX   400

// Maximum arrival time range (milliseconds, for random arrival)
#define CONFIG_RANGE_MAX_ARRIVAL_TIME_MIN   500
#define CONFIG_RANGE_MAX_ARRIVAL_TIME_MAX   800

// Minimum papers per job range
#define CONFIG_RANGE_MIN_PAPERS_MIN         5
#define CONFIG_RANGE_MIN_PAPERS_MAX         10

// Maximum papers per job range
#define CONFIG_RANGE_MAX_PAPERS_MIN         15
#define CONFIG_RANGE_MAX_PAPERS_MAX         30

#endif // CONFIG_H
