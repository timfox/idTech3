/*
===========================================================================
Copyright (C) 2024 Modern Networking Enhancements

This file is part of id Tech 3 engine.

id Tech 3 engine is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

===========================================================================
*/

#ifndef __CL_NET_ENHANCED_H__
#define __CL_NET_ENHANCED_H__

#ifdef USE_CURL

#include "cl_curl.h"
#include "../qcommon/q_shared.h"

// HTTP/2 support
#define NET_HTTP_VERSION_1_1     CURL_HTTP_VERSION_1_1
#define NET_HTTP_VERSION_2       CURL_HTTP_VERSION_2_0
#define NET_HTTP_VERSION_2TLS    CURL_HTTP_VERSION_2TLS
#define NET_HTTP_VERSION_2_PRIOR_KNOWLEDGE CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE
#define NET_HTTP_VERSION_3       CURL_HTTP_VERSION_3

// Rate limiting
typedef struct {
	int maxRequestsPerSecond;      // Maximum requests per second
	int maxConcurrentRequests;     // Maximum concurrent requests
	int requestWindowMs;           // Time window for rate limiting (ms)
	int currentRequests;           // Current requests in window
	int concurrentRequests;        // Current concurrent requests
	int windowStartTime;           // Start time of current window
	qboolean enabled;              // Enable/disable rate limiting
} net_rate_limit_t;

// Connection pool entry
typedef struct net_connection_pool_entry_s {
	CURL *easy_handle;
	char *hostname;
	int port;
	qboolean in_use;
	int last_used_time;
	struct net_connection_pool_entry_s *next;
} net_connection_pool_entry_t;

// Connection pool
typedef struct {
	net_connection_pool_entry_t *entries;
	int max_connections;
	int max_connections_per_host;
	int connection_timeout;         // Timeout before closing idle connections (ms)
	qboolean enabled;
} net_connection_pool_t;

// Network statistics
typedef struct {
	unsigned long long bytes_sent;
	unsigned long long bytes_received;
	unsigned long long requests_total;
	unsigned long long requests_success;
	unsigned long long requests_failed;
	int average_response_time;      // milliseconds
	int last_response_time;         // milliseconds
	int http2_requests;
	int http1_requests;
	int ipv6_requests;
	int ipv4_requests;
} net_stats_t;

// WebSocket connection
#ifdef USE_WEBSOCKETS
#include <libwebsockets.h>
#endif

typedef struct {
	qboolean connected;
	char url[MAX_OSPATH];
	int last_ping_time;
	int reconnect_attempts;
	int reconnect_delay;           // Current reconnect delay in ms
	int max_reconnect_delay;        // Maximum reconnect delay
	qboolean auto_reconnect;       // Auto-reconnect on disconnect
	void *user_data;               // User data pointer
	void (*on_message)(const char *data, int len, void *user_data);
	void (*on_connect)(void *user_data);
	void (*on_disconnect)(void *user_data);
	void (*on_error)(const char *error, void *user_data);
#ifdef USE_WEBSOCKETS
	struct lws *wsi;                // libwebsockets connection handle
	struct lws_context *context;   // libwebsockets context
#endif
} net_websocket_t;

// Enhanced curl context
typedef struct {
	CURL *easy_handle;
	CURLM *multi_handle;
	net_connection_pool_entry_t *pool_entry;
	net_rate_limit_t *rate_limit;
	qboolean use_http2;
	qboolean prefer_ipv6;
	int timeout_ms;
	int connect_timeout_ms;
} net_curl_context_t;

// External variable to check if enhanced networking is initialized
extern qboolean enhanced_initialized;

// Function prototypes
qboolean NET_Enhanced_Init(void);
void NET_Enhanced_Shutdown(void);

// HTTP/2 support
qboolean NET_EnableHTTP2(qboolean enable);
qboolean NET_IsHTTP2Supported(void);
void NET_SetHTTPVersion(int version);

// Connection pooling
qboolean NET_ConnectionPool_Init(int max_connections, int max_per_host);
void NET_ConnectionPool_Shutdown(void);
CURL *NET_ConnectionPool_Get(const char *hostname, int port);
void NET_ConnectionPool_Return(CURL *easy_handle);
void NET_ConnectionPool_CleanupIdle(void);

// Rate limiting
qboolean NET_RateLimit_Init(int max_per_second, int max_concurrent);
void NET_RateLimit_Shutdown(void);
qboolean NET_RateLimit_Check(void);
void NET_RateLimit_Update(void);

// IPv6 improvements
qboolean NET_EnableIPv6(qboolean enable);
qboolean NET_PreferIPv6(qboolean prefer);
qboolean NET_IsIPv6Available(void);
void NET_SetIPResolve(int resolve_mode); // CURL_IPRESOLVE_WHATEVER, CURL_IPRESOLVE_V4, CURL_IPRESOLVE_V6

// Statistics
void NET_Stats_Init(void);
void NET_Stats_Shutdown(void);
net_stats_t *NET_Stats_Get(void);
void NET_Stats_Reset(void);
void NET_Stats_UpdateRequest(qboolean success, int response_time, qboolean http2, qboolean ipv6, unsigned long long bytes);

// Enhanced download functions
qboolean NET_Enhanced_BeginDownload(const char *localName, const char *remoteURL, qboolean use_http2, qboolean prefer_ipv6);
void NET_Enhanced_PerformDownload(void);

// WebSocket
qboolean NET_WebSocket_Init(void);
void NET_WebSocket_Shutdown(void);
qboolean NET_WebSocket_Connect(const char *url, net_websocket_t *ws);
void NET_WebSocket_Disconnect(net_websocket_t *ws);
qboolean NET_WebSocket_Send(net_websocket_t *ws, const char *data, int len);
qboolean NET_WebSocket_IsConnected(net_websocket_t *ws);
void NET_WebSocket_Service(void);  // Call this regularly to process events
void NET_WebSocket_SetCallbacks(net_websocket_t *ws,
	void (*on_message)(const char *data, int len, void *user_data),
	void (*on_connect)(void *user_data),
	void (*on_disconnect)(void *user_data),
	void (*on_error)(const char *error, void *user_data),
	void *user_data);

// CVars
extern cvar_t *cl_http2_enable;
extern cvar_t *cl_http2_prefer;
extern cvar_t *cl_connection_pool_enable;
extern cvar_t *cl_connection_pool_max;
extern cvar_t *cl_connection_pool_timeout;
extern cvar_t *cl_rate_limit_enable;
extern cvar_t *cl_rate_limit_max_per_sec;
extern cvar_t *cl_rate_limit_max_concurrent;
extern cvar_t *cl_prefer_ipv6;
extern cvar_t *cl_net_stats;
#ifdef USE_WEBSOCKETS
extern cvar_t *cl_websocket_enable;
extern cvar_t *cl_websocket_auto_reconnect;
#endif

#endif /* USE_CURL */

#endif /* __CL_NET_ENHANCED_H__ */

