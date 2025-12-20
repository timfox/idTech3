/*
===========================================================================
Telemetry System Implementation
Remote telemetry reporting using CURL
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "q_telemetry.h"

#ifdef USE_CURL

#include "cl_curl.h"
#include <curl/curl.h>
#include <string.h>
#include <time.h>

// Telemetry state
static telemetry_config_t telemetry_config;
static telemetry_data_t last_telemetry_data;
static int last_report_time = 0;
static qboolean telemetry_initialized = qfalse;
static CURL *telemetry_curl_handle = NULL;
static char telemetry_post_buffer[8192];  // JSON buffer for POST data
static char telemetry_response_buffer[4096];  // Response buffer

// CVars
cvar_t *com_telemetry_enable;
cvar_t *com_telemetry_endpoint;
cvar_t *com_telemetry_interval;
cvar_t *com_telemetry_anonymize;

// Forward declarations
static void Telemetry_CollectData( telemetry_data_t *data );
static size_t Telemetry_WriteCallback( void *contents, size_t size, size_t nmemb, void *userp );
static qboolean Telemetry_SendHTTPPost( const char *url, const char *json_data );

/*
===========================================================================
Telemetry_Init
Initialize the telemetry system
===========================================================================
*/
void Telemetry_Init( void )
{
	if ( telemetry_initialized ) {
		return;
	}
	
	Com_Printf( "Initializing telemetry system...\n" );
	
	// Register CVars
	com_telemetry_enable = Cvar_Get( "com_telemetry_enable", "0", CVAR_ARCHIVE | CVAR_LATCH );
	com_telemetry_endpoint = Cvar_Get( "com_telemetry_endpoint", "https://telemetry.example.com/api/report", CVAR_ARCHIVE );
	com_telemetry_interval = Cvar_Get( "com_telemetry_interval", "60", CVAR_ARCHIVE );
	com_telemetry_anonymize = Cvar_Get( "com_telemetry_anonymize", "1", CVAR_ARCHIVE );
	
	// Initialize config from CVars
	Com_Memset( &telemetry_config, 0, sizeof( telemetry_config ) );
	telemetry_config.enabled = com_telemetry_enable->integer != 0;
	Q_strncpyz( telemetry_config.endpoint_url, com_telemetry_endpoint->string, sizeof( telemetry_config.endpoint_url ) );
	telemetry_config.report_interval_seconds = com_telemetry_interval->integer;
	telemetry_config.anonymize_data = com_telemetry_anonymize->integer != 0;
	telemetry_config.collect_performance = qtrue;
	telemetry_config.collect_system_info = qtrue;
	telemetry_config.collect_network_stats = qtrue;
	telemetry_config.collect_game_state = qtrue;
	
	// Initialize CURL handle
	if ( telemetry_config.enabled ) {
		telemetry_curl_handle = qcurl_easy_init();
		if ( !telemetry_curl_handle ) {
			Com_Printf( "WARNING: Failed to initialize CURL for telemetry\n" );
			telemetry_config.enabled = qfalse;
		}
	}
	
	Com_Memset( &last_telemetry_data, 0, sizeof( last_telemetry_data ) );
	last_report_time = 0;
	
	telemetry_initialized = qtrue;
	
	if ( telemetry_config.enabled ) {
		Com_Printf( "Telemetry system initialized (endpoint: %s, interval: %d seconds)\n",
			telemetry_config.endpoint_url, telemetry_config.report_interval_seconds );
	} else {
		Com_Printf( "Telemetry system initialized (disabled)\n" );
	}
}

/*
===========================================================================
Telemetry_Shutdown
Shutdown the telemetry system
===========================================================================
*/
void Telemetry_Shutdown( void )
{
	if ( !telemetry_initialized ) {
		return;
	}
	
	if ( telemetry_curl_handle ) {
		qcurl_easy_cleanup( telemetry_curl_handle );
		telemetry_curl_handle = NULL;
	}
	
	telemetry_initialized = qfalse;
	Com_Printf( "Telemetry system shutdown\n" );
}

