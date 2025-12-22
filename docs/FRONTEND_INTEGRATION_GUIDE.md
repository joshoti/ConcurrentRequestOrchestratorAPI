# Frontend Integration Guide: WebSocket Message Protocol

This guide documents the WebSocket message protocol for integrating the ConcurrentPrintServiceAPI backend with a real-time frontend UI.

## Connection

Connect to the WebSocket server at:
```
ws://localhost:8000/websocket
```

## Message Format

All messages follow this structure:
```json
{
  "type": "message_type",
  "data": { /* message-specific data */ }
}
```

## Message Types

### 1. `simulation_started`

Emitted when the simulation begins.

**Data Structure:**
```json
{
  "type": "simulation_started",
  "data": {
    "timestamp": 0
  }
}
```

**Frontend Action:**
- Reset all state
- Start animation clock at timestamp 0
- Initialize empty job queue
- Prepare to receive consumer pool

---

### 2. `consumer_update`

Emitted when a printer (consumer) changes state.

**Data Structure:**
```json
{
  "type": "consumer_update",
  "data": {
    "id": 1,
    "papersLeft": 88,
    "status": "serving",
    "currentJobId": null
  }
}
```

**Fields:**
- `id`: Printer ID (1-5)
- `papersLeft`: Current paper count
- `status`: `"idle"` or `"serving"`
- `currentJobId`: ID of job being served (currently always `null` - enhancement needed)

**Frontend Action:**
- Update printer card with new status
- Animate status change (idle → serving transition)
- Update paper level indicator
- If status is "idle", clear current job visualization

**When Emitted:**
- When printer starts serving a job (status: "serving")
- When printer finishes a job and becomes idle (status: "idle")
- After paper refill completes (papersLeft updated)

---

### 3. `log`

General simulation log messages.

**Data Structure:**
```json
{
  "type": "log",
  "data": {
    "timestamp": 123.456,
    "message": "job5 arrives, needs 10 papers, inter-arrival time = 50.123ms"
  }
}
```

**Fields:**
- `timestamp`: Time in milliseconds since simulation start
- `message`: Human-readable log message

**Frontend Action:**
- Append to scrolling log panel
- Optional: Parse message for specific events to trigger animations

**Message Types (by parsing):**
- Job arrival: `"job{id} arrives, needs {n} papers, inter-arrival time = {t}ms"`
- Job arrival (dropped): `"job{id} arrives, needs {n} papers, inter-arrival time = {t}ms, dropped"`
- Job removed: `"job{id} removed from system"`
- Queue entry: `"job{id} enters queue, queue length = {n}"`
- Queue exit: `"job{id} leaves queue, time in queue = {t}ms, queue_length = {n}"`
- Service start: `"job{id} begins service at printer{n}, printing {n} pages in about {t}ms"`
- Service complete: `"job{id} departs from printer{n}, service time = {t}ms"`
- Paper request: `"printer{n} does not have enough paper for job{id} and is requesting refill"`
- Refill start: `"printer{n} starts refilling {n} papers, estimated time = {t}ms"`
- Refill end: `"printer{n} finishes refilling, actual time = {t}ms"`
- Autoscaling: `"Autoscaling: Scaled UP to {n} printers (queue length: {n})"`
- Autoscaling: `"Autoscaling: Scaled DOWN to {n} printers (queue length: {n})"`

---

### 4. `simulation_complete`

Emitted when the simulation ends normally (all jobs processed).

**Data Structure:**
```json
{
  "type": "simulation_complete",
  "data": {
    "duration": 1234.567
  }
}
```

**Fields:**
- `duration`: Total simulation duration in milliseconds

**Frontend Action:**
- Stop animation clock
- Show "Simulation Complete" banner
- Display final statistics
- Enable "View Results" or "Restart" buttons

---

### 5. `params`

Simulation parameters sent at the start.

**Data Structure:**
```json
{
  "type": "params",
  "params": {
    "job_arrival_time": 50.0,
    "printing_rate": 10.0,
    "queue_capacity": 20,
    "printer_paper_capacity": 100,
    "refill_rate": 5.0,
    "num_jobs": 50,
    "papers_required_lower_bound": 5,
    "papers_required_upper_bound": 15
  }
}
```

