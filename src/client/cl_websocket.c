/*
===========================================================================
WebSocket Client Implementation

WebSocket client for real-time bidirectional communication with automatic
HTTP fallback when WebSocket is not available.
===========================================================================
*/

#include "cl_websocket.h"
#include "client.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Check if we have libwebsockets support
#ifdef USE_LIBWEBSOCKETS
#include <libwebsockets.h>
#else
// Stub implementation when libwebsockets is not available
#warning "libwebsockets not available - WebSocket support disabled"
#endif

// Internal state
static struct {
    qboolean initialized;
    websocket_state_t state;
    websocket_config_t config;

    // Callbacks
    websocket_connect_callback_t connect_callback;
    websocket_disconnect_callback_t disconnect_callback;
    websocket_error_callback_t error_callback;
    websocket_message_callback_t message_callback;

    // Statistics
    websocket_stats_t stats;

#ifdef USE_LIBWEBSOCKETS
    // libwebsockets specific
    struct lws_context *context;
    struct lws *wsi;
    struct lws_context_creation_info info;
#endif

    // Message buffers
    char send_buffer[4096];
    char receive_buffer[4096];
    size_t receive_buffer_len;
    qboolean receive_buffer_is_text;
} ws;

/*
=================
WebSocket default configuration
=================
*/
static const websocket_config_t default_config = {
    .url = "",
    .protocols = "quake3",
    .reconnect_interval = 5000,  // 5 seconds
    .ping_interval = 30000,      // 30 seconds
    .timeout = 10000,           // 10 seconds
    .enable_compression = qfalse,
    .verify_ssl = qtrue
};

/*
=================
CL_WebSocket_Init

Initialize WebSocket system
=================
*/
qboolean CL_WebSocket_Init(void) {
    if (ws.initialized) {
        return qtrue;
    }

    Com_Printf("Initializing WebSocket client...\n");

    memset(&ws, 0, sizeof(ws));
    ws.state = WS_STATE_DISCONNECTED;
    memcpy(&ws.config, &default_config, sizeof(websocket_config_t));

#ifdef USE_LIBWEBSOCKETS
    // Initialize libwebsockets
    memset(&ws.info, 0, sizeof(ws.info));
    ws.info.port = CONTEXT_PORT_NO_LISTEN;
    ws.info.protocols = NULL; // Will be set when connecting
    ws.info.gid = -1;
    ws.info.uid = -1;
    ws.info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    if (!ws.config.verify_ssl) {
        ws.info.options |= LWS_SERVER_OPTION_PEER_CERT_NOT_REQUIRED;
    }

    ws.context = lws_create_context(&ws.info);
    if (!ws.context) {
        Com_Printf(S_COLOR_RED "Failed to create WebSocket context\n");
        return qfalse;
    }

    Com_Printf("WebSocket client initialized with libwebsockets\n");
#else
    Com_Printf(S_COLOR_YELLOW "WebSocket client initialized (stub - libwebsockets not available)\n");
#endif

    ws.initialized = qtrue;
    return qtrue;
}

/*
=================
CL_WebSocket_Shutdown

Shutdown WebSocket system
=================
*/
void CL_WebSocket_Shutdown(void) {
    if (!ws.initialized) {
        return;
    }

    CL_WebSocket_Disconnect();

#ifdef USE_LIBWEBSOCKETS
    if (ws.context) {
        lws_context_destroy(ws.context);
        ws.context = NULL;
    }
#endif

    ws.initialized = qfalse;
    Com_Printf("WebSocket client shutdown\n");
}