/*
===========================================================================
Telemetry_Update
Update telemetry system (called each frame)
===========================================================================
*/
void Telemetry_Update( void )
{
	int current_time;
	
	if ( !telemetry_initialized || !telemetry_config.enabled ) {
		return;
	}
	
	// Update config from CVars (in case they changed)
	telemetry_config.enabled = com_telemetry_enable->integer != 0;
	if ( !telemetry_config.enabled ) {
		return;
	}
	
	Q_strncpyz( telemetry_config.endpoint_url, com_telemetry_endpoint->string, sizeof( telemetry_config.endpoint_url ) );
	telemetry_config.report_interval_seconds = com_telemetry_interval->integer;
	telemetry_config.anonymize_data = com_telemetry_anonymize->integer != 0;
	
	// Check if it's time to send a report
	current_time = Sys_Milliseconds();
	if ( last_report_time == 0 ) {
		last_report_time = current_time;
		return;
	}
	
	if ( ( current_time - last_report_time ) >= ( telemetry_config.report_interval_seconds * 1000 ) ) {
		telemetry_data_t data;
		
		// Collect current telemetry data
		Telemetry_CollectData( &data );
		
		// Send report
		Telemetry_SendReport( &data );
		
		// Update last report time
		last_report_time = current_time;
		last_telemetry_data = data;
	}
}

/*
===========================================================================
Telemetry_CollectData
Collect telemetry data from various engine subsystems
===========================================================================
*/
static void Telemetry_CollectData( telemetry_data_t *data )
{
	extern cvar_t *com_version;
	extern int Sys_GetCPUCount( void );
	extern int com_frameTime;
	extern int com_frameMsec;
	
	Com_Memset( data, 0, sizeof( telemetry_data_t ) );
	
	// Engine info
	if ( com_version ) {
		Q_strncpyz( data->engine_version, com_version->string, sizeof( data->engine_version ) );
	}
	// Build date from compile time
	Q_strncpyz( data->build_date, __DATE__, sizeof( data->build_date ) );
	
	// Platform info
#ifdef __linux__
	Q_strncpyz( data->platform, "Linux", sizeof( data->platform ) );
#elif defined(__APPLE__)
	Q_strncpyz( data->platform, "macOS", sizeof( data->platform ) );
#elif defined(_WIN32)
	Q_strncpyz( data->platform, "Windows", sizeof( data->platform ) );
#else
	Q_strncpyz( data->platform, "Unknown", sizeof( data->platform ) );
#endif

#ifdef __x86_64__
	Q_strncpyz( data->arch, "x86_64", sizeof( data->arch ) );
#elif defined(__i386__)
	Q_strncpyz( data->arch, "x86", sizeof( data->arch ) );
#elif defined(__aarch64__)
	Q_strncpyz( data->arch, "aarch64", sizeof( data->arch ) );
#elif defined(__arm__)
	Q_strncpyz( data->arch, "arm", sizeof( data->arch ) );
#else
	Q_strncpyz( data->arch, "unknown", sizeof( data->arch ) );
#endif
	
	// Performance metrics
	if ( telemetry_config.collect_performance ) {
		if ( com_frameMsec > 0 ) {
			data->fps = 1000.0f / com_frameMsec;
			data->frame_time_ms = com_frameMsec;
		}
		
		// Memory usage (rough estimate)
		extern int Z_AvailableMemory( void );
		int avail_mem = Z_AvailableMemory();
		if ( avail_mem > 0 ) {
			data->memory_total_mb = avail_mem / ( 1024 * 1024 );
		}
		// Note: Used memory would require tracking allocations
		
		// Renderer stats (if available)
		// These are renderer-specific and may not be available in all builds
		// For now, set to 0 if not available
		data->draw_calls = 0;
		data->triangles = 0;
		data->vertices = 0;
		data->textures_loaded = 0;
	}
	
	// Network stats (if available)
	if ( telemetry_config.collect_network_stats ) {
		// These are client-specific and may not be available
		data->ping_ms = 0;
		data->packet_loss = 0;
		// Bandwidth stats would need to be tracked separately
	}
	
	// Game state (if available)
	if ( telemetry_config.collect_game_state ) {
		// These are game-specific and may not be available
		data->map_name[0] = '\0';
		data->num_players = 0;
		data->gametype = 0;
	}
	
	// System info
	if ( telemetry_config.collect_system_info ) {
		data->cpu_count = Sys_GetCPUCount();
		// CPU model and GPU memory would require platform-specific code
	}
	
	// Timestamp
	data->timestamp = (uint64_t)time( NULL );
	
	// Session info (generate if not set)
	if ( data->session_id[0] == '\0' ) {
		// Generate a simple session ID (in production, use proper UUID)
		Com_sprintf( data->session_id, sizeof( data->session_id ), "%llx", (unsigned long long)data->timestamp );
	}
	
	if ( data->session_start_time == 0 ) {
		data->session_start_time = data->timestamp;
	}
	data->session_duration_seconds = (int)( data->timestamp - data->session_start_time );
}

