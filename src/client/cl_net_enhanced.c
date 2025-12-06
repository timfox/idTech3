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

#ifdef USE_CURL

#include "client.h"
#include "cl_net_enhanced.h"
#include "../qcommon/qcommon.h"

// Static variables
static net_connection_pool_t connection_pool;
static net_rate_limit_t rate_limit;
static net_stats_t net_stats;
static qboolean http2_enabled = qfalse;
static qboolean http2_supported = qfalse;
static int http_version = CURL_HTTP_VERSION_1_1;
static qboolean prefer_ipv6 = qfalse;
static int ip_resolve_mode = CURL_IPRESOLVE_WHATEVER;
qboolean enhanced_initialized = qfalse;  // Exported for cl_main.c

// CVars
cvar_t *cl_http2_enable;
cvar_t *cl_http2_prefer;
cvar_t *cl_connection_pool_enable;
cvar_t *cl_connection_pool_max;
cvar_t *cl_connection_pool_timeout;
cvar_t *cl_rate_limit_enable;
cvar_t *cl_rate_limit_max_per_sec;
cvar_t *cl_rate_limit_max_concurrent;
cvar_t *cl_prefer_ipv6;
cvar_t *cl_net_stats;
#ifdef USE_WEBSOCKETS
cvar_t *cl_websocket_enable;
cvar_t *cl_websocket_auto_reconnect;
#endif

/*
=================
NET_CheckHTTP2Support
=================
*/
static qboolean NET_CheckHTTP2Support(void)
{
	const char *version_info;
	
	if (!clc.cURLEnabled) {
		return qfalse;
	}
	
	version_info = qcurl_version();
	if (!version_info) {
		return qfalse;
	}
	
	// Check if HTTP/2 is supported (requires curl 7.33.0+)
	// HTTP/2 support is indicated by "HTTP2" in version string
	if (strstr(version_info, "HTTP2") != NULL) {
		return qtrue;
	}
	
	// Also check for HTTP/3 (which implies HTTP/2 support)
	if (strstr(version_info, "HTTP3") != NULL) {
		return qtrue;
	}
	
	return qfalse;
}