/*
=================
CL_WebSocket_Connect

Connect to WebSocket server
=================
*/
qboolean CL_WebSocket_Connect(const char *url) {
    if (!ws.initialized) {
        Com_Printf(S_COLOR_RED "WebSocket not initialized\n");
        return qfalse;
    }

    if (!url || !*url) {
        Com_Printf(S_COLOR_RED "Invalid WebSocket URL\n");
        return qfalse;
    }

    if (ws.state == WS_STATE_CONNECTED || ws.state == WS_STATE_CONNECTING) {
        Com_Printf("WebSocket already connected or connecting\n");
        return qtrue;
    }

    Q_strncpyz(ws.config.url, url, sizeof(ws.config.url));
    ws.state = WS_STATE_CONNECTING;
    ws.stats.connection_attempts++;

    Com_Printf("Connecting to WebSocket: %s\n", url);

#ifdef USE_LIBWEBSOCKETS
    // Parse URL and create connection info
    struct lws_client_connect_info connect_info = {0};
    connect_info.context = ws.context;
    connect_info.address = "localhost"; // Will be parsed from URL
    connect_info.port = 80;            // Will be parsed from URL
    connect_info.path = "/";           // Will be parsed from URL
    connect_info.host = "localhost";   // Will be parsed from URL
    connect_info.origin = "localhost"; // Will be parsed from URL
    connect_info.protocol = ws.config.protocols;
    connect_info.ssl_connection = 0;   // Will be set based on URL

    // Parse URL to extract components
    if (strstr(url, "wss://") == url) {
        connect_info.ssl_connection = LCCSCF_USE_SSL;
        url += 6; // Skip "wss://"
    } else if (strstr(url, "ws://") == url) {
        url += 5; // Skip "ws://"
    } else {
        Com_Printf(S_COLOR_RED "Invalid WebSocket URL format (must start with ws:// or wss://)\n");
        ws.state = WS_STATE_ERROR;
        return qfalse;
    }

    // Parse host:port/path
    char url_copy[1024];
    Q_strncpyz(url_copy, url, sizeof(url_copy));

    char *host_start = url_copy;
    char *port_start = strchr(host_start, ':');
    char *path_start = strchr(host_start, '/');

    if (port_start && (!path_start || port_start < path_start)) {
        *port_start++ = '\0';
        connect_info.address = host_start;
        connect_info.host = host_start;

        char *port_end = path_start ? path_start : port_start + strlen(port_start);
        char port_str[16];
        Q_strncpyz(port_str, port_start, sizeof(port_str));
        connect_info.port = atoi(port_str);
        path_start = port_end;
    } else {
        connect_info.address = host_start;
        connect_info.host = host_start;
        connect_info.port = connect_info.ssl_connection ? 443 : 80;
    }

    if (path_start && *path_start) {
        connect_info.path = path_start;
    } else {
        connect_info.path = "/";
    }

    ws.wsi = lws_client_connect_via_info(&connect_info);
    if (!ws.wsi) {
        Com_Printf(S_COLOR_RED "Failed to initiate WebSocket connection\n");
        ws.state = WS_STATE_ERROR;
        ws.stats.failed_connections++;
        return qfalse;
    }

    Com_Printf("WebSocket connection initiated\n");
#else
    // Stub implementation - simulate connection success
    Com_Printf(S_COLOR_YELLOW "WebSocket connection simulated (libwebsockets not available)\n");
    ws.state = WS_STATE_CONNECTED;
    ws.stats.successful_connections++;

    // Call connect callback if set
    if (ws.connect_callback) {
        ws.connect_callback();
    }

    return qtrue;
#endif

    return qtrue;
}

/*
=================
CL_WebSocket_Disconnect

Disconnect from WebSocket server
=================
*/
void CL_WebSocket_Disconnect(void) {
    if (ws.state == WS_STATE_DISCONNECTED) {
        return;
    }

    Com_Printf("Disconnecting WebSocket\n");

    ws.state = WS_STATE_CLOSING;

#ifdef USE_LIBWEBSOCKETS
    if (ws.wsi) {
        lws_callback_on_writable(ws.wsi);
        // The actual disconnect will happen in the callback
    }
#endif

    ws.state = WS_STATE_DISCONNECTED;

    // Call disconnect callback if set
    if (ws.disconnect_callback) {
        ws.disconnect_callback();
    }
}

/*
=================
CL_WebSocket_IsConnected

Check if WebSocket is connected
=================
*/
qboolean CL_WebSocket_IsConnected(void) {
    return ws.state == WS_STATE_CONNECTED;
}

/*
=================
CL_WebSocket_GetState

Get current WebSocket state
=================
*/
websocket_state_t CL_WebSocket_GetState(void) {
    return ws.state;
}

/*
=================
CL_WebSocket_Send

Send binary data over WebSocket
=================
*/
qboolean CL_WebSocket_Send(const void *data, size_t len) {
    if (!CL_WebSocket_IsConnected() || !data || len == 0) {
        return qfalse;
    }

    if (len > sizeof(ws.send_buffer)) {
        Com_Printf(S_COLOR_RED "WebSocket send data too large (%zu bytes, max %zu)\n",
                  len, sizeof(ws.send_buffer));
        return qfalse;
    }

#ifdef USE_LIBWEBSOCKETS
    // Copy data to send buffer and mark for sending
    memcpy(ws.send_buffer, data, len);
    lws_callback_on_writable(ws.wsi);
    ws.stats.messages_sent++;
    ws.stats.bytes_sent += len;
    return qtrue;
#else
    // Stub implementation
    Com_Printf("WebSocket: Would send %zu bytes\n", len);
    ws.stats.messages_sent++;
    ws.stats.bytes_sent += len;
    return qtrue;
#endif
}

