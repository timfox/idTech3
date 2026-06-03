/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

NanoVDB → dense float grid for Vulkan 3D fog textures (real-time path).
===========================================================================
*/

#include "vk_nanovdb_decode.h"
#include <string.h>
#include <math.h>

#define NANOVDB_MAGIC_NUMB 0x304244566f6e614eULL /* "NanoVDB0" */
#define NANOVDB_MAGIC_GRID 0x314244566f6e614eULL /* "NanoVDB1" */
#define NANOVDB_MAGIC_FILE 0x324244566f6e614eULL /* "NanoVDB2" */

#define NANOVDB_GRIDTYPE_FLOAT  1u
#define NANOVDB_GRIDTYPE_DOUBLE 2u
#define NANOVDB_GRIDTYPE_HALF   9u
#define NANOVDB_GRIDTYPE_END    27u

/* GridData field offsets (openvdb nanovdb GridData 672B) */
#define OFF_MAGIC           0
#define OFF_GRID_SIZE       32
#define OFF_GRID_NAME       40
#define OFF_WORLD_BBOX      560
#define OFF_VOXEL_SIZE      608
#define OFF_GRID_TYPE       636
#define OFF_BLIND_META_OFF  640
#define OFF_BLIND_META_CNT  648

/* TreeData (follows GridData) */
#define OFF_NODE_OFFSET     0
#define OFF_NODE_COUNT      32

/* LeafData<float> LOG2DIM=3 */
#define LEAF_LOG2DIM        3u
#define LEAF_DIM            ( 1u << LEAF_LOG2DIM )
#define LEAF_VOXELS         ( LEAF_DIM * LEAF_DIM * LEAF_DIM )
#define LEAF_MASK_BYTES     64u
#define LEAF_VALUES_OFF     96u
#define LEAF_STRIDE_FLOAT   ( LEAF_VALUES_OFF + LEAF_VOXELS * 4u )

static uint64_t read_u64( const byte *p ) {
	uint64_t v;
	memcpy( &v, p, sizeof( v ) );
	return v;
}

static uint32_t read_u32( const byte *p ) {
	uint32_t v;
	memcpy( &v, p, sizeof( v ) );
	return v;
}

static int32_t read_i32( const byte *p ) {
	int32_t v;
	memcpy( &v, p, sizeof( v ) );
	return v;
}

static int64_t read_i64( const byte *p ) {
	int64_t v;
	memcpy( &v, p, sizeof( v ) );
	return v;
}

static float read_f32( const byte *p ) {
	float v;
	memcpy( &v, p, sizeof( v ) );
	return v;
}

static double read_f64( const byte *p ) {
	double v;
	memcpy( &v, p, sizeof( v ) );
	return v;
}

static float half_to_float( uint16_t h ) {
	uint32_t sign = ( h >> 15 ) & 1u;
	uint32_t exp = ( h >> 10 ) & 0x1fu;
	uint32_t mant = h & 0x3ffu;
	uint32_t f;

	if ( exp == 0 ) {
		if ( mant == 0 ) {
			f = sign << 31;
		} else {
			exp = 1;
			while ( ( mant & 0x400 ) == 0 ) {
				mant <<= 1;
				exp--;
			}
			mant &= 0x3ff;
			exp = 127 - 15 + exp;
			f = ( sign << 31 ) | ( exp << 23 ) | ( mant << 13 );
		}
	} else if ( exp == 31 ) {
		f = ( sign << 31 ) | 0x7f800000u | ( mant << 13 );
	} else {
		f = ( sign << 31 ) | ( ( exp + 127 - 15 ) << 23 ) | ( mant << 13 );
	}
	{
		float out;
		memcpy( &out, &f, sizeof( out ) );
		return out;
	}
}

static qboolean grid_magic_valid( uint64_t magic ) {
	return magic == NANOVDB_MAGIC_NUMB || magic == NANOVDB_MAGIC_GRID;
}

static qboolean grid_header_valid( const byte *grid, int bufLen ) {
	uint64_t magic;
	uint32_t gridType, gridIndex, gridCount;
	uint64_t gridSize;

	if ( !grid || bufLen < NANOVDB_GRIDDATA_BYTES ) {
		return qfalse;
	}
	magic = read_u64( grid + OFF_MAGIC );
	if ( !grid_magic_valid( magic ) ) {
		return qfalse;
	}
	gridType = read_u32( grid + OFF_GRID_TYPE );
	gridIndex = read_u32( grid + 24 );
	gridCount = read_u32( grid + 28 );
	gridSize = read_u64( grid + OFF_GRID_SIZE );
	if ( gridType >= NANOVDB_GRIDTYPE_END || gridCount == 0 || gridIndex >= gridCount ) {
		return qfalse;
	}
	if ( gridSize < NANOVDB_GRIDDATA_BYTES || (int)gridSize > bufLen ) {
		return qfalse;
	}
	return qtrue;
}

