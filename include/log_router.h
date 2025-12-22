#ifndef LOG_ROUTER_H
#define LOG_ROUTER_H

#include <stddef.h>

// Forward decls to avoid pulling in all headers here
struct job;
struct printer;
struct simulation_parameters;
struct simulation_statistics;
struct timed_queue;

// Output modes
#define LOG_MODE_TERMINAL 0
#define LOG_MODE_SERVER   1

// Unified logging operations vtable
typedef struct log_ops {
    void (*simulation_parameters)(const struct simulation_parameters* params);
    void (*simulation_start)(struct simulation_statistics* stats);
    void (*simulation_end)(struct simulation_statistics* stats);

    void (*system_arrival)(struct job* job, unsigned long previous_job_arrival_time_us,
                           struct simulation_statistics* stats);
    void (*dropped_job)(struct job* job, unsigned long previous_job_arrival_time_us,
                        struct simulation_statistics* stats);
    void (*removed_job)(struct job* job);

    void (*queue_arrival)(const struct job* job, struct simulation_statistics* stats,
                          struct timed_queue* job_queue, unsigned long last_interaction_time_us);
    void (*queue_departure)(const struct job* job, struct simulation_statistics* stats,
                            struct timed_queue* job_queue, unsigned long last_interaction_time_us);
    void (*job_update)(const struct job* job);

    void (*printer_arrival)(const struct job* job, const struct printer* printer);
    void (*system_departure)(const struct job* job, const struct printer* printer,
                             struct simulation_statistics* stats);

    void (*paper_empty)(struct printer* printer, int job_id, unsigned long current_time_us);
    void (*paper_refill_start)(struct printer* printer, int papers_needed,
                               int time_to_refill_us, unsigned long current_time_us);
    void (*paper_refill_end)(struct printer* printer, int refill_duration_us,
                             unsigned long current_time_us);

    void (*scale_up)(int new_printer_count, int queue_length, unsigned long current_time_us);
    void (*scale_down)(int new_printer_count, int queue_length, unsigned long current_time_us);
    void (*printer_idle)(const struct printer* printer);
    void (*printer_busy)(const struct printer* printer, int job_id);
    void (*printer_waiting_refill)(const struct printer* printer);
    void (*stats_update)(struct simulation_statistics* stats, int queue_length);

    void (*simulation_stopped)(struct simulation_statistics* stats);
    void (*statistics)(struct simulation_statistics* stats);
} log_ops_t;

// Global active logger backend pointer bound via set_log_mode
extern const log_ops_t* logger;

// Bind mode and select an already-registered handler (console/websocket)
void set_log_mode(int mode);

/*
 * Allow CLI/server to register their respective handlers without creating
 * link-time dependencies in the router.
 */
void log_router_register_console_handler(const log_ops_t* ops);
void log_router_register_websocket_handler(const log_ops_t* ops);

// --- Wrapper API that routes to stdout or websocket ---

/**
 * @brief Emits the simulation parameters at the start of the simulation.
 * Routes to the active logger (console or websocket).
 * 
 * @param params The simulation parameters to emit.
 */
void emit_simulation_parameters(const struct simulation_parameters* params);

/**
 * @brief Emits the start of the simulation.
 * Routes to the active logger (console or websocket).
 * 
 * @param stats The simulation statistics to update.
 */
void emit_simulation_start(struct simulation_statistics* stats);

/**
 * @brief Emits the end of the simulation.
 * Routes to the active logger (console or websocket).
 * 
 * @param stats The simulation statistics to update.
 */
void emit_simulation_end(struct simulation_statistics* stats);

/**
 * @brief Emits an event when a new job is created in the system.
 * Routes to the active logger (console or websocket).
 *
 * @param job The job that has been created.
 * @param previous_job_arrival_time_us The arrival time of the previous job in microseconds.
 * @param stats The simulation statistics to update.
 */
void emit_system_arrival(struct job* job, unsigned long previous_job_arrival_time_us,
                         struct simulation_statistics* stats);

/**
 * @brief Emits an event when a job is dropped from the system.
 * Routes to the active logger (console or websocket).
 *
 * @param job The job that has been dropped.
 * @param previous_job_arrival_time_us The arrival time of the previous job in microseconds.
 * @param stats The simulation statistics to update.
 */
void emit_dropped_job(struct job* job, unsigned long previous_job_arrival_time_us,
                      struct simulation_statistics* stats);

/**
 * @brief Emits an event when a job is removed from the system without being processed.
 * Routes to the active logger (console or websocket).
 *
 * @param job The job that has been removed.
 */
void emit_removed_job(struct job* job);

/**
 * @brief Emits an event when a job arrives at the queue.
 * Routes to the active logger (console or websocket).
 * 
 * @param job The job that has arrived at the queue.
 * @param stats The simulation statistics to update.
 * @param job_queue The job queue to check the length of.
 * @param last_interaction_time_us The last interaction time of the queue in microseconds.
 */
void emit_queue_arrival(const struct job* job, struct simulation_statistics* stats,
                        struct timed_queue* job_queue, unsigned long last_interaction_time_us);

