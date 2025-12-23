// Mongoose-based websocket server that drives the print simulation.
// Websocket endpoint accepts text frames: "start", "stop", "status".

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "config.h"
#include "mongoose.h"
#include "preprocessing.h"
#include "linked_list.h"
#include "timed_queue.h"
#include "job_receiver.h"
#include "paper_refiller.h"
#include "printer.h"
#include "autoscaling.h"
#include "websocket_handler.h"
#include "ws_bridge.h"
#include "log_router.h"
#include "simulation_stats.h"
#include "signalcatcher.h"

// Default listen address and websocket paths
static const char *s_listen_on = "http://127.0.0.1:8000";
static const char *s_ws_path_primary = "/ws/simulation";
static const char *s_web_root = "./tests";

// Mongoose manager and active websocket tracking
static struct mg_mgr g_mgr; // used for mg_wakeup
static pthread_mutex_t g_ws_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned long g_ws_conn_id = 0; // 0 means none

extern int g_debug;
extern int g_terminate_now;

typedef struct simulation_context {
	// Threads
	printer_pool_t printer_pool;
	pthread_t autoscaling_thread;
	pthread_t job_receiver_thread;
	pthread_t paper_refill_thread;
	pthread_t simulation_runner_thread; // background wrapper

	// Sync primitives
	pthread_mutex_t job_queue_mutex;
	pthread_mutex_t paper_refill_queue_mutex;
	pthread_mutex_t stats_mutex;
	pthread_mutex_t simulation_state_mutex;
	pthread_cond_t job_queue_not_empty_cv;
	pthread_cond_t refill_needed_cv;
	pthread_cond_t refill_supplier_cv;

	// State
	simulation_parameters_t params;
	simulation_statistics_t stats;
	int all_jobs_arrived;
	int all_jobs_served;
	timed_queue_t job_queue;
	linked_list_t paper_refill_queue;

	// Args
	job_thread_args_t job_receiver_args;
	autoscaling_thread_args_t autoscaling_args;
	paper_refill_thread_args_t paper_refill_args;

	// Control
	int is_running;
} simulation_context_t;

static simulation_context_t g_ctx; // single simulation instance
static pthread_mutex_t g_server_state_mutex = PTHREAD_MUTEX_INITIALIZER;

static void init_context(simulation_context_t* ctx) {
	memset(ctx, 0, sizeof(*ctx));
	ctx->params = (simulation_parameters_t)SIMULATION_DEFAULT_PARAMS_HIGH_LOAD;
	ctx->stats = (simulation_statistics_t){0};
	ctx->all_jobs_arrived = 0;
	ctx->all_jobs_served = 0;
	ctx->is_running = 0;

	pthread_mutex_init(&ctx->job_queue_mutex, NULL);
	pthread_mutex_init(&ctx->paper_refill_queue_mutex, NULL);
	pthread_mutex_init(&ctx->stats_mutex, NULL);
	pthread_mutex_init(&ctx->simulation_state_mutex, NULL);
	pthread_cond_init(&ctx->job_queue_not_empty_cv, NULL);
	pthread_cond_init(&ctx->refill_needed_cv, NULL);
	pthread_cond_init(&ctx->refill_supplier_cv, NULL);

	timed_queue_init(&ctx->job_queue);
	list_init(&ctx->paper_refill_queue);
}

static void destroy_context(simulation_context_t* ctx) {
	pthread_mutex_destroy(&ctx->job_queue_mutex);
	pthread_mutex_destroy(&ctx->paper_refill_queue_mutex);
	pthread_mutex_destroy(&ctx->stats_mutex);
	pthread_mutex_destroy(&ctx->simulation_state_mutex);
	pthread_cond_destroy(&ctx->job_queue_not_empty_cv);
	pthread_cond_destroy(&ctx->refill_needed_cv);
	pthread_cond_destroy(&ctx->refill_supplier_cv);
}

