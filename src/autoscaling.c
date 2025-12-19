#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "autoscaling.h"
#include "config.h"
#include "printer.h"
#include "timed_queue.h"
#include "preprocessing.h"
#include "timeutils.h"
#include "common.h"
#include "log_router.h"

extern int g_debug;
extern int g_terminate_now;

int get_scale_up_threshold(int active_printers) {
    switch (active_printers) {
        case 2:
            return CONFIG_AUTOSCALE_THRESHOLD_2_PRINTERS;
        case 3:
            return CONFIG_AUTOSCALE_THRESHOLD_3_PRINTERS;
        case 4:
            return CONFIG_AUTOSCALE_THRESHOLD_4_PRINTERS;
        default:
            return 999999; // No scaling for 5+ printers
    }
}

int should_scale_up(printer_pool_t* pool, int queue_length, unsigned long current_time_us) {
    pthread_mutex_lock(&pool->pool_mutex);
    
    // Check if we're at max capacity
    if (pool->active_count >= CONFIG_RANGE_CONSUMER_COUNT_MAX) {
        pthread_mutex_unlock(&pool->pool_mutex);
        return 0;
    }
    
    // Check cooldown period
    if (current_time_us - pool->last_scale_time_us < CONFIG_AUTOSCALE_COOLDOWN_US) {
        pthread_mutex_unlock(&pool->pool_mutex);
        return 0;
    }
    
    // Check queue threshold
    int threshold = get_scale_up_threshold(pool->active_count);
    int should_scale = (queue_length >= threshold);
    
    pthread_mutex_unlock(&pool->pool_mutex);
    return should_scale;
}

int should_scale_down(printer_pool_t* pool, int queue_length, unsigned long current_time_us) {
    pthread_mutex_lock(&pool->pool_mutex);
    
    // Check if we're at minimum capacity
    if (pool->active_count <= pool->min_count) {
        pthread_mutex_unlock(&pool->pool_mutex);
        return 0;
    }
    
    // Check cooldown period
    if (current_time_us - pool->last_scale_time_us < CONFIG_AUTOSCALE_COOLDOWN_US) {
        pthread_mutex_unlock(&pool->pool_mutex);
        return 0;
    }
    
    // Check if queue is below scale-down threshold
    if (queue_length >= CONFIG_AUTOSCALE_SCALE_DOWN_THRESHOLD) {
        pool->low_queue_start_time_us = 0; // Reset timer
        pthread_mutex_unlock(&pool->pool_mutex);
        return 0;
    }
    
    // Start tracking low queue time
    if (pool->low_queue_start_time_us == 0) {
        pool->low_queue_start_time_us = current_time_us;
        pthread_mutex_unlock(&pool->pool_mutex);
        return 0;
    }
    
    // Check if queue has been low for long enough
    if (current_time_us - pool->low_queue_start_time_us < CONFIG_AUTOSCALE_SCALE_DOWN_WAIT_US) {
        pthread_mutex_unlock(&pool->pool_mutex);
        return 0;
    }
    
    // Check if at least one printer has been idle long enough
    int has_idle_printer = 0;
    for (int i = pool->active_count - 1; i >= pool->min_count; i--) {
        if (pool->printers[i].active && 
            pool->printers[i].printer.is_idle &&
            current_time_us - pool->printers[i].printer.last_job_completion_time_us >= CONFIG_AUTOSCALE_IDLE_TIMEOUT_US) {
            has_idle_printer = 1;
            break;
        }
    }
    
    pthread_mutex_unlock(&pool->pool_mutex);
    return has_idle_printer;
}

