# WebSocket Support

This document describes the WebSocket implementation added to the id Tech 3 engine.

## Overview

WebSocket support enables real-time bidirectional communication between the game client and servers. This is useful for:
- Real-time chat systems
- Live server updates
- Remote administration
- Real-time statistics
- Push notifications

## Implementation

The WebSocket implementation uses **libwebsockets**, a lightweight C library that provides both client and server capabilities.

## Features

### ✅ Implemented

1. **Client Connections**
   - Connect to WebSocket servers (ws:// and wss://)
   - Automatic reconnection with exponential backoff
   - Connection state management

2. **Message Handling**
   - Send text messages
   - Receive messages via callbacks
   - Event callbacks (connect, disconnect, error)

3. **Configuration**
   - Enable/disable WebSocket support
   - Configure auto-reconnect behavior
   - Multiple concurrent connections (up to 16)

4. **Integration**
   - Integrated with engine event loop
   - Automatic service calls every frame
   - Proper cleanup on shutdown

## Installation

### Linux

```bash
sudo apt-get install libwebsockets-dev
```

### Building from Source

If your distribution doesn't have libwebsockets, build from source:

```bash
git clone https://github.com/warmcat/libwebsockets.git
cd libwebsockets
mkdir build && cd build
cmake ..
make
sudo make install
```

## Usage

### Basic Example

```c
#include "cl_net_enhanced.h"

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
NET_WebSocket_Disconnect(&ws);
```

### CVars

- `cl_websocket_enable` - Enable WebSocket support (default: 1)
- `cl_websocket_auto_reconnect` - Automatically reconnect on disconnect (default: 1)

### API Reference

#### `NET_WebSocket_Init()`
Initialize WebSocket support. Called automatically if `cl_websocket_enable` is set.

#### `NET_WebSocket_Shutdown()`
Shutdown WebSocket support and close all connections.

#### `NET_WebSocket_Connect(const char *url, net_websocket_t *ws)`
Connect to a WebSocket server.

**Parameters:**
- `url` - WebSocket URL (must start with `ws://` or `wss://`)
- `ws` - Pointer to websocket structure

**Returns:** `qtrue` on success, `qfalse` on failure

#### `NET_WebSocket_Disconnect(net_websocket_t *ws)`
Disconnect from a WebSocket server.

#### `NET_WebSocket_Send(net_websocket_t *ws, const char *data, int len)`
Send a text message to the server.

**Parameters:**
- `ws` - WebSocket connection
- `data` - Message data
- `len` - Message length (max 4096 bytes)

**Returns:** `qtrue` on success, `qfalse` on failure

#### `NET_WebSocket_IsConnected(net_websocket_t *ws)`
Check if WebSocket is connected.

**Returns:** `qtrue` if connected, `qfalse` otherwise

#### `NET_WebSocket_Service()`
Process WebSocket events. Called automatically every frame.

#### `NET_WebSocket_SetCallbacks(...)`
Set callback functions for WebSocket events.

**Callbacks:**
- `on_message` - Called when a message is received
- `on_connect` - Called when connection is established
- `on_disconnect` - Called when connection is closed
- `on_error` - Called when an error occurs
- `user_data` - User data passed to callbacks

## URL Format

WebSocket URLs must follow this format:
- `ws://hostname:port/path` - Unencrypted WebSocket
- `wss://hostname:port/path` - Encrypted WebSocket (TLS)

Examples:
- `ws://localhost:8080/chat`
- `wss://example.com:443/api`
- `ws://192.168.1.100:9000/game`

## Auto-Reconnect

When auto-reconnect is enabled, the WebSocket will automatically attempt to reconnect after a disconnect. The reconnect delay uses exponential backoff:
- Initial delay: 1 second
- Maximum delay: 30 seconds
- Delay doubles after each failed attempt

## Limitations

- Maximum message size: 4096 bytes
- Maximum concurrent connections: 16
- Text messages only (binary not yet implemented)

## Security Considerations

- Use `wss://` (WebSocket Secure) for encrypted connections
- Validate all received messages
- Implement rate limiting for message sending
- Sanitize user input before sending

## Troubleshooting

### Connection Fails

1. Check that `cl_websocket_enable` is set to 1
2. Verify the URL format is correct
3. Ensure the server is running and accessible
4. Check firewall settings
5. For `wss://`, ensure SSL/TLS is properly configured

### Messages Not Received

1. Ensure `NET_WebSocket_Service()` is being called (automatic in CL_Frame)
2. Check that callbacks are properly set
3. Verify the server is sending messages correctly

### Auto-Reconnect Not Working

1. Check that `cl_websocket_auto_reconnect` is set to 1
2. Ensure `NET_WebSocket_Service()` is being called regularly
3. Check server logs for connection issues

## Future Enhancements

- Binary message support
- Subprotocol support
- Compression (permessage-deflate)
- Ping/pong keepalive
- Connection pooling
- Message queuing for offline scenarios

## References

- [libwebsockets Documentation](https://libwebsockets.org/lws-api-doc-main/html/index.html)
- [WebSocket RFC 6455](https://tools.ietf.org/html/rfc6455)
- [WebSocket API (MDN)](https://developer.mozilla.org/en-US/docs/Web/API/WebSocket)

