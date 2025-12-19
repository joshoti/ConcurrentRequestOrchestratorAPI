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

/**
 * @brief Writes to buffer the given time in milliseconds (ms) relative to the given reference
 * time.
 * 
 * @param time_us The time to log, in microseconds (us).
 * @param reference_time_us The reference time to log against, in microseconds (us).
 * @param buf The buffer to write the formatted time string into.
 *
 * Sample output called 250ms (250,000us) after reference time is "00000251.457ms: "
 */
static void write_time_to_buffer(unsigned long time_us, unsigned long reference_time_us, char* buf) {
    time_us -= reference_time_us;

    int milliseconds = 0;
    int microseconds = 0;
    time_in_us_to_ms(time_us, &milliseconds, &microseconds);
    sprintf(buf, time_format, milliseconds, microseconds);
}

void publish_simulation_parameters(const simulation_parameters_t* params) {
    char buf[1024];
    sprintf(buf, "{\"type\":\"params\", \"params\": {\"job_arrival_time\":%.6g,\
        \"printing_rate\":%.6g, \"queue_capacity\":%d,\
        \"printer_paper_capacity\":%d, \"refill_rate\":%.6g, \"num_jobs\":%d,\
        \"papers_required_lower_bound\":%d, \"papers_required_upper_bound\":%d}}",
            params->job_arrival_time_us / 1000.0, params->printing_rate, params->queue_capacity,
            params->printer_paper_capacity, params->refill_rate, params->num_jobs,
            params->papers_required_lower_bound, params->papers_required_upper_bound);
    // sending via bridge on the Mongoose loop
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

void publish_simulation_start(simulation_statistics_t* stats) {
    reference_time_us = get_time_in_us();
    stats->simulation_start_time_us = reference_time_us;
    char time_buf[64];
    char buf[1024];

    write_time_to_buffer(reference_time_us, reference_time_us, time_buf);
    sprintf(buf, "{\"type\":\"log\", \"message\":\"%s simulation begins\"}", time_buf);
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

void publish_simulation_end(simulation_statistics_t* stats)
{
    reference_end_time_us = get_time_in_us();
    char time_buf[64];
    char buf[1024];

    write_time_to_buffer(reference_end_time_us, reference_time_us, time_buf);
    stats->simulation_duration_us = reference_end_time_us - reference_time_us;
    sprintf(buf, "{\"type\":\"log\", \"message\":\"%s simulation ends, duration = %lu.%03lums\"}",
        time_buf, stats->simulation_duration_us / 1000, stats->simulation_duration_us % 1000);
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
    char time_buf[64];
    char buf[1024];

    write_time_to_buffer(current_job_arrival_time_us, reference_time_us, time_buf);

    int inter_arrival_time_us = current_job_arrival_time_us - previous_job_arrival_time_us;
    int time_in_ms = inter_arrival_time_us / 1000;
    int time_in_us = inter_arrival_time_us % 1000;
    sprintf(buf, "{\"type\":\"log\", \"message\":\"%s job%d arrives, needs %d paper%s, inter-arrival time = %d.%03dms%s\"}",
        time_buf, job_id, papers_required,
        papers_required == 1 ? "" : "s", time_in_ms, time_in_us,
        is_dropped ? ", dropped" : ""
    );
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

void publish_system_arrival(job_t* job, unsigned long previous_job_arrival_time_us,
    simulation_statistics_t* stats)
{
    stats->total_jobs_arrived++; // stats: total jobs arrived
    stats->total_inter_arrival_time_us += job->system_arrival_time_us - previous_job_arrival_time_us; // stats: avg job inter-arrival time
    job_arrival_helper(job->id, job->papers_required,
        previous_job_arrival_time_us, job->system_arrival_time_us, FALSE
    );
}

void publish_dropped_job(job_t* job, unsigned long previous_job_arrival_time_us,
    simulation_statistics_t* stats)
{
    stats->total_jobs_dropped++; // stats: total jobs dropped
    job_arrival_helper(job->id, job->papers_required,
        previous_job_arrival_time_us, job->system_arrival_time_us, TRUE
    );
}

void publish_removed_job(job_t* job) {
    unsigned long current_time_us = get_time_in_us();
    char time_buf[64];
    char buf[1024];

    write_time_to_buffer(current_time_us, reference_time_us, time_buf);
    sprintf(buf, "{\"type\":\"log\", \"message\":\"%s job%d removed from system\"}",
        time_buf, job->id);
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

void publish_queue_arrival(const job_t* job, simulation_statistics_t* stats,
    timed_queue_t* job_queue, unsigned long last_interaction_time_us)
{
    stats->area_num_in_job_queue_us +=
        (job->queue_arrival_time_us - last_interaction_time_us) * // duration of previous state
        (timed_queue_length(job_queue) - 1); // -1 for the job that just entered the queue
    // stats: avg job queue length
    job_queue->last_interaction_time_us = job->queue_arrival_time_us;

    char time_buf[64];
    char buf[1024];
    write_time_to_buffer(job->queue_arrival_time_us, reference_time_us, time_buf);

    sprintf(buf, "{\"type\":\"log\", \"message\":\"%s job%d enters queue, queue length = %d\"}",
        time_buf, job->id, timed_queue_length(job_queue));
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

void publish_queue_departure(const job_t* job, simulation_statistics_t* stats,
    timed_queue_t* job_queue, unsigned long last_interaction_time_us)
{
    stats->area_num_in_job_queue_us +=
        (job->queue_departure_time_us - last_interaction_time_us) * // duration of previous state
        (timed_queue_length(job_queue) + 1); // +1 for the job that just left the queue
    // stats: avg job queue length
    job_queue->last_interaction_time_us = job->queue_departure_time_us;

    char time_buf[64];
    char buf[1024];
    write_time_to_buffer(job->queue_departure_time_us, reference_time_us, time_buf);

    int queue_duration = job->queue_departure_time_us - job->queue_arrival_time_us;
    int time_ms = queue_duration / 1000;
    int time_us = queue_duration % 1000;
    sprintf(buf, "{\"type\":\"log\", \"message\":\"%s job%d leaves queue, time in queue = %d.%03dms, queue_length = %d\"}",
        time_buf, job->id, time_ms, time_us, timed_queue_length(job_queue));
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

void publish_printer_arrival(const job_t* job, const printer_t* printer)
{
    char time_buf[64];
    char buf[1024];
    write_time_to_buffer(job->service_arrival_time_us, reference_time_us, time_buf);
    sprintf(buf, "{\"type\":\"log\", \"message\":\"%s job%d begins service at printer%d, printing %d pages in about %dms\"}",
        time_buf, job->id, printer->id, job->papers_required, job->service_time_requested_ms);
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

void publish_system_departure(const job_t* job, const printer_t* printer,
    simulation_statistics_t* stats)
{
    char time_buf[64];
    char buf[1024];
    write_time_to_buffer(job->service_departure_time_us, reference_time_us, time_buf);

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

    int time_ms = service_duration / 1000;
    int time_us = service_duration % 1000;
    sprintf(buf, "{\"type\":\"log\", \"message\":\"%s job%d departs from printer%d, service time = %d.%03dms\"}",
        time_buf, job->id, printer->id, time_ms, time_us);
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

void publish_paper_empty(printer_t* printer, int job_id, unsigned long current_time_us)
{
    char time_buf[64];
    char buf[1024];
    write_time_to_buffer(current_time_us, reference_time_us, time_buf);
    sprintf(buf, "{\"type\":\"log\", \"message\":\"%s printer%d does not have enough paper for job%d and is requesting refill\"}",
        time_buf, printer->id, job_id);
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

void publish_paper_refill_start(printer_t* printer, int papers_needed,
    int time_to_refill_us, unsigned long current_time_us)
{
    char time_buf[64];
    char buf[1024];
    write_time_to_buffer(current_time_us, reference_time_us, time_buf);
    int time_ms = time_to_refill_us / 1000;
    int time_us = time_to_refill_us % 1000;
    sprintf(buf, "{\"type\":\"log\", \"message\":\"%s printer%d starts refilling %d papers, estimated time = %d.%03dms\"}",
        time_buf, printer->id, papers_needed, time_ms, time_us);
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

void publish_paper_refill_end(printer_t* printer, int refill_duration_us,
    unsigned long current_time_us)
{
    char time_buf[64];
    char buf[1024];
    write_time_to_buffer(current_time_us, reference_time_us, time_buf);
    int time_ms = refill_duration_us / 1000;
    int time_us = refill_duration_us % 1000;
    sprintf(buf, "{\"type\":\"log\", \"message\":\"%s printer%d finishes refilling, actual time = %d.%03dms\"}",
        time_buf, printer->id, time_ms, time_us);
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

void publish_simulation_stopped(simulation_statistics_t* stats) {
    char time_buf[64];
    char buf[1024];
    reference_end_time_us = get_time_in_us();

    write_time_to_buffer(reference_end_time_us, reference_time_us, time_buf);
    stats->simulation_duration_us = reference_end_time_us - reference_time_us;
    sprintf(buf, "{\"type\":\"log\", \"message\":\"%s simulation stopped, duration = %lu.%03lums\"}",
        time_buf, stats->simulation_duration_us / 1000, stats->simulation_duration_us % 1000);
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

void publish_statistics(simulation_statistics_t* stats) {
    if (stats == NULL) return;

    char buf[4096];
    if (write_statistics_to_buffer(stats, buf, sizeof(buf)) > 0) {
        ws_bridge_send_json_from_any_thread(buf, strlen(buf));
    }
}

void publish_scale_up(int new_printer_count, int queue_length, unsigned long current_time_us) {
    char time_buf[64];
    char buf[1024];
    
    write_time_to_buffer(current_time_us, reference_time_us, time_buf);
    sprintf(buf, "{\"type\":\"autoscale\", \"action\":\"scale_up\", \"time\":\"%s\", \
\"printer_count\":%d, \"queue_length\":%d, \"message\":\"%s Autoscaling: Scaled UP to %d printers (queue length: %d)\"}",
        time_buf, new_printer_count, queue_length, time_buf, new_printer_count, queue_length);
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

void publish_scale_down(int new_printer_count, int queue_length, unsigned long current_time_us) {
    char time_buf[64];
    char buf[1024];
    
    write_time_to_buffer(current_time_us, reference_time_us, time_buf);
    sprintf(buf, "{\"type\":\"autoscale\", \"action\":\"scale_down\", \"time\":\"%s\", \
\"printer_count\":%d, \"queue_length\":%d, \"message\":\"%s Autoscaling: Scaled DOWN to %d printers (queue length: %d)\"}",
        time_buf, new_printer_count, queue_length, time_buf, new_printer_count, queue_length);
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

void publish_printer_idle(const printer_t* printer, unsigned long current_time_us) {
    char time_buf[64];
    char buf[1024];
    
    write_time_to_buffer(current_time_us, reference_time_us, time_buf);
    sprintf(buf, "{\"type\":\"printer_status\", \"printer_id\":%d, \"status\":\"idle\", \
\"time\":\"%s\", \"message\":\"%s printer%d is now idle\"}",
        printer->id, time_buf, time_buf, printer->id);
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

void publish_printer_busy(const printer_t* printer, unsigned long current_time_us) {
    char time_buf[64];
    char buf[1024];
    
    write_time_to_buffer(current_time_us, reference_time_us, time_buf);
    sprintf(buf, "{\"type\":\"printer_status\", \"printer_id\":%d, \"status\":\"busy\", \
\"time\":\"%s\", \"message\":\"%s printer%d is now busy\"}",
        printer->id, time_buf, time_buf, printer->id);
    ws_bridge_send_json_from_any_thread(buf, strlen(buf));
}

void websocket_handler_register(void) {
    static const log_ops_t ops = {
        .simulation_parameters = publish_simulation_parameters,
        .simulation_start = publish_simulation_start,
        .simulation_end = publish_simulation_end,
        .system_arrival = publish_system_arrival,
        .dropped_job = publish_dropped_job,
        .removed_job = publish_removed_job,
        .queue_arrival = publish_queue_arrival,
        .queue_departure = publish_queue_departure,
        .printer_arrival = publish_printer_arrival,
        .system_departure = publish_system_departure,
        .paper_empty = publish_paper_empty,
        .paper_refill_start = publish_paper_refill_start,
        .paper_refill_end = publish_paper_refill_end,
        .scale_up = publish_scale_up,
        .scale_down = publish_scale_down,
        .printer_idle = publish_printer_idle,
        .printer_busy = publish_printer_busy,
        .simulation_stopped = publish_simulation_stopped,
        .statistics = publish_statistics,
    };
    log_router_register_websocket_handler(&ops);
}
