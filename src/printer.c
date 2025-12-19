#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "preprocessing.h"
#include "config.h"
#include "common.h"
#include "timeutils.h"
#include "log_router.h"
#include "simulation_stats.h"
#include "linked_list.h"
#include "timed_queue.h"
#include "job_receiver.h"
#include "printer.h"

extern int g_debug;
extern int g_terminate_now;

/**
 * @brief Checks if the exit condition for the server thread is met.
 * 
 * @return TRUE if exit condition is met, FALSE otherwise.
 */
static int is_exit_condition_met(int all_jobs_arrived, timed_queue_t* job_queue) {
    return all_jobs_arrived && timed_queue_is_empty(job_queue);
}

void debug_printer(const printer_t* printer) {
    printf("Debug: Printer %d has printed %d jobs and used %d papers\n",
        printer->id, printer->jobs_printed_count, printer->total_papers_used);
}

void* printer_thread_func(void* arg) {
    printer_thread_args_t* args = (printer_thread_args_t*)arg;

    if (g_debug) printf("Printer %d thread started\n", args->printer->id);

    while (1) {
        for (;;) {
            // Safely check shared flags
            pthread_mutex_lock(args->simulation_state_mutex);
            int terminate = g_terminate_now;
            pthread_mutex_unlock(args->simulation_state_mutex);

            pthread_mutex_lock(args->job_queue_mutex);
            if (terminate || is_exit_condition_met(*(args->all_jobs_arrived), args->job_queue)) {
                if (g_debug) printf("Printer %d is terminating or finished\n", args->printer->id);
                pthread_mutex_unlock(args->job_queue_mutex);
                goto exit_printer;
            }

            if (!timed_queue_is_empty(args->job_queue)) {
                break; // there's work
            }

            // Wait for a job to be available
            pthread_cond_wait(args->job_queue_not_empty_cv, args->job_queue_mutex);
            pthread_mutex_unlock(args->job_queue_mutex);
        }

        // Check if there are enough papers for the job at the front of the queue
        list_node_t* elem = timed_queue_first(args->job_queue);
        job_t* job_to_dequeue = (job_t*)elem->data;
        if (job_to_dequeue->papers_required > args->printer->current_paper_count) {
            // Not enough paper for the job at the front of the queue
            pthread_mutex_unlock(args->job_queue_mutex);
            
            pthread_mutex_lock(args->paper_refill_queue_mutex);
            unsigned long refill_start_time_us = get_time_in_us();
            emit_paper_empty(args->printer, job_to_dequeue->id, refill_start_time_us);
            list_append(args->paper_refill_queue, args->printer);
            pthread_cond_broadcast(args->refill_supplier_cv); // Notify refill thread
            
            // Wait until paper is refilled - loop until we actually have enough
            while (job_to_dequeue->papers_required > args->printer->current_paper_count) {
                pthread_cond_wait(args->refill_needed_cv, args->paper_refill_queue_mutex);
                
                // Check termination after waking up
                pthread_mutex_lock(args->simulation_state_mutex);
                int terminate = g_terminate_now;
                pthread_mutex_unlock(args->simulation_state_mutex);
                if (terminate) {
                    pthread_mutex_unlock(args->paper_refill_queue_mutex);
                    goto exit_printer;
                }
            }
            pthread_mutex_unlock(args->paper_refill_queue_mutex);
            
            // Update stats for paper empty duration
            pthread_mutex_lock(args->stats_mutex);
            int paper_empty_duration_us = get_time_in_us() - refill_start_time_us;
            
            // Track in array format (0-indexed)
            int idx = args->printer->id - 1;
            if (idx >= 0 && idx < MAX_PRINTERS) {
                args->stats->printer_paper_empty_time_us[idx] += paper_empty_duration_us;
            }
            
            // Keep backwards compatibility
            if (args->printer->id == 1) {
                args->stats->printer1_paper_empty_time_us +=
                    paper_empty_duration_us; // stats: total time printer 1 was idle due to no paper
            } else if (args->printer->id == 2) {
                args->stats->printer2_paper_empty_time_us +=
                    paper_empty_duration_us; // stats: total time printer 2 was idle due to no paper
            }
            pthread_mutex_unlock(args->stats_mutex);
            continue;
        }

        // Get the next job from the queue
        unsigned long queue_last_interaction_time_us = args->job_queue->last_interaction_time_us;
        elem = timed_queue_dequeue_front(args->job_queue);
        job_t* job = (job_t*)elem->data;
        job->queue_departure_time_us = get_time_in_us();
        emit_queue_departure(job, args->stats, args->job_queue, queue_last_interaction_time_us);

        pthread_mutex_unlock(args->job_queue_mutex);

        // Update job service_time_requested_ms based on printer speed
        job->service_time_requested_ms =
                (int)((job->papers_required / args->params->printing_rate) * 1000); // in ms

        // Log job arrival at printer
        job->service_arrival_time_us = get_time_in_us();
        emit_printer_arrival(job, args->printer);

        // Service the job
        args->printer->is_idle = 0; // Mark as busy
        emit_printer_busy(args->printer, get_time_in_us());
        usleep(job->service_time_requested_ms * 1000); // Convert ms to us
        args->printer->current_paper_count -= job->papers_required;
        args->printer->total_papers_used += job->papers_required;

        // Update job departure time
        job->service_departure_time_us = get_time_in_us();
        
        // Track completion time for idle detection
        args->printer->last_job_completion_time_us = job->service_departure_time_us;
        args->printer->is_idle = 1; // Mark as idle
        emit_printer_idle(args->printer, job->service_departure_time_us);

        // Update stats
        pthread_mutex_lock(args->stats_mutex);
        args->printer->jobs_printed_count++;
        // Log job departure from system and update stats
        emit_system_departure(job, args->printer, args->stats);
        pthread_mutex_unlock(args->stats_mutex);

        // Free job resources
        free(elem);
        free(job);

        // Check exit condition.
        pthread_mutex_lock(args->simulation_state_mutex);
        int have_all_jobs_arrived = *(args->all_jobs_arrived);
        pthread_mutex_unlock(args->simulation_state_mutex);
        pthread_mutex_lock(args->job_queue_mutex);
        if (is_exit_condition_met(have_all_jobs_arrived, args->job_queue)) {
            pthread_mutex_unlock(args->job_queue_mutex);
            if (g_debug) printf("Printer %d has finished\n", args->printer->id);
            goto exit_printer;
        }
        pthread_mutex_unlock(args->job_queue_mutex);

        if (g_debug) printf("Printer %d is looking for next job\n", args->printer->id);
        if (g_debug) debug_printer(args->printer);
    }

exit_printer:
    pthread_mutex_lock(args->simulation_state_mutex);
    *(args->all_jobs_served) = 1;
    pthread_mutex_unlock(args->simulation_state_mutex);
    
    pthread_mutex_lock(args->paper_refill_queue_mutex);
    pthread_cond_broadcast(args->refill_supplier_cv); // Notify refill thread in case it's waiting
    pthread_cond_broadcast(args->refill_needed_cv); // Notify printer thread in case it's waiting
    pthread_mutex_unlock(args->paper_refill_queue_mutex);
    pthread_cancel(*args->paper_refill_thread); // Cancel the paper refill thread in case it's refilling a printer
    if (g_debug) printf("Printer %d gracefully exited\n", args->printer->id);
    return NULL;
}
// ============================================================================
// Printer Pool Management
// ============================================================================