/*
=================
NET_Enhanced_Init
=================
*/
qboolean NET_Enhanced_Init(void)
{
	if (enhanced_initialized) {
		return qtrue;
	}
	
	if (!clc.cURLEnabled) {
		Com_Printf("NET_Enhanced_Init: cURL not enabled\n");
		return qfalse;
	}
	
	// Check HTTP/2 support
	http2_supported = NET_CheckHTTP2Support();
	if (http2_supported) {
		Com_Printf("HTTP/2 support detected\n");
	} else {
		Com_Printf("HTTP/2 not supported by curl library\n");
	}
	
	// Initialize CVars
	cl_http2_enable = Cvar_Get("cl_http2_enable", "1", CVAR_ARCHIVE | CVAR_LATCH);
	Cvar_SetDescription(cl_http2_enable, "Enable HTTP/2 support for downloads (requires curl with HTTP/2 support)");
	
	cl_http2_prefer = Cvar_Get("cl_http2_prefer", "1", CVAR_ARCHIVE);
	Cvar_SetDescription(cl_http2_prefer, "Prefer HTTP/2 over HTTP/1.1 when both are available");
	
	cl_connection_pool_enable = Cvar_Get("cl_connection_pool_enable", "1", CVAR_ARCHIVE);
	Cvar_SetDescription(cl_connection_pool_enable, "Enable connection pooling for better performance");
	
	cl_connection_pool_max = Cvar_Get("cl_connection_pool_max", "10", CVAR_ARCHIVE);
	Cvar_SetDescription(cl_connection_pool_max, "Maximum number of pooled connections");
	Cvar_CheckRange(cl_connection_pool_max, "1", "50", CV_INTEGER);
	
	cl_connection_pool_timeout = Cvar_Get("cl_connection_pool_timeout", "30000", CVAR_ARCHIVE);
	Cvar_SetDescription(cl_connection_pool_timeout, "Idle connection timeout in milliseconds");
	Cvar_CheckRange(cl_connection_pool_timeout, "1000", "300000", CV_INTEGER);
	
	cl_rate_limit_enable = Cvar_Get("cl_rate_limit_enable", "1", CVAR_ARCHIVE);
	Cvar_SetDescription(cl_rate_limit_enable, "Enable rate limiting for network requests");
	
	cl_rate_limit_max_per_sec = Cvar_Get("cl_rate_limit_max_per_sec", "10", CVAR_ARCHIVE);
	Cvar_SetDescription(cl_rate_limit_max_per_sec, "Maximum requests per second");
	Cvar_CheckRange(cl_rate_limit_max_per_sec, "1", "100", CV_INTEGER);
	
	cl_rate_limit_max_concurrent = Cvar_Get("cl_rate_limit_max_concurrent", "5", CVAR_ARCHIVE);
	Cvar_SetDescription(cl_rate_limit_max_concurrent, "Maximum concurrent requests");
	Cvar_CheckRange(cl_rate_limit_max_concurrent, "1", "20", CV_INTEGER);
	
	cl_prefer_ipv6 = Cvar_Get("cl_prefer_ipv6", "0", CVAR_ARCHIVE);
	Cvar_SetDescription(cl_prefer_ipv6, "Prefer IPv6 connections over IPv4");
	
	cl_net_stats = Cvar_Get("cl_net_stats", "0", CVAR_ARCHIVE);
	Cvar_SetDescription(cl_net_stats, "Show network statistics");
	
#ifdef USE_WEBSOCKETS
	cl_websocket_enable = Cvar_Get("cl_websocket_enable", "1", CVAR_ARCHIVE);
	Cvar_SetDescription(cl_websocket_enable, "Enable WebSocket support");
	
	cl_websocket_auto_reconnect = Cvar_Get("cl_websocket_auto_reconnect", "1", CVAR_ARCHIVE);
	Cvar_SetDescription(cl_websocket_auto_reconnect, "Automatically reconnect WebSocket on disconnect");
	
	// Initialize WebSocket support if enabled
	if (cl_websocket_enable->integer) {
		NET_WebSocket_Init();
	}
#endif
	
	// Initialize connection pool
	if (cl_connection_pool_enable->integer) {
		if (!NET_ConnectionPool_Init(cl_connection_pool_max->integer, 3)) {
			Com_Printf("WARNING: Failed to initialize connection pool\n");
		}
	}
	
	// Initialize rate limiting
	if (cl_rate_limit_enable->integer) {
		if (!NET_RateLimit_Init(cl_rate_limit_max_per_sec->integer, 
		                         cl_rate_limit_max_concurrent->integer)) {
			Com_Printf("WARNING: Failed to initialize rate limiting\n");
		}
	}
	
	// Initialize statistics
	NET_Stats_Init();
	
	// Set HTTP version based on CVar
	if (cl_http2_enable->integer && http2_supported) {
		if (cl_http2_prefer->integer) {
			NET_SetHTTPVersion(NET_HTTP_VERSION_2TLS);
		} else {
			NET_SetHTTPVersion(NET_HTTP_VERSION_1_1);
		}
		http2_enabled = qtrue;
	} else {
		NET_SetHTTPVersion(NET_HTTP_VERSION_1_1);
		http2_enabled = qfalse;
	}
	
	// Set IPv6 preference
	if (cl_prefer_ipv6->integer) {
		NET_PreferIPv6(qtrue);
	}
	
	enhanced_initialized = qtrue;
	Com_Printf("Enhanced networking initialized\n");
	return qtrue;
}

/*
=================
NET_Enhanced_Shutdown
=================
*/
void NET_Enhanced_Shutdown(void)
{
	if (!enhanced_initialized) {
		return;
	}
	
	NET_ConnectionPool_Shutdown();
	NET_RateLimit_Shutdown();
	NET_Stats_Shutdown();
	
	enhanced_initialized = qfalse;
}

/*
=================
NET_EnableHTTP2
=================
*/
qboolean NET_EnableHTTP2(qboolean enable)
{
	if (!http2_supported) {
		return qfalse;
	}
	
	http2_enabled = enable;
	if (enable) {
		if (cl_http2_prefer->integer) {
			NET_SetHTTPVersion(NET_HTTP_VERSION_2TLS);
		} else {
			NET_SetHTTPVersion(NET_HTTP_VERSION_2TLS);
		}
	} else {
		NET_SetHTTPVersion(NET_HTTP_VERSION_1_1);
	}
	
	return qtrue;
}

/*
=================
NET_IsHTTP2Supported
=================
*/
qboolean NET_IsHTTP2Supported(void)
{
	return http2_supported;
}

/*
=================
NET_SetHTTPVersion
=================
*/
void NET_SetHTTPVersion(int version)
{
	http_version = version;
}

/*
=================
NET_ConnectionPool_Init
=================
*/
qboolean NET_ConnectionPool_Init(int max_connections, int max_per_host)
{
	memset(&connection_pool, 0, sizeof(connection_pool));
	connection_pool.max_connections = max_connections;
	connection_pool.max_connections_per_host = max_per_host;
	connection_pool.connection_timeout = cl_connection_pool_timeout->integer;
	connection_pool.enabled = qtrue;
	connection_pool.entries = NULL;
	
	return qtrue;
}

