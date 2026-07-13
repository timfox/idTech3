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

#define EXPECT_GT( actual, expected ) \
	do { \
		if ( ( actual ) <= ( expected ) ) { \
			fprintf( stderr, "%s:%d: expected %d > %d\n", __FILE__, __LINE__, ( actual ), ( expected ) ); \
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

static void TestLoginSignonBuildsFlapFrame( void )
{
	byte frame[1024];
	oscarRawFlapFrame_t flap;
	int consumed = 0;
	int len = OSCAR_RawBuildLoginSignon( 3, "arenaBot", "secret", frame, sizeof( frame ) );

	EXPECT_GT( len, 0 );
	EXPECT_TRUE( OSCAR_RawParseFlap( frame, len, &flap, &consumed ) );
	EXPECT_EQ_INT( consumed, len );
	EXPECT_EQ_INT( flap.channel, OSCAR_RAW_FLAP_SIGNON );
	EXPECT_EQ_INT( flap.sequence, 3 );
	EXPECT_EQ_INT( flap.payload[0], 0 );
	EXPECT_EQ_INT( flap.payload[3], 1 );
}

static void TestAuthReplyParsesBosCookie( void )
{
	byte payload[1024];
	oscarRawAuthReply_t reply;
	const char host[] = "127.0.0.1:5190";
	const byte cookie[] = { 1, 2, 3, 4, 5 };
	int used = 0;

	Put32( payload, 1 );
	used = 4;
	used = AddTLV( payload, used, 0x0005, host, (int)strlen( host ) );
	used = AddTLV( payload, used, 0x0006, cookie, (int)sizeof( cookie ) );

	EXPECT_TRUE( OSCAR_RawParseAuthReply( payload, used, &reply ) );
	EXPECT_STREQ( reply.bosHost, "127.0.0.1" );
	EXPECT_EQ_INT( reply.bosPort, 5190 );
	EXPECT_EQ_INT( reply.cookieLen, (int)sizeof( cookie ) );
	EXPECT_EQ_INT( reply.cookie[4], 5 );
}

static void TestCookieSignonAndClientOnline( void )
{
	byte frame[1024];
	const byte cookie[] = { 9, 8, 7 };
	int len;

	len = OSCAR_RawBuildCookieSignon( 4, cookie, (int)sizeof( cookie ), frame, sizeof( frame ) );
	EXPECT_GT( len, 0 );
	EXPECT_EQ_INT( frame[1], OSCAR_RAW_FLAP_SIGNON );

	len = OSCAR_RawBuildClientOnline( 5, 11, frame, sizeof( frame ) );
	EXPECT_GT( len, 0 );
	EXPECT_EQ_INT( frame[1], OSCAR_RAW_FLAP_DATA );
	EXPECT_EQ_INT( frame[6], 0 );
	EXPECT_EQ_INT( frame[7], 1 );
	EXPECT_EQ_INT( frame[8], 0 );
	EXPECT_EQ_INT( frame[9], 2 );
}

static void TestIncomingIMParse( void )
{
	byte snacPayload[1024];
	byte body[512];
	byte tlvData[128];
	oscarRawSnac_t snac;
	oscarEvent_t event;
	const char sender[] = "Buddy";
	const char text[] = "hello";
	int bodyUsed = 0;
	int tlvUsed = 0;
	int used = 0;

	memset( body, 0, sizeof( body ) );
	bodyUsed = 8;
	Put16( body + bodyUsed, 1 );
	bodyUsed += 2;
	body[bodyUsed++] = (byte)strlen( sender );
	memcpy( body + bodyUsed, sender, strlen( sender ) );
	bodyUsed += (int)strlen( sender );

	tlvData[tlvUsed++] = 1;
	tlvData[tlvUsed++] = 1;
	Put16( tlvData + tlvUsed, (unsigned int)( strlen( text ) + 4 ) );
	tlvUsed += 2;
	Put16( tlvData + tlvUsed, 0 );
	Put16( tlvData + tlvUsed + 2, 0 );
	tlvUsed += 4;
	memcpy( tlvData + tlvUsed, text, strlen( text ) );
	tlvUsed += (int)strlen( text );
	bodyUsed = AddTLV( body, bodyUsed, 0x0002, tlvData, tlvUsed );

	Put16( snacPayload, 0x0004 );
	Put16( snacPayload + 2, 0x0007 );
	Put16( snacPayload + 4, 0 );
	Put32( snacPayload + 6, 99 );
	used = 10;
	memcpy( snacPayload + used, body, bodyUsed );
	used += bodyUsed;

	EXPECT_TRUE( OSCAR_RawParseSnac( snacPayload, used, &snac ) );
	EXPECT_TRUE( OSCAR_RawParseIncomingIM( &snac, &event ) );
	EXPECT_EQ_INT( event.type, OSCAR_EVENT_INSTANT_MESSAGE );
	EXPECT_STREQ( event.screenName, sender );
	EXPECT_STREQ( event.text, text );
}

static void TestPresenceBuildsSetUserInfoFields( void )
{
	byte frame[1024];
	int len;

	len = OSCAR_RawBuildPresence( 8, 44, "away", frame, sizeof( frame ) );
	EXPECT_GT( len, 0 );
	EXPECT_EQ_INT( frame[1], OSCAR_RAW_FLAP_DATA );
	EXPECT_EQ_INT( frame[6], 0 );
	EXPECT_EQ_INT( frame[7], 1 );
	EXPECT_EQ_INT( frame[8], 0 );
	EXPECT_EQ_INT( frame[9], 0x1e );
	EXPECT_EQ_INT( frame[16], 0 );
	EXPECT_EQ_INT( frame[17], 6 );
	EXPECT_EQ_INT( frame[18], 0 );
	EXPECT_EQ_INT( frame[19], 4 );
	EXPECT_EQ_INT( frame[23], 1 );

	len = OSCAR_RawBuildPresence( 9, 45, "definitely-not-real", frame, sizeof( frame ) );
	EXPECT_EQ_INT( len, 0 );
}

int main( void )
{
	TestLoginSignonBuildsFlapFrame();
	TestAuthReplyParsesBosCookie();
	TestCookieSignonAndClientOnline();
	TestIncomingIMParse();
	TestPresenceBuildsSetUserInfoFields();

	if ( failures != 0 ) {
		fprintf( stderr, "unit_oscar_raw: %d failure(s)\n", failures );
		return 1;
	}

	printf( "unit_oscar_raw: ok\n" );
	return 0;
}