static void* simulation_runner(void* arg) {
    if (g_debug) printf("Simulation runner thread started\n");
	simulation_context_t* ctx = (simulation_context_t*)arg;

	// Prepare thread args
	job_thread_args_t job_receiver_args = {
		.job_queue_mutex = &ctx->job_queue_mutex,
		.stats_mutex = &ctx->stats_mutex,
		.simulation_state_mutex = &ctx->simulation_state_mutex,
		.job_queue_not_empty_cv = &ctx->job_queue_not_empty_cv,
		.job_queue = &ctx->job_queue,
		.simulation_params = &ctx->params,
		.stats = &ctx->stats,
		.all_jobs_arrived = &ctx->all_jobs_arrived
	};
	ctx->job_receiver_args = job_receiver_args;

	// Initialize printer pool
	printer_pool_init(&ctx->printer_pool, ctx->params.consumer_count, ctx->params.printer_paper_capacity);

	// Shared args template for all printers
	printer_thread_args_t shared_printer_args = {
		.paper_refill_queue_mutex = &ctx->paper_refill_queue_mutex,
		.job_queue_mutex = &ctx->job_queue_mutex,
		.stats_mutex = &ctx->stats_mutex,
		.simulation_state_mutex = &ctx->simulation_state_mutex,
		.job_queue_not_empty_cv = &ctx->job_queue_not_empty_cv,
		.refill_needed_cv = &ctx->refill_needed_cv,
		.refill_supplier_cv = &ctx->refill_supplier_cv,
		.paper_refill_thread = &ctx->paper_refill_thread,
		.job_queue = &ctx->job_queue,
		.paper_refill_queue = &ctx->paper_refill_queue,
		.params = &ctx->params,
		.stats = &ctx->stats,
		.all_jobs_served = &ctx->all_jobs_served,
		.all_jobs_arrived = &ctx->all_jobs_arrived,
		.printer = NULL // Will be set by printer_pool_start_printer
	};

	// Autoscaling args
	autoscaling_thread_args_t autoscaling_args = {
		.pool = &ctx->printer_pool,
		.job_queue_mutex = &ctx->job_queue_mutex,
		.stats_mutex = &ctx->stats_mutex,
		.simulation_state_mutex = &ctx->simulation_state_mutex,
		.job_queue_not_empty_cv = &ctx->job_queue_not_empty_cv,
		.refill_needed_cv = &ctx->refill_needed_cv,
		.refill_supplier_cv = &ctx->refill_supplier_cv,
		.paper_refill_thread = &ctx->paper_refill_thread,
		.job_queue = &ctx->job_queue,
		.paper_refill_queue = &ctx->paper_refill_queue,
		.params = &ctx->params,
		.stats = &ctx->stats,
		.all_jobs_served = &ctx->all_jobs_served,
		.all_jobs_arrived = &ctx->all_jobs_arrived
	};
	ctx->autoscaling_args = autoscaling_args;

	paper_refill_thread_args_t paper_refill_args = {
		.paper_refill_queue_mutex = &ctx->paper_refill_queue_mutex,
		.stats_mutex = &ctx->stats_mutex,
		.simulation_state_mutex = &ctx->simulation_state_mutex,
		.refill_needed_cv = &ctx->refill_needed_cv,
		.refill_supplier_cv = &ctx->refill_supplier_cv,
		.paper_refill_queue = &ctx->paper_refill_queue,
		.job_queue = &ctx->job_queue,
		.params = &ctx->params,
		.stats = &ctx->stats,
		.all_jobs_served = &ctx->all_jobs_served
	};
	ctx->paper_refill_args = paper_refill_args;

	// Start of simulation logging
	emit_simulation_parameters(&ctx->params);
	emit_simulation_start(&ctx->stats);

	// Create threads
	pthread_create(&ctx->job_receiver_thread, NULL, job_receiver_thread_func, &ctx->job_receiver_args);
	pthread_create(&ctx->paper_refill_thread, NULL, paper_refill_thread_func, &ctx->paper_refill_args);

	// Start initial printers
	for (int i = 1; i <= ctx->params.consumer_count; i++) {
		printer_pool_start_printer(&ctx->printer_pool, i, &shared_printer_args);
	}

	// Autoscaling thread (if enabled)
	if (ctx->params.auto_scaling) {
		pthread_create(&ctx->autoscaling_thread, NULL, autoscaling_thread_func, &ctx->autoscaling_args);
		if (g_debug) printf("Autoscaling enabled\n");
	}

	// Join threads
	pthread_join(ctx->job_receiver_thread, NULL);
	if (g_debug) printf("job_receiver_thread joined\n");

	printer_pool_join_all(&ctx->printer_pool);
	if (g_debug) printf("all printer threads joined\n");

	pthread_join(ctx->paper_refill_thread, NULL);
	if (g_debug) printf("paper_refill_thread joined\n");

	if (ctx->params.auto_scaling) {
		pthread_join(ctx->autoscaling_thread, NULL);
		if (g_debug) printf("autoscaling_thread joined\n");
	}

	// Final logging
	emit_simulation_end(&ctx->stats);
	emit_statistics(&ctx->stats);

	// Clear stats
	ctx->stats = (simulation_statistics_t){0};

	pthread_mutex_lock(&g_server_state_mutex);
	ctx->is_running = 0;
	pthread_mutex_unlock(&g_server_state_mutex);
    if (g_debug) printf("Simulation runner thread finished\n");
	return NULL;
}