/*
=================
NET_ConnectionPool_Shutdown
=================
*/
void NET_ConnectionPool_Shutdown(void)
{
	net_connection_pool_entry_t *entry, *next;
	
	entry = connection_pool.entries;
	while (entry) {
		next = entry->next;
		if (entry->easy_handle) {
			qcurl_easy_cleanup(entry->easy_handle);
		}
		if (entry->hostname) {
			Z_Free(entry->hostname);
		}
		Z_Free(entry);
		entry = next;
	}
	
	memset(&connection_pool, 0, sizeof(connection_pool));
}

/*
=================
NET_ConnectionPool_Get
=================
*/
CURL *NET_ConnectionPool_Get(const char *hostname, int port)
{
	net_connection_pool_entry_t *entry;
	CURL *easy_handle = NULL;
	int current_time = Sys_Milliseconds();
	int count_per_host = 0;
	
	if (!connection_pool.enabled) {
		return qcurl_easy_init();
	}
	
	// Try to find an available connection
	entry = connection_pool.entries;
	while (entry) {
		if (!entry->in_use && entry->hostname && 
		    !Q_stricmp(entry->hostname, hostname) && 
		    entry->port == port) {
			// Check if connection is still valid (not too old)
			if (current_time - entry->last_used_time < connection_pool.connection_timeout) {
				entry->in_use = qtrue;
				entry->last_used_time = current_time;
				return entry->easy_handle;
			}
		}
		entry = entry->next;
	}
	
	// Count connections for this host
	entry = connection_pool.entries;
	while (entry) {
		if (entry->hostname && !Q_stricmp(entry->hostname, hostname)) {
			count_per_host++;
		}
		entry = entry->next;
	}
	
	// Check if we can create a new connection
	if (count_per_host < connection_pool.max_connections_per_host) {
		// Count total connections
		int total_connections = 0;
		entry = connection_pool.entries;
		while (entry) {
			total_connections++;
			entry = entry->next;
		}
		
		if (total_connections < connection_pool.max_connections) {
			// Create new pooled connection
			easy_handle = qcurl_easy_init();
			if (easy_handle) {
				entry = (net_connection_pool_entry_t *)Z_Malloc(sizeof(net_connection_pool_entry_t));
				memset(entry, 0, sizeof(net_connection_pool_entry_t));
				entry->easy_handle = easy_handle;
				entry->hostname = CopyString(hostname);
				entry->port = port;
				entry->in_use = qtrue;
				entry->last_used_time = current_time;
				entry->next = connection_pool.entries;
				connection_pool.entries = entry;
				
				// Configure connection for reuse
				qcurl_easy_setopt(easy_handle, CURLOPT_FRESH_CONNECT, 0L);
				qcurl_easy_setopt(easy_handle, CURLOPT_FORBID_REUSE, 0L);
				
				return easy_handle;
			}
		}
	}
	
	// Fallback: create non-pooled connection
	return qcurl_easy_init();
}

/*
=================
NET_ConnectionPool_Return
=================
*/
void NET_ConnectionPool_Return(CURL *easy_handle)
{
	net_connection_pool_entry_t *entry;
	
	if (!connection_pool.enabled || !easy_handle) {
		return;
	}
	
	entry = connection_pool.entries;
	while (entry) {
		if (entry->easy_handle == easy_handle) {
			entry->in_use = qfalse;
			entry->last_used_time = Sys_Milliseconds();
			return;
		}
		entry = entry->next;
	}
	
	// Not in pool, cleanup
	qcurl_easy_cleanup(easy_handle);
}

/*
=================
NET_ConnectionPool_CleanupIdle
=================
*/
void NET_ConnectionPool_CleanupIdle(void)
{
	net_connection_pool_entry_t *entry, *prev, *next;
	int current_time = Sys_Milliseconds();
	
	if (!connection_pool.enabled) {
		return;
	}
	
	prev = NULL;
	entry = connection_pool.entries;
	while (entry) {
		next = entry->next;
		
		// Remove idle connections that are too old
		if (!entry->in_use && 
		    (current_time - entry->last_used_time) > connection_pool.connection_timeout) {
			if (entry->easy_handle) {
				qcurl_easy_cleanup(entry->easy_handle);
			}
			if (entry->hostname) {
				Z_Free(entry->hostname);
			}
			
			if (prev) {
				prev->next = next;
			} else {
				connection_pool.entries = next;
			}
			
			Z_Free(entry);
		} else {
			prev = entry;
		}
		
		entry = next;
	}
}

/*
=================
NET_RateLimit_Init
=================
*/
qboolean NET_RateLimit_Init(int max_per_second, int max_concurrent)
{
	memset(&rate_limit, 0, sizeof(rate_limit));
	rate_limit.maxRequestsPerSecond = max_per_second;
	rate_limit.maxConcurrentRequests = max_concurrent;
	rate_limit.requestWindowMs = 1000; // 1 second window
	rate_limit.enabled = qtrue;
	rate_limit.windowStartTime = Sys_Milliseconds();
	
	return qtrue;
}

