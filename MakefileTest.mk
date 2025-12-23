# Compiler, flags, and libraries
CC = gcc

CFLAGS = -g -Wall -Iinclude -Iinclude/common -Iexternal -MMD -MP

# --- Configuration for Executables ---
TARGETS = test_linked_list test_preprocessing test_job_receiver test_simulation_stats test_timed_queue

# --- Rules ---
all: $(TARGETS)

# Direct compilation rules (one-line compile and link)
test_linked_list: tests/test_linked_list.c src/linked_list.c tests/test_utils.c include/linked_list.h include/test_utils.h include/common/common.h
	$(CC) $(CFLAGS) -o $@ tests/test_linked_list.c src/linked_list.c tests/test_utils.c

test_preprocessing: tests/test_preprocessing.c src/preprocessing.c tests/test_utils.c include/preprocessing.h include/test_utils.h include/common/common.h
	$(CC) $(CFLAGS) -o $@ tests/test_preprocessing.c src/preprocessing.c tests/test_utils.c

test_job_receiver: tests/test_job_receiver.c src/job_receiver.c tests/test_utils.c src/preprocessing.c src/timed_queue.c src/linked_list.c src/common/timeutils.c src/simulation_stats.c src/console_handler.c src/log_router.c include/job_receiver.h include/preprocessing.h include/linked_list.h include/timed_queue.h include/common/timeutils.h include/simulation_stats.h include/console_handler.h include/test_utils.h include/common/common.h include/log_router.h
	$(CC) $(CFLAGS) -o $@ tests/test_job_receiver.c src/job_receiver.c tests/test_utils.c src/preprocessing.c src/timed_queue.c src/linked_list.c src/common/timeutils.c src/simulation_stats.c src/console_handler.c src/log_router.c -lm -lpthread

test_simulation_stats: tests/test_simulation_stats.c src/simulation_stats.c tests/test_utils.c include/simulation_stats.h include/test_utils.h
	$(CC) $(CFLAGS) -o $@ tests/test_simulation_stats.c src/simulation_stats.c tests/test_utils.c -lm

test_timed_queue: tests/test_timed_queue.c src/timed_queue.c src/linked_list.c tests/test_utils.c src/common/timeutils.c include/timed_queue.h include/linked_list.h include/common/timeutils.h include/test_utils.h include/common/common.h
	$(CC) $(CFLAGS) -o $@ tests/test_timed_queue.c src/timed_queue.c src/linked_list.c tests/test_utils.c src/common/timeutils.c -lm

clean:
	rm -rf $(TARGETS) *.o *.d *.dSYM

# Include dependency files if they exist
-include *.d

# Declare targets that are not actual files
.PHONY: all clean