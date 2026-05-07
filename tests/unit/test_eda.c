/*
 * Unit test: in-process EDA event bus queue and channel behavior.
 * Run: ctest -R unit_eda
 */
#include <stdio.h>
#include <string.h>

#include "game/g_eda.h"

void StubEDA_SetEnabled( int enabled );
void StubEDA_SetLog( int enabled );
void StubEDA_ResetTelemetry( void );
int StubEDA_TelemetryCount( void );
const char *StubEDA_LastTelemetryName( void );
double StubEDA_LastTelemetryValue( void );

#define ASSERT(cond, msg) do { \
	if ( !( cond ) ) { \
		fprintf( stderr, "FAIL: %s\n", msg ); \
		return 1; \
	} \
} while ( 0 )

static int expect_string( const char *actual, const char *expected, const char *msg ) {
	if ( strcmp( actual, expected ) != 0 ) {
		fprintf( stderr, "FAIL: %s (got '%s', expected '%s')\n", msg, actual, expected );
		return 1;
	}
	return 0;
}

static int test_disabled_cvar_gates_queue( void ) {
	char channel[EDA_MAX_NAME];
	char payload[EDA_MAX_PAYLOAD];

	StubEDA_SetEnabled( 0 );
	StubEDA_SetLog( 0 );
	StubEDA_ResetTelemetry();
	EDA_Init();

	ASSERT( EDA_IsEnabled() == qfalse, "disabled EDA reports off" );
	ASSERT( EDA_RegisterChannel( "aiml.response" ) == qfalse, "disabled EDA rejects channel registration" );
	ASSERT( EDA_Publish( "aiml.response", "ignored" ) == qfalse, "disabled EDA rejects publishes" );
	ASSERT( EDA_Peek( channel, sizeof( channel ), payload, sizeof( payload ) ) == qfalse, "disabled EDA has no queued event" );
	ASSERT( EDA_QueueDepth() == 0, "disabled EDA queue stays empty" );
	EDA_Frame();
	ASSERT( StubEDA_TelemetryCount() == 0, "disabled EDA does not emit queue telemetry" );

	EDA_Shutdown();
	StubEDA_SetEnabled( 1 );
	return 0;
}

static int test_fifo_peek_drain_and_payload_copy( void ) {
	char channel[EDA_MAX_NAME];
	char payload[EDA_MAX_PAYLOAD];
	char longPayload[EDA_MAX_PAYLOAD + 32];
	edaEventRecord_t drained[4];
	int i;

	EDA_Init();

	ASSERT( EDA_Drain( NULL, 4 ) == 0, "drain rejects NULL output" );
	ASSERT( EDA_Drain( drained, 0 ) == 0, "drain rejects zero output capacity" );
	ASSERT( EDA_Publish( "aiml.response", "first" ) == qtrue, "publish first event" );
	ASSERT( EDA_Publish( "AIML.RESPONSE", "second" ) == qtrue, "publish case-insensitive channel event" );
	ASSERT( EDA_QueueDepth() == 2, "two events queued" );

	ASSERT( EDA_Peek( channel, sizeof( channel ), payload, sizeof( payload ) ) == qtrue, "peek first event" );
	if ( expect_string( channel, "aiml.response", "peek preserves registered channel spelling" ) != 0 ) {
		return 1;
	}
	if ( expect_string( payload, "first", "peek returns first payload" ) != 0 ) {
		return 1;
	}
	ASSERT( EDA_QueueDepth() == 2, "peek does not consume" );

	ASSERT( EDA_Pop( channel, sizeof( channel ), payload, sizeof( payload ) ) == qtrue, "pop first event" );
	if ( expect_string( channel, "aiml.response", "pop returns first channel" ) != 0 ) {
		return 1;
	}
	if ( expect_string( payload, "first", "pop returns first payload" ) != 0 ) {
		return 1;
	}

	ASSERT( EDA_Drain( drained, 4 ) == 1, "drain consumes remaining event" );
	if ( expect_string( drained[0].channel, "aiml.response", "drained event reuses registered channel" ) != 0 ) {
		return 1;
	}
	if ( expect_string( drained[0].payload, "second", "drained event preserves payload" ) != 0 ) {
		return 1;
	}
	ASSERT( EDA_QueueDepth() == 0, "drain leaves queue empty" );

	ASSERT( EDA_Publish( "empty.payload", NULL ) == qtrue, "publish NULL payload as empty string" );
	ASSERT( EDA_Pop( channel, sizeof( channel ), payload, sizeof( payload ) ) == qtrue, "pop NULL-payload event" );
	if ( expect_string( payload, "", "NULL payload is copied as empty string" ) != 0 ) {
		return 1;
	}

	for ( i = 0; i < (int)sizeof( longPayload ) - 1; i++ ) {
		longPayload[i] = (char)( 'a' + ( i % 26 ) );
	}
	longPayload[sizeof( longPayload ) - 1] = '\0';
	ASSERT( EDA_Publish( "long.payload", longPayload ) == qtrue, "publish long payload" );
	ASSERT( EDA_Pop( channel, sizeof( channel ), payload, sizeof( payload ) ) == qtrue, "pop long-payload event" );
	ASSERT( strlen( payload ) == EDA_MAX_PAYLOAD - 1, "payload is bounded to EDA_MAX_PAYLOAD - 1" );
	ASSERT( strncmp( payload, longPayload, EDA_MAX_PAYLOAD - 1 ) == 0, "payload truncation preserves prefix" );

	EDA_Shutdown();
	return 0;
}