/*
=================
NET_RateLimit_Shutdown
=================
*/
void NET_RateLimit_Shutdown(void)
{
	memset(&rate_limit, 0, sizeof(rate_limit));
}

/*
=================
NET_RateLimit_Check
=================
*/
qboolean NET_RateLimit_Check(void)
{
	int current_time = Sys_Milliseconds();
	
	if (!rate_limit.enabled) {
		return qtrue;
	}
	
	// Check concurrent requests
	if (rate_limit.concurrentRequests >= rate_limit.maxConcurrentRequests) {
		return qfalse;
	}
	
	// Check requests per second
	if (current_time - rate_limit.windowStartTime >= rate_limit.requestWindowMs) {
		// Reset window
		rate_limit.currentRequests = 0;
		rate_limit.windowStartTime = current_time;
	}
	
	if (rate_limit.currentRequests >= rate_limit.maxRequestsPerSecond) {
		return qfalse;
	}
	
	return qtrue;
}

/*
=================
NET_RateLimit_Update
=================
*/
void NET_RateLimit_Update(void)
{
	if (!rate_limit.enabled) {
		return;
	}
	
	NET_ConnectionPool_CleanupIdle();
}

/*
=================
NET_EnableIPv6
=================
*/
qboolean NET_EnableIPv6(qboolean enable)
{
	// IPv6 support is handled at compile time via USE_IPV6
	// This function just sets the preference
	return qtrue;
}

/*
=================
NET_PreferIPv6
=================
*/
qboolean NET_PreferIPv6(qboolean prefer)
{
	prefer_ipv6 = prefer;
	if (prefer) {
		ip_resolve_mode = CURL_IPRESOLVE_V6;
	} else {
		ip_resolve_mode = CURL_IPRESOLVE_WHATEVER;
	}
	return qtrue;
}

/*
=================
NET_IsIPv6Available
=================
*/
qboolean NET_IsIPv6Available(void)
{
#ifdef USE_IPV6
	return qtrue;
#else
	return qfalse;
#endif
}

/*
=================
NET_SetIPResolve
=================
*/
void NET_SetIPResolve(int resolve_mode)
{
	ip_resolve_mode = resolve_mode;
}

/*
=================
NET_Stats_Init
=================
*/
void NET_Stats_Init(void)
{
	memset(&net_stats, 0, sizeof(net_stats));
}

/*
=================
NET_Stats_Shutdown
=================
*/
void NET_Stats_Shutdown(void)
{
	// Nothing to do
}

/*
=================
NET_Stats_Get
=================
*/
net_stats_t *NET_Stats_Get(void)
{
	return &net_stats;
}

/*
=================
NET_Stats_Reset
=================
*/
void NET_Stats_Reset(void)
{
	memset(&net_stats, 0, sizeof(net_stats));
}

/*
=================
NET_Stats_UpdateRequest
=================
*/
void NET_Stats_UpdateRequest(qboolean success, int response_time, qboolean http2, qboolean ipv6, unsigned long long bytes)
{
	net_stats.requests_total++;
	if (success) {
		net_stats.requests_success++;
	} else {
		net_stats.requests_failed++;
	}
	
	net_stats.last_response_time = response_time;
	if (net_stats.requests_total > 0) {
		// Simple moving average
		net_stats.average_response_time = 
			(net_stats.average_response_time * (net_stats.requests_total - 1) + response_time) / 
			net_stats.requests_total;
	}
	
	if (http2) {
		net_stats.http2_requests++;
	} else {
		net_stats.http1_requests++;
	}
	
	if (ipv6) {
		net_stats.ipv6_requests++;
	} else {
		net_stats.ipv4_requests++;
	}
	
	net_stats.bytes_received += bytes;
}

