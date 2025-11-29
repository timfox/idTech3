<?php
/**
 * WebSocket Support Documentation
 */
$title = 'WebSocket Support - id Tech 3 Documentation';
$breadcrumbs = [
    '/networking' => 'Networking',
    '/networking/websocket' => 'WebSocket Support'
];
?>

<h1>WebSocket Support</h1>

<div class="section">
    <h2>Overview</h2>
    <p>WebSocket support enables real-time bidirectional communication between the game client and servers. This is useful for:</p>
    <ul>
        <li>Real-time chat systems</li>
        <li>Live server updates</li>
        <li>Remote administration</li>
        <li>Real-time statistics</li>
        <li>Push notifications</li>
    </ul>
    
    <p>The WebSocket implementation uses <strong>libwebsockets</strong>, a lightweight C library that provides both client and server capabilities.</p>
</div>

<div class="section">
    <h2>Features</h2>
    
    <h3>Implemented</h3>
    <ul>
        <li><strong>Client Connections:</strong> Connect to WebSocket servers (ws:// and wss://)</li>
        <li><strong>Automatic Reconnection:</strong> Automatic reconnection with exponential backoff</li>
        <li><strong>Connection State Management:</strong> Track connection state</li>
        <li><strong>Message Handling:</strong> Send text messages and receive messages via callbacks</li>
        <li><strong>Event Callbacks:</strong> Connect, disconnect, and error callbacks</li>
        <li><strong>Configuration:</strong> Enable/disable WebSocket support, configure auto-reconnect behavior</li>
        <li><strong>Multiple Connections:</strong> Support for up to 16 concurrent connections</li>
        <li><strong>Integration:</strong> Integrated with engine event loop, automatic service calls every frame</li>
    </ul>
</div>

<div class="section">
    <h2>Installation</h2>
    
    <h3>Linux</h3>
    <div class="code-block">
        <pre><code>sudo apt-get install libwebsockets-dev</code></pre>
    </div>
    
    <h3>Building from Source</h3>
    <p>If your distribution doesn't have libwebsockets, build from source:</p>
    <div class="code-block">
        <pre><code>git clone https://github.com/warmcat/libwebsockets.git
cd libwebsockets
mkdir build && cd build
cmake ..
make
sudo make install</code></pre>
    </div>
</div>

<div class="section">
    <h2>Usage</h2>
    
    <h3>Basic Example</h3>
    <div class="code-block">
        <pre><code>#include "cl_net_enhanced.h"

// Define callbacks
void on_message(const char *data, int len, void *user_data) {
    Com_Printf("Received: %.*s\n", len, data);
}

void on_connect(void *user_data) {
    Com_Printf("WebSocket connected!\n");
}

void on_disconnect(void *user_data) {
    Com_Printf("WebSocket disconnected!\n");
}

void on_error(const char *error, void *user_data) {
    Com_Printf("WebSocket error: %s\n", error);
}

// Connect to WebSocket server
net_websocket_t ws;
NET_WebSocket_SetCallbacks(&ws, on_message, on_connect, on_disconnect, on_error, NULL);

if (NET_WebSocket_Connect("ws://example.com:8080/chat", &ws)) {
    Com_Printf("Connection initiated\n");
}

// Send a message
if (NET_WebSocket_IsConnected(&ws)) {
    NET_WebSocket_Send(&ws, "Hello, server!", 14);
}

// Disconnect
NET_WebSocket_Disconnect(&ws);</code></pre>
    </div>
    
    <h3>CVars</h3>
    <table class="settings-table">
        <tr>
            <th>CVar</th>
            <th>Default</th>
            <th>Description</th>
        </tr>
        <tr>
            <td><code>cl_websocket_enable</code></td>
            <td>1</td>
            <td>Enable WebSocket support</td>
        </tr>
        <tr>
            <td><code>cl_websocket_auto_reconnect</code></td>
            <td>1</td>
            <td>Automatically reconnect on disconnect</td>
        </tr>
    </table>
</div>

<div class="section">
    <h2>API Reference</h2>
    
    <h3>NET_WebSocket_Init()</h3>
    <p>Initialize WebSocket support. Called automatically if <code>cl_websocket_enable</code> is set.</p>
    
    <h3>NET_WebSocket_Shutdown()</h3>
    <p>Shutdown WebSocket support and close all connections.</p>
    
    <h3>NET_WebSocket_Connect(const char *url, net_websocket_t *ws)</h3>
    <p>Connect to a WebSocket server.</p>
    <p><strong>Parameters:</strong></p>
    <ul>
        <li><code>url</code> - WebSocket URL (must start with <code>ws://</code> or <code>wss://</code>)</li>
        <li><code>ws</code> - Pointer to websocket structure</li>
    </ul>
    <p><strong>Returns:</strong> <code>qtrue</code> on success, <code>qfalse</code> on failure</p>
    
    <h3>NET_WebSocket_Disconnect(net_websocket_t *ws)</h3>
    <p>Disconnect from a WebSocket server.</p>
    
    <h3>NET_WebSocket_Send(net_websocket_t *ws, const char *data, int len)</h3>
    <p>Send a text message to the server.</p>
    <p><strong>Parameters:</strong></p>
    <ul>
        <li><code>ws</code> - WebSocket connection</li>
        <li><code>data</code> - Message data</li>
        <li><code>len</code> - Message length (max 4096 bytes)</li>
    </ul>
    <p><strong>Returns:</strong> <code>qtrue</code> on success, <code>qfalse</code> on failure</p>
    
    <h3>NET_WebSocket_IsConnected(net_websocket_t *ws)</h3>
    <p>Check if WebSocket is connected.</p>
    <p><strong>Returns:</strong> <code>qtrue</code> if connected, <code>qfalse</code> otherwise</p>
    
    <h3>NET_WebSocket_Service()</h3>
    <p>Process WebSocket events. Called automatically every frame.</p>
    
    <h3>NET_WebSocket_SetCallbacks(...)</h3>
    <p>Set callback functions for WebSocket events.</p>
    <p><strong>Callbacks:</strong></p>
    <ul>
        <li><code>on_message</code> - Called when a message is received</li>
        <li><code>on_connect</code> - Called when connection is established</li>
        <li><code>on_disconnect</code> - Called when connection is closed</li>
        <li><code>on_error</code> - Called when an error occurs</li>
        <li><code>user_data</code> - User data passed to callbacks</li>
    </ul>
</div>

<div class="section">
    <h2>URL Format</h2>
    <p>WebSocket URLs must follow this format:</p>
    <ul>
        <li><code>ws://hostname:port/path</code> - Unencrypted WebSocket</li>
        <li><code>wss://hostname:port/path</code> - Encrypted WebSocket (TLS)</li>
    </ul>
    
    <h3>Examples</h3>
    <div class="code-block">
        <pre><code>ws://localhost:8080/chat
wss://example.com:443/api
ws://192.168.1.100:9000/game</code></pre>
    </div>
</div>

<div class="section">
    <h2>Auto-Reconnect</h2>
    <p>When auto-reconnect is enabled, the WebSocket will automatically attempt to reconnect after a disconnect. The reconnect delay uses exponential backoff:</p>
    <ul>
        <li>Initial delay: 1 second</li>
        <li>Maximum delay: 30 seconds</li>
        <li>Delay doubles after each failed attempt</li>
    </ul>
</div>

<div class="section">
    <h2>Limitations</h2>
    <ul>
        <li>Maximum message size: 4096 bytes</li>
        <li>Maximum concurrent connections: 16</li>
        <li>Text messages only (binary not yet implemented)</li>
    </ul>
</div>

<div class="section">
    <h2>Security Considerations</h2>
    <ul>
        <li>Use <code>wss://</code> (WebSocket Secure) for encrypted connections</li>
        <li>Validate all received messages</li>
        <li>Implement rate limiting for message sending</li>
        <li>Sanitize user input before sending</li>
    </ul>
</div>

<div class="section">
    <h2>Troubleshooting</h2>
    
    <h3>Connection Fails</h3>
    <ol>
        <li>Check that <code>cl_websocket_enable</code> is set to 1</li>
        <li>Verify the URL format is correct</li>
        <li>Ensure the server is running and accessible</li>
        <li>Check firewall settings</li>
        <li>For <code>wss://</code>, ensure SSL/TLS is properly configured</li>
    </ol>
    
    <h3>Messages Not Received</h3>
    <ol>
        <li>Ensure <code>NET_WebSocket_Service()</code> is being called (automatic in CL_Frame)</li>
        <li>Check that callbacks are properly set</li>
        <li>Verify the server is sending messages correctly</li>
    </ol>
    
    <h3>Auto-Reconnect Not Working</h3>
    <ol>
        <li>Check that <code>cl_websocket_auto_reconnect</code> is set to 1</li>
        <li>Ensure <code>NET_WebSocket_Service()</code> is being called regularly</li>
        <li>Check server logs for connection issues</li>
    </ol>
</div>

<div class="section">
    <h2>Future Enhancements</h2>
    <ul>
        <li>Binary message support</li>
        <li>Subprotocol support</li>
        <li>Compression (permessage-deflate)</li>
        <li>Ping/pong keepalive</li>
        <li>Connection pooling</li>
        <li>Message queuing for offline scenarios</li>
    </ul>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="networking/networking">Networking</a></li>
        <li><a href="core/structured-logging">Structured Logging</a></li>
        <li><a href="development/debugging">Debugging Tools</a></li>
    </ul>
</div>

