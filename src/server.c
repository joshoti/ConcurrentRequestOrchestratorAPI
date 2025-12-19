// Mongoose-based websocket server that drives the print simulation.
// Websocket endpoint accepts text frames: "start", "stop", "status".

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "mongoose.h"
#include "preprocessing.h"
#include "linked_list.h"
#include "timed_queue.h"
#include "job_receiver.h"
#include "paper_refiller.h"
#include "printer.h"
#include "websocket_handler.h"
#include "ws_bridge.h"
#include "log_router.h"
#include "simulation_stats.h"
#include "signalcatcher.h"

// Default listen address and websocket paths
static const char *s_listen_on = "http://127.0.0.1:8000";
static const char *s_ws_path_primary = "/websocket";
static const char *s_web_root = "./tests";

// Mongoose manager and active websocket tracking
static struct mg_mgr g_mgr; // used for mg_wakeup
static pthread_mutex_t g_ws_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned long g_ws_conn_id = 0; // 0 means none

extern int g_debug;
extern int g_terminate_now;

typedef struct simulation_context {
	// Threads
	pthread_t printer1_thread;
	pthread_t printer2_thread;
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
	printer_thread_args_t printer1_args;
	printer_thread_args_t printer2_args;
	paper_refill_thread_args_t paper_refill_args;

	// Control
	int is_running;
} simulation_context_t;

static simulation_context_t g_ctx; // single simulation instance
static pthread_mutex_t g_server_state_mutex = PTHREAD_MUTEX_INITIALIZER;

static void init_context(simulation_context_t* ctx) {
	memset(ctx, 0, sizeof(*ctx));
	ctx->params = (simulation_parameters_t)SIMULATION_DEFAULT_PARAMS;
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

	// Concrete printers
	printer_t printer1 = {.id = 1, .current_paper_count = ctx->params.printer_paper_capacity, .capacity = ctx->params.printer_paper_capacity, .total_papers_used = 0, .jobs_printed_count = 0};
	printer_t printer2 = {.id = 2, .current_paper_count = ctx->params.printer_paper_capacity, .capacity = ctx->params.printer_paper_capacity, .total_papers_used = 0, .jobs_printed_count = 0};

	printer_thread_args_t printer1_args = {
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
		.printer = &printer1
	};
	ctx->printer1_args = printer1_args;

	printer_thread_args_t printer2_args = printer1_args;
	printer2_args.printer = &printer2;
	ctx->printer2_args = printer2_args;

	paper_refill_thread_args_t paper_refill_args = {
		.paper_refill_queue_mutex = &ctx->paper_refill_queue_mutex,
		.stats_mutex = &ctx->stats_mutex,
		.simulation_state_mutex = &ctx->simulation_state_mutex,
		.refill_needed_cv = &ctx->refill_needed_cv,
		.refill_supplier_cv = &ctx->refill_supplier_cv,
		.paper_refill_queue = &ctx->paper_refill_queue,
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
	pthread_create(&ctx->printer1_thread, NULL, printer_thread_func, &ctx->printer1_args);
	pthread_create(&ctx->printer2_thread, NULL, printer_thread_func, &ctx->printer2_args);

	// Join threads
	pthread_join(ctx->job_receiver_thread, NULL);
	pthread_join(ctx->printer1_thread, NULL);
	pthread_join(ctx->printer2_thread, NULL);
	pthread_join(ctx->paper_refill_thread, NULL);

	// Final logging
	emit_simulation_end(&ctx->stats);
	emit_statistics(&ctx->stats);

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
		if (mg_match(hm->uri, mg_str(s_ws_path_primary), NULL)) {
			mg_ws_upgrade(c, hm, NULL);
		} else if (mg_match(hm->uri, mg_str("/api/config"), NULL)) {
			// Read and serve config.json
			FILE *fp = fopen("config.json", "r");
			if (fp) {
				fseek(fp, 0, SEEK_END);
				long fsize = ftell(fp);
				fseek(fp, 0, SEEK_SET);
				char *config_data = malloc(fsize + 1);
				if (config_data) {
					fread(config_data, 1, fsize, fp);
					config_data[fsize] = 0;
					mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s", config_data);
					free(config_data);
				} else {
					mg_http_reply(c, 500, "Content-Type: text/plain\r\n", "Memory allocation failed");
				}
				fclose(fp);
			} else {
				mg_http_reply(c, 404, "Content-Type: text/plain\r\n", "Config file not found");
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
		if (ws_msg_equals(wm->data, "start")) {
			start_simulation_async(&g_ctx);
			const char *resp = "{\"status\":\"starting\"}";
			mg_ws_send(c, resp, strlen(resp), WEBSOCKET_OP_TEXT);
		} else if (ws_msg_equals(wm->data, "stop")) {
			request_stop_simulation(&g_ctx);
			const char *resp = "{\"status\":\"stopping\"}";
			mg_ws_send(c, resp, strlen(resp), WEBSOCKET_OP_TEXT);
		} else if (ws_msg_equals(wm->data, "status")) {
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