/**
 * @brief Emits an event when a job departs from the queue.
 * Routes to the active logger (console or websocket).
 *
 * @param job The job that has departed from the queue.
 * @param stats The simulation statistics to update.
 * @param job_queue The job queue to check the length of.
 * @param last_interaction_time_us The last interaction time of the queue in microseconds.
 */
void emit_queue_departure(const struct job* job, struct simulation_statistics* stats,
                          struct timed_queue* job_queue, unsigned long last_interaction_time_us);

/**
 * @brief Emits a job state update for real-time frontend synchronization.
 * Routes to the active logger (console or websocket).
 * 
 * @param job The job to broadcast (id and papers required).
 */
void emit_job_update(const struct job* job);

/**
 * @brief Emits an event when a job arrives at a printer for processing.
 * Routes to the active logger (console or websocket).
 *
 * @param job The job that has arrived at the printer.
 * @param printer The printer that the job has arrived at.
 */
void emit_printer_arrival(const struct job* job, const struct printer* printer);

/**
 * @brief Emits an event when a job departs from a printer after processing.
 * Routes to the active logger (console or websocket).
 * 
 * @param job The job that has departed from the printer.
 * @param printer The printer that the job has departed from.
 * @param stats The simulation statistics to update.
 */
void emit_system_departure(const struct job* job, const struct printer* printer,
                           struct simulation_statistics* stats);

/**
 * @brief Emits an event when a printer runs out of paper.
 * Routes to the active logger (console or websocket).
 *
 * @param printer The printer that has run out of paper.
 * @param job_id The id of the job that cannot be processed due to lack of paper.
 * @param current_time_us The current simulation time in microseconds.
 */
void emit_paper_empty(struct printer* printer, int job_id, unsigned long current_time_us);

/**
 * @brief Emits an event when a printer starts refilling paper.
 * Routes to the active logger (console or websocket).
 *
 * @param printer The printer that is starting to refill paper.
 * @param papers_needed The number of papers needed to refill the printer to full capacity.
 * @param time_to_refill_us The time it will take to refill the printer in microseconds.
 * @param current_time_us The current simulation time in microseconds.
 */
void emit_paper_refill_start(struct printer* printer, int papers_needed,
                             int time_to_refill_us, unsigned long current_time_us);

/**
 * @brief Emits an event when a printer finishes refilling paper.
 * Routes to the active logger (console or websocket).
 *
 * @param printer The printer that has finished refilling paper.
 * @param refill_duration_us The duration of the refill in microseconds.
 * @param current_time_us The current simulation time in microseconds.
 */
void emit_paper_refill_end(struct printer* printer, int refill_duration_us,
                           unsigned long current_time_us);

/**
 * @brief Emits an event when the system scales up (adds a printer).
 * Routes to the active logger (console or websocket).
 * 
 * @param new_printer_count The new total number of active printers.
 * @param queue_length The current queue length that triggered the scale-up.
 * @param current_time_us The current simulation time in microseconds.
 */
void emit_scale_up(int new_printer_count, int queue_length, unsigned long current_time_us);

/**
 * @brief Emits an event when the system scales down (removes a printer).
 * Routes to the active logger (console or websocket).
 * 
 * @param new_printer_count The new total number of active printers.
 * @param queue_length The current queue length that triggered the scale-down.
 * @param current_time_us The current simulation time in microseconds.
 */
void emit_scale_down(int new_printer_count, int queue_length, unsigned long current_time_us);

/**
 * @brief Emits an event when a printer becomes idle.
 * Routes to the active logger (console or websocket).
 * 
 * @param printer The printer that is now idle.
 */
void emit_printer_idle(const struct printer* printer);

/**
 * @brief Emits an event when a printer becomes busy.
 * Routes to the active logger (console or websocket).
 * 
 * @param printer The printer that is now busy.
 * @param job_id The id of the job being served.
 */
void emit_printer_busy(const struct printer* printer, int job_id);

/**
 * @brief Emits an event when a printer is waiting for a paper refill.
 * Routes to the active logger (console or websocket).
 * 
 * @param printer The printer that is waiting for refill.
 */
void emit_printer_waiting_refill(const struct printer* printer);

/**
 * @brief Emits real-time statistics update for frontend dashboard.
 * Routes to the active logger (console or websocket).
 * 
 * @param stats The simulation statistics to broadcast.
 * @param queue_length The current queue length.
 */
void emit_stats_update(struct simulation_statistics* stats, int queue_length);

/**
 * @brief Emits an event when the simulation is stopped prematurely (e.g., Ctrl+C).
 * Routes to the active logger (console or websocket).
 * 
 * @param stats The simulation statistics to finalize.
 */
void emit_simulation_stopped(struct simulation_statistics* stats);

/**
 * @brief Emits the final comprehensive statistics at simulation end.
 * Routes to the active logger (console or websocket).
 * 
 * @param stats The simulation statistics to report.
 */
void emit_statistics(struct simulation_statistics* stats);

#endif // LOG_ROUTER_H
