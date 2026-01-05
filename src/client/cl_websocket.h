/*
===========================================================================
WebSocket Client Header

WebSocket client implementation for real-time bidirectional communication.
Provides automatic fallback to HTTP when WebSocket is not available.
===========================================================================
*/

#ifndef __CL_WEBSOCKET_H__
#define __CL_WEBSOCKET_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "q_shared.h"

// WebSocket connection states
typedef enum {
    WS_STATE_DISCONNECTED,
    WS_STATE_CONNECTING,
    WS_STATE_CONNECTED,
    WS_STATE_CLOSING,
    WS_STATE_ERROR
} websocket_state_t;

// WebSocket connection configuration
typedef struct {
    char url[1024];              // WebSocket URL (ws:// or wss://)
    char protocols[256];         // Protocol list (comma-separated)
    int reconnect_interval;      // Reconnection interval in ms
    int ping_interval;          // Ping interval in ms
    int timeout;                // Connection timeout in ms
    qboolean enable_compression; // Enable per-message-deflate
    qboolean verify_ssl;        // Verify SSL certificates
} websocket_config_t;

// WebSocket statistics
typedef struct {
    int messages_sent;
    int messages_received;
    int bytes_sent;
    int bytes_received;
    int connection_attempts;
    int successful_connections;
    int failed_connections;
    int reconnect_count;
} websocket_stats_t;

// Public API functions
qboolean CL_WebSocket_Init(void);
void CL_WebSocket_Shutdown(void);

qboolean CL_WebSocket_Connect(const char *url);
void CL_WebSocket_Disconnect(void);

qboolean CL_WebSocket_IsConnected(void);
websocket_state_t CL_WebSocket_GetState(void);

qboolean CL_WebSocket_Send(const void *data, size_t len);
qboolean CL_WebSocket_SendText(const char *text);
qboolean CL_WebSocket_SendBinary(const void *data, size_t len);

int CL_WebSocket_Receive(void *buffer, size_t max_len);
qboolean CL_WebSocket_ReceiveText(char *buffer, size_t max_len);
qboolean CL_WebSocket_ReceiveBinary(void *buffer, size_t max_len, size_t *received_len);

void CL_WebSocket_SetConfig(const websocket_config_t *config);
void CL_WebSocket_GetConfig(websocket_config_t *config);

void CL_WebSocket_GetStats(websocket_stats_t *stats);
void CL_WebSocket_ResetStats(void);

// Callback registration for events
typedef void (*websocket_connect_callback_t)(void);
typedef void (*websocket_disconnect_callback_t)(void);
typedef void (*websocket_error_callback_t)(const char *error);
typedef void (*websocket_message_callback_t)(const void *data, size_t len, qboolean is_text);

void CL_WebSocket_SetConnectCallback(websocket_connect_callback_t callback);
void CL_WebSocket_SetDisconnectCallback(websocket_disconnect_callback_t callback);
void CL_WebSocket_SetErrorCallback(websocket_error_callback_t callback);
void CL_WebSocket_SetMessageCallback(websocket_message_callback_t callback);

// Utility functions
qboolean CL_WebSocket_IsSupported(void);
const char *CL_WebSocket_GetVersion(void);
qboolean CL_WebSocket_TestConnection(const char *url);

// HTTP fallback functions (when WebSocket fails)
qboolean CL_HTTP_Fallback_Send(const char *url, const void *data, size_t len);
qboolean CL_HTTP_Fallback_Receive(const char *url, void *buffer, size_t max_len, size_t *received_len);

#ifdef __cplusplus
}
#endif

#endif // __CL_WEBSOCKET_H__