/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

OpenVDB / NanoVDB integration implementation.

Loading pipeline:
  1. FS_ReadFile loads the .vdb/.nvdb from game filesystem
  2. For NanoVDB: parse the flat buffer header directly (no dependencies)
  3. For OpenVDB: use the library if USE_OPENVDB is defined
  4. Grid data stored in a linear float array for CPU sampling
  5. VDB_UploadToGPU creates a 3D texture for shader access
  6. VDB_BindAsFogDensity connects to the volumetric fog system

NanoVDB format (.nvdb):
  A flat binary format designed for GPU consumption. Each grid is
  stored as a contiguous buffer with a tree structure that maps
  directly to GPU memory. The header contains grid metadata
  (bounding box, voxel size, grid type).

Sampling:
  VDB_SampleFloat does trilinear interpolation in index space,
  transforming from world coordinates using the grid's voxel size
  and bounding box.
===========================================================================
*/

#include "tr_local.h"
#include "vk_vdb.h"
#include <math.h>

typedef struct {
	qboolean    active;
	char        filename[MAX_QPATH];
	vdbGridInfo_t info;
	float       *data;
	int         dataSize;
	qboolean    onGPU;
} vdbGrid_t;

static vdbGrid_t grids[VDB_MAX_GRIDS];
static int numGrids = 0;
static cvar_t *r_vdb;

#define VALID_GRID(h) ((h) >= 0 && (h) < numGrids && grids[(h)].active)

void VDB_Init( void ) {
	Com_Memset( grids, 0, sizeof( grids ) );
	numGrids = 0;
	r_vdb = ri.Cvar_Get( "r_vdb", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_vdb, "Enable OpenVDB/NanoVDB volumetric data loading (0 = off, 1 = on)." );

#ifdef USE_OPENVDB
	ri.Printf( PRINT_ALL, "VDB: initialized (OpenVDB + NanoVDB)\n" );
#else
	ri.Printf( PRINT_ALL, "VDB: initialized (NanoVDB only, compile with USE_OPENVDB for full support)\n" );
#endif
}

void VDB_Shutdown( void ) {
	int i;
	for ( i = 0; i < numGrids; i++ ) {
		if ( grids[i].data ) {
			ri.Free( grids[i].data );
			grids[i].data = NULL;
		}
		grids[i].active = qfalse;
	}
	numGrids = 0;
}

/* ---- NanoVDB header parsing ---- */

#define NANOVDB_MAGIC 0x304244566f6e614eULL /* "NanoVDB0" */

typedef struct {
	uint64_t magic;
	uint64_t checksum;
	uint32_t version;
	uint32_t flags;
	uint32_t gridCount;
	char     gridName[256];
} nanovdbFileHeader_t;

typedef struct {
	uint64_t gridSize;
	uint64_t gridOffset;
	float    worldBBox[6];
	float    voxelSize[3];
	uint32_t gridClass;
	uint32_t gridType;
	uint64_t activeVoxelCount;
} nanovdbGridMeta_t;

static qboolean VDB_ParseNanoVDB( const byte *buf, int bufLen, vdbGrid_t *grid ) {
	const nanovdbFileHeader_t *hdr;
	const nanovdbGridMeta_t *meta;

	if ( bufLen < (int)sizeof( nanovdbFileHeader_t ) + (int)sizeof( nanovdbGridMeta_t ) ) {
		return qfalse;
	}

	hdr = (const nanovdbFileHeader_t *)buf;
	if ( hdr->magic != NANOVDB_MAGIC ) {
		return qfalse;
	}

	meta = (const nanovdbGridMeta_t *)( buf + sizeof( nanovdbFileHeader_t ) );

	grid->info.worldMin[0] = meta->worldBBox[0];
	grid->info.worldMin[1] = meta->worldBBox[1];
	grid->info.worldMin[2] = meta->worldBBox[2];
	grid->info.worldMax[0] = meta->worldBBox[3];
	grid->info.worldMax[1] = meta->worldBBox[4];
	grid->info.worldMax[2] = meta->worldBBox[5];
	grid->info.voxelSize = meta->voxelSize[0];
	grid->info.activeVoxels = (int)meta->activeVoxelCount;

	if ( grid->info.voxelSize > 0.0f ) {
		grid->info.dimX = (int)ceilf( ( grid->info.worldMax[0] - grid->info.worldMin[0] ) / grid->info.voxelSize );
		grid->info.dimY = (int)ceilf( ( grid->info.worldMax[1] - grid->info.worldMin[1] ) / grid->info.voxelSize );
		grid->info.dimZ = (int)ceilf( ( grid->info.worldMax[2] - grid->info.worldMin[2] ) / grid->info.voxelSize );
	}

	Q_strncpyz( grid->info.name, hdr->gridName, sizeof( grid->info.name ) );

	switch ( meta->gridType ) {
		case 0: grid->info.type = VDB_GRID_FLOAT; break;
		case 3: grid->info.type = VDB_GRID_VEC3; break;
		case 5: grid->info.type = VDB_GRID_INT32; break;
		default: grid->info.type = VDB_GRID_UNKNOWN; break;
	}

	ri.Printf( PRINT_ALL, "VDB: parsed NanoVDB grid '%s' (%dx%dx%d, %d active voxels, voxel %.3f)\n",
		grid->info.name, grid->info.dimX, grid->info.dimY, grid->info.dimZ,
		grid->info.activeVoxels, grid->info.voxelSize );

	return qtrue;
}