void printer_pool_init(printer_pool_t* pool, int min_printers, int paper_capacity) {
    memset(pool, 0, sizeof(printer_pool_t));
    pool->min_count = min_printers;
    pool->active_count = 0;
    pthread_mutex_init(&pool->pool_mutex, NULL);
    pool->last_scale_time_us = 0;
    pool->low_queue_start_time_us = 0;
    
    // Initialize all printer slots as inactive
    for (int i = 0; i < CONFIG_RANGE_CONSUMER_COUNT_MAX; i++) {
        pool->printers[i].active = 0;
        pool->printers[i].printer.id = i + 1;
        pool->printers[i].printer.current_paper_count = paper_capacity;
        pool->printers[i].printer.capacity = paper_capacity;
        pool->printers[i].printer.total_papers_used = 0;
        pool->printers[i].printer.jobs_printed_count = 0;
        pool->printers[i].printer.last_job_completion_time_us = 0;
        pool->printers[i].printer.is_idle = 1;
    }
}

int printer_pool_start_printer(printer_pool_t* pool, int printer_id, const printer_thread_args_t* shared_args) {
    if (pool->active_count >= CONFIG_RANGE_CONSUMER_COUNT_MAX) {
        return 0;
    }
    
    int index = printer_id - 1; // 0-indexed
    
    if (pool->printers[index].active) {
        return 0; // Already active
    }
    
    // Copy shared args
    pool->printers[index].args = *shared_args;
    // Point to this printer's instance
    pool->printers[index].args.printer = &pool->printers[index].printer;
    
    // Create thread
    int result = pthread_create(&pool->printers[index].thread, NULL, 
                                printer_thread_func, &pool->printers[index].args);
    
    if (result == 0) {
        pool->printers[index].active = 1;
        pool->active_count++;
        return 1;
    }
    
    return 0;
}

void printer_pool_join_all(printer_pool_t* pool) {
    for (int i = 0; i < CONFIG_RANGE_CONSUMER_COUNT_MAX; i++) {
        if (pool->printers[i].active) {
            pthread_join(pool->printers[i].thread, NULL);
            if (g_debug) printf("Joined printer %d thread\n", i + 1);
        }
    }
}

void printer_pool_destroy(printer_pool_t* pool) {
    pthread_mutex_destroy(&pool->pool_mutex);
}
