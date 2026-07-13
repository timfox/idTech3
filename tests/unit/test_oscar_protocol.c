#include "net_oscar_protocol.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define EXPECT_TRUE( expr ) \
	do { \
		if ( !( expr ) ) { \
			fprintf( stderr, "%s:%d: expected true: %s\n", __FILE__, __LINE__, #expr ); \
			failures++; \
		} \
	} while ( 0 )

#define EXPECT_FALSE( expr ) \
	do { \
		if ( ( expr ) ) { \
			fprintf( stderr, "%s:%d: expected false: %s\n", __FILE__, __LINE__, #expr ); \
			failures++; \
		} \
	} while ( 0 )

#define EXPECT_STREQ( actual, expected ) \
	do { \
		if ( strcmp( ( actual ), ( expected ) ) != 0 ) { \
			fprintf( stderr, "%s:%d: expected \"%s\", got \"%s\"\n", __FILE__, __LINE__, ( expected ), ( actual ) ); \
			failures++; \
		} \
	} while ( 0 )

#define EXPECT_EQ_INT( actual, expected ) \
	do { \
		if ( ( actual ) != ( expected ) ) { \
			fprintf( stderr, "%s:%d: expected %d, got %d\n", __FILE__, __LINE__, ( expected ), ( actual ) ); \
			failures++; \
		} \
	} while ( 0 )

static void ExpectContains( const char *haystack, const char *needle )
{
	if ( strstr( haystack, needle ) == NULL ) {
		fprintf( stderr, "%s:%d: expected JSON to contain \"%s\", got \"%s\"\n",
			__FILE__, __LINE__, needle, haystack );
		failures++;
	}
}

static void TestBuildAuthEscapesCredentials( void )
{
	char json[512];

	EXPECT_TRUE( OSCAR_ProtocolBuildAuth( json, sizeof( json ), 7, "server\"one", "tok\\en\n2" ) );
	ExpectContains( json, "\"type\":\"authenticate\"" );
	ExpectContains( json, "\"request_id\":7" );
	ExpectContains( json, "\"account\":\"server\\\"one\"" );
	ExpectContains( json, "\"token\":\"tok\\\\en\\n2\"" );
}

static void TestBuildRoomMessageEscapesPayload( void )
{
	char json[512];

	EXPECT_TRUE( OSCAR_ProtocolBuildRoomMessage( json, sizeof( json ), 9, "arena", "sv\"bot", "line1\nline2" ) );
	ExpectContains( json, "\"type\":\"send_room_message\"" );
	ExpectContains( json, "\"room\":\"arena\"" );
	ExpectContains( json, "\"sender\":\"sv\\\"bot\"" );
	ExpectContains( json, "\"text\":\"line1\\nline2\"" );
}

static void TestParseRoomMessage( void )
{
	oscarEvent_t event;

	EXPECT_TRUE( OSCAR_ProtocolParseEvent(
		"{\"type\":\"room_message\",\"room\":\"arena\",\"screen_name\":\"Buddy\",\"text\":\"hello\\nthere\"}",
		&event ) );
	EXPECT_EQ_INT( event.type, OSCAR_EVENT_ROOM_MESSAGE );
	EXPECT_STREQ( event.room, "arena" );
	EXPECT_STREQ( event.screenName, "Buddy" );
	EXPECT_STREQ( event.text, "hello\nthere" );
}

static void TestParseInstantMessage( void )
{
	oscarEvent_t event;

	EXPECT_TRUE( OSCAR_ProtocolParseEvent(
		"{\"type\":\"instant_message\",\"screen_name\":\"Buddy\",\"text\":\"join?\"}",
		&event ) );
	EXPECT_EQ_INT( event.type, OSCAR_EVENT_INSTANT_MESSAGE );
	EXPECT_STREQ( event.screenName, "Buddy" );
	EXPECT_STREQ( event.text, "join?" );
}

static void TestParsePresenceAndRequestComplete( void )
{
	oscarEvent_t event;

	EXPECT_TRUE( OSCAR_ProtocolParseEvent(
		"{\"type\":\"presence_changed\",\"screen_name\":\"Buddy\",\"status\":\"away\",\"away_message\":\"brb\"}",
		&event ) );
	EXPECT_EQ_INT( event.type, OSCAR_EVENT_PRESENCE_CHANGED );
	EXPECT_STREQ( event.screenName, "Buddy" );
	EXPECT_STREQ( event.status, "away" );
	EXPECT_STREQ( event.text, "brb" );

	EXPECT_TRUE( OSCAR_ProtocolParseEvent(
		"{\"type\":\"request_complete\",\"request_id\":42,\"ok\":true}",
		&event ) );
	EXPECT_EQ_INT( event.type, OSCAR_EVENT_REQUEST_COMPLETE );
	EXPECT_EQ_INT( event.requestId, 42 );
	EXPECT_TRUE( event.ok );
}

static void TestRejectsMalformedOrOversizedEvents( void )
{
	char oversized[OSCAR_MAX_JSON_FRAME + 32];
	oscarEvent_t event;

	EXPECT_FALSE( OSCAR_ProtocolParseEvent( "{\"room\":\"arena\"}", &event ) );
	EXPECT_FALSE( OSCAR_ProtocolParseEvent( "{\"type\":\"unknown\"}", &event ) );

	memset( oversized, 'x', sizeof( oversized ) - 1 );
	oversized[sizeof( oversized ) - 1] = '\0';
	EXPECT_FALSE( OSCAR_ProtocolParseEvent( oversized, &event ) );
}

int main( void )
{
	TestBuildAuthEscapesCredentials();
	TestBuildRoomMessageEscapesPayload();
	TestParseRoomMessage();
	TestParseInstantMessage();
	TestParsePresenceAndRequestComplete();
	TestRejectsMalformedOrOversizedEvents();

	if ( failures != 0 ) {
		fprintf( stderr, "unit_oscar_protocol: %d failure(s)\n", failures );
		return 1;
	}

	printf( "unit_oscar_protocol: ok\n" );
	return 0;
}
