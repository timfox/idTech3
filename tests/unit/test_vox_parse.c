/*
 * Unit test: MagicaVoxel .vox parse (R_Vox_Parse).
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "qcommon/q_shared.h"
#include "tr_vox_parse.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

#define ASSERT_EQ(a, b, msg) do { \
	if ((int)(a) != (int)(b)) { \
		fprintf(stderr, "FAIL: %s (got %d want %d)\n", msg, (int)(a), (int)(b)); \
		return 1; \
	} \
} while (0)

static void write_i32( unsigned char **pp, int v )
{
	unsigned char *p = *pp;
	p[0] = (unsigned char)( v & 0xff );
	p[1] = (unsigned char)( ( v >> 8 ) & 0xff );
	p[2] = (unsigned char)( ( v >> 16 ) & 0xff );
	p[3] = (unsigned char)( ( v >> 24 ) & 0xff );
	*pp = p + 4;
}

static void write_id( unsigned char **pp, const char *id )
{
	memcpy( *pp, id, 4 );
	*pp += 4;
}

/* Minimal MagicaVoxel file: 2x2x2 SIZE, 2 voxels, RGBA palette. */
static int build_fixture( unsigned char *buf, int bufSize )
{
	unsigned char *p = buf;
	unsigned char *mainContent;
	unsigned char *childrenStart;
	int childrenSize;
	int i;

	if ( bufSize < 1280 ) {
		return -1;
	}

	/* VOX  header */
	memcpy( p, "VOX ", 4 );
	p += 4;
	write_i32( &p, 150 );

	write_id( &p, "MAIN" );
	write_i32( &p, 0 ); /* content N */
	mainContent = p;
	write_i32( &p, 0 ); /* children M placeholder */
	childrenStart = p;

	/* SIZE */
	write_id( &p, "SIZE" );
	write_i32( &p, 12 );
	write_i32( &p, 0 );
	write_i32( &p, 2 );
	write_i32( &p, 2 );
	write_i32( &p, 2 );

	/* XYZI: 2 voxels */
	write_id( &p, "XYZI" );
	write_i32( &p, 4 + 2 * 4 );
	write_i32( &p, 0 );
	write_i32( &p, 2 );
	p[0] = 0; p[1] = 0; p[2] = 0; p[3] = 1; p += 4;
	p[0] = 1; p[1] = 0; p[2] = 0; p[3] = 2; p += 4;

	/* RGBA */
	write_id( &p, "RGBA" );
	write_i32( &p, 1024 );
	write_i32( &p, 0 );
	for ( i = 0; i < 256; i++ ) {
		p[0] = (unsigned char)( i );
		p[1] = 64;
		p[2] = 128;
		p[3] = 255;
		p += 4;
	}

	childrenSize = (int)( p - childrenStart );
	write_i32( &mainContent, childrenSize );
	return (int)( p - buf );
}

static int test_parse_fixture( void )
{
	unsigned char buf[2048];
	int n;
	voxModel_t vox;

	n = build_fixture( buf, (int)sizeof( buf ) );
	ASSERT( n > 0, "fixture build" );
	ASSERT( R_Vox_Parse( buf, n, &vox ), "parse ok" );
	ASSERT_EQ( vox.sizeX, 2, "sizeX" );
	ASSERT_EQ( vox.sizeY, 2, "sizeY" );
	ASSERT_EQ( vox.sizeZ, 2, "sizeZ" );
	ASSERT_EQ( vox.numVoxels, 2, "numVoxels" );
	ASSERT( vox.xyzc != NULL, "xyzc" );
	ASSERT_EQ( vox.xyzc[0], 0, "v0.x" );
	ASSERT_EQ( vox.xyzc[3], 1, "v0.ci" );
	ASSERT_EQ( vox.xyzc[4], 1, "v1.x" );
	ASSERT_EQ( vox.xyzc[7], 2, "v1.ci" );
	ASSERT( vox.hasRgbaChunk, "has rgba" );
	/* File color 0 → palette[1] */
	ASSERT_EQ( vox.palette[1][0], 0, "pal1.r" );
	ASSERT_EQ( vox.palette[1][1], 64, "pal1.g" );
	R_Vox_Free( &vox );
	return 0;
}

static int test_reject_bad_magic( void )
{
	unsigned char bad[16] = { 0 };
	voxModel_t vox;

	memcpy( bad, "NOPE", 4 );
	ASSERT( !R_Vox_Parse( bad, 16, &vox ), "reject bad magic" );
	return 0;
}

int main( void )
{
	if ( test_parse_fixture() != 0 ) {
		return 1;
	}
	if ( test_reject_bad_magic() != 0 ) {
		return 1;
	}
	printf( "test_vox_parse unit: ok\n" );
	return 0;
}
