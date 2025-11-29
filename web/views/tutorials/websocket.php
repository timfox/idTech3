<?php
/**
 * WebSocket Integration Tutorial
 */
$title = 'WebSocket Integration Tutorial - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/websocket' => 'WebSocket Integration Tutorial'
];
?>

<h1>WebSocket Integration Tutorial</h1>

<div class="section">
    <h2>Introduction</h2>
    <p>This tutorial will guide you through integrating WebSocket support into your id Tech 3 application. WebSockets enable real-time bidirectional communication between the game client/server and web services.</p>
    
    <div class="feature-list">
        <h3>What You'll Learn</h3>
        <ul>
            <li>Installing and configuring WebSocket support</li>
            <li>Connecting to WebSocket servers</li>
            <li>Sending and receiving messages</li>
            <li>Handling connection events</li>
            <li>Implementing auto-reconnect</li>
            <li>Using WebSocket CVars</li>
            <li>Building a simple WebSocket server</li>
            <li>Troubleshooting common issues</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Prerequisites</h2>
    <ul>
        <li>id Tech 3 engine with WebSocket support compiled</li>
        <li>libwebsockets library installed</li>
        <li>Basic understanding of network programming</li>
        <li>Access to a WebSocket server (or ability to create one)</li>
    </ul>
</div>

<div class="section">
    <h2>Tutorial: Installation and Setup</h2>
    
    <h3>Step 1: Install libwebsockets</h3>
    <p>WebSocket support requires libwebsockets. Install it:</p>
    
    <h4>Linux (Ubuntu/Debian)</h4>
    <div class="code-block">
        <pre><code>sudo apt-get update
sudo apt-get install libwebsockets-dev</code></pre>
    </div>
    
    <h4>Linux (Fedora/RHEL)</h4>
    <div class="code-block">
        <pre><code>sudo dnf install libwebsockets-devel</code></pre>
    </div>
    
    <h4>macOS (Homebrew)</h4>
    <div class="code-block">
        <pre><code>brew install libwebsockets</code></pre>
    </div>
    
    <h4>Windows</h4>
    <p>Download pre-built binaries or build from source from <a href="https://libwebsockets.org/">libwebsockets.org</a></p>
    
    <h3>Step 2: Build id Tech 3 with WebSocket Support</h3>
    <div class="code-block">
        <pre><code>cd build
cmake .. -DENABLE_WEBSOCKET=ON
make</code></pre>
    </div>
    
    <h3>Step 3: Verify Installation</h3>
    <p>Check that WebSocket support is available:</p>
    <div class="code-block">
        <pre><code>./idtech3.x86_64 +set cl_websocket_enable 1
# Check console for WebSocket initialization messages</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tutorial: Basic Connection</h2>
    
    <h3>Step 1: Enable WebSocket Support</h3>
    <div class="code-block">
        <pre><code># Enable WebSocket client
/set cl_websocket_enable 1</code></pre>
    </div>
    
    <h3>Step 2: Connect to a WebSocket Server</h3>
    <p>Connect using the WebSocket URL format:</p>
    <div class="code-block">
        <pre><code># Connect to a WebSocket server
/websocket_connect ws://localhost:8080/game

# Connect with SSL/TLS
/websocket_connect wss://example.com/game

# Connect with authentication
/websocket_connect ws://user:pass@example.com/game</code></pre>
    </div>
    
    <h3>Step 3: Check Connection Status</h3>
    <p>Verify the connection was established:</p>
    <div class="code-block">
        <pre><code># Check connection status
/websocket_status

# Or use ImGui network overlay
/set cl_imgui_debug_network 1</code></pre>
    </div>
    
    <h3>Step 4: Disconnect</h3>
    <div class="code-block">
        <pre><code># Disconnect from WebSocket server
/websocket_disconnect</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tutorial: Sending and Receiving Messages</h2>
    
    <h3>Step 1: Send a Message</h3>
    <p>Send text messages to the WebSocket server:</p>
    <div class="code-block">
        <pre><code># Send a simple message
/websocket_send Hello, Server!

# Send JSON data
/websocket_send {"type":"ping","timestamp":1234567890}

# Send from console script
/websocket_send {"action":"join","room":"lobby"}</code></pre>
    </div>
    
    <h3>Step 2: Receive Messages</h3>
    <p>Messages from the server are displayed in the console. Enable structured logging to capture them:</p>
    <div class="code-block">
        <pre><code># Enable logging to see received messages
/set log_enable 1
/set log_category_filter network