/*
=================
NET_Enhanced_BeginDownload
=================
*/
qboolean NET_Enhanced_BeginDownload(const char *localName, const char *remoteURL, qboolean use_http2, qboolean prefer_ipv6_setting)
{
	CURL *easy_handle;
	CURLMcode result;
	char hostname[256];
	int port = 80;
	const char *url = remoteURL;
	qboolean is_https = qfalse;
	int start_time = Sys_Milliseconds();
	
	// Parse URL to extract hostname
	if (strncmp(url, "http://", 7) == 0) {
		url += 7;
		port = 80;
	} else if (strncmp(url, "https://", 8) == 0) {
		url += 8;
		port = 443;
		is_https = qtrue;
	}
	
	// Extract hostname
	{
		const char *slash = strchr(url, '/');
		const char *colon = strchr(url, ':');
		int hostname_len;
		
		if (colon && (!slash || colon < slash)) {
			hostname_len = colon - url;
			port = atoi(colon + 1);
		} else if (slash) {
			hostname_len = slash - url;
		} else {
			hostname_len = strlen(url);
		}
		
		if (hostname_len >= sizeof(hostname)) {
			hostname_len = sizeof(hostname) - 1;
		}
		Q_strncpyz(hostname, url, hostname_len + 1);
	}
	
	// Check rate limiting
	if (!NET_RateLimit_Check()) {
		Com_Printf("Rate limit exceeded, please wait\n");
		return qfalse;
	}
	
	// Get connection from pool or create new one
	if (cl_connection_pool_enable->integer) {
		easy_handle = NET_ConnectionPool_Get(hostname, port);
	} else {
		easy_handle = qcurl_easy_init();
	}
	
	if (!easy_handle) {
		Com_Error(ERR_DROP, "NET_Enhanced_BeginDownload: Failed to create curl handle");
		return qfalse;
	}
	
	// Configure curl handle
	if (com_developer->integer) {
		qcurl_easy_setopt(easy_handle, CURLOPT_VERBOSE, 1L);
	}
	
	qcurl_easy_setopt(easy_handle, CURLOPT_URL, remoteURL);
	qcurl_easy_setopt(easy_handle, CURLOPT_TRANSFERTEXT, 0L);
	qcurl_easy_setopt(easy_handle, CURLOPT_USERAGENT, Q3_VERSION);
	
	// Set HTTP version
	if (use_http2 && http2_supported) {
		qcurl_easy_setopt(easy_handle, CURLOPT_HTTP_VERSION, (long)NET_HTTP_VERSION_2TLS);
	} else {
		qcurl_easy_setopt(easy_handle, CURLOPT_HTTP_VERSION, (long)NET_HTTP_VERSION_1_1);
	}
	
	// Set IP resolution preference
	if (prefer_ipv6_setting && NET_IsIPv6Available()) {
		qcurl_easy_setopt(easy_handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6);
	} else {
		qcurl_easy_setopt(easy_handle, CURLOPT_IPRESOLVE, ip_resolve_mode);
	}
	
	// Connection reuse settings
	qcurl_easy_setopt(easy_handle, CURLOPT_FRESH_CONNECT, 0L);
	qcurl_easy_setopt(easy_handle, CURLOPT_FORBID_REUSE, 0L);
	
	// Timeouts
	qcurl_easy_setopt(easy_handle, CURLOPT_CONNECTTIMEOUT, 10L);
	qcurl_easy_setopt(easy_handle, CURLOPT_TIMEOUT, 300L);
	
	// Other options
	qcurl_easy_setopt(easy_handle, CURLOPT_FAILONERROR, 1L);
	qcurl_easy_setopt(easy_handle, CURLOPT_FOLLOWLOCATION, 1L);
	qcurl_easy_setopt(easy_handle, CURLOPT_MAXREDIRS, 5L);
	
#if CURL_AT_LEAST_VERSION(7, 85, 0)
	qcurl_easy_setopt(easy_handle, CURLOPT_PROTOCOLS_STR, ALLOWED_PROTOCOLS_STR);
#else
	qcurl_easy_setopt(easy_handle, CURLOPT_PROTOCOLS, ALLOWED_PROTOCOLS);
#endif
	
	// Use existing download infrastructure
	clc.downloadCURL = easy_handle;
	clc.downloadCURLM = qcurl_multi_init();
	if (!clc.downloadCURLM) {
		if (cl_connection_pool_enable->integer) {
			NET_ConnectionPool_Return(easy_handle);
		} else {
			qcurl_easy_cleanup(easy_handle);
		}
		Com_Error(ERR_DROP, "NET_Enhanced_BeginDownload: multi_init failed");
		return qfalse;
	}
	
	result = qcurl_multi_add_handle(clc.downloadCURLM, easy_handle);
	if (result != CURLM_OK) {
		qcurl_multi_cleanup(clc.downloadCURLM);
		clc.downloadCURLM = NULL;
		if (cl_connection_pool_enable->integer) {
			NET_ConnectionPool_Return(easy_handle);
		} else {
			qcurl_easy_cleanup(easy_handle);
		}
		Com_Error(ERR_DROP, "NET_Enhanced_BeginDownload: multi_add_handle failed: %s",
		          qcurl_multi_strerror(result));
		return qfalse;
	}
	
	// Update rate limit
	rate_limit.concurrentRequests++;
	rate_limit.currentRequests++;
	
	// Update statistics
	NET_Stats_UpdateRequest(qtrue, Sys_Milliseconds() - start_time, use_http2, prefer_ipv6_setting, 0);
	
	return qtrue;
}