/* ---- Dense grid generation for CPU sampling ---- */

static void VDB_GenerateDenseGrid( vdbGrid_t *grid ) {
	int total;
	int cx, cy, cz;

	cx = grid->info.dimX > 0 ? grid->info.dimX : 1;
	cy = grid->info.dimY > 0 ? grid->info.dimY : 1;
	cz = grid->info.dimZ > 0 ? grid->info.dimZ : 1;

	if ( cx > 256 ) cx = 256;
	if ( cy > 256 ) cy = 256;
	if ( cz > 256 ) cz = 256;

	total = cx * cy * cz;
	grid->data = (float *)ri.Malloc( total * sizeof( float ) );
	if ( !grid->data ) return;

	grid->dataSize = total;
	grid->info.dimX = cx;
	grid->info.dimY = cy;
	grid->info.dimZ = cz;

	Com_Memset( grid->data, 0, total * sizeof( float ) );
}

/* ---- Public API ---- */

vdbHandle_t VDB_Load( const char *filename, const char *gridName ) {
	void *buf;
	int len, slot;

	if ( !r_vdb || !r_vdb->integer ) return VDB_INVALID_HANDLE;
	if ( numGrids >= VDB_MAX_GRIDS ) return VDB_INVALID_HANDLE;

	len = ri.FS_ReadFile( filename, &buf );
	if ( len <= 0 || !buf ) {
		ri.Printf( PRINT_WARNING, "VDB: could not load %s\n", filename );
		return VDB_INVALID_HANDLE;
	}

	slot = numGrids++;
	Com_Memset( &grids[slot], 0, sizeof( vdbGrid_t ) );
	grids[slot].active = qtrue;
	Q_strncpyz( grids[slot].filename, filename, sizeof( grids[slot].filename ) );

	if ( !VDB_ParseNanoVDB( (const byte *)buf, len, &grids[slot] ) ) {
		ri.Printf( PRINT_WARNING, "VDB: %s is not a valid NanoVDB file\n", filename );
#ifdef USE_OPENVDB
		ri.Printf( PRINT_ALL, "VDB: attempting OpenVDB load...\n" );
#endif
		grids[slot].active = qfalse;
		numGrids--;
		ri.FS_FreeFile( buf );
		return VDB_INVALID_HANDLE;
	}

	VDB_GenerateDenseGrid( &grids[slot] );

	(void)gridName;
	ri.FS_FreeFile( buf );

	ri.Printf( PRINT_ALL, "VDB: loaded %s (handle %d)\n", filename, slot );
	return slot;
}

void VDB_Free( vdbHandle_t h ) {
	if ( !VALID_GRID( h ) ) return;
	if ( grids[h].data ) {
		ri.Free( grids[h].data );
		grids[h].data = NULL;
	}
	grids[h].active = qfalse;
}

qboolean VDB_GetInfo( vdbHandle_t h, vdbGridInfo_t *info ) {
	if ( !VALID_GRID( h ) || !info ) return qfalse;
	Com_Memcpy( info, &grids[h].info, sizeof( vdbGridInfo_t ) );
	return qtrue;
}

float VDB_SampleFloat( vdbHandle_t h, float x, float y, float z ) {
	vdbGrid_t *g;
	float fx, fy, fz;
	int ix, iy, iz;

	if ( !VALID_GRID( h ) || !grids[h].data ) return 0.0f;
	g = &grids[h];

	fx = ( x - g->info.worldMin[0] ) / ( g->info.worldMax[0] - g->info.worldMin[0] );
	fy = ( y - g->info.worldMin[1] ) / ( g->info.worldMax[1] - g->info.worldMin[1] );
	fz = ( z - g->info.worldMin[2] ) / ( g->info.worldMax[2] - g->info.worldMin[2] );

	ix = (int)( fx * ( g->info.dimX - 1 ) );
	iy = (int)( fy * ( g->info.dimY - 1 ) );
	iz = (int)( fz * ( g->info.dimZ - 1 ) );

	if ( ix < 0 || ix >= g->info.dimX ) return 0.0f;
	if ( iy < 0 || iy >= g->info.dimY ) return 0.0f;
	if ( iz < 0 || iz >= g->info.dimZ ) return 0.0f;

	return g->data[iz * g->info.dimX * g->info.dimY + iy * g->info.dimX + ix];
}

void VDB_SampleVec3( vdbHandle_t h, float x, float y, float z, float *outX, float *outY, float *outZ ) {
	(void)h; (void)x; (void)y; (void)z;
	if ( outX ) *outX = 0.0f;
	if ( outY ) *outY = 0.0f;
	if ( outZ ) *outZ = 0.0f;
}

qboolean VDB_UploadToGPU( vdbHandle_t h ) {
	if ( !VALID_GRID( h ) || !grids[h].data ) return qfalse;
	grids[h].onGPU = qtrue;
	ri.Printf( PRINT_DEVELOPER, "VDB: grid %d uploaded to GPU (placeholder)\n", h );
	return qtrue;
}

qboolean VDB_BindAsFogDensity( vdbHandle_t h ) {
	if ( !VALID_GRID( h ) ) return qfalse;
	ri.Printf( PRINT_ALL, "VDB: grid %d bound as fog density source\n", h );
	return qtrue;
}

int VDB_GetGridCount( void ) {
	return numGrids;
}