static const byte *locate_grid_in_buffer( const byte *buf, int bufLen, int *outGridOff ) {
	uint64_t magic;
	const byte *grid;
	int off;

	if ( !buf || bufLen < NANOVDB_GRIDDATA_BYTES || !outGridOff ) {
		return NULL;
	}

	magic = read_u64( buf );
	if ( grid_magic_valid( magic ) && grid_header_valid( buf, bufLen ) ) {
		*outGridOff = 0;
		return buf;
	}

	if ( magic == NANOVDB_MAGIC_FILE || magic == NANOVDB_MAGIC_NUMB ) {
		uint32_t nameSize;
		if ( bufLen < NANOVDB_FILE_HEADER_BYTES + NANOVDB_FILE_META_BYTES ) {
			return NULL;
		}
		nameSize = read_u32( buf + NANOVDB_FILE_HEADER_BYTES + 136 );
		if ( nameSize == 0 || nameSize > 4096u ) {
			return NULL;
		}
		off = NANOVDB_FILE_HEADER_BYTES + NANOVDB_FILE_META_BYTES + (int)nameSize;
		if ( off + NANOVDB_GRIDDATA_BYTES > bufLen ) {
			return NULL;
		}
		grid = buf + off;
		if ( !grid_header_valid( grid, bufLen - off ) ) {
			return NULL;
		}
		*outGridOff = off;
		return grid;
	}

	return NULL;
}

static const byte *find_named_grid( const byte *buf, int bufLen, const char *gridName ) {
	int off = 0;

	while ( off + NANOVDB_GRIDDATA_BYTES <= bufLen ) {
		const byte *grid = buf + off;
		uint64_t gridSize;
		char name[256];

		if ( !grid_header_valid( grid, bufLen - off ) ) {
			break;
		}
		Q_strncpyz( name, (const char *)( grid + OFF_GRID_NAME ), sizeof( name ) );
		if ( !gridName || !gridName[0] || !Q_stricmp( name, gridName ) ) {
			return grid;
		}
		gridSize = read_u64( grid + OFF_GRID_SIZE );
		if ( gridSize < NANOVDB_GRIDDATA_BYTES ) {
			break;
		}
		off += (int)gridSize;
	}
	return NULL;
}

static qboolean nanovdb_mask_on( const byte *mask, uint32_t i ) {
	const uint64_t *w = (const uint64_t *)mask;
	if ( i >= LEAF_VOXELS ) {
		return qfalse;
	}
	return ( w[i >> 6] >> ( i & 63u ) ) & 1u;
}

static void read_index_bbox( const byte *tree, int *indexMin, int *indexMax ) {
	int64_t rootOff;
	const byte *root;

	rootOff = read_i64( tree + OFF_NODE_OFFSET + 24 );
	if ( rootOff == 0 ) {
		indexMin[0] = indexMin[1] = indexMin[2] = 0;
		indexMax[0] = indexMax[1] = indexMax[2] = 0;
		return;
	}
	root = tree + rootOff;
	indexMin[0] = read_i32( root + 0 );
	indexMin[1] = read_i32( root + 4 );
	indexMin[2] = read_i32( root + 8 );
	indexMax[0] = read_i32( root + 12 );
	indexMax[1] = read_i32( root + 16 );
	indexMax[2] = read_i32( root + 20 );
}

static int dense_index( int dimX, int dimY, int ix, int iy, int iz ) {
	return iz * dimX * dimY + iy * dimX + ix;
}

