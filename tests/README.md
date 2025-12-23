# Test Suite

## Overview

This directory contains unit tests and integration tests for the ConcurrentPrintServiceAPI.

## Test Files

### Unit Tests (C)

- **test_linked_list.c** - Tests for doubly-linked list implementation
- **test_timed_queue.c** - Tests for timed queue wrapper
- **test_preprocessing.c** - Tests for job preprocessing logic
- **test_simulation_stats.c** - Tests for statistics tracking
- **test_job_receiver.c** - Tests for job receiver functionality

### Integration Tests (Python)

- **test_server_integration.py** - WebSocket server integration tests

### Manual Testing

- **test_websocket.html** - Browser-based WebSocket client for manual testing

## Running Tests

### C Unit Tests

```bash
# Run all C unit tests with colorized summary
bash run_unit_tests.sh
```

This script will:
- Build all tests using `tests/Makefile`
- Run each test suite (linked_list, preprocessing, job_receiver, simulation_stats, timed_queue)
- Display a summary: "X passed, Y failed"
- Clean up test binaries automatically
- Exit with code 1 if any tests fail (CI-friendly)
```

### Python Integration Tests

**First-time Setup:**
```bash
# Setup virtual environment and install dependencies
bash tests/setup_test_env.sh
```

**Running:**
```bash
# Start the server first
./bin/server

# In another terminal, run integration tests
bash tests/run_integration_tests.sh
```

**Manual activation (optional):**
```bash
# Activate virtual environment
source tests/venv/bin/activate

# Run tests
python3 tests/test_server_integration.py

# Deactivate when done
deactivate
```

## Integration Test Coverage

The Python integration tests cover:

### HTTP Endpoints
- ✅ CORS headers (OPTIONS preflight)
- ✅ /api/config endpoint (GET)
- ✅ Config JSON structure validation
- ✅ Config ranges validation

### WebSocket Commands
- ✅ Connection establishment
- ✅ `status` command
- ✅ `start` command (basic)
- ✅ `start` command with config overrides
- ✅ `stop` command
- ✅ Unknown command error handling

### Message Formats
- ✅ Message type field validation
- ✅ Message data field validation
- ✅ JSON structure validation
- ✅ Simulation lifecycle messages

## Test Structure

### Integration Test Example

```python
def test_start_command_with_config(self):
    """Test start command with config overrides"""
    config = {
        "jobCount": 10,
        "printRate": 2.0,
        "consumerCount": 2
    }
    self.send_command("start", config)
    
    # Verify config was applied
    param_messages = [
        msg for msg in self.messages 
        if msg.get("type") == "simulation_parameters"
    ]
    assert param_messages[0]["data"]["jobCount"] == 10
```

## Manual Testing with HTML Client

1. Start the server: `./bin/server`
2. Open `test_websocket.html` in a browser
3. Click "connect"
4. Send commands: `start`, `stop`, `status`
5. Observe real-time messages in the event log

## Adding New Tests

### C Unit Tests

1. Create `test_<module>.c` in `tests/`
2. Include `test_utils.h` for helper functions
3. Use the `RUN_TEST` macro for automatic test counting:

```c
#include "test_utils.h"

int test_my_function() {
    // Test logic here
    if (condition_passes) {
        printf("Test passed\n");
        return 0;  // Pass
    } else {
        printf("Test failed\n");
        return 1;  // Fail
    }
}

int main() {
    char test_name[] = "MY_MODULE";
    print_test_start(test_name);
    
    int total_tests = 0;      // Automatically incremented by RUN_TEST
    int failed_tests = 0;     // Automatically incremented by RUN_TEST
    
    RUN_TEST(test_my_function());    // Counts as 1 test
    RUN_TEST(test_another_function()); // Counts as 1 test
    
    int passed_tests = total_tests - failed_tests;
    print_test_end(test_name, passed_tests, failed_tests);
    return failed_tests > 0 ? 1 : 0;
}
```

4. Add test to `tests/Makefile`
5. Add test binary to `run_unit_tests.sh` TESTS array

**Key Points:**
- Test functions return 0 for pass, non-zero for fail
- Use `RUN_TEST(func())` macro instead of `failed_tests += func()`
- No need to manually track `total_tests` count
- The macro automatically increments both counters

### Python Integration Tests

1. Add new test method to `ServerIntegrationTest` class
2. Call from `run_all_tests()`
3. Use `assert_true()` and `assert_equal()` helpers
4. Clear `self.messages` before each test

## Test Output

### Successful Run
```
🧪 WebSocket Server Integration Tests
======================================
✅ PASS: OPTIONS returns 204
✅ PASS: GET /api/config returns 200
✅ PASS: Received status response
...
📊 Test Results:
   ✅ Passed: 25
   ❌ Failed: 0
```

### Failed Run
```
❌ FAIL: Config contains 'printRate'
❌ FAIL: Status is 'idle' or 'running' (got: error)
...
📊 Test Results:
   ✅ Passed: 20
   ❌ Failed: 5
```

## CI/CD Integration

The integration tests can be automated:

```bash
#!/bin/bash
# Start server in background
./bin/server &
SERVER_PID=$!

# Wait for server to start
sleep 2

# Run tests
python3 tests/test_server_integration.py
TEST_EXIT=$?

# Cleanup
kill $SERVER_PID

exit $TEST_EXIT
```

## Troubleshooting

**Connection refused:**
- Ensure server is running on port 8000
- Check firewall settings

**Tests timeout:**
- Server may be under load
- Increase timeout values in test script

**WebSocket errors:**
- Check server logs for errors
- Verify WebSocket upgrade is working

**JSON parse errors:**
- Check message format in server logs
- Verify Content-Type headers