# Messages will appear in console and log file
# Format: [WebSocket] Received: message content</code></pre>
    </div>
    
    <h3>Step 3: Handle Messages in Code</h3>
    <p>In your game code, handle WebSocket messages:</p>
    <div class="code-block">
        <pre><code>// Example: Handle WebSocket message in client code
void CL_WebSocket_MessageReceived(const char *message) {
    // Parse JSON message
    json_t *json = json_loads(message, 0, NULL);
    if (!json) {
        Com_Printf("Invalid JSON received\n");
        return;
    }
    
    // Extract message type
    json_t *type = json_object_get(json, "type");
    if (type && json_is_string(type)) {
        const char *msg_type = json_string_value(type);
        
        if (strcmp(msg_type, "player_update") == 0) {
            // Handle player update
            HandlePlayerUpdate(json);
        } else if (strcmp(msg_type, "chat") == 0) {
            // Handle chat message
            HandleChatMessage(json);
        }
    }
    
    json_decref(json);
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tutorial: Connection Events</h2>
    
    <h3>Understanding Connection States</h3>
    <p>WebSocket connections have several states:</p>
    <ul>
        <li><strong>Disconnected:</strong> Not connected</li>
        <li><strong>Connecting:</strong> Connection in progress</li>
        <li><strong>Connected:</strong> Successfully connected</li>
        <li><strong>Reconnecting:</strong> Attempting to reconnect</li>
        <li><strong>Error:</strong> Connection error occurred</li>
    </ul>
    
    <h3>Handling Connection Events</h3>
    <p>Monitor connection state changes:</p>
    <div class="code-block">
        <pre><code>// Example: Handle connection events
void CL_WebSocket_OnConnect(void) {
    Com_Printf("WebSocket connected successfully\n");
    // Send initial handshake or authentication
    CL_WebSocket_Send("{\"type\":\"auth\",\"token\":\"...\"}");
}

void CL_WebSocket_OnDisconnect(void) {
    Com_Printf("WebSocket disconnected\n");
    // Clean up or attempt reconnect
}

void CL_WebSocket_OnError(const char *error) {
    Com_Printf("WebSocket error: %s\n", error);
    // Handle error appropriately
}</code></pre>
    </div>
    
    <h3>Check Connection Status</h3>
    <div class="code-block">
        <pre><code># Check if connected
/websocket_status

# Output shows:
# WebSocket Status: Connected
# URL: ws://localhost:8080/game
# Messages Sent: 42
# Messages Received: 38</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tutorial: Auto-Reconnect</h2>
    
    <h3>Step 1: Enable Auto-Reconnect</h3>
    <p>Configure automatic reconnection on disconnect:</p>
    <div class="code-block">
        <pre><code># Enable auto-reconnect
/set cl_websocket_auto_reconnect 1

# Set reconnect delay (seconds)
/set cl_websocket_reconnect_delay 5

# Set maximum reconnect attempts (0 = unlimited)
/set cl_websocket_max_reconnect_attempts 10</code></pre>
    </div>
    
    <h3>Step 2: Configure Reconnect Behavior</h3>
    <p>Customize reconnect settings:</p>
    <div class="code-block">
        <pre><code># Reconnect immediately (0 seconds)
/set cl_websocket_reconnect_delay 0

# Reconnect after 10 seconds
/set cl_websocket_reconnect_delay 10

# Unlimited reconnect attempts
/set cl_websocket_max_reconnect_attempts 0</code></pre>
    </div>
    
    <h3>Step 3: Monitor Reconnection</h3>
    <p>Watch console for reconnect messages:</p>
    <div class="code-block">
        <pre><code># Enable logging to see reconnect attempts
/set log_enable 1
/set log_category_filter network

# You'll see messages like:
# [WebSocket] Connection lost, reconnecting in 5 seconds...
# [WebSocket] Reconnect attempt 1/10
# [WebSocket] Reconnected successfully</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tutorial: WebSocket CVars</h2>
    
    <h3>Configuration CVars</h3>
    <p>Available WebSocket CVars:</p>
    <table>
        <tr>
            <th>CVar</th>
            <th>Default</th>
            <th>Description</th>
        </tr>
        <tr>
            <td><code>cl_websocket_enable</code></td>
            <td>0</td>
            <td>Enable WebSocket client</td>
        </tr>
        <tr>
            <td><code>cl_websocket_url</code></td>
            <td>""</td>
            <td>WebSocket server URL</td>
        </tr>
        <tr>
            <td><code>cl_websocket_auto_reconnect</code></td>
            <td>1</td>
            <td>Enable automatic reconnection</td>
        </tr>
        <tr>
            <td><code>cl_websocket_reconnect_delay</code></td>
            <td>5</td>
            <td>Seconds to wait before reconnecting</td>
        </tr>
        <tr>
            <td><code>cl_websocket_max_reconnect_attempts</code></td>
            <td>10</td>
            <td>Maximum reconnect attempts (0 = unlimited)</td>
        </tr>
        <tr>
            <td><code>cl_websocket_timeout</code></td>
            <td>30</td>
            <td>Connection timeout in seconds</td>
        </tr>
    </table>
    
    <h3>Example Configuration</h3>
    <p>Add to your <code>autoexec.cfg</code>:</p>
    <div class="code-block">
        <pre><code>// WebSocket Configuration
set cl_websocket_enable 1
set cl_websocket_url "ws://localhost:8080/game"
set cl_websocket_auto_reconnect 1
set cl_websocket_reconnect_delay 5
set cl_websocket_max_reconnect_attempts 10
set cl_websocket_timeout 30</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tutorial: Building a Simple WebSocket Server</h2>
    
    <h3>Step 1: Node.js Server Example</h3>
    <p>Create a simple WebSocket server using Node.js:</p>
    <div class="code-block">
        <pre><code>// server.js
const WebSocket = require('ws');

const wss = new WebSocket.Server({ port: 8080 });

wss.on('connection', function connection(ws) {
    console.log('Client connected');
    
    // Send welcome message
    ws.send(JSON.stringify({
        type: 'welcome',
        message: 'Connected to game server'
    }));
    
    // Handle incoming messages
    ws.on('message', function incoming(message) {
        console.log('Received:', message);
        
        // Echo message back
        ws.send(JSON.stringify({
            type: 'echo',
            original: message.toString()
        }));
    });
    
    // Handle disconnect
    ws.on('close', function() {
        console.log('Client disconnected');
    });
});

console.log('WebSocket server running on ws://localhost:8080');</code></pre>
    </div>
    
    <h3>Step 2: Python Server Example</h3>
    <div class="code-block">
        <pre><code># server.py
import asyncio
import websockets
import json

async def handle_client(websocket, path):
    print("Client connected")
    
    # Send welcome message
    await websocket.send(json.dumps({
        "type": "welcome",
        "message": "Connected to game server"
    }))
    
    try:
        async for message in websocket:
            data = json.loads(message)
            print(f"Received: {data}")
            
            # Echo back
            await websocket.send(json.dumps({
                "type": "echo",
                "original": data
            }))
    except websockets.exceptions.ConnectionClosed:
        print("Client disconnected")

start_server = websockets.serve(handle_client, "localhost", 8080)

asyncio.get_event_loop().run_until_complete(start_server)
asyncio.get_event_loop().run_forever()</code></pre>
    </div>
    
    <h3>Step 3: Test the Connection</h3>
    <div class="code-block">
        <pre><code># Start your server
node server.js
# or
python server.py

# In id Tech 3, connect
/websocket_connect ws://localhost:8080

# Send a test message
/websocket_send {"type":"test","message":"Hello from game!"}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tutorial: Practical Use Cases</h2>
    
    <h3>Use Case 1: Real-Time Chat</h3>
    <p>Implement chat using WebSocket:</p>
    <div class="code-block">
        <pre><code>// Send chat message
void SendChatMessage(const char *message) {
    json_t *json = json_object();
    json_object_set_new(json, "type", json_string("chat"));
    json_object_set_new(json, "message", json_string(message));
    json_object_set_new(json, "player", json_string(CL_GetPlayerName()));
    
    char *json_str = json_dumps(json, 0);
    CL_WebSocket_Send(json_str);
    free(json_str);
    json_decref(json);
}

// Receive chat message
void HandleChatMessage(json_t *json) {
    const char *player = json_string_value(json_object_get(json, "player"));
    const char *message = json_string_value(json_object_get(json, "message"));
    
    // Display in game chat
    CG_AddChatMessage(va("%s: %s", player, message));
}</code></pre>
    </div>
    
    <h3>Use Case 2: Player Statistics</h3>
    <p>Send player stats to a web dashboard:</p>
    <div class="code-block">
        <pre><code>void SendPlayerStats(void) {
    json_t *json = json_object();
    json_object_set_new(json, "type", json_string("stats"));
    json_object_set_new(json, "kills", json_integer(playerStats.kills));
    json_object_set_new(json, "deaths", json_integer(playerStats.deaths));
    json_object_set_new(json, "score", json_integer(playerStats.score));
    
    char *json_str = json_dumps(json, 0);
    CL_WebSocket_Send(json_str);
    free(json_str);
    json_decref(json);
}</code></pre>
    </div>
    
    <h3>Use Case 3: Server Notifications</h3>
    <p>Receive server announcements:</p>
    <div class="code-block">
        <pre><code>void HandleServerNotification(json_t *json) {
    const char *title = json_string_value(json_object_get(json, "title"));
    const char *message = json_string_value(json_object_get(json, "message"));
    int priority = json_integer_value(json_object_get(json, "priority"));
    
    // Display notification in game
    CG_ShowNotification(title, message, priority);
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tutorial: Troubleshooting</h2>
    
    <h3>Connection Fails</h3>
    <p><strong>Problem:</strong> Cannot connect to WebSocket server.</p>
    <p><strong>Solutions:</strong></p>
    <ul>
        <li>Verify server is running and accessible</li>
        <li>Check URL format (ws:// or wss://)</li>
        <li>Check firewall settings</li>
        <li>Verify port is correct</li>
        <li>Check SSL certificate for wss:// connections</li>
        <li>Review console for error messages</li>
    </ul>
    
    <h3>Messages Not Received</h3>
    <p><strong>Problem:</strong> Messages sent but not received.</p>
    <p><strong>Solutions:</strong></p>
    <ul>
        <li>Enable network logging: <code>/set log_category_filter network</code></li>
        <li>Verify connection status: <code>/websocket_status</code></li>
        <li>Check server logs for received messages</li>
        <li>Verify message format is correct</li>
        <li>Check for connection drops</li>
    </ul>
    
    <h3>High Latency</h3>
    <p><strong>Problem:</strong> WebSocket messages have high latency.</p>
    <p><strong>Solutions:</strong></p>
    <ul>
        <li>Check network connection quality</li>
        <li>Use server closer to clients</li>
        <li>Optimize message size</li>
        <li>Check server processing time</li>
        <li>Consider using binary frames for large data</li>
    </ul>
    
    <h3>Reconnection Issues</h3>
    <p><strong>Problem:</strong> Auto-reconnect not working.</p>
    <p><strong>Solutions:</strong></p>
    <ul>
        <li>Verify <code>cl_websocket_auto_reconnect</code> is enabled</li>
        <li>Check reconnect delay setting</li>
        <li>Verify max reconnect attempts isn't exceeded</li>
        <li>Check server allows reconnections</li>
        <li>Review connection logs</li>
    </ul>
</div>

<div class="section">
    <h2>Best Practices</h2>
    <ul>
        <li><strong>Use JSON for Messages:</strong> Structured data is easier to parse and extend</li>
        <li><strong>Handle Errors Gracefully:</strong> Always check connection status before sending</li>
        <li><strong>Enable Auto-Reconnect:</strong> Improves user experience on network issues</li>
        <li><strong>Log WebSocket Events:</strong> Helps with debugging and monitoring</li>
        <li><strong>Use WSS in Production:</strong> Encrypt connections for security</li>
        <li><strong>Validate Messages:</strong> Always validate received message format</li>
        <li><strong>Rate Limit Messages:</strong> Prevent flooding the server</li>
        <li><strong>Monitor Connection:</strong> Use status commands to monitor health</li>
    </ul>
</div>

<div class="section">
    <h2>Security Considerations</h2>
    <ul>
        <li><strong>Use WSS:</strong> Always use encrypted connections (wss://) in production</li>
        <li><strong>Authenticate:</strong> Implement authentication in your WebSocket protocol</li>
        <li><strong>Validate Input:</strong> Always validate and sanitize received messages</li>
        <li><strong>Rate Limiting:</strong> Implement rate limiting to prevent abuse</li>
        <li><strong>Origin Checking:</strong> Verify WebSocket origin on server side</li>
        <li><strong>Secure Tokens:</strong> Use secure tokens for authentication</li>
    </ul>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="networking/websocket">WebSocket Documentation</a> - Complete reference</li>
        <li><a href="networking/networking">Networking Enhancements</a> - Other networking features</li>
        <li><a href="core/structured-logging">Structured Logging</a> - Logging WebSocket events</li>
        <li><a href="imgui">ImGui Debug Overlays</a> - Monitor WebSocket in overlay</li>
    </ul>
</div>