/*
===========================================================================
Telemetry_WriteCallback
CURL write callback for response data
===========================================================================
*/
static size_t Telemetry_WriteCallback( void *contents, size_t size, size_t nmemb, void *userp )
{
	size_t realsize = size * nmemb;
	char *buffer = (char *)userp;
	
	if ( realsize < sizeof( telemetry_response_buffer ) ) {
		Q_strncpyz( buffer, (char *)contents, realsize + 1 );
	}
	
	return realsize;
}

/*
===========================================================================
Telemetry_SendHTTPPost
Send HTTP POST request with JSON data
===========================================================================
*/
static qboolean Telemetry_SendHTTPPost( const char *url, const char *json_data )
{
	CURLcode res;
	long response_code = 0;
	struct curl_slist *headers = NULL;
	
	if ( !telemetry_curl_handle ) {
		telemetry_curl_handle = qcurl_easy_init();
		if ( !telemetry_curl_handle ) {
			return qfalse;
		}
	}
	
	// Set URL
	qcurl_easy_setopt( telemetry_curl_handle, CURLOPT_URL, url );
	
	// Set POST data
	qcurl_easy_setopt( telemetry_curl_handle, CURLOPT_POSTFIELDS, json_data );
	
	// Set headers
	headers = qcurl_slist_append( headers, "Content-Type: application/json" );
	headers = qcurl_slist_append( headers, "User-Agent: idTech3-Telemetry/1.0" );
	qcurl_easy_setopt( telemetry_curl_handle, CURLOPT_HTTPHEADER, headers );
	
	// Set write callback
	qcurl_easy_setopt( telemetry_curl_handle, CURLOPT_WRITEFUNCTION, Telemetry_WriteCallback );
	qcurl_easy_setopt( telemetry_curl_handle, CURLOPT_WRITEDATA, telemetry_response_buffer );
	
	// Timeouts
	qcurl_easy_setopt( telemetry_curl_handle, CURLOPT_CONNECTTIMEOUT, 5L );
	qcurl_easy_setopt( telemetry_curl_handle, CURLOPT_TIMEOUT, 10L );
	
	// Perform request (non-blocking would be better, but simpler for now)
	res = qcurl_easy_perform( telemetry_curl_handle );
	
	// Get response code
	qcurl_easy_getinfo( telemetry_curl_handle, CURLINFO_RESPONSE_CODE, &response_code );
	
	// Cleanup headers
	if ( headers ) {
		qcurl_slist_free_all( headers );
	}
	
	if ( res == CURLE_OK && response_code >= 200 && response_code < 300 ) {
		if ( com_developer->integer ) {
			Com_Printf( "Telemetry: Report sent successfully (HTTP %ld)\n", response_code );
		}
		return qtrue;
	} else {
		if ( com_developer->integer ) {
			Com_Printf( "Telemetry: Failed to send report (error: %s, HTTP %ld)\n",
				qcurl_easy_strerror( res ), response_code );
		}
		return qfalse;
	}
}