**Fields:**
- `job_arrival_time`: Mean inter-arrival time (ms)
- `printing_rate`: Pages per second
- `queue_capacity`: Max jobs in queue
- `printer_paper_capacity`: Max paper capacity per printer
- `refill_rate`: Paper refill rate (papers per ms)
- `num_jobs`: Total jobs to generate
- `papers_required_lower_bound`: Min pages per job
- `papers_required_upper_bound`: Max pages per job

**Frontend Action:**
- Display in "Configuration" panel
- Initialize visualization constraints (queue size, paper capacity)

---

## Message Sequencing

### Typical Workflow

```
1. params                    → Display configuration
2. simulation_started        → Reset UI, start clock
3. log (job arrives)         → Show job entering system
4. log (job enters queue)    → Add job to queue visualization
5. log (job leaves queue)    → Remove from queue
6. consumer_update (serving) → Show printer busy
7. log (job begins service)  → Animate job at printer
8. log (job departs)         → Complete animation
9. consumer_update (idle)    → Show printer idle
10. [repeat 3-9 for each job]
11. simulation_complete      → Show final summary
```

### Autoscaling Workflow

```
1. log (Autoscaling: Scaled UP...)
2. consumer_update (new printer becomes available)
3. [new printer can now receive jobs]
4. [if idle timeout expires]
5. log (Autoscaling: Scaled DOWN...)
6. consumer_update (printer removed, status: idle)
```

### Paper Refill Workflow

```
1. log (printer does not have enough paper)
2. log (printer starts refilling)
3. [refill duration passes]
4. log (printer finishes refilling)
5. consumer_update (papersLeft updated)
```

---

## Implementation Recommendations

### State Management

Maintain frontend state for:
```typescript
interface FrontendState {
  simulationRunning: boolean;
  currentTime: number;  // in ms
  consumers: Map<number, Consumer>;  // keyed by printer ID
  jobQueue: Job[];
  statistics: SimulationStats;
  logs: LogEntry[];
}

interface Consumer {
  id: number;
  status: "idle" | "serving" | "waiting_refill";
  papersLeft: number;
  currentJobId: number | null;
}

interface Job {
  id: number;
  papersRequired: number;
}

interface SimulationStats {
  jobsProcessed: number;
  jobsReceived: number;
  queueLength: number;
  avgCompletionTime: number;
  papersUsed: number;
  refillEvents: number;
  avgServiceTime: number;
}

interface LogEntry {
  timestamp: number;
  message: string;
}
```

### Message Handlers

```typescript
function handleMessage(event: MessageEvent) {
  const message = JSON.parse(event.data);
  
  switch (message.type) {
    case "simulation_started":
      onSimulationStarted(message.data);
      break;
    case "consumer_update":
      onConsumerUpdate(message.data);
      break;
    case "log":
      onLog(message.data);
      break;
    case "simulation_complete":
      onSimulationComplete(message.data);
      break;
    case "params":
      onParams(message.params);
      break;
  }
}
```

### Animation Sync

Use timestamps for smooth animations:
```typescript
function onLog(data: LogData) {
  const { timestamp, message } = data;
  
  // Schedule animation at correct time
  const delay = timestamp - currentSimulationTime;
  setTimeout(() => {
    animateEvent(message);
  }, delay);
}
```

### Job Tracking

Parse log messages to track jobs:
```typescript
function parseJobArrival(message: string): JobInfo | null {
  const match = message.match(/job(\d+) arrives, needs (\d+) papers/);
  if (match) {
    return {
      jobId: parseInt(match[1]),
      papersRequired: parseInt(match[2]),
      dropped: message.includes("dropped")
    };
  }
  return null;
}
```

---

## Testing

### Manual Test with websocat

```bash
# Install websocat (macOS)
brew install websocat

# Start server
./bin/server

# Connect and observe messages
websocat ws://localhost:8000/websocket

# Send start command
{"command":"start"}
```

### Expected Output

```json
{"type":"params","params":{...}}
{"type":"simulation_started","data":{"timestamp":0}}
{"type":"log","data":{"timestamp":0,"message":"simulation begins"}}
{"type":"log","data":{"timestamp":0.123,"message":"job1 arrives..."}}
{"type":"log","data":{"timestamp":1.456,"message":"job1 enters queue..."}}
{"type":"consumer_update","data":{"id":1,"papersLeft":100,"status":"serving","currentJobId":null}}
...
{"type":"simulation_complete","data":{"duration":1234.567}}
```