int scale_up(autoscaling_thread_args_t* args) {
    printer_pool_t* pool = args->pool;
    
    pthread_mutex_lock(&pool->pool_mutex);
    
    if (pool->active_count >= CONFIG_RANGE_CONSUMER_COUNT_MAX) {
        pthread_mutex_unlock(&pool->pool_mutex);
        return 0;
    }
    
    int new_printer_id = pool->active_count + 1;
    
    // Create template args for new printer
    printer_thread_args_t shared_args = {
        .paper_refill_queue_mutex = args->job_queue_mutex,
        .job_queue_mutex = args->job_queue_mutex,
        .stats_mutex = args->stats_mutex,
        .simulation_state_mutex = args->simulation_state_mutex,
        .job_queue_not_empty_cv = args->job_queue_not_empty_cv,
        .refill_needed_cv = args->refill_needed_cv,
        .refill_supplier_cv = args->refill_supplier_cv,
        .paper_refill_thread = args->paper_refill_thread,
        .job_queue = args->job_queue,
        .paper_refill_queue = args->paper_refill_queue,
        .params = args->params,
        .stats = args->stats,
        .all_jobs_served = args->all_jobs_served,
        .all_jobs_arrived = args->all_jobs_arrived,
        .printer = NULL // Will be set by printer_pool_start_printer
    };
    
    if (printer_pool_start_printer(pool, new_printer_id, &shared_args)) {
        unsigned long current_time_us = get_time_in_us();
        pool->last_scale_time_us = current_time_us;
        pool->low_queue_start_time_us = 0; // Reset scale-down timer
        
        pthread_mutex_lock(args->job_queue_mutex);
        int queue_length = timed_queue_length(args->job_queue);
        pthread_mutex_unlock(args->job_queue_mutex);
        
        emit_scale_up(pool->active_count, queue_length, current_time_us);
        if (g_debug) {
            printf("Printer %d thread started\n", new_printer_id);
        }
        pthread_mutex_unlock(&pool->pool_mutex);
        return 1;
    }
    
    pthread_mutex_unlock(&pool->pool_mutex);
    return 0;
}

int scale_down(autoscaling_thread_args_t* args) {
    printer_pool_t* pool = args->pool;
    
    pthread_mutex_lock(&pool->pool_mutex);
    
    if (pool->active_count <= pool->min_count) {
        pthread_mutex_unlock(&pool->pool_mutex);
        return 0;
    }
    
    // Find the most recently added printer that is idle
    int printer_to_remove = -1;
    for (int i = pool->active_count - 1; i >= pool->min_count; i--) {
        if (pool->printers[i].active && pool->printers[i].printer.is_idle) {
            printer_to_remove = i;
            break;
        }
    }
    
    if (printer_to_remove < 0) {
        pthread_mutex_unlock(&pool->pool_mutex);
        return 0;
    }
    
    // Cancel and join the printer thread
    pthread_cancel(pool->printers[printer_to_remove].thread);
    pthread_mutex_unlock(&pool->pool_mutex); // Unlock before join to avoid deadlock
    pthread_join(pool->printers[printer_to_remove].thread, NULL);
    pthread_mutex_lock(&pool->pool_mutex);
    
    pool->printers[printer_to_remove].active = 0;
    pool->active_count--;
    unsigned long current_time_us = get_time_in_us();
    pool->last_scale_time_us = current_time_us;
    pool->low_queue_start_time_us = 0; // Reset timer
    
    pthread_mutex_lock(args->job_queue_mutex);
    int queue_length = timed_queue_length(args->job_queue);
    pthread_mutex_unlock(args->job_queue_mutex);
    
    emit_scale_down(pool->active_count, queue_length, current_time_us);
    if (g_debug) {
        printf("Printer %d thread stopped\n", printer_to_remove + 1);
    }
    
    pthread_mutex_unlock(&pool->pool_mutex);
    return 1;
}

void* autoscaling_thread_func(void* arg) {
    autoscaling_thread_args_t* args = (autoscaling_thread_args_t*)arg;
    
    if (g_debug) printf("Autoscaling thread started\n");
    
    while (1) {
        // Check termination
        pthread_mutex_lock(args->simulation_state_mutex);
        int terminate = g_terminate_now || *args->all_jobs_served;
        pthread_mutex_unlock(args->simulation_state_mutex);
        
        if (terminate) break;
        
        // Get current queue length
        pthread_mutex_lock(args->job_queue_mutex);
        int queue_length = timed_queue_length(args->job_queue);
        pthread_mutex_unlock(args->job_queue_mutex);
        
        unsigned long current_time_us = get_time_in_us();
        
        // Check scaling conditions
        if (should_scale_up(args->pool, queue_length, current_time_us)) {
            scale_up(args);
        } else if (should_scale_down(args->pool, queue_length, current_time_us)) {
            scale_down(args);
        }
        
        // Sleep for check interval
        usleep(CONFIG_AUTOSCALE_CHECK_INTERVAL_US);
    }
    
    if (g_debug) printf("Autoscaling thread exiting\n");
    return NULL;
}
