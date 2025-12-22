#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "paper_refiller.h"
#include "common.h"
#include "timeutils.h"
#include "printer.h"
#include "linked_list.h"
#include "timed_queue.h"
#include "preprocessing.h"
#include "log_router.h"
#include "simulation_stats.h"

extern int g_debug;
extern int g_terminate_now;

void debug_refiller(int papers_supplied) {
    printf("Debug: Paper Refiller supplied %d papers\n", papers_supplied);
}

/**
 * @brief Checks if the exit condition for the paper refiller thread is met.
 * 
 * @param all_jobs_served Indicator if all jobs have been served.
 * @return TRUE if exit condition is met, FALSE otherwise.
 */
static int is_exit_condition_met(int all_jobs_served) {
    if (all_jobs_served) {
        if (g_debug) printf("Paper refiller thread has finished\n");
        return TRUE;
    }
    return FALSE;
}

void* paper_refill_thread_func(void* arg) {
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    
    paper_refill_thread_args_t* args = (paper_refill_thread_args_t*)arg;

    if (g_debug) printf("Paper refiller thread started\n");
    while (1) {
        pthread_mutex_lock(args->paper_refill_queue_mutex);

        for (;;) {
            // Safely check shared flags
            pthread_mutex_lock(args->simulation_state_mutex);
            int terminate_now = g_terminate_now;
            int are_all_jobs_served = *(args->all_jobs_served);
            pthread_mutex_unlock(args->simulation_state_mutex);

            if (terminate_now || is_exit_condition_met(are_all_jobs_served)) {
                if (g_debug) printf("Paper refiller thread signaled to terminate\n");
                pthread_cond_broadcast(args->refill_needed_cv); // wake up printer threads to let them exit if needed
                pthread_mutex_unlock(args->paper_refill_queue_mutex);
                goto exit_refiller;
            }

            if (!list_is_empty(args->paper_refill_queue)) {
                break; // there's work
            }

            /* 
             * Disabling cancellation to avoid deadlock from thread
             * being cancelled while holding mutex
             *
             * Wait until signaled to refill paper or terminate
             */
            pthread_cond_wait(args->refill_supplier_cv, args->paper_refill_queue_mutex);
        }
        unsigned long refill_start_time_us = get_time_in_us();
        list_node_t* elem = list_pop_left(args->paper_refill_queue);
        printer_t* printer = (printer_t*)elem->data;
        pthread_mutex_unlock(args->paper_refill_queue_mutex); // unlock while refilling

        // Refill paper
        int papers_needed = printer->capacity - printer->current_paper_count;
        if (papers_needed <= 0) {
            if (g_debug) printf("Debug: Paper Refiller found printer %d already full, skipping refill\n", printer->id);
            free(elem);
            // Still broadcast to wake up any waiting printers
            pthread_mutex_lock(args->paper_refill_queue_mutex);
            pthread_cond_broadcast(args->refill_needed_cv);
            pthread_mutex_unlock(args->paper_refill_queue_mutex);
            continue;
        }

        int time_to_refill_us = (unsigned long)((papers_needed / args->params->refill_rate) * 1000000);
        emit_paper_refill_start(printer, papers_needed, time_to_refill_us, refill_start_time_us);
        
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        usleep(time_to_refill_us);
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        
        unsigned long refill_end_time_us = get_time_in_us();
        int refill_duration_us = refill_end_time_us - refill_start_time_us;
        emit_paper_refill_end(printer, refill_duration_us, refill_end_time_us);
        
        // Done refilling: update printer state and simulation stats
        printer->current_paper_count += papers_needed;
        pthread_mutex_lock(args->stats_mutex);
        args->stats->papers_refilled += papers_needed;
        args->stats->total_refill_service_time_us += refill_end_time_us - refill_start_time_us;
        args->stats->paper_refill_events++;
        emit_stats_update(args->stats, timed_queue_length(args->job_queue));
        pthread_mutex_unlock(args->stats_mutex);
        free(elem);
        if (g_debug) debug_refiller(papers_needed);

        // Notify waiting printers that refill is done
        pthread_mutex_lock(args->paper_refill_queue_mutex);
        pthread_cond_broadcast(args->refill_needed_cv);
        pthread_mutex_unlock(args->paper_refill_queue_mutex);
    }
exit_refiller:
    if (g_debug) printf("Paper refiller gracefully exited\n");
    return NULL;
}