#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "linked_list.h"
#include "timed_queue.h"
#include "job_receiver.h"
#include "paper_refiller.h"
#include "printer.h"
#include "autoscaling.h"
#include "common.h"
#include "preprocessing.h"
#include "log_router.h"
#include "console_handler.h"
#include "simulation_stats.h"
#include "signalcatcher.h"

extern int g_debug;
extern int g_terminate_now;

int main(int argc, char *argv[]) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigprocmask(SIG_BLOCK, &set, (sigset_t*)0);

    // --- Thread identifiers ---
    pthread_t job_receiver_thread;
    pthread_t paper_refill_thread;
    pthread_t signal_catching_thread;
    pthread_t autoscaling_thread;

    // --- Synchronization primitives (shared) ---
    pthread_mutex_t job_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t paper_refill_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t simulation_state_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t job_queue_not_empty_cv = PTHREAD_COND_INITIALIZER;
    pthread_cond_t refill_needed_cv = PTHREAD_COND_INITIALIZER;
    pthread_cond_t refill_supplier_cv = PTHREAD_COND_INITIALIZER;

    // --- Simulation state ---
    simulation_parameters_t params = SIMULATION_DEFAULT_PARAMS_HIGH_LOAD;
    simulation_statistics_t stats = (simulation_statistics_t){0};
    int all_jobs_arrived = 0;
    int all_jobs_served = 0;
    timed_queue_t job_queue;
    linked_list_t paper_refill_queue;
    timed_queue_init(&job_queue);
    list_init(&paper_refill_queue);

    if (!process_args(argc, argv, &params)) return 1;

    // --- Printer Pool ---
    printer_pool_t printer_pool;
    printer_pool_init(&printer_pool, params.consumer_count, params.printer_paper_capacity);

    // --- Thread argument structs ---
    job_thread_args_t job_receiver_args = {
        .job_queue_mutex = &job_queue_mutex,
        .stats_mutex = &stats_mutex,
        .simulation_state_mutex = &simulation_state_mutex,
        .job_queue_not_empty_cv = &job_queue_not_empty_cv,
        .job_queue = &job_queue,
        .simulation_params = &params,
        .stats = &stats,
        .all_jobs_arrived = &all_jobs_arrived
    };

    // Shared printer args template (printer pointer will be set by pool)
    printer_thread_args_t shared_printer_args = {
        .paper_refill_queue_mutex = &paper_refill_queue_mutex,
        .job_queue_mutex = &job_queue_mutex,
        .stats_mutex = &stats_mutex,
        .simulation_state_mutex = &simulation_state_mutex,
        .job_queue_not_empty_cv = &job_queue_not_empty_cv,
        .refill_needed_cv = &refill_needed_cv,
        .refill_supplier_cv = &refill_supplier_cv,
        .paper_refill_thread = &paper_refill_thread,
        .job_queue = &job_queue,
        .paper_refill_queue = &paper_refill_queue,
        .params = &params,
        .stats = &stats,
        .all_jobs_served = &all_jobs_served,
        .all_jobs_arrived = &all_jobs_arrived,
        .printer = NULL // Set by printer_pool_start_printer
    };

    paper_refill_thread_args_t paper_refill_args = {
        .paper_refill_queue_mutex = &paper_refill_queue_mutex,
        .stats_mutex = &stats_mutex,
        .simulation_state_mutex = &simulation_state_mutex,
        .refill_needed_cv = &refill_needed_cv,
        .refill_supplier_cv = &refill_supplier_cv,
        .paper_refill_queue = &paper_refill_queue,
        .job_queue = &job_queue,
        .params = &params,
        .stats = &stats,
        .all_jobs_served = &all_jobs_served
    };

    autoscaling_thread_args_t autoscaling_args = {
        .job_queue_mutex = &job_queue_mutex,
        .stats_mutex = &stats_mutex,
        .simulation_state_mutex = &simulation_state_mutex,
        .job_queue_not_empty_cv = &job_queue_not_empty_cv,
        .refill_needed_cv = &refill_needed_cv,
        .refill_supplier_cv = &refill_supplier_cv,
        .paper_refill_thread = &paper_refill_thread,
        .job_queue = &job_queue,
        .paper_refill_queue = &paper_refill_queue,
        .params = &params,
        .stats = &stats,
        .all_jobs_served = &all_jobs_served,
        .all_jobs_arrived = &all_jobs_arrived,
        .pool = &printer_pool
    };

    signal_catching_thread_args_t signal_catching_args = {
        .signal_set = &set,
        .job_queue_mutex = &job_queue_mutex,
        .simulation_state_mutex = &simulation_state_mutex,
        .paper_refill_queue_mutex = &paper_refill_queue_mutex,
        .stats_mutex = &stats_mutex,
        .job_queue_not_empty_cv = &job_queue_not_empty_cv,
        .refill_needed_cv = &refill_needed_cv,
        .refill_supplier_cv = &refill_supplier_cv,
        .job_queue = &job_queue,
        .stats = &stats,
        .job_receiver_thread = &job_receiver_thread,
        .paper_refill_thread = &paper_refill_thread,
        .all_jobs_arrived = &all_jobs_arrived
    };

    // Register console handler (stdout logger) via handler module
    console_handler_register();
    // Terminal mode: print to stdout
    set_log_mode(LOG_MODE_TERMINAL);
    // --- Start of simulation logging ---
    emit_simulation_parameters(&params);
    emit_simulation_start(&stats);

    // --- Create threads in order ---
    // 1) Job receiver (produces jobs)
    pthread_create(&job_receiver_thread, NULL, job_receiver_thread_func, &job_receiver_args);

    // 2) Paper refiller (services refill requests)
    pthread_create(&paper_refill_thread, NULL, paper_refill_thread_func, &paper_refill_args);

    // 3) Start initial printers (minimum count)
    for (int i = 1; i <= params.consumer_count; i++) {
        printer_pool_start_printer(&printer_pool, i, &shared_printer_args);
    }

    // 4) Autoscaling thread (if enabled)
    if (params.auto_scaling) {
        pthread_create(&autoscaling_thread, NULL, autoscaling_thread_func, &autoscaling_args);
        if (g_debug) printf("Autoscaling enabled\n");
    }

    // 5) Signal catcher (created last, after we have thread IDs to pass by pointer)
    pthread_create(&signal_catching_thread, NULL, sig_int_catching_thread_func, &signal_catching_args);

    // --- Wait for threads to finish ---
    // Join producer first so no new jobs are created
    pthread_join(job_receiver_thread, NULL);
    if (g_debug) printf("job_receiver_thread joined\n");

    // Join all printers
    printer_pool_join_all(&printer_pool);
    if (g_debug) printf("all printer threads joined\n");

    // Join paper refiller
    pthread_join(paper_refill_thread, NULL);
    if (g_debug) printf("paper_refill_thread joined\n");

    // Join autoscaling thread if it was started
    if (params.auto_scaling) {
        pthread_cancel(autoscaling_thread);
        pthread_join(autoscaling_thread, NULL);
        if (g_debug) printf("autoscaling thread joined\n");
    }

    // Signal catcher might still be waiting for SIGINT; cancel and join
    pthread_cancel(signal_catching_thread);
    pthread_join(signal_catching_thread, NULL);
    if (g_debug) printf("signal catching thread joined\n");

    // --- Final logging ---
    emit_simulation_end(&stats);
    emit_statistics(&stats);

    // --- Cleanup printer pool ---
    printer_pool_destroy(&printer_pool);

    // --- Cleanup synchronization primitives ---
    pthread_mutex_destroy(&job_queue_mutex);
    pthread_mutex_destroy(&paper_refill_queue_mutex);
    pthread_mutex_destroy(&stats_mutex);
    pthread_mutex_destroy(&simulation_state_mutex);
    pthread_cond_destroy(&job_queue_not_empty_cv);
    pthread_cond_destroy(&refill_needed_cv);
    pthread_cond_destroy(&refill_supplier_cv);

    if (g_debug) printf("All threads joined and resources cleaned up.\n");
    return 0;
}