/*
===========================================================================
Telemetry_SendReport
Send telemetry report to remote endpoint
===========================================================================
*/
void Telemetry_SendReport( const telemetry_data_t *data )
{
	char json_buffer[8192];
	int len;
	
	if ( !telemetry_config.enabled || !data ) {
		return;
	}
	
	// Build JSON payload
	len = Com_sprintf( json_buffer, sizeof( json_buffer ),
		"{"
		"\"engine_version\":\"%s\","
		"\"build_date\":\"%s\","
		"\"platform\":\"%s\","
		"\"arch\":\"%s\","
		"\"fps\":%.2f,"
		"\"frame_time_ms\":%.2f,"
		"\"memory_total_mb\":%d,"
		"\"draw_calls\":%d,"
		"\"triangles\":%d,"
		"\"vertices\":%d,"
		"\"textures_loaded\":%d,"
		"\"ping_ms\":%d,"
		"\"packet_loss\":%d,"
		"\"map_name\":\"%s\","
		"\"num_players\":%d,"
		"\"gametype\":%d,"
		"\"cpu_count\":%d,"
		"\"timestamp\":%llu,"
		"\"session_id\":\"%s\","
		"\"session_duration_seconds\":%d"
		"}",
		data->engine_version,
		data->build_date,
		data->platform,
		data->arch,
		data->fps,
		data->frame_time_ms,
		data->memory_total_mb,
		data->draw_calls,
		data->triangles,
		data->vertices,
		data->textures_loaded,
		data->ping_ms,
		data->packet_loss,
		data->map_name,
		data->num_players,
		data->gametype,
		data->cpu_count,
		(unsigned long long)data->timestamp,
		data->session_id,
		data->session_duration_seconds
	);
	
	if ( len < 0 || len >= sizeof( json_buffer ) ) {
		Com_Printf( "Telemetry: JSON buffer overflow\n" );
		return;
	}
	
	// Send HTTP POST request
	Telemetry_SendHTTPPost( telemetry_config.endpoint_url, json_buffer );
}

/*
===========================================================================
Telemetry_IsEnabled
Check if telemetry is enabled
===========================================================================
*/
qboolean Telemetry_IsEnabled( void )
{
	return telemetry_initialized && telemetry_config.enabled;
}

/*
===========================================================================
Telemetry_SetEnabled
Enable or disable telemetry
===========================================================================
*/
void Telemetry_SetEnabled( qboolean enabled )
{
	if ( !telemetry_initialized ) {
		return;
	}
	
	telemetry_config.enabled = enabled;
	if ( com_telemetry_enable ) {
		Cvar_Set( "com_telemetry_enable", enabled ? "1" : "0" );
	}
}

/*
===========================================================================
Telemetry_SetEndpoint
Set telemetry endpoint URL
===========================================================================
*/
void Telemetry_SetEndpoint( const char *url )
{
	if ( !telemetry_initialized || !url ) {
		return;
	}
	
	Q_strncpyz( telemetry_config.endpoint_url, url, sizeof( telemetry_config.endpoint_url ) );
	if ( com_telemetry_endpoint ) {
		Cvar_Set( "com_telemetry_endpoint", url );
	}
}

/*
===========================================================================
Telemetry_SetInterval
Set telemetry report interval
===========================================================================
*/
void Telemetry_SetInterval( int seconds )
{
	if ( !telemetry_initialized || seconds < 1 ) {
		return;
	}
	
	telemetry_config.report_interval_seconds = seconds;
	if ( com_telemetry_interval ) {
		Cvar_SetValue( "com_telemetry_interval", seconds );
	}
}

#endif // USE_CURL