static void start_simulation_async(simulation_context_t* ctx) {
	pthread_mutex_lock(&g_server_state_mutex);
	if (ctx->is_running) {
		pthread_mutex_unlock(&g_server_state_mutex);
		return;
	}
	ctx->is_running = 1;
	pthread_mutex_unlock(&g_server_state_mutex);
	pthread_create(&ctx->simulation_runner_thread, NULL, simulation_runner, ctx);
}

static void request_stop_simulation(simulation_context_t* ctx) {
	// Emulate signal catcher logic to stop simulation gracefully
	pthread_mutex_lock(&ctx->simulation_state_mutex);
	g_terminate_now = 1;
	ctx->all_jobs_arrived = 1;
	pthread_mutex_unlock(&ctx->simulation_state_mutex);

	pthread_mutex_lock(&ctx->stats_mutex);
	emit_simulation_stopped(&ctx->stats);
	pthread_mutex_unlock(&ctx->stats_mutex);

    pthread_cancel(ctx->job_receiver_thread);
    pthread_cancel(ctx->paper_refill_thread);

	// Lock in defined order and empty queue
	pthread_mutex_lock(&ctx->job_queue_mutex);
	pthread_mutex_lock(&ctx->stats_mutex);
	empty_queue_if_terminating(&ctx->job_queue, &ctx->stats);
	pthread_cond_broadcast(&ctx->job_queue_not_empty_cv);
	pthread_mutex_unlock(&ctx->stats_mutex);
	pthread_mutex_unlock(&ctx->job_queue_mutex);

    // Wake up any printers or refiller that might be waiting
    pthread_mutex_lock(&ctx->paper_refill_queue_mutex);
    pthread_cond_broadcast(&ctx->refill_needed_cv);
    pthread_cond_broadcast(&ctx->refill_supplier_cv);
    pthread_mutex_unlock(&ctx->paper_refill_queue_mutex);
}

// Helper to compare incoming ws message with a C string literal
static int ws_msg_equals(struct mg_str s, const char *lit) {
	size_t n = strlen(lit);
	return s.len == n && memcmp(s.buf, lit, n) == 0;
}

// Mongoose event handler
/**
 * @brief Mongoose event handler for HTTP and WebSocket events
 * 
 * @param c The Mongoose connection
 * @param ev The event type
 * @param ev_data Event-specific data
 */
