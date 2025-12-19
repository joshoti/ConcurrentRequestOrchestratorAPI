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
    void (*printer_idle)(const struct printer* printer, unsigned long current_time_us);
    void (*printer_busy)(const struct printer* printer, unsigned long current_time_us);

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
void emit_simulation_parameters(const struct simulation_parameters* params);
void emit_simulation_start(struct simulation_statistics* stats);
void emit_simulation_end(struct simulation_statistics* stats);
void emit_system_arrival(struct job* job, unsigned long previous_job_arrival_time_us,
                         struct simulation_statistics* stats);
void emit_dropped_job(struct job* job, unsigned long previous_job_arrival_time_us,
                      struct simulation_statistics* stats);
void emit_removed_job(struct job* job);
void emit_queue_arrival(const struct job* job, struct simulation_statistics* stats,
                        struct timed_queue* job_queue, unsigned long last_interaction_time_us);
void emit_queue_departure(const struct job* job, struct simulation_statistics* stats,
                          struct timed_queue* job_queue, unsigned long last_interaction_time_us);
void emit_printer_arrival(const struct job* job, const struct printer* printer);
void emit_system_departure(const struct job* job, const struct printer* printer,
                           struct simulation_statistics* stats);
void emit_paper_empty(struct printer* printer, int job_id, unsigned long current_time_us);
void emit_paper_refill_start(struct printer* printer, int papers_needed,
                             int time_to_refill_us, unsigned long current_time_us);
void emit_paper_refill_end(struct printer* printer, int refill_duration_us,
                           unsigned long current_time_us);
void emit_scale_up(int new_printer_count, int queue_length, unsigned long current_time_us);
void emit_scale_down(int new_printer_count, int queue_length, unsigned long current_time_us);
void emit_printer_idle(const struct printer* printer, unsigned long current_time_us);
void emit_printer_busy(const struct printer* printer, unsigned long current_time_us);
void emit_simulation_stopped(struct simulation_statistics* stats);
void emit_statistics(struct simulation_statistics* stats);

#endif // LOG_ROUTER_H
