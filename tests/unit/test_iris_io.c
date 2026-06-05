/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Unit tests for Iris .iris atlas container I/O.
===========================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iris/iris_io.h"

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

static void test_roundtrip_small( void )
{
	uint8_t rgba[64 * 64 * 4];
	uint32_t state[1 * 1];
	uint8_t filebuf[64 * 64 * 4 + 128];
	iris_file_header_t hdr;
	const uint8_t *payload;
	const uint8_t *statePayload;
	int fileLen;
	uint32_t x;

	for ( x = 0; x < sizeof( rgba ); x++ ) {
		rgba[x] = (uint8_t)( x & 255 );
	}
	state[0] = 2u;

	fileLen = Iris_SerializeAtlas( rgba, state, 64, 64, 64, 1, 1, filebuf, (int)sizeof( filebuf ) );
	expect_true( fileLen > 0, "serialize small atlas v2" );
	expect_true( Iris_ParseAtlasBuffer( filebuf, fileLen, &hdr, &payload, &statePayload ), "parse roundtrip v2" );
	expect_true( hdr.version == IRIS_FILE_VERSION_V2, "version 2" );
	expect_true( hdr.atlas_w == 64 && hdr.atlas_h == 64, "header dimensions" );
	expect_true( memcmp( payload, rgba, sizeof( rgba ) ) == 0, "payload bytes match" );
	expect_true( statePayload && ((const uint32_t *)statePayload)[0] == 2u, "tile state roundtrip" );
}

static void test_v1_compat( void )
{
	iris_file_header_t hdr;
	uint8_t rgba[64 * 64 * 4];
	uint8_t filebuf[sizeof( iris_file_header_t ) + sizeof( rgba )];
	const uint8_t *payload;
	const uint8_t *statePayload;
	int fileLen;
	int minHeader;

	memset( rgba, 0xAB, sizeof( rgba ) );
	memset( &hdr, 0, sizeof( hdr ) );
	hdr.magic = IRIS_FILE_MAGIC;
	hdr.version = IRIS_FILE_VERSION;
	hdr.tile_px = 64;
	hdr.tiles_x = 1;
	hdr.tiles_y = 1;
	hdr.atlas_w = 64;
	hdr.atlas_h = 64;
	hdr.payload_bytes = (uint32_t)sizeof( rgba );
	minHeader = (int)( offsetof( iris_file_header_t, state_bytes ) );
	memcpy( filebuf, &hdr, (size_t)minHeader );
	memcpy( filebuf + minHeader, rgba, sizeof( rgba ) );
	fileLen = minHeader + (int)sizeof( rgba );

	expect_true( Iris_ParseAtlasBuffer( filebuf, fileLen, &hdr, &payload, &statePayload ), "parse v1 file" );
	expect_true( statePayload == NULL, "v1 has no tile state" );
	expect_true( payload[0] == 0xAB, "v1 payload readable" );
}

static void test_reject_bad_magic( void )
{
	iris_file_header_t hdr;

	memset( &hdr, 0, sizeof( hdr ) );
	hdr.magic = 0xdeadbeefu;
	hdr.version = IRIS_FILE_VERSION_V2;
	hdr.tile_px = 256;
	hdr.tiles_x = 1;
	hdr.tiles_y = 1;
	hdr.atlas_w = 256;
	hdr.atlas_h = 256;
	hdr.payload_bytes = 256u * 256u * 4u;
	hdr.state_bytes = sizeof( uint32_t );
	expect_true( !Iris_ValidateHeader( &hdr ), "reject bad magic" );
}

int main( void )
{
	test_roundtrip_small();
	test_v1_compat();
	test_reject_bad_magic();

	printf( "test_iris_io: %d run, %d failed\n", tests_run, tests_failed );
	return tests_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
