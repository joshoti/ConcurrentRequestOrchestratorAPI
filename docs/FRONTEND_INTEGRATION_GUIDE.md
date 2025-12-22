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
  status: "idle" | "serving";
  papersLeft: number;
  currentJobId: number | null;
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

## Known Limitations

### Current Implementation

1. **No `consumers_update` message**: Backend doesn't send initial pool state. Frontend should track consumers as they appear via `consumer_update` messages.

2. **No `jobs_update` message**: Backend doesn't send queue state changes. Frontend must reconstruct queue from log messages:
   - "enters queue" → add to queue
   - "leaves queue" → remove from queue
   - "dropped" → don't add to queue

3. **No `stats_update` message**: Backend only sends final statistics via `simulation_complete`. Real-time stats must be calculated from logs.

4. **`currentJobId` always null**: Printer structure doesn't track current job. Enhancement needed in [printer.h](include/printer.h) to add `current_job_id` field.

### Future Enhancements

**Priority 1: Job Tracking in Printers**
```c
// In include/printer.h
typedef struct {
    int id;
    int current_paper_count;
    int current_job_id;  // ADD THIS
    pthread_mutex_t printer_mutex;
    pthread_cond_t refill_cv;
    int termination_flag;
} printer_t;
```

**Priority 2: Queue State Messages**

Add to log_ops_t:
```c
void (*jobs_update)(job_t** queue_array, int queue_length);
```

Emit when:
- Job enters queue
- Job leaves queue
- Job dropped

**Priority 3: Real-time Statistics**

Add to log_ops_t:
```c
void (*stats_update)(simulation_statistics_t* stats);
```

Emit after each job completes.

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
{"command":"start","params":{"num_jobs":10,"job_arrival_time":100}}
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

---

## Frontend Component Structure

Recommended React component hierarchy:

```
<SimulationDashboard>
  <ControlPanel>
    <ConnectionStatus />
    <SimulationControls />
    <ConfigDisplay params={params} />
  </ControlPanel>
  
  <VisualizationArea>
    <JobQueueViz queue={jobQueue} />
    <ConsumerPoolViz consumers={consumers} />
    <StatisticsPanel stats={stats} />
  </VisualizationArea>
  
  <LogPanel logs={logs} />
</SimulationDashboard>
```

---

## Error Handling

### Connection Errors

```typescript
ws.onerror = (error) => {
  console.error("WebSocket error:", error);
  showErrorBanner("Connection to server lost");
};

ws.onclose = () => {
  showReconnectButton();
};
```

### Malformed Messages

```typescript
try {
  const message = JSON.parse(event.data);
  handleMessage(message);
} catch (error) {
  console.error("Failed to parse message:", event.data, error);
  // Log but don't crash
}
```

---

## Performance Considerations

### Message Rate

- High job arrival rates (10-50ms) generate ~20-100 messages/second
- Autoscaling adds ~2 messages per scale event
- Consider throttling log panel updates to 60fps

### Memory Management

- Limit log history to last 1000 entries
- Clean up completed jobs from tracking maps
- Use virtual scrolling for large log lists

---

## Support

For questions or issues:
1. Check [websockets.json](websockets.json) for expected format examples
2. Check [events-server.json](events-server.json) for actual backend output
3. Review source: [src/websocket_handler.c](src/websocket_handler.c)
4. Open GitHub issue with message trace

---

## Change Log

### Version 2.0 (Current)
- ✅ All messages wrapped in `{"type": "...", "data": {...}}` format
- ✅ Changed `printer_status` → `consumer_update`
- ✅ Changed `autoscale` → `log` messages
- ✅ Added `simulation_started` message
- ✅ Added `simulation_complete` message  
- ✅ All timestamps in floating-point milliseconds
- ✅ `consumer_update` includes papersLeft and status

### Version 1.0 (Deprecated)
- ❌ Mixed message formats (some with direct properties)
- ❌ Time strings like "00000123.456ms: "
- ❌ Integer milliseconds + microseconds
