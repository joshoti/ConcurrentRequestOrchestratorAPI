#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "common.h"
#include "job_receiver.h"
#include "preprocessing.h"
#include "linked_list.h"
#include "timed_queue.h"
#include "timeutils.h"
#include "log_router.h"
#include "simulation_stats.h"

extern int g_terminate_now;
extern int g_debug;

int init_job(job_t* job, int job_id, int inter_arrival_time_us, int papers_required) {
    if (job == NULL) {
        return FALSE;
    }
    job->id = job_id;
    job->inter_arrival_time_us = inter_arrival_time_us;
    job->papers_required = papers_required;

    // Initialize service time to 0; will be set later based on printing rate
    job->service_time_requested_ms = 0;

    // Initialize timestamps to 0
    job->system_arrival_time_us = 0;
    job->queue_arrival_time_us = 0;
    job->queue_departure_time_us = 0;
    job->service_arrival_time_us = 0;
    job->service_departure_time_us = 0;

    return TRUE;
}

void drop_job_from_system(job_t* job, unsigned long previous_job_arrival_time_us, simulation_statistics_t* stats) {
    if (job == NULL) return;
    
    // Log the dropped job (this will update statistics internally)
    emit_dropped_job(job, previous_job_arrival_time_us, stats);

    // Free the job memory
    free(job);
}

void debug_job(job_t* job) {
    if (job == NULL) {
        printf("Job is NULL\n");
        return;
    }
    
    printf("\nJob Debug Info:\n");
    printf("  Job ID: %d\n", job->id);
    printf("  Inter-arrival time: %d us\n", job->inter_arrival_time_us);
    printf("  Papers required: %d\n", job->papers_required);
    printf("  Service time requested: %d ms\n", job->service_time_requested_ms);
    printf("  System arrival time: %lu us\n", job->system_arrival_time_us);
    printf("  Queue arrival time: %lu us\n", job->queue_arrival_time_us);
    printf("  Queue departure time: %lu us\n", job->queue_departure_time_us);
    printf("  Service arrival time: %lu us\n", job->service_arrival_time_us);
    printf("  Service departure time: %lu us\n", job->service_departure_time_us);
}

void* job_receiver_thread_func(void* arg) {
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

    job_thread_args_t* args = (job_thread_args_t*)arg;
    if (args == NULL) {
        fprintf(stderr, "Error: JobThreadArgs is NULL\n");
        return NULL;
    }
    
    if (g_debug) printf("Job receiver thread started\n");
    // Extract arguments
    pthread_mutex_t* job_queue_mutex = args->job_queue_mutex;
    pthread_mutex_t* stats_mutex = args->stats_mutex;
    pthread_mutex_t* simulation_state_mutex = args->simulation_state_mutex;
    pthread_cond_t* job_queue_not_empty_cv = args->job_queue_not_empty_cv;
    timed_queue_t* job_queue = args->job_queue;
    simulation_parameters_t* params = args->simulation_params;
    simulation_statistics_t* stats = args->stats;
    int* all_jobs_arrived = args->all_jobs_arrived;

    unsigned long previous_job_arrival_time_us = stats->simulation_start_time_us;
    
    for (int job_id = 0; job_id < params->num_jobs; job_id++) {
        const int inter_arrival_time_us = (int)params->job_arrival_time_us;
        const int papers_required = random_between(params->papers_required_lower_bound, params->papers_required_upper_bound);

        // Allocate and initialize job
        job_t* job = (job_t*)malloc(sizeof(job_t));
        if (!init_job(job, job_id + 1, inter_arrival_time_us, papers_required)) {
            fprintf(stderr, "Error: Failed to initialize job %d\n", job_id + 1);
            free(job);
            continue;
        }
        
        // Sleep for inter-arrival time
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        usleep(inter_arrival_time_us);
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        
        // Check for termination signal
        pthread_mutex_lock(simulation_state_mutex);
        int terminate_now = g_terminate_now;
        pthread_mutex_unlock(simulation_state_mutex);
        if (terminate_now) {
            *all_jobs_arrived = 1;
            free(job);
            break;
        }
        
        // Set system arrival time
        job->system_arrival_time_us = get_time_in_us();
        pthread_mutex_lock(stats_mutex);
        emit_system_arrival(job, previous_job_arrival_time_us, stats);
        pthread_mutex_unlock(stats_mutex);
        
        // Check if job should be dropped (e.g., if queue is full)
        pthread_mutex_lock(job_queue_mutex);

        int queue_length = timed_queue_length(job_queue);
        // Only check capacity if it's not unlimited (-1)
        if (params->queue_capacity != -1 && queue_length >= params->queue_capacity) {
            // Drop the job
            pthread_mutex_unlock(job_queue_mutex);
            unsigned long temp_arrival_time_us = job->system_arrival_time_us; // store before freeing
            
            pthread_mutex_lock(stats_mutex);
            drop_job_from_system(job, previous_job_arrival_time_us, stats);
            pthread_mutex_unlock(stats_mutex);

            previous_job_arrival_time_us = temp_arrival_time_us;
            continue;
        }
        
        // Add job to queue
        job->queue_arrival_time_us = get_time_in_us();
        unsigned long queue_last_interaction_time_us = job_queue->last_interaction_time_us;
        timed_queue_enqueue(job_queue, job);
        
        // Update statistics
        pthread_mutex_lock(stats_mutex);
        stats->max_job_queue_length = 
            (queue_length > stats->max_job_queue_length) ? (queue_length) : stats->max_job_queue_length;
        emit_queue_arrival(job, stats, job_queue, queue_last_interaction_time_us);
        pthread_mutex_unlock(stats_mutex);
        
        previous_job_arrival_time_us = job->system_arrival_time_us;
        
        // Signal that a job is available
        pthread_cond_broadcast(job_queue_not_empty_cv);
        pthread_mutex_unlock(job_queue_mutex);
    }
    
    // Mark that all jobs have arrived
    pthread_mutex_lock(simulation_state_mutex);
    *all_jobs_arrived = 1;
    pthread_mutex_unlock(simulation_state_mutex);
    
    // Wake up any waiting threads
    pthread_mutex_lock(job_queue_mutex);
    pthread_cond_broadcast(job_queue_not_empty_cv);
    pthread_mutex_unlock(job_queue_mutex);
    if (g_debug) printf("Job receiver thread gracefully exited\n");
    return NULL;
}