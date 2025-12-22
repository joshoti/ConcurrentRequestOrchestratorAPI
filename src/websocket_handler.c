#include <stdio.h>
#include <string.h>

#include "common.h"
#include "websocket_handler.h"
#include "preprocessing.h"
#include "timeutils.h"
#include "mongoose.h"
#include "log_router.h"
#include "simulation_stats.h"
#include "ws_bridge.h"
#include "job_receiver.h"
#include "linked_list.h"
#include "timed_queue.h"
#include "printer.h"

static unsigned long reference_time_us = 0;
static unsigned long reference_end_time_us = 0;


// --- Private Helper Functions ---
static void publish_simulation_start(simulation_statistics_t* stats) {
    reference_time_us = get_time_in_us();
    stats->simulation_start_time_us = reference_time_us;
    char buf[1024];

    // Send simulation_started message
    sprintf(buf, "{\"type\":\"simulation_started\", \"data\":{\"timestamp\":0}}");
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));

    // Send log message
    sprintf(buf, "{\"type\":\"log\", \"data\":{\"timestamp\":0, \"message\":\"simulation begins\"}}");
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

static void publish_simulation_end(simulation_statistics_t* stats)
{
    reference_end_time_us = get_time_in_us();
    char buf[1024];

    stats->simulation_duration_us = reference_end_time_us - reference_time_us;
    double timestamp_ms = (reference_end_time_us - reference_time_us) / 1000.0;
    double duration_ms = stats->simulation_duration_us / 1000.0;

    // Send log message
    sprintf(buf, "{\"type\":\"log\", \"data\":{\"timestamp\":%.3f, \"message\":\"simulation ends, duration = %.3fms\"}}",
        timestamp_ms, duration_ms);
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));

    // Send simulation_complete message
    sprintf(buf, "{\"type\":\"simulation_complete\", \"data\":{\"duration\":%.3f}}", duration_ms);
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}


/**
 * @brief Publishes an event when a new job is created in the system or when a job is dropped
 * 
 * @param job_id The id of the job that has been created or dropped.
 * @param papers_required The number of papers required by the job.
 * @param previous_job_arrival_time_us The arrival time of the previous job in microseconds
 * @param current_job_arrival_time_us The current simulation time in microseconds.
 * @param is_dropped Whether the job was dropped (TRUE) or created (FALSE).
 */