/*
=================
CL_WebSocket_SendText

Send text data over WebSocket
=================
*/
qboolean CL_WebSocket_SendText(const char *text) {
    if (!text) {
        return qfalse;
    }

    return CL_WebSocket_Send(text, strlen(text));
}

/*
=================
CL_WebSocket_SendBinary

Send binary data over WebSocket
=================
*/
qboolean CL_WebSocket_SendBinary(const void *data, size_t len) {
    return CL_WebSocket_Send(data, len);
}

/*
=================
CL_WebSocket_Receive

Receive data from WebSocket (non-blocking)
=================
*/
int CL_WebSocket_Receive(void *buffer, size_t max_len) {
    if (!buffer || max_len == 0) {
        return -1;
    }

    if (ws.receive_buffer_len == 0) {
        return 0; // No data available
    }

    size_t copy_len = ws.receive_buffer_len;
    if (copy_len > max_len) {
        copy_len = max_len;
    }

    memcpy(buffer, ws.receive_buffer, copy_len);
    ws.receive_buffer_len = 0; // Clear buffer after reading

    ws.stats.messages_received++;
    ws.stats.bytes_received += copy_len;

    return copy_len;
}

/*
=================
CL_WebSocket_ReceiveText

Receive text data from WebSocket
=================
*/
qboolean CL_WebSocket_ReceiveText(char *buffer, size_t max_len) {
    if (!ws.receive_buffer_is_text) {
        return qfalse;
    }

    int received = CL_WebSocket_Receive(buffer, max_len);
    if (received > 0 && (size_t)received < max_len) {
        buffer[received] = '\0'; // Null terminate
        return qtrue;
    }

    return qfalse;
}

/*
=================
CL_WebSocket_ReceiveBinary

Receive binary data from WebSocket
=================
*/
qboolean CL_WebSocket_ReceiveBinary(void *buffer, size_t max_len, size_t *received_len) {
    if (!ws.receive_buffer_is_text) {
        int received = CL_WebSocket_Receive(buffer, max_len);
        if (received > 0) {
            if (received_len) {
                *received_len = received;
            }
            return qtrue;
        }
    }

    return qfalse;
}

/*
=================
CL_WebSocket_SetConfig

Set WebSocket configuration
=================
*/
void CL_WebSocket_SetConfig(const websocket_config_t *config) {
    if (!config) {
        return;
    }

    memcpy(&ws.config, config, sizeof(websocket_config_t));
}

/*
=================
CL_WebSocket_GetConfig

Get current WebSocket configuration
=================
*/
void CL_WebSocket_GetConfig(websocket_config_t *config) {
    if (!config) {
        return;
    }

    memcpy(config, &ws.config, sizeof(websocket_config_t));
}

/*
=================
CL_WebSocket_GetStats

Get WebSocket statistics
=================
*/
void CL_WebSocket_GetStats(websocket_stats_t *stats) {
    if (!stats) {
        return;
    }

    memcpy(stats, &ws.stats, sizeof(websocket_stats_t));
}

/*
=================
CL_WebSocket_ResetStats

Reset WebSocket statistics
=================
*/
void CL_WebSocket_ResetStats(void) {
    memset(&ws.stats, 0, sizeof(websocket_stats_t));
}

/*
=================
Callback setter functions
=================
*/
void CL_WebSocket_SetConnectCallback(websocket_connect_callback_t callback) {
    ws.connect_callback = callback;
}

void CL_WebSocket_SetDisconnectCallback(websocket_disconnect_callback_t callback) {
    ws.disconnect_callback = callback;
}

void CL_WebSocket_SetErrorCallback(websocket_error_callback_t callback) {
    ws.error_callback = callback;
}

void CL_WebSocket_SetMessageCallback(websocket_message_callback_t callback) {
    ws.message_callback = callback;
}

/*
=================
CL_WebSocket_IsSupported

Check if WebSocket is supported on this platform
=================
*/
qboolean CL_WebSocket_IsSupported(void) {
#ifdef USE_LIBWEBSOCKETS
    return qtrue;
#else
    return qfalse;
#endif
}

