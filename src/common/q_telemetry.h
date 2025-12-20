/*
===========================================================================
Telemetry System Header
Remote telemetry reporting using CURL
===========================================================================
*/

#ifndef __Q_TELEMETRY_H__
#define __Q_TELEMETRY_H__

#include "q_shared.h"

#ifdef USE_CURL

// Telemetry data structure
typedef struct {
	// Engine info
	char engine_version[64];
	char build_date[32];
	char platform[32];
	char arch[16];
	
	// Performance metrics
	float fps;
	float frame_time_ms;
	int memory_used_mb;
	int memory_total_mb;
	
	// Renderer stats
	int draw_calls;
	int triangles;
	int vertices;
	int textures_loaded;
	
	// Network stats
	int ping_ms;
	int packet_loss;
	int bandwidth_in;
	int bandwidth_out;
	
	// Game state
	char map_name[MAX_QPATH];
	int num_players;
	int gametype;
	
	// System info
	int cpu_count;
	char cpu_model[128];
	int gpu_memory_mb;
	
	// Timestamp
	uint64_t timestamp;
	
	// Session info
	char session_id[64];
	uint64_t session_start_time;
	int session_duration_seconds;
} telemetry_data_t;

// Telemetry configuration
typedef struct {
	qboolean enabled;
	char endpoint_url[512];
	int report_interval_seconds;
	qboolean collect_performance;
	qboolean collect_system_info;
	qboolean collect_network_stats;
	qboolean collect_game_state;
	qboolean anonymize_data;
	char user_id[64];  // Optional user identifier (hashed/anonymized)
} telemetry_config_t;

// Function prototypes
void Telemetry_Init( void );
void Telemetry_Shutdown( void );
void Telemetry_Update( void );
void Telemetry_SendReport( const telemetry_data_t *data );
qboolean Telemetry_IsEnabled( void );
void Telemetry_SetEnabled( qboolean enabled );
void Telemetry_SetEndpoint( const char *url );
void Telemetry_SetInterval( int seconds );

// CVars (extern declarations)
extern cvar_t *com_telemetry_enable;
extern cvar_t *com_telemetry_endpoint;
extern cvar_t *com_telemetry_interval;
extern cvar_t *com_telemetry_anonymize;

#endif // USE_CURL

#endif // __Q_TELEMETRY_H__

