#include "net_oscar_roster.h"
#include "net_oscar_raw.h"

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

#define EXPECT_EQ_INT( actual, expected ) \
	do { \
		if ( ( actual ) != ( expected ) ) { \
			fprintf( stderr, "%s:%d: expected %d, got %d\n", __FILE__, __LINE__, ( expected ), ( actual ) ); \
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

#define EXPECT_GT( actual, expected ) \
	do { \
		if ( ( actual ) <= ( expected ) ) { \
			fprintf( stderr, "%s:%d: expected %d > %d\n", __FILE__, __LINE__, ( actual ), ( expected ) ); \
			failures++; \
		} \
	} while ( 0 )

static void Put16( byte *p, unsigned int v )
{
	p[0] = (byte)( v >> 8 );
	p[1] = (byte)v;
}

static void Put32( byte *p, unsigned int v )
{
	p[0] = (byte)( v >> 24 );
	p[1] = (byte)( v >> 16 );
	p[2] = (byte)( v >> 8 );
	p[3] = (byte)v;
}

static int AddTLV( byte *buf, int used, unsigned int tag, const void *data, int len )
{
	Put16( buf + used, tag );
	Put16( buf + used + 2, (unsigned int)len );
	used += 4;
	if ( len > 0 ) {
		memcpy( buf + used, data, len );
	}
	return used + len;
}

static int AddUserInfoTLV( byte *body, int used, const char *screenName, unsigned int flags, unsigned int status )
{
	byte value[4];

	body[used++] = (byte)strlen( screenName );
	memcpy( body + used, screenName, strlen( screenName ) );
	used += (int)strlen( screenName );
	Put16( body + used, 0 );
	used += 2;
	Put16( body + used, 2 );
	used += 2;
	value[0] = (byte)( flags >> 8 );
	value[1] = (byte)flags;
	used = AddTLV( body, used, 0x0001, value, 2 );
	Put32( value, status );
	used = AddTLV( body, used, 0x0006, value, 4 );
	return used;
}

static void TestRosterEnsureGetClearGeneration( void )
{
	oscarRoster_t roster;
	oscarBuddy_t buddy;
	unsigned int gen0;

	OSCAR_Roster_Init( &roster );
	gen0 = OSCAR_Roster_Generation( &roster );
	EXPECT_EQ_INT( OSCAR_Roster_Count( &roster ), 0 );
	EXPECT_FALSE( OSCAR_Roster_Get( &roster, 0, &buddy ) );

	EXPECT_EQ_INT( OSCAR_Roster_Ensure( &roster, "Alice" ), 0 );
	EXPECT_EQ_INT( OSCAR_Roster_Count( &roster ), 1 );
	EXPECT_TRUE( OSCAR_Roster_Generation( &roster ) > gen0 );
	EXPECT_TRUE( OSCAR_Roster_Get( &roster, 0, &buddy ) );
	EXPECT_STREQ( buddy.screenName, "Alice" );
	EXPECT_STREQ( buddy.status, "offline" );
	EXPECT_FALSE( buddy.online );

	EXPECT_EQ_INT( OSCAR_Roster_Ensure( &roster, "alice" ), 0 );
	EXPECT_EQ_INT( OSCAR_Roster_Count( &roster ), 1 );

	OSCAR_Roster_Clear( &roster );
	EXPECT_EQ_INT( OSCAR_Roster_Count( &roster ), 0 );
	EXPECT_FALSE( OSCAR_Roster_Get( &roster, 0, &buddy ) );
}

static void TestRosterPresenceFromParsedSnac( void )
{
	byte snacPayload[1024];
	byte body[512];
	oscarRawSnac_t snac;
	oscarEvent_t event;
	oscarRoster_t roster;
	oscarBuddy_t buddy;
	int bodyUsed;
	int used;
	unsigned int gen;

	OSCAR_Roster_Init( &roster );
	bodyUsed = AddUserInfoTLV( body, 0, "Buddy", 0x0020, 0x00000001 );
	Put16( snacPayload, 0x0003 );
	Put16( snacPayload + 2, 0x000b );
	Put16( snacPayload + 4, 0 );
	Put32( snacPayload + 6, 100 );
	used = 10;
	memcpy( snacPayload + used, body, bodyUsed );
	used += bodyUsed;

	EXPECT_TRUE( OSCAR_RawParseSnac( snacPayload, used, &snac ) );
	EXPECT_TRUE( OSCAR_RawParsePresence( &snac, &event ) );
	EXPECT_EQ_INT( event.type, OSCAR_EVENT_PRESENCE_CHANGED );
	EXPECT_STREQ( event.screenName, "Buddy" );
	EXPECT_STREQ( event.status, "away" );

	gen = OSCAR_Roster_Generation( &roster );
	OSCAR_Roster_ApplyPresence( &roster, &event );
	EXPECT_TRUE( OSCAR_Roster_Generation( &roster ) > gen );
	EXPECT_EQ_INT( OSCAR_Roster_Count( &roster ), 1 );
	EXPECT_TRUE( OSCAR_Roster_Get( &roster, 0, &buddy ) );
	EXPECT_STREQ( buddy.status, "away" );
	EXPECT_TRUE( buddy.online );

	Put16( snacPayload + 2, 0x000c );
	EXPECT_TRUE( OSCAR_RawParseSnac( snacPayload, used, &snac ) );
	EXPECT_TRUE( OSCAR_RawParsePresence( &snac, &event ) );
	EXPECT_STREQ( event.status, "offline" );
	OSCAR_Roster_ApplyPresence( &roster, &event );
	EXPECT_TRUE( OSCAR_Roster_Get( &roster, 0, &buddy ) );
	EXPECT_STREQ( buddy.status, "offline" );
	EXPECT_FALSE( buddy.online );

	OSCAR_Roster_Remove( &roster, "Buddy" );
	EXPECT_EQ_INT( OSCAR_Roster_Count( &roster ), 0 );
}

static void TestRosterSnapshotFormat( void )
{
	oscarRoster_t roster;
	char buf[256];
	oscarEvent_t ev;

	OSCAR_Roster_Init( &roster );
	OSCAR_Roster_Ensure( &roster, "A" );
	OSCAR_Roster_Ensure( &roster, "B" );
	Com_Memset( &ev, 0, sizeof( ev ) );
	ev.type = OSCAR_EVENT_PRESENCE_CHANGED;
	Q_strncpyz( ev.screenName, "A", sizeof( ev.screenName ) );
	Q_strncpyz( ev.status, "available", sizeof( ev.status ) );
	OSCAR_Roster_ApplyPresence( &roster, &ev );

	EXPECT_GT( OSCAR_Roster_FormatSnapshot( &roster, buf, sizeof( buf ) ), 0 );
	EXPECT_TRUE( strstr( buf, "A:available" ) != NULL );
	EXPECT_TRUE( strstr( buf, "B:offline" ) != NULL );
}

static void TestChatLeaveSignoffFrame( void )
{
	byte frame[64];
	oscarRawFlapFrame_t flap;
	int consumed = 0;
	int len = OSCAR_RawBuildChatLeave( 9, frame, sizeof( frame ) );

	EXPECT_EQ_INT( len, 6 );
	EXPECT_TRUE( OSCAR_RawParseFlap( frame, len, &flap, &consumed ) );
	EXPECT_EQ_INT( consumed, 6 );
	EXPECT_EQ_INT( flap.channel, OSCAR_RAW_FLAP_SIGNOFF );
	EXPECT_EQ_INT( flap.sequence, 9 );
	EXPECT_EQ_INT( flap.payloadLen, 0 );
}

int main( void )
{
	TestRosterEnsureGetClearGeneration();
	TestRosterPresenceFromParsedSnac();
	TestRosterSnapshotFormat();
	TestChatLeaveSignoffFrame();

	if ( failures ) {
		fprintf( stderr, "%d OSCAR roster test failure(s)\n", failures );
		return 1;
	}
	printf( "OK: unit_oscar_roster\n" );
	return 0;
}