/*
=================
NET_Enhanced_PerformDownload
=================
*/
void NET_Enhanced_PerformDownload(void)
{
	// Use existing CL_cURL_PerformDownload logic
	// This is a wrapper that adds statistics tracking
	int start_time = Sys_Milliseconds();
	qboolean http2_used = qfalse;
	qboolean ipv6_used = qfalse;
	long response_code = 0;
	curl_off_t bytes_downloaded = 0;
	
	CL_cURL_PerformDownload();
	
	// Get statistics from curl handle
	if (clc.downloadCURL) {
		long http_version_used;
		if (qcurl_easy_getinfo(clc.downloadCURL, CURLINFO_HTTP_VERSION, &http_version_used) == CURLE_OK) {
			if (http_version_used >= CURL_HTTP_VERSION_2_0) {
				http2_used = qtrue;
			}
		}
		
		qcurl_easy_getinfo(clc.downloadCURL, CURLINFO_RESPONSE_CODE, &response_code);
		qcurl_easy_getinfo(clc.downloadCURL, CURLINFO_SIZE_DOWNLOAD_T, &bytes_downloaded);
		
		// Check if IPv6 was used (this is approximate)
		// We can't easily determine this from curl, so we use the preference setting
		ipv6_used = prefer_ipv6;
	}
	
	// Update statistics
	NET_Stats_UpdateRequest(
		(response_code >= 200 && response_code < 300),
		Sys_Milliseconds() - start_time,
		http2_used,
		ipv6_used,
		(unsigned long long)bytes_downloaded
	);
	
	// Return connection to pool if using pooling
	if (cl_connection_pool_enable->integer && clc.downloadCURL) {
		NET_ConnectionPool_Return(clc.downloadCURL);
		clc.downloadCURL = NULL;
	}
	
	// Update rate limit
	if (rate_limit.concurrentRequests > 0) {
		rate_limit.concurrentRequests--;
	}
}

#ifdef USE_WEBSOCKETS

// WebSocket static variables
static struct lws_context *websocket_context = NULL;
static net_websocket_t *active_websockets[16];  // Support up to 16 concurrent connections
static int num_active_websockets = 0;
static qboolean websocket_initialized = qfalse;

// WebSocket protocol callbacks
static int NET_WebSocket_Callback(struct lws *wsi, enum lws_callback_reasons reason,
	void *user, void *in, size_t len)
{
	net_websocket_t *ws = NULL;
	int i;
	
	// Find the websocket structure
	for (i = 0; i < num_active_websockets; i++) {
		if (active_websockets[i] && active_websockets[i]->wsi == wsi) {
			ws = active_websockets[i];
			break;
		}
	}
	
	if (!ws && reason != LWS_CALLBACK_CLIENT_ESTABLISHED) {
		return 0;
	}
	
	switch (reason) {
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
			// Find websocket by user pointer (set during connection)
			for (i = 0; i < num_active_websockets; i++) {
				if (active_websockets[i] && active_websockets[i]->wsi == NULL) {
					ws = active_websockets[i];
					ws->wsi = wsi;
					break;
				}
			}
			if (ws) {
				ws->connected = qtrue;
				ws->reconnect_attempts = 0;
				ws->reconnect_delay = 1000;  // Reset reconnect delay
				if (ws->on_connect) {
					ws->on_connect(ws->user_data);
				}
				Com_Printf("WebSocket connected to %s\n", ws->url);
			}
			break;
			
		case LWS_CALLBACK_CLIENT_RECEIVE:
			if (ws && ws->on_message && in && len > 0) {
				ws->on_message((const char *)in, (int)len, ws->user_data);
			}
			break;
			
		case LWS_CALLBACK_CLIENT_WRITEABLE:
			// Can write data now - handled by send function
			break;
			
		case LWS_CALLBACK_CLIENT_CLOSED:
		case LWS_CALLBACK_CLOSED:
			if (ws) {
				ws->connected = qfalse;
				ws->wsi = NULL;
				if (ws->on_disconnect) {
					ws->on_disconnect(ws->user_data);
				}
				Com_Printf("WebSocket disconnected from %s\n", ws->url);
				
				// Auto-reconnect if enabled
				if (ws->auto_reconnect) {
					ws->reconnect_attempts++;
					if (ws->reconnect_delay < ws->max_reconnect_delay) {
						ws->reconnect_delay *= 2;  // Exponential backoff
					}
					Com_Printf("WebSocket will reconnect in %d ms (attempt %d)\n",
						ws->reconnect_delay, ws->reconnect_attempts);
				}
			}
			break;
			
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			if (ws && ws->on_error) {
				ws->on_error("Connection error", ws->user_data);
			}
			if (ws) {
				ws->connected = qfalse;
				ws->wsi = NULL;
			}
			break;
			
		default:
			break;
	}
	
	return 0;
}

// WebSocket protocol definition
static struct lws_protocols websocket_protocols[] = {
	{
		"http",
		NET_WebSocket_Callback,
		0,
		4096,  // rx buffer size
	},
	{ NULL, NULL, 0, 0 }
};

#endif /* USE_WEBSOCKETS */

