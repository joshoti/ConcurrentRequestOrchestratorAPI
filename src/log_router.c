#include "log_router.h"

static int log_mode = LOG_MODE_TERMINAL;

// Registered handlers provided by CLI/server at startup
static const log_ops_t* s_console_handler = NULL;
static const log_ops_t* s_websocket_handler = NULL;

// Active backend pointer
const log_ops_t* logger = NULL;

void log_router_register_console_handler(const log_ops_t* ops) {
    s_console_handler = ops;
}

void log_router_register_websocket_handler(const log_ops_t* ops) {
    s_websocket_handler = ops;
}

void set_log_mode(int mode) {
    log_mode = mode;
    if (log_mode == LOG_MODE_SERVER) {
        logger = s_websocket_handler;
    } else {
        logger = s_console_handler;
    }
}

static inline int has(const void* fn) { return fn != NULL; }

void emit_simulation_parameters(const struct simulation_parameters* params) {
    if (logger && has(logger->simulation_parameters)) logger->simulation_parameters(params);
}

void emit_simulation_start(struct simulation_statistics* stats) {
    if (logger && has(logger->simulation_start)) logger->simulation_start(stats);
}

void emit_simulation_end(struct simulation_statistics* stats) {
    if (logger && has(logger->simulation_end)) logger->simulation_end(stats);
}

void emit_system_arrival(struct job* job, unsigned long previous_job_arrival_time_us,
                         struct simulation_statistics* stats) {
    if (logger && has(logger->system_arrival)) logger->system_arrival(job, previous_job_arrival_time_us, stats);
}

void emit_dropped_job(struct job* job, unsigned long previous_job_arrival_time_us,
                      struct simulation_statistics* stats) {
    if (logger && has(logger->dropped_job)) logger->dropped_job(job, previous_job_arrival_time_us, stats);
}

void emit_removed_job(struct job* job) {
    if (logger && has(logger->removed_job)) logger->removed_job(job);
}

void emit_queue_arrival(const struct job* job, struct simulation_statistics* stats,
                        struct timed_queue* job_queue, unsigned long last_interaction_time_us) {
    if (logger && has(logger->queue_arrival)) logger->queue_arrival(job, stats, job_queue, last_interaction_time_us);
}

void emit_queue_departure(const struct job* job, struct simulation_statistics* stats,
                          struct timed_queue* job_queue, unsigned long last_interaction_time_us) {
    if (logger && has(logger->queue_departure)) logger->queue_departure(job, stats, job_queue, last_interaction_time_us);
}

void emit_job_update(const struct job* job) {
    if (logger && has(logger->job_update)) logger->job_update(job);
}

void emit_printer_arrival(const struct job* job, const struct printer* printer) {
    if (logger && has(logger->printer_arrival)) logger->printer_arrival(job, printer);
}

void emit_system_departure(const struct job* job, const struct printer* printer,
                           struct simulation_statistics* stats) {
    if (logger && has(logger->system_departure)) logger->system_departure(job, printer, stats);
}

void emit_paper_empty(struct printer* printer, int job_id, unsigned long current_time_us) {
    if (logger && has(logger->paper_empty)) logger->paper_empty(printer, job_id, current_time_us);
}

void emit_paper_refill_start(struct printer* printer, int papers_needed,
                             int time_to_refill_us, unsigned long current_time_us) {
    if (logger && has(logger->paper_refill_start)) logger->paper_refill_start(printer, papers_needed, time_to_refill_us, current_time_us);
}

void emit_paper_refill_end(struct printer* printer, int refill_duration_us,
                           unsigned long current_time_us) {
    if (logger && has(logger->paper_refill_end)) logger->paper_refill_end(printer, refill_duration_us, current_time_us);
}

void emit_scale_up(int new_printer_count, int queue_length, unsigned long current_time_us) {
    if (logger && has(logger->scale_up)) logger->scale_up(new_printer_count, queue_length, current_time_us);
}

void emit_scale_down(int new_printer_count, int queue_length, unsigned long current_time_us) {
    if (logger && has(logger->scale_down)) logger->scale_down(new_printer_count, queue_length, current_time_us);
}

void emit_printer_idle(const struct printer* printer) {
    if (logger && has(logger->printer_idle)) logger->printer_idle(printer);
}

void emit_printer_busy(const struct printer* printer, int job_id) {
    if (logger && has(logger->printer_busy)) logger->printer_busy(printer, job_id);
}

void emit_printer_waiting_refill(const struct printer* printer) {
    if (logger && has(logger->printer_waiting_refill)) logger->printer_waiting_refill(printer);
}

void emit_stats_update(struct simulation_statistics* stats, int queue_length) {
    if (logger && has(logger->stats_update)) logger->stats_update(stats, queue_length);
}

void emit_simulation_stopped(struct simulation_statistics* stats) {
    if (logger && has(logger->simulation_stopped)) logger->simulation_stopped(stats);
}

void emit_statistics(struct simulation_statistics* stats) {
    if (logger && has(logger->statistics)) logger->statistics(stats);
}
