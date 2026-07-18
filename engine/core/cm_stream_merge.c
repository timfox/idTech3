/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "cm_public.h"
#include "cm_stream_merge.h"
#include "cm_stream.h"
#include "cm_local.h"
#include "../world/world_config.h"

#define CM_MERGE_MAX_SECTORS 64

typedef struct {
	qboolean     active;
	int          cellX;
	int          cellY;
	vec3_t       bounds[2];
	int          numShaders;
	dshader_t    *shaders;
	int          numPlanes;
	cplane_t     *planes;
	int          numBrushSides;
	cbrushside_t *brushsides;
	int          numBrushes;
	cbrush_t     *brushes;
} cmStreamSectorPatch_t;

static cmStreamSectorPatch_t s_patches[CM_MERGE_MAX_SECTORS];
static cvar_t *cm_streamMerge;
static cvar_t *cm_streamSectorSize;

static void CM_Stream_BoundBrush( cbrush_t *b ) {
	b->bounds[0][0] = -b->sides[0].plane->dist;
	b->bounds[1][0] = b->sides[1].plane->dist;
	b->bounds[0][1] = -b->sides[2].plane->dist;
	b->bounds[1][1] = b->sides[3].plane->dist;
	b->bounds[0][2] = -b->sides[4].plane->dist;
	b->bounds[1][2] = b->sides[5].plane->dist;
}

static int CM_Stream_FindPatch( int cellX, int cellY ) {
	int i;

	for ( i = 0; i < CM_MERGE_MAX_SECTORS; i++ ) {
		if ( s_patches[i].active && s_patches[i].cellX == cellX && s_patches[i].cellY == cellY ) {
			return i;
		}
	}
	return -1;
}

static int CM_Stream_AllocPatch( int cellX, int cellY ) {
	int i;

	i = CM_Stream_FindPatch( cellX, cellY );
	if ( i >= 0 ) {
		return i;
	}
	for ( i = 0; i < CM_MERGE_MAX_SECTORS; i++ ) {
		if ( !s_patches[i].active ) {
			Com_Memset( &s_patches[i], 0, sizeof( s_patches[i] ) );
			s_patches[i].active = qtrue;
			s_patches[i].cellX = cellX;
			s_patches[i].cellY = cellY;
			return i;
		}
	}
	return -1;
}

static void CM_Stream_FreePatch( cmStreamSectorPatch_t *patch ) {
	if ( !patch ) {
		return;
	}
	Z_Free( patch->shaders );
	Z_Free( patch->planes );
	Z_Free( patch->brushsides );
	Z_Free( patch->brushes );
	Com_Memset( patch, 0, sizeof( *patch ) );
}

static void CM_Stream_OffsetPlanes( cplane_t *planes, int count, const vec3_t origin ) {
	int i;

	for ( i = 0; i < count; i++ ) {
		planes[i].dist += DotProduct( planes[i].normal, origin );
	}
}