static qboolean decode_blind_dense( const byte *grid, float *dense, int denseCount,
	const vdbNanoIndexBBox_t *idx ) {
	uint32_t blindCount;
	int64_t blindOff;
	const byte *meta;
	uint32_t semantic, valueCount, valueSize, dataType;
	int64_t dataRel;
	const byte *data;
	int n, total;

	blindCount = read_u32( grid + OFF_BLIND_META_CNT );
	blindOff = read_i64( grid + OFF_BLIND_META_OFF );
	if ( blindCount == 0 || blindOff <= 0 ) {
		return qfalse;
	}
	if ( blindOff + (int64_t)( blindCount * 288 ) > read_i64( grid + OFF_GRID_SIZE ) ) {
		return qfalse;
	}

	meta = grid + blindOff;
	dataRel = read_i64( meta + 0 );
	valueCount = (uint32_t)read_u64( meta + 8 );
	valueSize = read_u32( meta + 16 );
	semantic = read_u32( meta + 20 );
	dataType = read_u32( meta + 28 );
	(void)semantic;
	if ( semantic != 11u ) { /* GridBlindDataSemantic::FogVolume */
		return qfalse;
	}

	total = idx->dimX * idx->dimY * idx->dimZ;
	if ( (uint32_t)total != valueCount || total > denseCount ) {
		return qfalse;
	}
	if ( dataType != NANOVDB_GRIDTYPE_FLOAT || valueSize != 4u ) {
		return qfalse;
	}
	data = meta + dataRel;
	for ( n = 0; n < total; n++ ) {
		dense[n] = read_f32( data + n * 4 );
	}
	return qtrue;
}

static float read_leaf_value( const byte *leaf, uint32_t gridType, uint32_t localIdx ) {
	const byte *valBase = leaf + LEAF_VALUES_OFF;

	switch ( gridType ) {
	case NANOVDB_GRIDTYPE_FLOAT:
		return read_f32( valBase + localIdx * 4u );
	case NANOVDB_GRIDTYPE_DOUBLE:
		return (float)read_f64( valBase + localIdx * 8u );
	case NANOVDB_GRIDTYPE_HALF: {
		uint16_t h;
		memcpy( &h, valBase + localIdx * 2u, sizeof( h ) );
		return half_to_float( h );
	}
	default:
		return 0.0f;
	}
}

static uint32_t leaf_stride_for_type( uint32_t gridType ) {
	switch ( gridType ) {
	case NANOVDB_GRIDTYPE_FLOAT:
		return LEAF_STRIDE_FLOAT;
	case NANOVDB_GRIDTYPE_DOUBLE:
		return LEAF_VALUES_OFF + LEAF_VOXELS * 8u;
	case NANOVDB_GRIDTYPE_HALF:
		return LEAF_VALUES_OFF + LEAF_VOXELS * 2u;
	default:
		return 0;
	}
}

static qboolean decode_leaf_array( const byte *grid, const byte *tree, uint32_t gridType,
	float *dense, const vdbNanoIndexBBox_t *idx ) {
	uint32_t leafCount;
	int64_t leafBaseOff, leafEndOff;
	uint32_t leafStride, i;
	int dimX, dimY, dimZ;
	int64_t gridSize;

	leafCount = read_u32( tree + OFF_NODE_COUNT );
	leafBaseOff = read_i64( tree + OFF_NODE_OFFSET );
	leafEndOff = read_i64( tree + OFF_NODE_OFFSET + 8 );
	leafStride = leaf_stride_for_type( gridType );
	gridSize = (int64_t)read_u64( grid + OFF_GRID_SIZE );

	if ( leafCount == 0 || leafBaseOff == 0 || leafStride == 0 ) {
		return qfalse;
	}

	if ( leafEndOff > leafBaseOff ) {
		uint64_t span = (uint64_t)( leafEndOff - leafBaseOff );
		if ( span % leafCount == 0 ) {
			leafStride = (uint32_t)( span / leafCount );
		}
	}

	dimX = idx->dimX;
	dimY = idx->dimY;
	dimZ = idx->dimZ;

	for ( i = 0; i < leafCount; i++ ) {
		const byte *leaf;
		int32_t ox, oy, oz;
		uint32_t n;

		leaf = tree + leafBaseOff + (int64_t)i * (int64_t)leafStride;
		if ( leaf + (int64_t)leafStride > grid + gridSize ) {
			break;
		}

		ox = read_i32( leaf + 0 );
		oy = read_i32( leaf + 4 );
		oz = read_i32( leaf + 8 );

		for ( n = 0; n < LEAF_VOXELS; n++ ) {
			int lx, ly, lz;
			int gx, gy, gz;
			int di;

			if ( !nanovdb_mask_on( leaf + 16, n ) ) {
				continue;
			}
			lx = (int)( n & 7u );
			ly = (int)( ( n >> 3 ) & 7u );
			lz = (int)( ( n >> 6 ) & 7u );
			gx = ox + lx - idx->indexMin[0];
			gy = oy + ly - idx->indexMin[1];
			gz = oz + lz - idx->indexMin[2];
			if ( gx < 0 || gy < 0 || gz < 0 || gx >= dimX || gy >= dimY || gz >= dimZ ) {
				continue;
			}
			di = dense_index( dimX, dimY, gx, gy, gz );
			if ( di >= 0 && di < dimX * dimY * dimZ ) {
				dense[di] = read_leaf_value( leaf, gridType, n );
			}
		}
	}
	return qtrue;
}