/*
=================
NET_WebSocket_Init
=================
*/
qboolean NET_WebSocket_Init(void)
{
#ifdef USE_WEBSOCKETS
	struct lws_context_creation_info info;
	
	if (websocket_initialized) {
		return qtrue;
	}
	
	memset(&info, 0, sizeof(info));
	info.port = CONTEXT_PORT_NO_LISTEN;
	info.protocols = websocket_protocols;
	info.gid = -1;
	info.uid = -1;
	info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	
	websocket_context = lws_create_context(&info);
	if (!websocket_context) {
		Com_Printf("Failed to create WebSocket context\n");
		return qfalse;
	}
	
	memset(active_websockets, 0, sizeof(active_websockets));
	num_active_websockets = 0;
	websocket_initialized = qtrue;
	
	Com_Printf("WebSocket support initialized\n");
	return qtrue;
#else
	Com_Printf("WebSocket support not compiled in (USE_WEBSOCKETS not defined)\n");
	return qfalse;
#endif
}

/*
=================
NET_WebSocket_Shutdown
=================
*/
void NET_WebSocket_Shutdown(void)
{
#ifdef USE_WEBSOCKETS
	int i;
	
	if (!websocket_initialized) {
		return;
	}
	
	// Disconnect all active connections
	for (i = 0; i < num_active_websockets; i++) {
		if (active_websockets[i]) {
			NET_WebSocket_Disconnect(active_websockets[i]);
		}
	}
	
	if (websocket_context) {
		lws_context_destroy(websocket_context);
		websocket_context = NULL;
	}
	
	memset(active_websockets, 0, sizeof(active_websockets));
	num_active_websockets = 0;
	websocket_initialized = qfalse;
#endif
}

/*
=================
NET_WebSocket_Connect
=================
*/
qboolean NET_WebSocket_Connect(const char *url, net_websocket_t *ws)
{
#ifdef USE_WEBSOCKETS
	struct lws_client_connect_info ccinfo;
	char address[256], path[256];
	int port = 80;
	const char *protocol = "ws://";
	qboolean use_ssl = qfalse;
	
	if (!websocket_initialized) {
		if (!NET_WebSocket_Init()) {
			return qfalse;
		}
	}
	
	if (!ws) {
		Com_Printf("NET_WebSocket_Connect: Invalid websocket pointer\n");
		return qfalse;
	}
	
	// Parse URL
	if (strncmp(url, "ws://", 5) == 0) {
		url += 5;
		protocol = "ws://";
		use_ssl = qfalse;
	} else if (strncmp(url, "wss://", 6) == 0) {
		url += 6;
		protocol = "wss://";
		use_ssl = qtrue;
		port = 443;
	} else {
		Com_Printf("NET_WebSocket_Connect: Invalid URL format (must start with ws:// or wss://)\n");
		return qfalse;
	}
	
	// Extract address and path
	{
		const char *slash = strchr(url, '/');
		const char *colon = strchr(url, ':');
		
		if (colon && (!slash || colon < slash)) {
			int addr_len = colon - url;
			if (addr_len >= sizeof(address)) {
				addr_len = sizeof(address) - 1;
			}
			Q_strncpyz(address, url, addr_len + 1);
			port = atoi(colon + 1);
			if (slash) {
				Q_strncpyz(path, slash, sizeof(path));
			} else {
				Q_strncpyz(path, "/", sizeof(path));
			}
		} else if (slash) {
			int addr_len = slash - url;
			if (addr_len >= sizeof(address)) {
				addr_len = sizeof(address) - 1;
			}
			Q_strncpyz(address, url, addr_len + 1);
			Q_strncpyz(path, slash, sizeof(path));
		} else {
			Q_strncpyz(address, url, sizeof(address));
			Q_strncpyz(path, "/", sizeof(path));
		}
	}
	
	// Initialize websocket structure
	memset(ws, 0, sizeof(net_websocket_t));
	Q_strncpyz(ws->url, url, sizeof(ws->url));
	ws->auto_reconnect = (cl_websocket_auto_reconnect && cl_websocket_auto_reconnect->integer) ? qtrue : qfalse;
	ws->max_reconnect_delay = 30000;  // 30 seconds max
	ws->reconnect_delay = 1000;      // Start with 1 second
	
	// Find free slot
	if (num_active_websockets >= (int)(sizeof(active_websockets) / sizeof(active_websockets[0]))) {
		Com_Printf("NET_WebSocket_Connect: Maximum number of WebSocket connections reached\n");
		return qfalse;
	}
	
	active_websockets[num_active_websockets++] = ws;
	
	// Setup connection info
	memset(&ccinfo, 0, sizeof(ccinfo));
	ccinfo.context = websocket_context;
	ccinfo.address = address;
	ccinfo.port = port;
	ccinfo.path = path;
	ccinfo.host = address;
	ccinfo.origin = address;
	ccinfo.protocol = websocket_protocols[0].name;
	ccinfo.ssl_connection = use_ssl ? 1 : 0;
	
	ws->wsi = lws_client_connect_via_info(&ccinfo);
	if (!ws->wsi) {
		Com_Printf("NET_WebSocket_Connect: Failed to create connection\n");
		// Remove from active list
		for (int i = 0; i < num_active_websockets; i++) {
			if (active_websockets[i] == ws) {
				active_websockets[i] = active_websockets[--num_active_websockets];
				break;
			}
		}
		return qfalse;
	}
	
	ws->context = websocket_context;
	
	return qtrue;
#else
	Com_Printf("WebSocket support not compiled in\n");
	return qfalse;
#endif
}