static void fn(struct mg_connection *c, int ev, void *ev_data) {
	if (ev == MG_EV_HTTP_MSG) {
		struct mg_http_message *hm = (struct mg_http_message *) ev_data;
		
		// Handle CORS preflight requests
		if (mg_strcmp(hm->method, mg_str("OPTIONS")) == 0) {
			mg_http_reply(c, 204, 
				"Access-Control-Allow-Origin: *\r\n"
				"Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
				"Access-Control-Allow-Headers: Content-Type\r\n", "");
			return;
		}
		
		if (mg_match(hm->uri, mg_str(s_ws_path_primary), NULL)) {
			mg_ws_upgrade(c, hm, NULL);
		} else if (mg_match(hm->uri, mg_str("/api/config"), NULL)) {
			// Build config JSON from C constants
			char json_buffer[2048];
			int len = snprintf(json_buffer, sizeof(json_buffer),
				"{"
				"\"config\":{"
				"\"printRate\":%g,"
				"\"consumerCount\":%d,"
				"\"autoScaling\":%s,"
				"\"refillRate\":%g,"
				"\"paperCapacity\":%d,"
				"\"jobArrivalTime\":%d,"
				"\"jobCount\":%d,"
				"\"fixedArrival\":%s,"
				"\"minArrivalTime\":%d,"
				"\"maxArrivalTime\":%d,"
				"\"maxQueue\":%d,"
				"\"minPapers\":%d,"
				"\"maxPapers\":%d,"
				"\"showTime\":%s,"
				"\"showSimulationStats\":%s,"
				"\"showLogs\":%s,"
				"\"showComponents\":%s"
				"},"
				"\"ranges\":{"
				"\"printRate\":{\"min\":%g,\"max\":%g},"
				"\"consumerCount\":{\"min\":%d,\"max\":%d},"
				"\"refillRate\":{\"min\":%g,\"max\":%g},"
				"\"paperCapacity\":{\"min\":%d,\"max\":%d},"
				"\"jobArrivalTime\":{\"min\":%d,\"max\":%d},"
				"\"minArrivalTime\":{\"min\":%d,\"max\":%d},"
				"\"maxArrivalTime\":{\"min\":%d,\"max\":%d},"
				"\"minPapers\":{\"min\":%d,\"max\":%d},"
				"\"maxPapers\":{\"min\":%d,\"max\":%d}"
				"}"
				"}",
				// config values
				CONFIG_DEFAULT_PRINT_RATE,
				CONFIG_DEFAULT_CONSUMER_COUNT,
				CONFIG_DEFAULT_AUTO_SCALING ? "true" : "false",
				CONFIG_DEFAULT_REFILL_RATE,
				CONFIG_DEFAULT_PAPER_CAPACITY,
				CONFIG_DEFAULT_JOB_ARRIVAL_TIME,
				CONFIG_DEFAULT_JOB_COUNT,
				CONFIG_DEFAULT_FIXED_ARRIVAL ? "true" : "false",
				CONFIG_DEFAULT_MIN_ARRIVAL_TIME,
				CONFIG_DEFAULT_MAX_ARRIVAL_TIME,
				CONFIG_DEFAULT_MAX_QUEUE,
				CONFIG_DEFAULT_MIN_PAPERS,
				CONFIG_DEFAULT_MAX_PAPERS,
				CONFIG_DEFAULT_SHOW_TIME ? "true" : "false",
				CONFIG_DEFAULT_SHOW_STATS ? "true" : "false",
				CONFIG_DEFAULT_SHOW_LOGS ? "true" : "false",
				CONFIG_DEFAULT_SHOW_COMPONENTS ? "true" : "false",
				// ranges
				CONFIG_RANGE_PRINT_RATE_MIN, CONFIG_RANGE_PRINT_RATE_MAX,
				CONFIG_RANGE_CONSUMER_COUNT_MIN, CONFIG_RANGE_CONSUMER_COUNT_MAX,
				CONFIG_RANGE_REFILL_RATE_MIN, CONFIG_RANGE_REFILL_RATE_MAX,
				CONFIG_RANGE_PAPER_CAPACITY_MIN, CONFIG_RANGE_PAPER_CAPACITY_MAX,
				CONFIG_RANGE_JOB_ARRIVAL_TIME_MIN, CONFIG_RANGE_JOB_ARRIVAL_TIME_MAX,
				CONFIG_RANGE_MIN_ARRIVAL_TIME_MIN, CONFIG_RANGE_MIN_ARRIVAL_TIME_MAX,
				CONFIG_RANGE_MAX_ARRIVAL_TIME_MIN, CONFIG_RANGE_MAX_ARRIVAL_TIME_MAX,
				CONFIG_RANGE_MIN_PAPERS_MIN, CONFIG_RANGE_MIN_PAPERS_MAX,
				CONFIG_RANGE_MAX_PAPERS_MIN, CONFIG_RANGE_MAX_PAPERS_MAX
			);
			
			if (len > 0 && len < (int)sizeof(json_buffer)) {
				mg_http_reply(c, 200, 
					"Content-Type: application/json\r\n"
					"Access-Control-Allow-Origin: *\r\n", "%s", json_buffer);
			} else {
				mg_http_reply(c, 500, 
					"Content-Type: text/plain\r\n"
					"Access-Control-Allow-Origin: *\r\n", "Config buffer overflow");
			}
		} else {
			// mg_http_reply(c, 200, "Content-Type: text/plain\r\n", "ConcurrentPrintService API\n");
            struct mg_http_serve_opts opts = {.root_dir = s_web_root};
            mg_http_serve_dir(c, ev_data, &opts);
		}
	} else if (ev == MG_EV_WS_OPEN) {
		// Track the active websocket client
		pthread_mutex_lock(&g_ws_mutex);
		g_ws_conn_id = c->id;
		pthread_mutex_unlock(&g_ws_mutex);
	} else if (ev == MG_EV_WS_MSG) {
		struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
		printf("DBG ws_message %.*s\n", (int)wm->data.len, wm->data.buf);
		
		// Parse JSON command
		char* command = mg_json_get_str(wm->data, "$.command");
		printf("DBG command %s\n", command ? command : "null");
		int cmd_len = command ? strlen(command) : -1;
		// int cmd_len = mg_json_get_str(wm->data, "$.command", command, sizeof(command));
		
		if (cmd_len < 0) {
			// Not JSON format, try legacy string commands for backward compatibility
			if (ws_msg_equals(wm->data, "start")) {
				start_simulation_async(&g_ctx);
				const char *resp = "{\"status\":\"starting\"}";
				mg_ws_send(c, resp, strlen(resp), WEBSOCKET_OP_TEXT);
				return;
			} else if (ws_msg_equals(wm->data, "stop")) {
				request_stop_simulation(&g_ctx);
				const char *resp = "{\"status\":\"stopping\"}";
				mg_ws_send(c, resp, strlen(resp), WEBSOCKET_OP_TEXT);
				return;
			} else if (ws_msg_equals(wm->data, "status")) {
				pthread_mutex_lock(&g_server_state_mutex);
				int running = g_ctx.is_running;
				pthread_mutex_unlock(&g_server_state_mutex);
				const char *resp = running ? "{\"status\":\"running\"}" : "{\"status\":\"idle\"}";
				mg_ws_send(c, resp, strlen(resp), WEBSOCKET_OP_TEXT);
				return;
			}
			const char *resp = "{\"error\":\"invalid message format\"}";
			mg_ws_send(c, resp, strlen(resp), WEBSOCKET_OP_TEXT);
			return;
		}

		// Handle JSON commands
		if (strcmp(command, "start") == 0) {
			// Check if config is provided
			double num_jobs = 0;
			mg_json_get_num(wm->data, "$.config.jobCount", &num_jobs);

			if (num_jobs > 0) {
				// Apply config overrides
				pthread_mutex_lock(&g_server_state_mutex);
				g_ctx.params.num_jobs = (int)num_jobs;

				double print_rate;
				if (1 == mg_json_get_num(wm->data, "$.config.printRate", &print_rate))
					g_ctx.params.printing_rate = print_rate;

				double consumer_count;
				if (1 == mg_json_get_num(wm->data, "$.config.consumerCount", &consumer_count))
					g_ctx.params.consumer_count = (int)consumer_count;

				double refill_rate;
				if (1 == mg_json_get_num(wm->data, "$.config.refillRate", &refill_rate))
					g_ctx.params.refill_rate = refill_rate;

				double paper_capacity;
				if (1 == mg_json_get_num(wm->data, "$.config.paperCapacity", &paper_capacity))
					g_ctx.params.printer_paper_capacity = (int)paper_capacity;

				double job_arrival;
				if (1 == mg_json_get_num(wm->data, "$.config.jobArrivalTime", &job_arrival))
					g_ctx.params.job_arrival_time_us = (int)job_arrival;

				double max_queue;
				if (1 == mg_json_get_num(wm->data, "$.config.maxQueue", &max_queue))
					g_ctx.params.queue_capacity = (int)max_queue;

				double min_papers;
				if (1 == mg_json_get_num(wm->data, "$.config.minPapers", &min_papers))
					g_ctx.params.papers_required_lower_bound = (int)min_papers;

				double max_papers;
				if (1 == mg_json_get_num(wm->data, "$.config.maxPapers", &max_papers))
					g_ctx.params.papers_required_upper_bound = (int)max_papers;

				bool auto_scaling;
				if (1 == mg_json_get_bool(wm->data, "$.config.autoScaling", &auto_scaling))
					g_ctx.params.auto_scaling = (int)auto_scaling;
				
				pthread_mutex_unlock(&g_server_state_mutex);
			}
			
			start_simulation_async(&g_ctx);
			const char *resp = "{\"status\":\"starting\"}";
			mg_ws_send(c, resp, strlen(resp), WEBSOCKET_OP_TEXT);
		} else if (strcmp(command, "stop") == 0) {
			request_stop_simulation(&g_ctx);
			const char *resp = "{\"status\":\"stopping\"}";
			mg_ws_send(c, resp, strlen(resp), WEBSOCKET_OP_TEXT);
		} else if (strcmp(command, "status") == 0) {
			pthread_mutex_lock(&g_server_state_mutex);
			int running = g_ctx.is_running;
			pthread_mutex_unlock(&g_server_state_mutex);
			const char *resp = running ? "{\"status\":\"running\"}" : "{\"status\":\"idle\"}";
			mg_ws_send(c, resp, strlen(resp), WEBSOCKET_OP_TEXT);
		} else {
			const char *resp = "{\"error\":\"unknown command\"}";
			mg_ws_send(c, resp, strlen(resp), WEBSOCKET_OP_TEXT);
		}
	} else if (ev == MG_EV_WAKEUP) {
		// Deliver data enqueued from other threads
		struct mg_str *data = (struct mg_str *) ev_data;
		if (data && data->buf && data->len > 0) {
			mg_ws_send(c, data->buf, data->len, WEBSOCKET_OP_TEXT);
		}
	} else if (ev == MG_EV_CLOSE) {
		// Clear active websocket if it is closing
		pthread_mutex_lock(&g_ws_mutex);
		if (g_ws_conn_id == c->id) g_ws_conn_id = 0;
		pthread_mutex_unlock(&g_ws_mutex);
	}
}