static qboolean resolve_grid( const byte *buf, int bufLen, const char *gridName,
	const byte **outGrid, uint32_t *outGridType ) {
	const byte *grid;
	int gridOff = 0;

	if ( !buf || bufLen <= 0 || !outGrid || !outGridType ) {
		return qfalse;
	}

	grid = locate_grid_in_buffer( buf, bufLen, &gridOff );
	(void)gridOff;
	if ( !grid ) {
		return qfalse;
	}
	if ( gridName && gridName[0] ) {
		const byte *named = find_named_grid( buf, bufLen, gridName );
		if ( !named ) {
			return qfalse;
		}
		grid = named;
	}

	*outGridType = read_u32( grid + OFF_GRID_TYPE );
	if ( *outGridType != NANOVDB_GRIDTYPE_FLOAT &&
	     *outGridType != NANOVDB_GRIDTYPE_DOUBLE &&
	     *outGridType != NANOVDB_GRIDTYPE_HALF ) {
		return qfalse;
	}

	*outGrid = grid;
	return qtrue;
}

static void index_dims_from_tree( const byte *tree, vdbNanoIndexBBox_t *idx ) {
	int spanX, spanY, spanZ;

	read_index_bbox( tree, idx->indexMin, idx->indexMax );
	spanX = idx->indexMax[0] - idx->indexMin[0] + 1;
	spanY = idx->indexMax[1] - idx->indexMin[1] + 1;
	spanZ = idx->indexMax[2] - idx->indexMin[2] + 1;
	if ( spanX < 1 ) {
		spanX = 1;
	}
	if ( spanY < 1 ) {
		spanY = 1;
	}
	if ( spanZ < 1 ) {
		spanZ = 1;
	}
	if ( spanX > 256 ) {
		spanX = 256;
	}
	if ( spanY > 256 ) {
		spanY = 256;
	}
	if ( spanZ > 256 ) {
		spanZ = 256;
	}
	idx->dimX = spanX;
	idx->dimY = spanY;
	idx->dimZ = spanZ;
}

qboolean VDB_NanoVDB_ResolveGrid( const byte *buf, int bufLen, const char *gridName,
	const byte **outGrid ) {
	uint32_t gridType;

	if ( !outGrid ) {
		return qfalse;
	}
	*outGrid = NULL;
	return resolve_grid( buf, bufLen, gridName, outGrid, &gridType );
}

qboolean VDB_NanoVDB_GetIndexDims( const byte *buf, int bufLen, const char *gridName,
	vdbNanoIndexBBox_t *outIndex ) {
	const byte *grid;
	const byte *tree;
	uint32_t gridType;

	if ( !outIndex ) {
		return qfalse;
	}
	Com_Memset( outIndex, 0, sizeof( *outIndex ) );
	if ( !resolve_grid( buf, bufLen, gridName, &grid, &gridType ) ) {
		return qfalse;
	}
	tree = grid + NANOVDB_GRIDDATA_BYTES;
	index_dims_from_tree( tree, outIndex );
	return outIndex->dimX > 0 && outIndex->dimY > 0 && outIndex->dimZ > 0;
}

qboolean VDB_NanoVDB_DecodeToDense( const byte *buf, int bufLen, const char *gridName,
	float *dense, int denseCount, vdbNanoIndexBBox_t *outIndex ) {
	const byte *grid;
	const byte *tree;
	uint32_t gridType;
	int total;
	vdbNanoIndexBBox_t idx;

	if ( !buf || bufLen <= 0 || !dense || denseCount <= 0 || !outIndex ) {
		return qfalse;
	}

	Com_Memset( &idx, 0, sizeof( idx ) );
	if ( !resolve_grid( buf, bufLen, gridName, &grid, &gridType ) ) {
		return qfalse;
	}

	tree = grid + NANOVDB_GRIDDATA_BYTES;
	index_dims_from_tree( tree, &idx );

	total = idx.dimX * idx.dimY * idx.dimZ;
	if ( total > denseCount || total <= 0 ) {
		return qfalse;
	}

	Com_Memset( dense, 0, (size_t)total * sizeof( float ) );

	if ( decode_blind_dense( grid, dense, denseCount, &idx ) ) {
		idx.decoded = qtrue;
		*outIndex = idx;
		return qtrue;
	}

	if ( !decode_leaf_array( grid, tree, gridType, dense, &idx ) ) {
		return qfalse;
	}

	idx.decoded = qtrue;
	*outIndex = idx;
	return qtrue;
}