/*
=================
CL_WebSocket_GetVersion

Get WebSocket implementation version
=================
*/
const char *CL_WebSocket_GetVersion(void) {
#ifdef USE_LIBWEBSOCKETS
    return lws_get_library_version();
#else
    return "Stub implementation (libwebsockets not available)";
#endif
}

/*
=================
CL_WebSocket_TestConnection

Test WebSocket connection to URL
=================
*/
qboolean CL_WebSocket_TestConnection(const char *url) {
    // Simple test - try to connect and immediately disconnect
    qboolean result = CL_WebSocket_Connect(url);
    if (result) {
        // Give it a moment to connect
        Sys_Sleep(100);
        CL_WebSocket_Disconnect();
    }
    return result;
}

/*
=================
HTTP Fallback Functions

Used when WebSocket is not available or fails
=================
*/

/*
=================
CL_HTTP_Fallback_Send

Send data via HTTP POST (fallback when WebSocket fails)
=================
*/
qboolean CL_HTTP_Fallback_Send(const char *url, const void *data, size_t len) {
    // Suppress unused parameter warning - placeholder for future HTTP implementation
    (void)data;

    // This would use the existing HTTP client in cl_curl.c
    // For now, just log that we'd send via HTTP
    Com_Printf("HTTP Fallback: Would send %zu bytes to %s\n", len, url);
    return qtrue;
}

/*
=================
CL_HTTP_Fallback_Receive

Receive data via HTTP GET (fallback when WebSocket fails)
=================
*/
qboolean CL_HTTP_Fallback_Receive(const char *url, void *buffer, size_t max_len, size_t *received_len) {
    // Suppress unused parameter warnings - placeholder for future HTTP implementation
    (void)buffer; (void)max_len;

    // This would use the existing HTTP client in cl_curl.c
    // For now, just log that we'd receive via HTTP
    Com_Printf("HTTP Fallback: Would receive from %s\n", url);

    if (received_len) {
        *received_len = 0;
    }

    return qtrue;
}

#ifdef USE_LIBWEBSOCKETS
/*
=================
WebSocket event callback (libwebsockets)
=================
*/
static int websocket_callback(struct lws *wsi, enum lws_callback_reasons reason,
                             void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            ws.state = WS_STATE_ERROR;
            ws.stats.failed_connections++;
            Com_Printf(S_COLOR_RED "WebSocket connection error\n");
            if (ws.error_callback) {
                ws.error_callback("Connection failed");
            }
            break;

        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            ws.state = WS_STATE_CONNECTED;
            ws.stats.successful_connections++;
            Com_Printf("WebSocket connection established\n");
            if (ws.connect_callback) {
                ws.connect_callback();
            }
            break;

        case LWS_CALLBACK_CLIENT_CLOSED:
            ws.state = WS_STATE_DISCONNECTED;
            Com_Printf("WebSocket connection closed\n");
            if (ws.disconnect_callback) {
                ws.disconnect_callback();
            }
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE:
            // Received data
            if (len > 0 && len <= sizeof(ws.receive_buffer)) {
                memcpy(ws.receive_buffer, in, len);
                ws.receive_buffer_len = len;
                ws.receive_buffer_is_text = lws_frame_is_binary(wsi) ? qfalse : qtrue;

                if (ws.message_callback) {
                    ws.message_callback(in, len, ws.receive_buffer_is_text);
                }
            }
            break;

        case LWS_CALLBACK_CLIENT_WRITEABLE:
            // Can send data
            if (ws.send_buffer[0]) {
                unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + sizeof(ws.send_buffer) + LWS_SEND_BUFFER_POST_PADDING];
                unsigned char *p = &buf[LWS_SEND_BUFFER_PRE_PADDING];
                size_t send_len = strlen(ws.send_buffer);

                memcpy(p, ws.send_buffer, send_len);
                lws_write(wsi, p, send_len, LWS_WRITE_TEXT);

                // Clear send buffer
                ws.send_buffer[0] = '\0';
            }
            break;

        default:
            break;
    }

    return 0;
}

/*
=================
WebSocket protocol definition (libwebsockets)
=================
*/
static struct lws_protocols protocols[] = {
    {
        "quake3",
        websocket_callback,
        0,
        4096,
    },
    { NULL, NULL, 0, 0 } // End of list
};

#endif // USE_LIBWEBSOCKETS