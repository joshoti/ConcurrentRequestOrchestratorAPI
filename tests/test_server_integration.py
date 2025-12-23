#!/usr/bin/env python3
"""
Integration tests for the WebSocket server.
Tests WebSocket commands, HTTP endpoints, and message formats.

Usage:
    python3 tests/test_server_integration.py
    
Requirements:
    pip install websocket-client requests
"""

import json
import time
import requests
import websocket
import threading
import sys
from typing import List, Dict, Any

class ServerIntegrationTest:
    def __init__(self, ws_url="ws://localhost:8000/ws/simulation", 
                 http_url="http://localhost:8000"):
        self.ws_url = ws_url
        self.http_url = http_url
        self.ws = None
        self.messages: List[Dict[str, Any]] = []
        self.connected = False
        self.failed_tests = 0
        self.passed_tests = 0
        
    def on_message(self, ws, message):
        """Callback for WebSocket messages"""
        try:
            data = json.loads(message)
            self.messages.append(data)
            print(f"  📨 Received: {data.get('type', 'unknown')}")
        except json.JSONDecodeError:
            print(f"  ⚠️  Invalid JSON: {message}")
    
    def on_error(self, ws, error):
        """Callback for WebSocket errors"""
        print(f"  ❌ WebSocket error: {error}")
    
    def on_close(self, ws, close_status_code, close_msg):
        """Callback for WebSocket close"""
        self.connected = False
        print(f"  🔌 WebSocket closed: {close_status_code}")
    
    def on_open(self, ws):
        """Callback for WebSocket open"""
        self.connected = True
        print(f"  ✅ WebSocket connected")
    
    def connect_websocket(self):
        """Connect to WebSocket server"""
        print("\n🔗 Connecting to WebSocket...")
        self.ws = websocket.WebSocketApp(
            self.ws_url,
            on_open=self.on_open,
            on_message=self.on_message,
            on_error=self.on_error,
            on_close=self.on_close
        )
        
        # Run WebSocket in background thread
        wst = threading.Thread(target=self.ws.run_forever)
        wst.daemon = True
        wst.start()
        
        # Wait for connection
        timeout = 5
        start = time.time()
        while not self.connected and time.time() - start < timeout:
            time.sleep(0.1)
        
        if not self.connected:
            raise Exception("Failed to connect to WebSocket")
        
        return True
    
    def send_command(self, command: str, config: Dict[str, Any] = None):
        """Send a command to the WebSocket server"""
        payload = {"command": command}
        if config:
            payload["config"] = config
        
        message = json.dumps(payload)
        print(f"  📤 Sending: {message}")
        self.ws.send(message)
        time.sleep(0.5)  # Wait for response
    
    def assert_true(self, condition: bool, message: str):
        """Assert a condition is true"""
        if condition:
            print(f"  ✅ PASS: {message}")
            self.passed_tests += 1
        else:
            print(f"  ❌ FAIL: {message}")
            self.failed_tests += 1
    
    def assert_equal(self, actual, expected, message: str):
        """Assert two values are equal"""
        if actual == expected:
            print(f"  ✅ PASS: {message}")
            self.passed_tests += 1
        else:
            print(f"  ❌ FAIL: {message} (expected: {expected}, got: {actual})")
            self.failed_tests += 1
    
    def test_cors_headers(self):
        """Test CORS headers on /api/config endpoint"""
        print("\n🧪 Test: CORS headers on /api/config")
        
        # Test OPTIONS preflight
        response = requests.options(f"{self.http_url}/api/config")
        self.assert_equal(response.status_code, 204, "OPTIONS returns 204")
        self.assert_true(
            "Access-Control-Allow-Origin" in response.headers,
            "OPTIONS includes Access-Control-Allow-Origin"
        )
        
        # Test GET with CORS
        response = requests.get(f"{self.http_url}/api/config")
        self.assert_equal(response.status_code, 200, "GET /api/config returns 200")
        self.assert_true(
            "Access-Control-Allow-Origin" in response.headers,
            "GET includes Access-Control-Allow-Origin"
        )
        self.assert_equal(
            response.headers.get("Content-Type"),
            "application/json",
            "Content-Type is application/json"
        )
    
    def test_config_endpoint(self):
        """Test /api/config endpoint returns valid JSON"""
        print("\n🧪 Test: /api/config endpoint")
        
        response = requests.get(f"{self.http_url}/api/config")
        self.assert_equal(response.status_code, 200, "GET /api/config returns 200")
        
        try:
            data = response.json()
            self.assert_true("config" in data, "Response has 'config' key")
            self.assert_true("ranges" in data, "Response has 'ranges' key")
            
            # Check config fields
            config = data.get("config", {})
            expected_fields = [
                "printRate", "consumerCount", "autoScaling", "refillRate",
                "paperCapacity", "jobArrivalTime", "jobCount", "maxQueue",
                "minPapers", "maxPapers"
            ]
            for field in expected_fields:
                self.assert_true(
                    field in config,
                    f"Config contains '{field}'"
                )
            
            # Check ranges
            ranges = data.get("ranges", {})
            self.assert_true(len(ranges) > 0, "Ranges object is not empty")
            
            print(f"  📋 Config: {json.dumps(config, indent=2)}")
        except json.JSONDecodeError:
            self.assert_true(False, "Response is valid JSON")
    
    def test_status_command(self):
        """Test status command"""
        print("\n🧪 Test: Status command")
        
        self.messages.clear()
        self.send_command("status")
        
        # Check for status response
        self.assert_true(len(self.messages) > 0, "Received status response")
        if self.messages:
            msg = self.messages[-1]
            self.assert_true("status" in msg, "Response has 'status' field")
            status = msg.get("status")
            self.assert_true(
                status in ["idle", "running"],
                f"Status is 'idle' or 'running' (got: {status})"
            )
    
    def test_start_command_basic(self):
        """Test basic start command"""
        print("\n🧪 Test: Start command (basic)")
        
        self.messages.clear()
        self.send_command("start")
        
        # Wait for simulation messages
        time.sleep(2)
        
        # Check for simulation messages
        message_types = [msg.get("type") for msg in self.messages]
        print(f"  📊 Received message types: {message_types}")
        
        # Should receive various simulation messages
        self.assert_true(len(self.messages) > 0, "Received simulation messages")
        
        # Stop the simulation
        self.send_command("stop")
        time.sleep(1)
    
    def test_start_command_with_config(self):
        """Test start command with config overrides"""
        print("\n🧪 Test: Start command with config")
        
        self.messages.clear()
        config = {
            "jobCount": 10,
            "printRate": 2.0,
            "consumerCount": 2,
            "paperCapacity": 50
        }
        self.send_command("start", config)
        
        # Wait for simulation messages
        time.sleep(2)
        
        # Check for simulation_parameters message
        param_messages = [
            msg for msg in self.messages 
            if msg.get("type") == "simulation_parameters"
        ]
        
        if param_messages:
            params = param_messages[0].get("data", {})
            print(f"  📋 Simulation params: {json.dumps(params, indent=2)}")
            self.assert_equal(
                params.get("jobCount"),
                10,
                "jobCount override applied"
            )
        
        # Stop the simulation
        self.send_command("stop")
        time.sleep(1)
    
    def test_message_formats(self):
        """Test that all messages follow the expected format"""
        print("\n🧪 Test: Message formats")
        
        self.messages.clear()
        self.send_command("start")
        time.sleep(2)
        self.send_command("stop")
        time.sleep(1)
        
        # Check message structure
        for msg in self.messages[:10]:  # Check first 10 messages
            msg_type = msg.get("type", "unknown")
            
            # All messages should have 'type'
            self.assert_true("type" in msg, f"Message has 'type' field")
            
            # Most messages should have 'data'
            if msg_type not in ["simulation_start", "simulation_end", "simulation_stopped"]:
                self.assert_true(
                    "data" in msg,
                    f"Message type '{msg_type}' has 'data' field"
                )
    
    def test_unknown_command(self):
        """Test unknown command handling"""
        print("\n🧪 Test: Unknown command")
        
        self.messages.clear()
        self.send_command("invalid_command_xyz")
        
        # Should receive error response
        self.assert_true(len(self.messages) > 0, "Received error response")
        if self.messages:
            msg = self.messages[-1]
            self.assert_true(
                "error" in msg,
                "Error response contains 'error' field"
            )
    
    def run_all_tests(self):
        """Run all integration tests"""
        print("=" * 70)
        print("🧪 WebSocket Server Integration Tests")
        print("=" * 70)
        
        try:
            # Test HTTP endpoints first (no WebSocket needed)
            self.test_cors_headers()
            self.test_config_endpoint()
            
            # Connect WebSocket
            self.connect_websocket()
            
            # Run WebSocket tests
            self.test_status_command()
            self.test_start_command_basic()
            self.test_start_command_with_config()
            self.test_message_formats()
            self.test_unknown_command()
            
            # Close WebSocket
            if self.ws:
                self.ws.close()
            
        except Exception as e:
            print(f"\n❌ Test suite error: {e}")
            self.failed_tests += 1
        
        # Print summary
        print("\n" + "=" * 70)
        print(f"📊 Test Results:")
        print(f"   ✅ Passed: {self.passed_tests}")
        print(f"   ❌ Failed: {self.failed_tests}")
        print("=" * 70)
        
        return self.failed_tests == 0

def main():
    """Main entry point"""
    print("\n⚠️  Make sure the server is running: ./bin/server\n")
    time.sleep(1)
    
    tester = ServerIntegrationTest()
    success = tester.run_all_tests()
    
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
