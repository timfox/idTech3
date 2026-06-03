/*
 * Unit test: NanoVDB CPU decode (blind FogVolume + float leaf paths).
 */
#include <stdio.h>
#include <string.h>

#include "qcommon/q_shared.h"
#include "../../src/renderers/vulkan/vk_nanovdb_decode.h"

#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "tests/data"
#endif

#define NANOVDB_MAGIC_GRID 0x314244566f6e614eULL
#define GRID_BYTES           672
#define TREE_BYTES           64
#define ROOT_BYTES           64
#define LEAF_BYTES           2144
#define BLIND_META_BYTES     288
#define BLIND_DATA_FLOATS    8
#define BLIND_DATA_BYTES     ( BLIND_DATA_FLOATS * 4 )
#define BLIND_TOTAL_BYTES    ( GRID_BYTES + TREE_BYTES + ROOT_BYTES + BLIND_META_BYTES + BLIND_DATA_BYTES )
#define LEAF_TOTAL_BYTES     ( GRID_BYTES + TREE_BYTES + ROOT_BYTES + LEAF_BYTES )

static void wr_u64( byte *b, int o, uint64_t v ) { memcpy( b + o, &v, 8 ); }
static void wr_u32( byte *b, int o, uint32_t v ) { memcpy( b + o, &v, 4 ); }
static void wr_i32( byte *b, int o, int32_t v ) { memcpy( b + o, &v, 4 ); }
static void wr_f64( byte *b, int o, double v ) { memcpy( b + o, &v, 8 ); }
static void wr_f32( byte *b, int o, float v ) { memcpy( b + o, &v, 4 ); }

static int build_blind_fog_grid( byte *buf )
{
	byte *tree;
	byte *root;
	byte *meta;
	byte *data;
	int i;

	memset( buf, 0, BLIND_TOTAL_BYTES );

	wr_u64( buf, 0, NANOVDB_MAGIC_GRID );
	wr_u32( buf, 24, 0 );
	wr_u32( buf, 28, 1 );
	wr_u64( buf, 32, (uint64_t)BLIND_TOTAL_BYTES );

	wr_f64( buf, 560, 0.0 );
	wr_f64( buf, 568, 0.0 );
	wr_f64( buf, 576, 0.0 );
	wr_f64( buf, 584, 2.0 );
	wr_f64( buf, 592, 2.0 );
	wr_f64( buf, 600, 2.0 );

	wr_f64( buf, 608, 1.0 );
	wr_f64( buf, 616, 1.0 );
	wr_f64( buf, 624, 1.0 );

	wr_u32( buf, 636, 1 );
	wr_u64( buf, 640, (uint64_t)( GRID_BYTES + TREE_BYTES + ROOT_BYTES ) );
	wr_u32( buf, 648, 1 );

	tree = buf + GRID_BYTES;
	wr_u64( tree, 56, 8 );
	wr_u64( tree, 24, (uint64_t)TREE_BYTES );

	root = buf + GRID_BYTES + TREE_BYTES;
	wr_i32( root, 0, 0 );
	wr_i32( root, 4, 0 );
	wr_i32( root, 8, 0 );
	wr_i32( root, 12, 1 );
	wr_i32( root, 16, 1 );
	wr_i32( root, 20, 1 );

	meta = buf + GRID_BYTES + TREE_BYTES + ROOT_BYTES;
	wr_u64( meta, 0, (uint64_t)BLIND_META_BYTES );
	wr_u64( meta, 8, (uint64_t)BLIND_DATA_FLOATS );
	wr_u32( meta, 16, 4 );
	wr_u32( meta, 20, 11 );
	wr_u32( meta, 28, 1 );

	data = meta + BLIND_META_BYTES;
	for ( i = 0; i < BLIND_DATA_FLOATS; i++ ) {
		wr_f32( data, i * 4, (float)( i + 1 ) );
	}

	return BLIND_TOTAL_BYTES;
}

static int build_leaf_float_grid( byte *buf )
{
	byte *tree;
	byte *root;
	byte *leaf;
	uint64_t maskWord;

	memset( buf, 0, LEAF_TOTAL_BYTES );

	wr_u64( buf, 0, NANOVDB_MAGIC_GRID );
	wr_u32( buf, 24, 0 );
	wr_u32( buf, 28, 1 );
	wr_u64( buf, 32, (uint64_t)LEAF_TOTAL_BYTES );

	wr_f64( buf, 560, 0.0 );
	wr_f64( buf, 568, 0.0 );
	wr_f64( buf, 576, 0.0 );
	wr_f64( buf, 584, 8.0 );
	wr_f64( buf, 592, 8.0 );
	wr_f64( buf, 600, 8.0 );

	wr_f64( buf, 608, 1.0 );
	wr_f64( buf, 616, 1.0 );
	wr_f64( buf, 624, 1.0 );

	wr_u32( buf, 636, 1 );
	wr_u64( buf, 640, (uint64_t)LEAF_TOTAL_BYTES );
	wr_u32( buf, 648, 0 );

	tree = buf + GRID_BYTES;
	wr_u64( tree, 0, (uint64_t)( TREE_BYTES + ROOT_BYTES ) );
	wr_u64( tree, 8, (uint64_t)( TREE_BYTES + ROOT_BYTES + LEAF_BYTES ) );
	wr_u32( tree, 32, 1 );
	wr_u64( tree, 24, (uint64_t)TREE_BYTES );
	wr_u64( tree, 56, 1 );

	root = buf + GRID_BYTES + TREE_BYTES;
	wr_i32( root, 0, 0 );
	wr_i32( root, 4, 0 );
	wr_i32( root, 8, 0 );
	wr_i32( root, 12, 7 );
	wr_i32( root, 16, 7 );
	wr_i32( root, 20, 7 );

	leaf = buf + GRID_BYTES + TREE_BYTES + ROOT_BYTES;
	wr_i32( leaf, 0, 0 );
	wr_i32( leaf, 4, 0 );
	wr_i32( leaf, 8, 0 );
	maskWord = 1ULL;
	memcpy( leaf + 16, &maskWord, sizeof( maskWord ) );
	wr_f32( leaf, 96, 42.0f );

	return LEAF_TOTAL_BYTES;
}