/*
=================
NET_WebSocket_Disconnect
=================
*/
void NET_WebSocket_Disconnect(net_websocket_t *ws)
{
#ifdef USE_WEBSOCKETS
	int i;
	
	if (!ws) {
		return;
	}
	
	ws->auto_reconnect = qfalse;  // Disable auto-reconnect
	
	if (ws->wsi) {
		lws_close_reason(ws->wsi, LWS_CLOSE_STATUS_NORMAL, NULL, 0);
		ws->wsi = NULL;
	}
	
	ws->connected = qfalse;
	
	// Remove from active list
	for (i = 0; i < num_active_websockets; i++) {
		if (active_websockets[i] == ws) {
			active_websockets[i] = active_websockets[num_active_websockets - 1];
			active_websockets[--num_active_websockets] = NULL;
			break;
		}
	}
#endif
}

/*
=================
NET_WebSocket_Send
=================
*/
qboolean NET_WebSocket_Send(net_websocket_t *ws, const char *data, int len)
{
#ifdef USE_WEBSOCKETS
	unsigned char *buf;
	int ret;
	
	if (!ws || !ws->connected || !ws->wsi) {
		return qfalse;
	}
	
	if (len <= 0 || len > 4096) {
		Com_Printf("NET_WebSocket_Send: Invalid data length %d\n", len);
		return qfalse;
	}
	
	// Allocate buffer with LWS_PRE bytes for libwebsockets
	buf = (unsigned char *)Z_Malloc(LWS_PRE + len + 1);
	if (!buf) {
		return qfalse;
	}
	
	memcpy(buf + LWS_PRE, data, len);
	buf[LWS_PRE + len] = '\0';
	
	ret = lws_write(ws->wsi, buf + LWS_PRE, len, LWS_WRITE_TEXT);
	
	Z_Free(buf);
	
	if (ret < 0) {
		Com_Printf("NET_WebSocket_Send: Failed to send data\n");
		return qfalse;
	}
	
	return qtrue;
#else
	return qfalse;
#endif
}

/*
=================
NET_WebSocket_IsConnected
=================
*/
qboolean NET_WebSocket_IsConnected(net_websocket_t *ws)
{
#ifdef USE_WEBSOCKETS
	if (!ws) {
		return qfalse;
	}
	return ws->connected && ws->wsi != NULL;
#else
	return qfalse;
#endif
}

/*
=================
NET_WebSocket_Service
=================
*/
void NET_WebSocket_Service(void)
{
#ifdef USE_WEBSOCKETS
	int i;
	
	if (!websocket_initialized || !websocket_context) {
		return;
	}
	
	// Service libwebsockets
	lws_service(websocket_context, 0);
	
	// Handle auto-reconnect
	for (i = 0; i < num_active_websockets; i++) {
		net_websocket_t *ws = active_websockets[i];
		if (ws && !ws->connected && ws->auto_reconnect) {
			static int last_reconnect_check[16] = {0};
			int current_time = Sys_Milliseconds();
			
			// Check if it's time to reconnect
			if (current_time - last_reconnect_check[i] >= ws->reconnect_delay) {
				last_reconnect_check[i] = current_time;
				Com_Printf("Attempting to reconnect WebSocket to %s...\n", ws->url);
				NET_WebSocket_Connect(ws->url, ws);
			}
		}
	}
#endif
}

/*
=================
NET_WebSocket_SetCallbacks
=================
*/
void NET_WebSocket_SetCallbacks(net_websocket_t *ws,
	void (*on_message)(const char *data, int len, void *user_data),
	void (*on_connect)(void *user_data),
	void (*on_disconnect)(void *user_data),
	void (*on_error)(const char *error, void *user_data),
	void *user_data)
{
	if (!ws) {
		return;
	}
	
	ws->on_message = on_message;
	ws->on_connect = on_connect;
	ws->on_disconnect = on_disconnect;
	ws->on_error = on_error;
	ws->user_data = user_data;
}

#else
// Keep translation unit non-empty when CURL support is disabled
static const int cl_net_enhanced_stub = 0;
#endif /* USE_CURL */

