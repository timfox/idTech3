/*
 * Unit tests: app_crdt semver merge + versioned event queue (Algorithm 1/2).
 */
#include "qcommon/app_crdt.h"
#include "qcommon/q_shared.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run;
static int tests_failed;

static void expect_true( int cond, const char *msg )
{
	tests_run++;
	if ( !cond ) {
		tests_failed++;
		fprintf( stderr, "FAIL: %s\n", msg );
	}
}

static int g_deliverCount;
static int g_adaptCount;
static char g_lastPayload[APP_CRDT_MAX_PAYLOAD];

static void test_deliver( int msgMajor, const char *payload, void *userData )
{
	(void)userData;
	(void)msgMajor;
	g_deliverCount++;
	Q_strncpyz( g_lastPayload, payload, sizeof( g_lastPayload ) );
}

static void test_adapt( int msgMajor, const char *payload, void *userData )
{
	(void)userData;
	(void)msgMajor;
	(void)payload;
	g_adaptCount++;
}

static void test_semver_parse_compare( void )
{
	appCrdtVersion_t a, b;

	expect_true( AppCrdt_ParseVersion( "1.2.3", &a ), "parse 1.2.3" );
	expect_true( a.major == 1 && a.minor == 2 && a.patch == 3, "fields 1.2.3" );

	expect_true( AppCrdt_ParseVersion( "2.0", &b ), "parse 2.0" );
	expect_true( AppCrdt_CompareVersion( &b, &a ) > 0, "2.0 > 1.2.3" );
	expect_true( AppCrdt_CompareVersion( &a, &b ) < 0, "1.2.3 < 2.0" );
}

static void test_lww_merge( void )
{
	appCrdtVersion_t local = { 1, 0, 0 };
	appCrdtVersion_t remote = { 1, 1, 0 };

	expect_true( AppCrdt_MergeLWW( &local, &remote ), "merge newer minor" );
	expect_true( local.minor == 1, "local minor updated" );

	expect_true( !AppCrdt_MergeLWW( &local, &remote ), "merge same rejected" );
}

static void test_manifest_json( void )
{
	appCrdtSpec_t spec;
	const char *json =
		"{\"version\":\"1.0.0\",\"scripts\":[\"scripts/lua/app_spec.lua\"]}";

	expect_true( AppCrdt_ParseManifestJson( json, &spec ), "parse manifest" );
	expect_true( spec.version.major == 1 && spec.scriptCount == 1, "manifest fields" );
	expect_true( !strcmp( spec.scriptPaths[0], "scripts/lua/app_spec.lua" ), "script path" );
}

static void test_queue_algorithm2( void )
{
	appCrdtQueue_t queue;
	appCrdtDispatchResult_t r;

	g_deliverCount = 0;
	g_adaptCount = 0;
	AppCrdt_QueueInit( &queue, 8, test_deliver, test_adapt, NULL );

	r = AppCrdt_QueueDispatch( &queue, 1, 1, "hello" );
	expect_true( r == APP_CRDT_DISPATCH_DELIVER, "same major delivers" );
	expect_true( g_deliverCount == 1, "deliver called" );

	r = AppCrdt_QueueDispatch( &queue, 1, 2, "future" );
	expect_true( r == APP_CRDT_DISPATCH_BUFFER, "newer major buffers" );
	expect_true( queue.count == 1, "one buffered" );

	r = AppCrdt_QueueDispatch( &queue, 2, 1, "old" );
	expect_true( r == APP_CRDT_DISPATCH_ADAPT, "older major adapts" );
	expect_true( g_adaptCount == 1, "adapt called" );

	expect_true( AppCrdt_QueueFlushUpToMajor( &queue, 2 ) == 1, "flush on catch-up" );
	expect_true( g_deliverCount == 2, "buffered event delivered" );
}

int main( void )
{
	test_semver_parse_compare();
	test_lww_merge();
	test_manifest_json();
	test_queue_algorithm2();

	printf( "test_app_crdt: %d run, %d failed\n", tests_run, tests_failed );
	return tests_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