static void job_arrival_helper(int job_id, int papers_required,
    unsigned long previous_job_arrival_time_us, unsigned long current_job_arrival_time_us,
    int is_dropped)
{
    char buf[1024];

    double timestamp_ms = (current_job_arrival_time_us - reference_time_us) / 1000.0;

    int inter_arrival_time_us = current_job_arrival_time_us - previous_job_arrival_time_us;
    double inter_arrival_ms = inter_arrival_time_us / 1000.0;
    sprintf(buf, "{\"type\":\"log\", \"data\":{\"timestamp\":%.3f, \"message\":\"job%d arrives, needs %d paper%s, inter-arrival time = %.3fms%s\"}}",
        timestamp_ms, job_id, papers_required,
        papers_required == 1 ? "" : "s", inter_arrival_ms,
        is_dropped ? ", dropped" : ""
    );
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

static void publish_system_arrival(job_t* job, unsigned long previous_job_arrival_time_us,
    simulation_statistics_t* stats)
{
    stats->total_jobs_arrived++; // stats: total jobs arrived
    stats->total_inter_arrival_time_us += job->system_arrival_time_us - previous_job_arrival_time_us; // stats: avg job inter-arrival time
    job_arrival_helper(job->id, job->papers_required,
        previous_job_arrival_time_us, job->system_arrival_time_us, FALSE
    );
}

static void publish_dropped_job(job_t* job, unsigned long previous_job_arrival_time_us,
    simulation_statistics_t* stats)
{
    stats->total_jobs_dropped++; // stats: total jobs dropped
    job_arrival_helper(job->id, job->papers_required,
        previous_job_arrival_time_us, job->system_arrival_time_us, TRUE
    );
}

static void publish_removed_job(job_t* job) {
    unsigned long current_time_us = get_time_in_us();
    char buf[1024];

    double timestamp_ms = (current_time_us - reference_time_us) / 1000.0;
    sprintf(buf, "{\"type\":\"log\", \"data\":{\"timestamp\":%.3f, \"message\":\"job%d removed from system\"}}",
        timestamp_ms, job->id);
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

static void publish_queue_arrival(const job_t* job, simulation_statistics_t* stats,
    timed_queue_t* job_queue, unsigned long last_interaction_time_us)
{
    stats->area_num_in_job_queue_us +=
        (job->queue_arrival_time_us - last_interaction_time_us) * // duration of previous state
        (timed_queue_length(job_queue) - 1); // -1 for the job that just entered the queue
    // stats: avg job queue length
    job_queue->last_interaction_time_us = job->queue_arrival_time_us;

    char buf[1024];
    double timestamp_ms = (job->queue_arrival_time_us - reference_time_us) / 1000.0;

    sprintf(buf, "{\"type\":\"log\", \"data\":{\"timestamp\":%.3f, \"message\":\"job%d enters queue, queue length = %d\"}}",
        timestamp_ms, job->id, timed_queue_length(job_queue));
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

static void publish_queue_departure(const job_t* job, simulation_statistics_t* stats,
    timed_queue_t* job_queue, unsigned long last_interaction_time_us)
{
    stats->area_num_in_job_queue_us +=
        (job->queue_departure_time_us - last_interaction_time_us) * // duration of previous state
        (timed_queue_length(job_queue) + 1); // +1 for the job that just left the queue
    // stats: avg job queue length
    job_queue->last_interaction_time_us = job->queue_departure_time_us;

    char buf[1024];
    double timestamp_ms = (job->queue_departure_time_us - reference_time_us) / 1000.0;

    double queue_duration_ms = (job->queue_departure_time_us - job->queue_arrival_time_us) / 1000.0;
    sprintf(buf, "{\"type\":\"log\", \"data\":{\"timestamp\":%.3f, \"message\":\"job%d leaves queue, time in queue = %.3fms, queue_length = %d\"}}",
        timestamp_ms, job->id, queue_duration_ms, timed_queue_length(job_queue));
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

static void publish_job_update(const job_t* job, int queue_length) {
    char buf[512];
    
    sprintf(buf, "{\"type\":\"job_update\", \"data\":{\"id\":%d, \"papersRequired\":%d, \"queueLength\":%d}}", 
            job->id, job->papers_required, queue_length);
    
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

static void publish_printer_arrival(const job_t* job, const printer_t* printer)
{
    char buf[1024];
    double timestamp_ms = (job->service_arrival_time_us - reference_time_us) / 1000.0;
    sprintf(buf, "{\"type\":\"log\", \"data\":{\"timestamp\":%.3f, \"message\":\"job%d begins service at printer%d, printing %d pages in about %dms\"}}",
        timestamp_ms, job->id, printer->id, job->papers_required, job->service_time_requested_ms);
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

static void publish_system_departure(const job_t* job, const printer_t* printer,
    simulation_statistics_t* stats)
{
    char buf[1024];
    double timestamp_ms = (job->service_departure_time_us - reference_time_us) / 1000.0;

    int system_time = job->service_departure_time_us - job->system_arrival_time_us;
    stats->total_system_time_us += system_time; // stats: avg job system time
    stats->sum_of_system_time_squared_us2 += system_time * system_time; // stats: stddev job system time
    stats->total_jobs_served += 1; // stats: total jobs served

    int service_duration = job->service_departure_time_us - job->service_arrival_time_us;
    if (printer->id == 1) {
        stats->total_service_time_p1_us += service_duration; // stats: avg job service time
        stats->jobs_served_by_printer1 += 1; // stats: total jobs served by printer 1
    } else if (printer->id == 2) {
        stats->total_service_time_p2_us += service_duration; // stats: avg job service time
        stats->jobs_served_by_printer2 += 1; // stats: total jobs served by printer 2
    }
    stats->total_queue_wait_time_us +=
        (job->queue_departure_time_us - job->queue_arrival_time_us); // stats: avg job queue wait time

    double service_duration_ms = service_duration / 1000.0;
    sprintf(buf, "{\"type\":\"log\", \"data\":{\"timestamp\":%.3f, \"message\":\"job%d departs from printer%d, service time = %.3fms\"}}",
        timestamp_ms, job->id, printer->id, service_duration_ms);
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

static void publish_paper_empty(printer_t* printer, int job_id, unsigned long current_time_us)
{
    char buf[1024];
    double timestamp_ms = (current_time_us - reference_time_us) / 1000.0;
    sprintf(buf, "{\"type\":\"log\", \"data\":{\"timestamp\":%.3f, \"message\":\"printer%d does not have enough paper for job%d and is requesting refill\"}}",
        timestamp_ms, printer->id, job_id);
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

static void publish_paper_refill_start(printer_t* printer, int papers_needed,
    int time_to_refill_us, unsigned long current_time_us)
{
    char buf[1024];
    double timestamp_ms = (current_time_us - reference_time_us) / 1000.0;
    double refill_time_ms = time_to_refill_us / 1000.0;
    sprintf(buf, "{\"type\":\"log\", \"data\":{\"timestamp\":%.3f, \"message\":\"printer%d starts refilling %d papers, estimated time = %.3fms\"}}",
        timestamp_ms, printer->id, papers_needed, refill_time_ms);
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

static void publish_paper_refill_end(printer_t* printer, int refill_duration_us,
    unsigned long current_time_us)
{
    char buf[1024];
    double timestamp_ms = (current_time_us - reference_time_us) / 1000.0;
    double refill_time_ms = refill_duration_us / 1000.0;
    sprintf(buf, "{\"type\":\"log\", \"data\":{\"timestamp\":%.3f, \"message\":\"printer%d finishes refilling, actual time = %.3fms\"}}",
        timestamp_ms, printer->id, refill_time_ms);
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

static void publish_simulation_stopped(simulation_statistics_t* stats) {
    char buf[1024];
    reference_end_time_us = get_time_in_us();

    stats->simulation_duration_us = reference_end_time_us - reference_time_us;
    double timestamp_ms = (reference_end_time_us - reference_time_us) / 1000.0;
    double duration_ms = stats->simulation_duration_us / 1000.0;
    sprintf(buf, "{\"type\":\"log\", \"data\":{\"timestamp\":%.3f, \"message\":\"simulation stopped, duration = %.3fms\"}}",
        timestamp_ms, duration_ms);
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

static void publish_statistics(simulation_statistics_t* stats) {
    if (stats == NULL) return;

    char buf[4096];
    if (write_statistics_to_buffer(stats, buf, sizeof(buf)) > 0) {
        ws_bridge_send_json_from_any_thread(buf, strlen(buf));
    }
}

static void publish_scale_up(int new_printer_count, int queue_length, unsigned long current_time_us) {
    char buf[1024];
    
    double timestamp_ms = (current_time_us - reference_time_us) / 1000.0;
    sprintf(buf, "{\"type\":\"log\", \"data\":{\"timestamp\":%.3f, \"message\":\"Autoscaling: Scaled UP to %d printers (queue length: %d)\"}}",
        timestamp_ms, new_printer_count, queue_length);
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

static void publish_scale_down(int new_printer_count, int queue_length, unsigned long current_time_us) {
    char buf[1024];
    
    double timestamp_ms = (current_time_us - reference_time_us) / 1000.0;
    sprintf(buf, "{\"type\":\"log\", \"data\":{\"timestamp\":%.3f, \"message\":\"Autoscaling: Scaled DOWN to %d printers (queue length: %d)\"}}",
        timestamp_ms, new_printer_count, queue_length);
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

static void publish_printer_idle(const printer_t* printer, unsigned long current_time_us, int job_id) {
    char buf[1024];
    
    sprintf(buf, "{\"type\":\"consumer_update\", \"data\":{\"id\":%d, \"papersLeft\":%d, \"status\":\"idle\", \"currentJobId\":%d}}",
        printer->id, printer->current_paper_count, job_id);
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

static void publish_printer_busy(const printer_t* printer, unsigned long current_time_us, int job_id) {
    char buf[1024];

    sprintf(buf, "{\"type\":\"consumer_update\", \"data\":{\"id\":%d, \"papersLeft\":%d, \"status\":\"serving\", \"currentJobId\":%d}}",
        printer->id, printer->current_paper_count, job_id);
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

static void publish_stats_update(simulation_statistics_t* stats, int queue_length) {
    char buf[1024];
    
    sprintf(buf, "{\"type\":\"stats_update\", \"data\":{"
                 "\"jobsProcessed\":%.0f, "
                 "\"jobsReceived\":%.0f, "
                 "\"queueLength\":%d, "
                 "\"avgCompletionTime\":%.2f, "
                 "\"papersUsed\":%d, "
                 "\"refillEvents\":%.0f, "
                 "\"avgServiceTime\":%.2f}}",
            stats->total_jobs_served,
            stats->total_jobs_arrived,
            queue_length,
            calculate_average_system_time(stats),
            calculate_total_papers_used(stats),
            stats->paper_refill_events,
            calculate_overall_average_service_time(stats));
    
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}


// --- Public API Function Implementations ---
void websocket_handler_register(void) {
    static const log_ops_t ops = {
        .simulation_parameters = NULL,
        .simulation_start = publish_simulation_start,
        .simulation_end = publish_simulation_end,
        .system_arrival = publish_system_arrival,
        .dropped_job = publish_dropped_job,
        .removed_job = publish_removed_job,
        .queue_arrival = publish_queue_arrival,
        .queue_departure = publish_queue_departure,
        .job_update = publish_job_update,
        .printer_arrival = publish_printer_arrival,
        .system_departure = publish_system_departure,
        .paper_empty = publish_paper_empty,
        .paper_refill_start = publish_paper_refill_start,
        .paper_refill_end = publish_paper_refill_end,
        .scale_up = publish_scale_up,
        .scale_down = publish_scale_down,
        .printer_idle = publish_printer_idle,
        .printer_busy = publish_printer_busy,
        .stats_update = publish_stats_update,
        .simulation_stopped = publish_simulation_stopped,
        .statistics = publish_statistics,
    };
    log_router_register_websocket_handler(&ops);
}