static int test_blind_decode( void )
{
	byte buf[BLIND_TOTAL_BYTES];
	float dense[8];
	vdbNanoIndexBBox_t idx;
	int len;
	int i;

	len = build_blind_fog_grid( buf );
	if ( !VDB_NanoVDB_GetIndexDims( buf, len, NULL, &idx ) ) {
		fprintf( stderr, "blind: GetIndexDims failed\n" );
		return 1;
	}
	if ( idx.dimX != 2 || idx.dimY != 2 || idx.dimZ != 2 ) {
		fprintf( stderr, "blind: unexpected dims %dx%dx%d\n", idx.dimX, idx.dimY, idx.dimZ );
		return 1;
	}
	memset( dense, 0, sizeof( dense ) );
	if ( !VDB_NanoVDB_DecodeToDense( buf, len, NULL, dense, 8, &idx ) ) {
		fprintf( stderr, "blind: DecodeToDense failed\n" );
		return 1;
	}
	for ( i = 0; i < 8; i++ ) {
		if ( dense[i] != (float)( i + 1 ) ) {
			fprintf( stderr, "blind: dense[%d]=%f expected %d\n", i, dense[i], i + 1 );
			return 1;
		}
	}
	return 0;
}

static int test_leaf_decode( void )
{
	byte buf[LEAF_TOTAL_BYTES];
	float dense[512];
	vdbNanoIndexBBox_t idx;
	int len;

	len = build_leaf_float_grid( buf );
	if ( !VDB_NanoVDB_GetIndexDims( buf, len, NULL, &idx ) ) {
		fprintf( stderr, "leaf: GetIndexDims failed\n" );
		return 1;
	}
	if ( idx.dimX != 8 || idx.dimY != 8 || idx.dimZ != 8 ) {
		fprintf( stderr, "leaf: unexpected dims %dx%dx%d\n", idx.dimX, idx.dimY, idx.dimZ );
		return 1;
	}
	memset( dense, 0, sizeof( dense ) );
	if ( !VDB_NanoVDB_DecodeToDense( buf, len, NULL, dense, 512, &idx ) ) {
		fprintf( stderr, "leaf: DecodeToDense failed\n" );
		return 1;
	}
	if ( dense[0] != 42.0f ) {
		fprintf( stderr, "leaf: dense[0]=%f expected 42\n", dense[0] );
		return 1;
	}
	if ( dense[1] != 0.0f || dense[511] != 0.0f ) {
		fprintf( stderr, "leaf: unexpected inactive voxels set\n" );
		return 1;
	}
	return 0;
}

static int test_fixture_file( void )
{
	char path[512];
	FILE *fp;
	byte *buf;
	long sz;
	float dense[8];
	vdbNanoIndexBBox_t idx;

	Com_sprintf( path, sizeof( path ), "%s/fog_2cubed.nvdb", TEST_DATA_DIR );
	fp = fopen( path, "rb" );
	if ( !fp ) {
		fprintf( stderr, "fixture: skip (no file at %s)\n", path );
		return 0;
	}
	if ( fseek( fp, 0, SEEK_END ) != 0 ) {
		fclose( fp );
		fprintf( stderr, "fixture: fseek failed\n" );
		return 1;
	}
	sz = ftell( fp );
	if ( sz <= 0 || sz > 1024 * 1024 ) {
		fclose( fp );
		fprintf( stderr, "fixture: bad size %ld\n", sz );
		return 1;
	}
	rewind( fp );
	buf = (byte *)malloc( (size_t)sz );
	if ( !buf ) {
		fclose( fp );
		return 1;
	}
	if ( fread( buf, 1, (size_t)sz, fp ) != (size_t)sz ) {
		free( buf );
		fclose( fp );
		fprintf( stderr, "fixture: fread failed\n" );
		return 1;
	}
	fclose( fp );

	if ( !VDB_NanoVDB_GetIndexDims( buf, (int)sz, NULL, &idx ) ) {
		free( buf );
		fprintf( stderr, "fixture: GetIndexDims failed\n" );
		return 1;
	}
	memset( dense, 0, sizeof( dense ) );
	if ( !VDB_NanoVDB_DecodeToDense( buf, (int)sz, NULL, dense, 8, &idx ) ) {
		free( buf );
		fprintf( stderr, "fixture: DecodeToDense failed\n" );
		return 1;
	}
	free( buf );
	if ( dense[7] != 8.0f ) {
		fprintf( stderr, "fixture: dense[7]=%f expected 8\n", dense[7] );
		return 1;
	}
	return 0;
}

int main( void )
{
	if ( test_blind_decode() != 0 ) {
		return 1;
	}
	if ( test_leaf_decode() != 0 ) {
		return 1;
	}
	if ( test_fixture_file() != 0 ) {
		return 1;
	}
	printf( "test_nanovdb_decode unit: ok (blind + leaf + fixture)\n" );
	return 0;
}
