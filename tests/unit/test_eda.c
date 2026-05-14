/*
 * Unit test: EDA event bus queueing and bounds
 * Run: ctest -R unit_eda
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "game/g_eda.h"
#include "qcommon/qcommon.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

static cvar_t g_eda_stub;
static cvar_t g_eda_log_stub;
static char g_eda_string[4] = "1";
static char g_eda_log_string[4] = "0";
static int stub_g_eda = 1;
static int stub_g_eda_log = 0;
static int telemetry_count;
static char telemetry_name[64];
static double telemetry_value;

static void configure_cvar( cvar_t *cv, const char *name, char *string, int value ) {
	memset( cv, 0, sizeof( *cv ) );
	cv->name = (char *)name;
	snprintf( string, 4, "%d", value );
	cv->string = string;
	cv->resetString = string;
	cv->integer = value;
	cv->value = (float)value;
	cv->flags = CVAR_ARCHIVE_ND;
}

static void reset_stubs( int enabled ) {
	stub_g_eda = enabled;
	stub_g_eda_log = 0;
	telemetry_count = 0;
	telemetry_name[0] = '\0';
	telemetry_value = 0.0;
}

cvar_t *Cvar_Get( const char *var_name, const char *value, int flags ) {
	(void)value;
	(void)flags;
	if ( var_name && strcmp( var_name, "g_eda" ) == 0 ) {
		configure_cvar( &g_eda_stub, "g_eda", g_eda_string, stub_g_eda );
		return &g_eda_stub;
	}
	if ( var_name && strcmp( var_name, "g_edaLog" ) == 0 ) {
		configure_cvar( &g_eda_log_stub, "g_edaLog", g_eda_log_string, stub_g_eda_log );
		return &g_eda_log_stub;
	}
	return NULL;
}

void Cvar_SetDescription( cvar_t *var, const char *var_description ) {
	(void)var;
	(void)var_description;
}

void QDECL Com_DPrintf( const char *fmt, ... ) {
	(void)fmt;
}

void EngineTelemetry_Record( const char *name, double value ) {
	telemetry_count++;
	snprintf( telemetry_name, sizeof( telemetry_name ), "%s", name ? name : "" );
	telemetry_value = value;
}

static int test_disabled_gate( void ) {
	reset_stubs( 0 );
	EDA_Init();

	ASSERT( EDA_IsEnabled() == qfalse, "disabled EDA_IsEnabled" );
	ASSERT( EDA_RegisterChannel( "events" ) == qfalse, "disabled register" );
	ASSERT( EDA_Publish( "events", "payload" ) == qfalse, "disabled publish" );
	ASSERT( EDA_QueueDepth() == 0, "disabled queue depth" );

	EDA_Shutdown();
	return 0;
}

static int test_validation_and_fifo( void ) {
	char channel[EDA_MAX_NAME];
	char payload[EDA_MAX_PAYLOAD];
	edaEventRecord_t drained[2];

	reset_stubs( 1 );
	EDA_Init();

	ASSERT( EDA_RegisterChannel( NULL ) == qfalse, "NULL channel register" );
	ASSERT( EDA_RegisterChannel( "" ) == qfalse, "empty channel register" );
	ASSERT( EDA_Publish( NULL, "payload" ) == qfalse, "NULL channel publish" );
	ASSERT( EDA_Publish( "", "payload" ) == qfalse, "empty channel publish" );

	ASSERT( EDA_RegisterChannel( "AI" ) == qtrue, "register AI" );
	ASSERT( EDA_RegisterChannel( "ai" ) == qtrue, "case-insensitive duplicate register" );
	ASSERT( EDA_Publish( "ai", "first" ) == qtrue, "publish duplicate channel" );
	ASSERT( EDA_Publish( "lua", NULL ) == qtrue, "auto-register null payload channel" );
	ASSERT( EDA_QueueDepth() == 2, "fifo depth after publishes" );

	ASSERT( EDA_Peek( channel, sizeof( channel ), payload, sizeof( payload ) ) == qtrue, "peek first event" );
	ASSERT( strcmp( channel, "AI" ) == 0, "peek preserves registered channel spelling" );
	ASSERT( strcmp( payload, "first" ) == 0, "peek payload" );
	ASSERT( EDA_QueueDepth() == 2, "peek does not consume" );

	ASSERT( EDA_Drain( drained, 2 ) == 2, "drain two events" );
	ASSERT( strcmp( drained[0].channel, "AI" ) == 0, "drain first channel" );
	ASSERT( strcmp( drained[0].payload, "first" ) == 0, "drain first payload" );
	ASSERT( strcmp( drained[1].channel, "lua" ) == 0, "drain second channel" );
	ASSERT( strcmp( drained[1].payload, "" ) == 0, "drain null payload as empty string" );
	ASSERT( EDA_QueueDepth() == 0, "drain consumes queue" );
	ASSERT( EDA_Pop( channel, sizeof( channel ), payload, sizeof( payload ) ) == qfalse, "pop empty queue" );

	EDA_Shutdown();
	return 0;
}

static int test_capacity_and_payload_bounds( void ) {
	char name[EDA_MAX_NAME];
	char payload[EDA_MAX_PAYLOAD * 2];
	char out_channel[EDA_MAX_NAME];
	char out_payload[EDA_MAX_PAYLOAD];
	int i;

	reset_stubs( 1 );
	EDA_Init();

	for ( i = 0; i < EDA_MAX_CHANNELS; i++ ) {
		snprintf( name, sizeof( name ), "ch%02d", i );
		ASSERT( EDA_RegisterChannel( name ) == qtrue, "register channel within capacity" );
	}
	ASSERT( EDA_RegisterChannel( "CH00" ) == qtrue, "duplicate still succeeds at capacity" );
	ASSERT( EDA_RegisterChannel( "overflow" ) == qfalse, "new channel rejected at capacity" );

	memset( payload, 'x', sizeof( payload ) );
	payload[sizeof( payload ) - 1] = '\0';
	ASSERT( EDA_Publish( "ch00", payload ) == qtrue, "publish long payload" );
	ASSERT( EDA_Pop( out_channel, sizeof( out_channel ), out_payload, sizeof( out_payload ) ) == qtrue,
		"pop long payload" );
	ASSERT( strcmp( out_channel, "ch00" ) == 0, "long payload channel" );
	ASSERT( strlen( out_payload ) == EDA_MAX_PAYLOAD - 1, "payload truncated to buffer" );
	ASSERT( out_payload[EDA_MAX_PAYLOAD - 2] == 'x', "payload truncation preserves data" );

	EDA_Shutdown();
	return 0;
}

static int test_full_ring_wraparound( void ) {
	char payload[EDA_MAX_PAYLOAD];
	char out_channel[EDA_MAX_NAME];
	char out_payload[EDA_MAX_PAYLOAD];
	int i;

	reset_stubs( 1 );
	EDA_Init();

	for ( i = 0; i < EDA_MAX_QUEUE; i++ ) {
		snprintf( payload, sizeof( payload ), "event-%03d", i );
		ASSERT( EDA_Publish( "queue", payload ) == qtrue, "publish until queue is full" );
	}
	ASSERT( EDA_QueueDepth() == EDA_MAX_QUEUE, "full queue depth" );
	ASSERT( EDA_Publish( "queue", "overflow" ) == qfalse, "full queue rejects overflow" );
	ASSERT( EDA_QueueDepth() == EDA_MAX_QUEUE, "overflow does not change depth" );

	ASSERT( EDA_Peek( out_channel, sizeof( out_channel ), out_payload, sizeof( out_payload ) ) == qtrue,
		"peek full ring" );
	ASSERT( strcmp( out_channel, "queue" ) == 0, "full ring peek channel" );
	ASSERT( strcmp( out_payload, "event-000" ) == 0, "full ring peek payload" );

	EDA_Frame();
	ASSERT( telemetry_count == 1, "frame records queue depth telemetry" );
	ASSERT( strcmp( telemetry_name, "eda_queue_depth" ) == 0, "telemetry name" );
	ASSERT( telemetry_value == (double)EDA_MAX_QUEUE, "telemetry value" );

	for ( i = 0; i < EDA_MAX_QUEUE; i++ ) {
		snprintf( payload, sizeof( payload ), "event-%03d", i );
		ASSERT( EDA_Pop( out_channel, sizeof( out_channel ), out_payload, sizeof( out_payload ) ) == qtrue,
			"pop full ring event" );
		ASSERT( strcmp( out_channel, "queue" ) == 0, "full ring pop channel" );
		ASSERT( strcmp( out_payload, payload ) == 0, "full ring FIFO payload" );
	}
	ASSERT( EDA_QueueDepth() == 0, "full ring drained depth" );
	ASSERT( EDA_Pop( out_channel, sizeof( out_channel ), out_payload, sizeof( out_payload ) ) == qfalse,
		"pop after draining full ring" );

	EDA_Shutdown();
	return 0;
}

int main( void ) {
	if ( test_disabled_gate() != 0 ) {
		return 1;
	}
	if ( test_validation_and_fifo() != 0 ) {
		return 1;
	}
	if ( test_capacity_and_payload_bounds() != 0 ) {
		return 1;
	}
	if ( test_full_ring_wraparound() != 0 ) {
		return 1;
	}

	printf( "PASS: unit_eda\n" );
	return 0;
}