static int test_channel_capacity_and_existing_channel_publish( void ) {
	char name[EDA_MAX_NAME];
	int i;

	EDA_Init();

	for ( i = 0; i < EDA_MAX_CHANNELS; i++ ) {
		snprintf( name, sizeof( name ), "channel%02d", i );
		ASSERT( EDA_RegisterChannel( name ) == qtrue, "register channel within capacity" );
	}

	ASSERT( EDA_RegisterChannel( "CHANNEL00" ) == qtrue, "duplicate channel names are accepted case-insensitively" );
	ASSERT( EDA_RegisterChannel( "overflow" ) == qfalse, "new channel past capacity is rejected" );
	ASSERT( EDA_Publish( "CHANNEL31", "existing" ) == qtrue, "existing channel can publish when table is full" );
	ASSERT( EDA_Publish( "overflow", "dropped" ) == qfalse, "unknown channel publish fails when table is full" );
	ASSERT( EDA_QueueDepth() == 1, "only existing-channel publish queued" );

	EDA_Shutdown();
	return 0;
}

static int test_full_queue_wraparound_remains_readable( void ) {
	static edaEventRecord_t drained[EDA_MAX_QUEUE];
	char channel[EDA_MAX_NAME];
	char payload[EDA_MAX_PAYLOAD];
	char expected[EDA_MAX_PAYLOAD];
	int i;

	EDA_Init();

	for ( i = 0; i < EDA_MAX_QUEUE; i++ ) {
		snprintf( expected, sizeof( expected ), "evt%03d", i );
		ASSERT( EDA_Publish( "tick", expected ) == qtrue, "publish event until queue is full" );
	}
	ASSERT( EDA_QueueDepth() == EDA_MAX_QUEUE, "queue reaches full capacity" );
	ASSERT( EDA_Publish( "tick", "overflow" ) == qfalse, "full queue rejects additional event" );

	StubEDA_ResetTelemetry();
	EDA_Frame();
	ASSERT( StubEDA_TelemetryCount() == 1, "full queue emits one telemetry sample" );
	if ( expect_string( StubEDA_LastTelemetryName(), "eda_queue_depth", "telemetry sample name" ) != 0 ) {
		return 1;
	}
	ASSERT( StubEDA_LastTelemetryValue() == (double)EDA_MAX_QUEUE, "telemetry records full queue depth" );

	ASSERT( EDA_Peek( channel, sizeof( channel ), payload, sizeof( payload ) ) == qtrue, "peek works when full ring head equals tail" );
	if ( expect_string( payload, "evt000", "peek full queue returns oldest event" ) != 0 ) {
		return 1;
	}

	for ( i = 0; i < 10; i++ ) {
		snprintf( expected, sizeof( expected ), "evt%03d", i );
		ASSERT( EDA_Pop( channel, sizeof( channel ), payload, sizeof( payload ) ) == qtrue, "pop from full queue" );
		if ( expect_string( payload, expected, "full queue pop preserves FIFO order" ) != 0 ) {
			return 1;
		}
	}
	ASSERT( EDA_QueueDepth() == EDA_MAX_QUEUE - 10, "ten events popped from full queue" );

	for ( i = 0; i < 10; i++ ) {
		snprintf( expected, sizeof( expected ), "wrap%02d", i );
		ASSERT( EDA_Publish( "tick", expected ) == qtrue, "publish after head advances wraps tail" );
	}
	ASSERT( EDA_QueueDepth() == EDA_MAX_QUEUE, "queue refills after wraparound publishes" );
	ASSERT( EDA_Drain( drained, EDA_MAX_QUEUE ) == EDA_MAX_QUEUE, "drain full wrapped queue" );

	for ( i = 0; i < EDA_MAX_QUEUE - 10; i++ ) {
		snprintf( expected, sizeof( expected ), "evt%03d", i + 10 );
		if ( expect_string( drained[i].payload, expected, "drained wrapped queue keeps original FIFO order" ) != 0 ) {
			return 1;
		}
	}
	for ( i = 0; i < 10; i++ ) {
		snprintf( expected, sizeof( expected ), "wrap%02d", i );
		if ( expect_string( drained[EDA_MAX_QUEUE - 10 + i].payload, expected, "drained wrapped queue ends with wrapped publishes" ) != 0 ) {
			return 1;
		}
	}
	ASSERT( EDA_QueueDepth() == 0, "drain empties wrapped queue" );
	ASSERT( EDA_Pop( channel, sizeof( channel ), payload, sizeof( payload ) ) == qfalse, "empty queue pop fails after drain" );

	EDA_Shutdown();
	return 0;
}

int main( void ) {
	if ( test_disabled_cvar_gates_queue() != 0 ) {
		return 1;
	}
	if ( test_fifo_peek_drain_and_payload_copy() != 0 ) {
		return 1;
	}
	if ( test_channel_capacity_and_existing_channel_publish() != 0 ) {
		return 1;
	}
	if ( test_full_queue_wraparound_remains_readable() != 0 ) {
		return 1;
	}

	printf( "PASS: unit_eda\n" );
	return 0;
}