void CM_Stream_Merge_Init( void ) {
	cm_streamMerge = Cvar_Get( "cm_streamMerge", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( cm_streamMerge,
		"When 1, CM_Stream_LoadSector merges sector BSP brushes as collision overlay (keeps base CM)." );
	cm_streamSectorSize = Cvar_Get( "cm_streamSectorSize", "4096", CVAR_ARCHIVE );
	Cvar_SetDescription( cm_streamSectorSize,
		"World units per sector when translating local sector BSP into world space." );
	Com_Memset( s_patches, 0, sizeof( s_patches ) );
}

void CM_Stream_Merge_ClearAll( void ) {
	int i;

	for ( i = 0; i < CM_MERGE_MAX_SECTORS; i++ ) {
		if ( s_patches[i].active ) {
			CM_Stream_FreePatch( &s_patches[i] );
		}
	}
}

qboolean CM_Stream_MergeSector( int cellX, int cellY ) {
	char mapName[MAX_QPATH];
	void *buf;
	int length;
	dheader_t header;
	byte *base;
	cmStreamSectorPatch_t *patch;
	int patchIdx;
	vec3_t worldOrigin;
	float sectorSize;
	int i, count, bits, j;
	lump_t *l;

	if ( !cm_streamMerge || !cm_streamMerge->integer ) {
		return qfalse;
	}
	if ( !CM_Stream_SectorOverlayPermitted() ) {
		return qfalse;
	}
	if ( CM_Stream_FindPatch( cellX, cellY ) >= 0 ) {
		return qtrue;
	}

	patchIdx = CM_Stream_AllocPatch( cellX, cellY );
	if ( patchIdx < 0 ) {
		Com_Printf( S_COLOR_YELLOW "[cm_stream_merge] patch table full (%d,%d)\n", cellX, cellY );
		return qfalse;
	}
	patch = &s_patches[patchIdx];

	{
		char preferred[MAX_QPATH];
		char fallback[MAX_QPATH];

		Com_sprintf( fallback, sizeof( fallback ), "maps/sector_%d_%d.bsp", cellX, cellY );
		WorldConfig_FormatSectorBsp( cellX, cellY, preferred, sizeof( preferred ) );
		if ( !WorldConfig_ResolveReadable( preferred, fallback, mapName, sizeof( mapName ) ) ) {
			patch->active = qfalse;
			return qfalse;
		}
	}
	length = FS_ReadFile( mapName, &buf );
	if ( length <= 0 || !buf ) {
		patch->active = qfalse;
		return qfalse;
	}
	if ( (size_t)length < sizeof( dheader_t ) ) {
		FS_FreeFile( buf );
		patch->active = qfalse;
		return qfalse;
	}

	header = *(dheader_t *)buf;
	for ( i = 0; (size_t)i < sizeof( dheader_t ) / sizeof( int32_t ); i++ ) {
		( (int32_t *)&header )[i] = LittleLong( ( (int32_t *)&header )[i] );
	}
	if ( header.version != BSP_VERSION ) {
		FS_FreeFile( buf );
		patch->active = qfalse;
		Com_Printf( S_COLOR_YELLOW "[cm_stream_merge] %s bad BSP version\n", mapName );
		return qfalse;
	}

	base = (byte *)buf;
	sectorSize = cm_streamSectorSize ? cm_streamSectorSize->value : 4096.0f;
	if ( sectorSize < 256.0f ) {
		sectorSize = 256.0f;
	}
	worldOrigin[0] = (float)cellX * sectorSize;
	worldOrigin[1] = (float)cellY * sectorSize;
	worldOrigin[2] = 0.0f;

	/* shaders */
	l = &header.lumps[LUMP_SHADERS];
	{
		dshader_t *in = (dshader_t *)( base + l->fileofs );
		count = l->filelen / sizeof( *in );
		if ( count < 1 ) {
			FS_FreeFile( buf );
			CM_Stream_FreePatch( patch );
			return qfalse;
		}
		patch->shaders = Z_Malloc( count * sizeof( *patch->shaders ) );
		patch->numShaders = count;
		Com_Memcpy( patch->shaders, in, count * sizeof( *in ) );
		for ( i = 0; i < count; i++ ) {
			patch->shaders[i].contentFlags = LittleLong( patch->shaders[i].contentFlags );
			patch->shaders[i].surfaceFlags = LittleLong( patch->shaders[i].surfaceFlags );
		}
	}

	/* planes */
	l = &header.lumps[LUMP_PLANES];
	{
		dplane_t *in = (dplane_t *)( base + l->fileofs );
		count = l->filelen / sizeof( *in );
		patch->planes = Z_Malloc( count * sizeof( *patch->planes ) );
		patch->numPlanes = count;
		for ( i = 0; i < count; i++, in++ ) {
			bits = 0;
			for ( j = 0; j < 3; j++ ) {
				patch->planes[i].normal[j] = LittleFloat( in->normal[j] );
				if ( patch->planes[i].normal[j] < 0 ) {
					bits |= 1 << j;
				}
			}
			patch->planes[i].dist = LittleFloat( in->dist );
			patch->planes[i].type = PlaneTypeForNormal( patch->planes[i].normal );
			patch->planes[i].signbits = bits;
		}
		CM_Stream_OffsetPlanes( patch->planes, patch->numPlanes, worldOrigin );
	}

	/* brush sides */
	l = &header.lumps[LUMP_BRUSHSIDES];
	{
		dbrushside_t *in = (dbrushside_t *)( base + l->fileofs );
		count = l->filelen / sizeof( *in );
		patch->brushsides = Z_Malloc( count * sizeof( *patch->brushsides ) );
		patch->numBrushSides = count;
		Com_Memset( patch->brushsides, 0, count * sizeof( *patch->brushsides ) );
		for ( i = 0; i < count; i++, in++ ) {
			int planeNum = LittleLong( in->planeNum );
			int shaderNum = LittleLong( in->shaderNum );
			if ( planeNum < 0 || planeNum >= patch->numPlanes ) {
				continue;
			}
			patch->brushsides[i].plane = &patch->planes[planeNum];
			patch->brushsides[i].shaderNum = shaderNum;
			if ( shaderNum >= 0 && shaderNum < patch->numShaders ) {
				patch->brushsides[i].surfaceFlags = patch->shaders[shaderNum].surfaceFlags;
			}
		}
	}

	/* brushes */
	l = &header.lumps[LUMP_BRUSHES];
	{
		dbrush_t *in = (dbrush_t *)( base + l->fileofs );
		count = l->filelen / sizeof( *in );
		patch->brushes = Z_Malloc( count * sizeof( *patch->brushes ) );
		patch->numBrushes = count;
		Com_Memset( patch->brushes, 0, count * sizeof( *patch->brushes ) );
		for ( i = 0; i < count; i++, in++ ) {
			int firstSide = LittleLong( in->firstSide );
			int numSides = LittleLong( in->numSides );
			int shaderNum = LittleLong( in->shaderNum );

			if ( firstSide < 0 || numSides < 6 ||
				firstSide + numSides > patch->numBrushSides ) {
				patch->brushes[i].numsides = 0;
				continue;
			}
			patch->brushes[i].sides = patch->brushsides + firstSide;
			patch->brushes[i].numsides = numSides;
			patch->brushes[i].shaderNum = shaderNum;
			if ( shaderNum >= 0 && shaderNum < patch->numShaders ) {
				patch->brushes[i].contents = patch->shaders[shaderNum].contentFlags;
			}
			patch->brushes[i].checkcount = 0;
			if ( !patch->brushes[i].sides[0].plane ) {
				patch->brushes[i].numsides = 0;
				continue;
			}
			CM_Stream_BoundBrush( &patch->brushes[i] );
		}
	}

	FS_FreeFile( buf );

	patch->bounds[0][0] = worldOrigin[0];
	patch->bounds[0][1] = worldOrigin[1];
	patch->bounds[0][2] = -65536.0f;
	patch->bounds[1][0] = worldOrigin[0] + sectorSize;
	patch->bounds[1][1] = worldOrigin[1] + sectorSize;
	patch->bounds[1][2] = 65536.0f;

	Com_Printf( "[cm_stream_merge] merged sector %d,%d (%s, %d brushes)\n",
		cellX, cellY, mapName, patch->numBrushes );
	return qtrue;
}

void CM_Stream_UnmergeSector( int cellX, int cellY ) {
	int idx = CM_Stream_FindPatch( cellX, cellY );

	if ( idx < 0 ) {
		return;
	}
	CM_Stream_FreePatch( &s_patches[idx] );
	Com_DPrintf( "[cm_stream_merge] unmerged sector %d,%d\n", cellX, cellY );
}

qboolean CM_Stream_IsSectorMerged( int cellX, int cellY ) {
	return CM_Stream_FindPatch( cellX, cellY ) >= 0;
}

int CM_Stream_MergedCount( void ) {
	int i, n = 0;

	for ( i = 0; i < CM_MERGE_MAX_SECTORS; i++ ) {
		if ( s_patches[i].active ) {
			n++;
		}
	}
	return n;
}

void CM_Stream_TraceMerged( traceWork_t *tw ) {
	int i, b;

	if ( !tw || !cm_streamMerge || !cm_streamMerge->integer ) {
		return;
	}

	cm.checkcount++;
	for ( i = 0; i < CM_MERGE_MAX_SECTORS; i++ ) {
		cmStreamSectorPatch_t *patch = &s_patches[i];

		if ( !patch->active || !patch->brushes || patch->numBrushes <= 0 ) {
			continue;
		}
		if ( !CM_BoundsIntersect( tw->bounds[0], tw->bounds[1], patch->bounds[0], patch->bounds[1] ) ) {
			continue;
		}
		for ( b = 0; b < patch->numBrushes; b++ ) {
			cbrush_t *brush = &patch->brushes[b];

			if ( brush->numsides < 6 || !brush->sides || !brush->sides[0].plane ) {
				continue;
			}
			if ( brush->checkcount == cm.checkcount ) {
				continue;
			}
			brush->checkcount = cm.checkcount;
			if ( !( brush->contents & tw->contents ) ) {
				continue;
			}
			if ( !CM_BoundsIntersect( tw->bounds[0], tw->bounds[1], brush->bounds[0], brush->bounds[1] ) ) {
				continue;
			}
			CM_TraceThroughBrush( tw, brush );
			if ( !tw->trace.fraction ) {
				return;
			}
		}
	}
}

int CM_Stream_PointContentsAt( const vec3_t p ) {
	traceWork_t tw;

	Com_Memset( &tw, 0, sizeof( tw ) );
	CM_Stream_PointContentsMerged( p, &tw );
	return tw.trace.contents;
}

void CM_Stream_PointContentsMerged( const vec3_t p, traceWork_t *tw ) {
	int i, b, k;
	float d;

	if ( !p || !tw || !cm_streamMerge || !cm_streamMerge->integer ) {
		return;
	}

	for ( i = 0; i < CM_MERGE_MAX_SECTORS; i++ ) {
		cmStreamSectorPatch_t *patch = &s_patches[i];

		if ( !patch->active || !patch->brushes ) {
			continue;
		}
		if ( !CM_BoundsIntersectPoint( patch->bounds[0], patch->bounds[1], p ) ) {
			continue;
		}
		for ( b = 0; b < patch->numBrushes; b++ ) {
			cbrush_t *brush = &patch->brushes[b];

			if ( brush->numsides < 6 || !brush->sides || !brush->sides[0].plane ) {
				continue;
			}
			if ( !CM_BoundsIntersectPoint( brush->bounds[0], brush->bounds[1], p ) ) {
				continue;
			}
			for ( k = 0; k < brush->numsides; k++ ) {
				d = DotProduct( p, brush->sides[k].plane->normal );
				if ( d > brush->sides[k].plane->dist ) {
					break;
				}
			}
			if ( k == brush->numsides ) {
				tw->trace.startsolid = qtrue;
				tw->trace.allsolid = qtrue;
				tw->trace.contents |= brush->contents;
			}
		}
	}
}
