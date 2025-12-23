# Compiler, flags, and libraries
CC = gcc

# Directory paths
SRC_DIR = ../src
INC_DIR = ../include
EXT_DIR = ../external

CFLAGS = -g -Wall -I$(INC_DIR) -I$(INC_DIR)/common -I$(EXT_DIR) -MMD -MP

# --- Configuration for Executables ---
TARGETS = test_linked_list test_preprocessing test_job_receiver test_simulation_stats test_timed_queue

# --- Rules ---
all: $(TARGETS)

# Direct compilation rules (one-line compile and link)
test_linked_list: test_linked_list.c $(SRC_DIR)/linked_list.c test_utils.c $(INC_DIR)/linked_list.h $(INC_DIR)/test_utils.h $(INC_DIR)/common/common.h
	$(CC) $(CFLAGS) -o $@ test_linked_list.c $(SRC_DIR)/linked_list.c test_utils.c

test_preprocessing: test_preprocessing.c $(SRC_DIR)/preprocessing.c test_utils.c $(INC_DIR)/preprocessing.h $(INC_DIR)/test_utils.h $(INC_DIR)/common/common.h
	$(CC) $(CFLAGS) -o $@ test_preprocessing.c $(SRC_DIR)/preprocessing.c test_utils.c

test_job_receiver: test_job_receiver.c $(SRC_DIR)/job_receiver.c test_utils.c $(SRC_DIR)/preprocessing.c $(SRC_DIR)/timed_queue.c $(SRC_DIR)/linked_list.c $(SRC_DIR)/common/timeutils.c $(SRC_DIR)/simulation_stats.c $(SRC_DIR)/console_handler.c $(SRC_DIR)/log_router.c $(INC_DIR)/job_receiver.h $(INC_DIR)/preprocessing.h $(INC_DIR)/linked_list.h $(INC_DIR)/timed_queue.h $(INC_DIR)/common/timeutils.h $(INC_DIR)/simulation_stats.h $(INC_DIR)/console_handler.h $(INC_DIR)/test_utils.h $(INC_DIR)/common/common.h $(INC_DIR)/log_router.h
	$(CC) $(CFLAGS) -o $@ test_job_receiver.c $(SRC_DIR)/job_receiver.c test_utils.c $(SRC_DIR)/preprocessing.c $(SRC_DIR)/timed_queue.c $(SRC_DIR)/linked_list.c $(SRC_DIR)/common/timeutils.c $(SRC_DIR)/simulation_stats.c $(SRC_DIR)/console_handler.c $(SRC_DIR)/log_router.c -lm -lpthread

test_simulation_stats: test_simulation_stats.c $(SRC_DIR)/simulation_stats.c test_utils.c $(INC_DIR)/simulation_stats.h $(INC_DIR)/test_utils.h
	$(CC) $(CFLAGS) -o $@ test_simulation_stats.c $(SRC_DIR)/simulation_stats.c test_utils.c -lm

test_timed_queue: test_timed_queue.c $(SRC_DIR)/timed_queue.c $(SRC_DIR)/linked_list.c test_utils.c $(SRC_DIR)/common/timeutils.c $(INC_DIR)/timed_queue.h $(INC_DIR)/linked_list.h $(INC_DIR)/common/timeutils.h $(INC_DIR)/test_utils.h $(INC_DIR)/common/common.h
	$(CC) $(CFLAGS) -o $@ test_timed_queue.c $(SRC_DIR)/timed_queue.c $(SRC_DIR)/linked_list.c test_utils.c $(SRC_DIR)/common/timeutils.c -lm

clean:
	rm -rf $(TARGETS) *.o *.d *.dSYM

# Include dependency files if they exist
-include *.d

# Declare targets that are not actual files
.PHONY: all clean