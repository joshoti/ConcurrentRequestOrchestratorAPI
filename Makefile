# Compiler, flags, and libraries
CC = gcc
# -MMD and -MP for automatic dependency generation
CFLAGS = -g -Wall -Werror -pthread -Iinclude -Iinclude/common -Iexternal -MMD -MP
SERVER_LDFLAGS = -lm -ldl
CLI_LDFLAGS = -lm

# --- Configuration for Executables ---
BINDIR = bin
SERVER_TARGET = $(BINDIR)/server
CLI_TARGET    = $(BINDIR)/cli
ODIR = build

# --- Source File Organization ---
SHARED_SRCS = src/linked_list.c src/timed_queue.c src/job_receiver.c src/common/timeutils.c src/paper_refiller.c src/printer.c src/simulation_stats.c src/preprocessing.c src/log_router.c src/signalcatcher.c src/autoscaling.c
SERVER_SRCS = src/server.c src/websocket_handler.c
CLI_SRCS = src/cli.c src/console_handler.c
EXTERNAL_SRCS = external/mongoose.c

# --- Automatic Object File Generation ---
SHARED_OBJS = $(patsubst %.c, $(ODIR)/%.o, $(SHARED_SRCS))
SERVER_OBJS = $(patsubst %.c, $(ODIR)/%.o, $(SERVER_SRCS))
CLI_OBJS = $(patsubst %.c, $(ODIR)/%.o, $(CLI_SRCS))
EXTERNAL_OBJS = $(patsubst %.c, $(ODIR)/%.o, $(EXTERNAL_SRCS))
DEPS = $(SHARED_OBJS:.o=.d) $(SERVER_OBJS:.o=.d) $(CLI_OBJS:.o=.d) $(EXTERNAL_OBJS:.o=.d)

# --- Rules ---
all: $(SERVER_TARGET) $(CLI_TARGET)

$(SERVER_TARGET): $(SHARED_OBJS) $(SERVER_OBJS) $(EXTERNAL_OBJS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -o $@ $^ $(SERVER_LDFLAGS)

$(CLI_TARGET): $(SHARED_OBJS) $(CLI_OBJS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -o $@ $^ $(CLI_LDFLAGS)

# Generic rule to compile any .c file into a .o file in the build directory
$(ODIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# Include dependency files if they exist
-include $(DEPS)

clean:
	rm -rf $(ODIR) $(BINDIR)

# Declare targets that are not actual files
.PHONY: all clean