/**
 * @brief Thread-safe enqueue of a JSON frame for websocket client
 * 
 * @param json The JSON data to send
 * @param len The length of the JSON data
 */
void ws_bridge_send_json_from_any_thread(const char *json, size_t len) {
	if (json == NULL || len == 0) return;
	pthread_mutex_lock(&g_ws_mutex);
	unsigned long id = g_ws_conn_id;
	pthread_mutex_unlock(&g_ws_mutex);
	if (id != 0) {
		mg_wakeup(&g_mgr, id, json, len);
	}
}

int main(int argc, char *argv[]) {
	// Initialize context and process args
	init_context(&g_ctx);
	if (!process_args(argc, argv, &g_ctx.params)) return 1;

	// Register websocket handler
	websocket_handler_register();

	// Server mode: send over websocket
	set_log_mode(LOG_MODE_SERVER);

	mg_mgr_init(&g_mgr); // Initialise event manager

	// Initialise wakeup pipe for cross-thread notifications
	if (!mg_wakeup_init(&g_mgr)) {
		fprintf(stderr, "Failed to initialise Mongoose wakeup pipe\n");
		mg_mgr_free(&g_mgr);
		destroy_context(&g_ctx);
		return 1;
	}

    // Create HTTP listener
	if (mg_http_listen(&g_mgr, s_listen_on, fn, NULL) == NULL) {
		fprintf(stderr, "Failed to start Mongoose at %s\n", s_listen_on);
		mg_mgr_free(&g_mgr);
		destroy_context(&g_ctx);
		return 1;
	}

	printf("Starting WS listener on %s%s\n", s_listen_on, s_ws_path_primary);
	for (;;) mg_mgr_poll(&g_mgr, 100); // Infinite event loop

	// Unreachable in normal flow
	mg_mgr_free(&g_mgr);
	destroy_context(&g_ctx);
	return 0;
}

