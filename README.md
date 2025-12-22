# ConcurrentRequestOrchestratorAPI
This project is a simulation of a multithreaded resource orchestrator (modeled as a print service) to visualize traffic and concurrency control. It implements a multi-stage producer-consumer pattern involving multiple producers, multiple consumers, and the management of shared, finite resources.

- See React frontend [here](https://github.com/joshoti/ConcurrentRequestOrchestrator)

## Features

✨ **Dynamic Autoscaling** - Scales from 1-5 printers based on queue length with intelligent thresholds  
📊 **Real-time WebSocket Updates** - Frontend-friendly JSON protocol for live visualization  
🔄 **Paper Management** - Automatic paper refill system with thread-safe coordination  
📈 **Comprehensive Statistics** - Per-printer metrics, queue analysis, and utilization tracking  
🎯 **Production-Ready** - Event-driven architecture with proper encapsulation and error handling

## Architecture
![Architecture diagram](docs/Architecture%20diagram.png)

<details><summary><i>Click to see architecture explanation</i></summary>
The service operates as a pipeline with distinct roles handled by different threads:

1. **Job Arrival:** The **Job Receiver** thread acts as the first producer. It simulates the arrival of new print jobs and places them into the `Job Queue`.
2. **Resource Provisioning:** In parallel, the **Paper Refill** thread acts as a second producer. On signal, it replenishes the `Paper Supply` in the requesting printer when it lacks sufficient paper to process the job at the front of the `Job Queue`.
3. **Job Servicing:** Multiple **Printer** threads (1-5, dynamically scaled) act as consumers. They concurrently pull jobs from the `Job Queue` and simulate the "printing" process, after which the job is complete.
4. **Autoscaling:** The **Autoscaling Monitor** thread adjusts the printer pool size based on queue length, scaling up when demand increases and down when printers are idle.
</details>

## Local Environment Setup

### CLI Version
To compile and run terminal version:
```sh
make -s bin/cli && ./bin/cli
```

To see list of configurable options:
```sh
./bin/cli -help
```

Example with autoscaling:
```sh
./bin/cli -num 50 -auto_scale 1 -job_arr_time 200
```

### WebSocket Server
1. To run the WebSocket server for frontend integration:
```sh
make -s bin/server && ./bin/server
```

2. Connect to `http://localhost:8000/test_websocket.html`

3. Click the ***connect button***, type `start`, and hit the ***send message button***

See [FRONTEND_INTEGRATION_GUIDE.md](FRONTEND_INTEGRATION_GUIDE.md) for complete WebSocket protocol documentation.

## WebSocket Protocol (v2.0)

All messages follow the structure:
```json
{
  "type": "message_type",
  "data": { /* message-specific data */ }
}
```

### Message Types

- **`simulation_started`** - Simulation begins (timestamp: 0)
- **`consumer_update`** - Printer state change (id, papersLeft, status, currentJobId)
- **`job_update`** - Job queue state change (id, papersRequired)
- **`log`** - Event messages with timestamp and human-readable text
- **`simulation_complete`** - Simulation ends (duration in s)

📘 **Full Documentation:** [FRONTEND_INTEGRATION_GUIDE.md](docs/FRONTEND_INTEGRATION_GUIDE.md)

## Configuration

Key parameters (see `include/config.h`):

- **Max Printers:** 5 (CONFIG_RANGE_CONSUMER_COUNT_MAX)
- **Autoscaling Thresholds:**
  - 2 printers @ queue length ≥ 10
  - 3 printers @ queue length ≥ 15
  - 4 printers @ queue length ≥ 20
  - 5 printers @ queue length ≥ 25
- **Scale-down:** After 5 seconds of idle time
- **Cooldown:** 3 seconds between scale operations

## Testing

Run all tests:
```sh
./run-tests.sh
```

## Development

### Building
```sh
make              # Build all targets
make clean        # Clean build artifacts
make bin/cli      # Build CLI only
make bin/server   # Build server only
```

### Debugging
```sh
# Save logs to file
./bin/cli -num 20 > output.log 2>&1

# Filter specific events
./bin/cli -num 20 2>&1 | grep "autoscal"
./bin/cli -num 20 2>&1 | grep "refill"

# Test with WebSocket client
brew install websocat
websocat ws://localhost:8000/websocket
```
