# Build stage
FROM gcc:12 AS builder

WORKDIR /app

# Copy source files
COPY . .

# Build the server
RUN make clean && make

# Runtime stage
FROM debian:bullseye-slim

WORKDIR /app

# Install runtime dependencies
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Copy binary from builder
COPY --from=builder /app/bin/server /app/server

# Copy configuration and static files
COPY config.json /app/config.json
COPY tests/test_websocket.html /app/tests/test_websocket.html

# Expose WebSocket port
EXPOSE 8000

# Health check
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
  CMD timeout 2 bash -c "</dev/tcp/localhost/8000" || exit 1

# Run the server
CMD ["./server"]
