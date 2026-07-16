/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "tr_local.h"
#include "tr_bsp_stream.h"
#include "vk_vt.h"
#include "qfiles.h"

#define BSP_STREAM_MAX_PATCHES 64
#define BSP_STREAM_MAX_FACES 16
#define BSP_STREAM_MAX_SECTOR_LIGHTMAPS 8
#define BSP_STREAM_LIGHTMAP_BYTES (128 * 128 * 3)

typedef struct {
	surfaceType_t   *surface;
	shader_t        *shader;
	int             surfaceLightmapNum;
} bspStreamFace_t;

typedef struct {
	qboolean        active;
	int             cellX;
	int             cellY;
	vec3_t          bounds[2];
	bspStreamFace_t faces[BSP_STREAM_MAX_FACES];
	int             numFaces;
	int             lightmapAtlasIndex;
	int             numSectorLightmaps;
	bspStreamLightmapSlot_t sectorLightmaps[BSP_STREAM_MAX_SECTOR_LIGHTMAPS];
	float           sectorSize;
} bspStreamPatch_t;

typedef struct {
	int cellX;
	int cellY;
	int patchIdx;
	qboolean used;
} bspStreamHashSlot_t;

#define BSP_STREAM_HASH_SIZE 128

_Static_assert( ( BSP_STREAM_HASH_SIZE & ( BSP_STREAM_HASH_SIZE - 1 ) ) == 0,
	"BSP_STREAM_HASH_SIZE must be a power of two" );

typedef struct {
	bspStreamPatch_t    patches[BSP_STREAM_MAX_PATCHES];
	bspStreamHashSlot_t patchHash[BSP_STREAM_HASH_SIZE];
} bspStreamModule_t;

static bspStreamModule_t s_stream;

static cvar_t *r_bspStream;
static cvar_t *r_bspStreamResident;
static cvar_t *r_bspStreamBake;
static cvar_t *r_bspStreamVbo;
static cvar_t *r_bspStreamLightmaps;
static cvar_t *r_bspStreamLod;
static qboolean s_cmds;

static void BspStream_Status_f( void )
{
	int i;
	int active = 0;
	int faces = 0;
	int lightmaps = 0;

	for ( i = 0; i < BSP_STREAM_MAX_PATCHES; i++ ) {
		if ( !s_stream.patches[i].active ) {
			continue;
		}
		active++;
		faces += s_stream.patches[i].numFaces;
		lightmaps += s_stream.patches[i].numSectorLightmaps;
	}
	ri.Printf( PRINT_ALL,
		"[bsp_stream] active=%d patches=%d/%d faces=%d lightmaps=%d lod=%d bake=%d vbo=%d\n",
		( r_bspStream && r_bspStream->integer ) ? 1 : 0,
		active, BSP_STREAM_MAX_PATCHES, faces, lightmaps,
		r_bspStreamLod ? r_bspStreamLod->integer : 0,
		( r_bspStreamBake && r_bspStreamBake->integer ) ? 1 : 0,
		( r_bspStreamVbo && r_bspStreamVbo->integer ) ? 1 : 0 );
	for ( i = 0; i < BSP_STREAM_MAX_PATCHES; i++ ) {
		if ( !s_stream.patches[i].active ) {
			continue;
		}
		ri.Printf( PRINT_ALL, "  sector %d,%d faces=%d lm=%d\n",
			s_stream.patches[i].cellX, s_stream.patches[i].cellY,
			s_stream.patches[i].numFaces, s_stream.patches[i].numSectorLightmaps );
	}
}

static const uint32_t BSP_STREAM_HASH_MUL_X = 73856093u;
static const uint32_t BSP_STREAM_HASH_MUL_Y = 19349663u;

static uint32_t R_BspStream_CellHash( int cellX, int cellY )
{
	return ( cellX * BSP_STREAM_HASH_MUL_X ) ^ ( cellY * BSP_STREAM_HASH_MUL_Y );
}

static void R_BspStream_HashClear( void )
{
	Com_Memset( s_stream.patchHash, 0, sizeof( s_stream.patchHash ) );
}

static void R_BspStream_HashInsert( int cellX, int cellY, int patchIdx )
{
	uint32_t hash = R_BspStream_CellHash( cellX, cellY );
	int probe;

	for ( probe = 0; probe < BSP_STREAM_HASH_SIZE; probe++ ) {
		int idx = (int)( ( hash + (uint32_t)probe ) % BSP_STREAM_HASH_SIZE );

		if ( !s_stream.patchHash[idx].used ) {
			s_stream.patchHash[idx].used = qtrue;
			s_stream.patchHash[idx].cellX = cellX;
			s_stream.patchHash[idx].cellY = cellY;
			s_stream.patchHash[idx].patchIdx = patchIdx;
			return;
		}
	}
	ri.Printf( PRINT_WARNING, "[bsp_stream] patch hash table full (%d,%d)\n", cellX, cellY );
}

static void R_BspStream_HashRemove( int cellX, int cellY )
{
	uint32_t hash = R_BspStream_CellHash( cellX, cellY );
	int probe;

	for ( probe = 0; probe < BSP_STREAM_HASH_SIZE; probe++ ) {
		int idx = (int)( ( hash + (uint32_t)probe ) % BSP_STREAM_HASH_SIZE );

		if ( !s_stream.patchHash[idx].used ) {
			return;
		}
		if ( s_stream.patchHash[idx].cellX == cellX && s_stream.patchHash[idx].cellY == cellY ) {
			s_stream.patchHash[idx].used = qfalse;
			return;
		}
	}
}

static int R_BspStream_FindPatch( int cellX, int cellY ) {
	uint32_t hash = R_BspStream_CellHash( cellX, cellY );
	int probe;

	for ( probe = 0; probe < BSP_STREAM_HASH_SIZE; probe++ ) {
		int idx = (int)( ( hash + (uint32_t)probe ) % BSP_STREAM_HASH_SIZE );
		int patchIdx;

		if ( !s_stream.patchHash[idx].used ) {
			break;
		}
		if ( s_stream.patchHash[idx].cellX == cellX && s_stream.patchHash[idx].cellY == cellY ) {
			patchIdx = s_stream.patchHash[idx].patchIdx;
			if ( patchIdx >= 0 && patchIdx < BSP_STREAM_MAX_PATCHES &&
				s_stream.patches[patchIdx].active ) {
				return patchIdx;
			}
			return -1;
		}
	}

	for ( probe = 0; probe < BSP_STREAM_MAX_PATCHES; probe++ ) {
		if ( s_stream.patches[probe].active && s_stream.patches[probe].cellX == cellX &&
			s_stream.patches[probe].cellY == cellY ) {
			return probe;
		}
	}
	return -1;
}

static int R_BspStream_AllocPatch( int cellX, int cellY ) {
	int i;

	i = R_BspStream_FindPatch( cellX, cellY );
	if ( i >= 0 ) {
		return i;
	}
	for ( i = 0; i < BSP_STREAM_MAX_PATCHES; i++ ) {
		if ( !s_stream.patches[i].active ) {
			Com_Memset( &s_stream.patches[i], 0, sizeof( s_stream.patches[i] ) );
			s_stream.patches[i].active = qtrue;
			s_stream.patches[i].cellX = cellX;
			s_stream.patches[i].cellY = cellY;
			s_stream.patches[i].sectorSize = 4096.0f;
			R_BspStream_HashInsert( cellX, cellY, i );
			return i;
		}
	}
	ri.Printf( PRINT_WARNING, "[bsp_stream] patch table full (%d,%d, max %d)\n",
		cellX, cellY, BSP_STREAM_MAX_PATCHES );
	return -1;
}

static qboolean R_BspStream_BoundsFromBrush( dplane_t *planes, int numPlanes,
	dbrushside_t *sides, int numSides, int firstSide, int brushSides,
	vec3_t outMins, vec3_t outMaxs ) {
	int axis, planeNum;
	float dmin, dmax;

	if ( firstSide < 0 || brushSides < 6 || firstSide + brushSides > numSides ) {
		return qfalse;
	}

	for ( axis = 0; axis < 3; axis++ ) {
		planeNum = LittleLong( sides[firstSide + axis * 2].planeNum );
		if ( planeNum < 0 || planeNum >= numPlanes ) {
			return qfalse;
		}
		dmin = LittleFloat( planes[planeNum].dist );
		planeNum = LittleLong( sides[firstSide + axis * 2 + 1].planeNum );
		if ( planeNum < 0 || planeNum >= numPlanes ) {
			return qfalse;
		}
		dmax = LittleFloat( planes[planeNum].dist );
		outMins[axis] = -dmin;
		outMaxs[axis] = dmax;
	}

	return outMaxs[0] > outMins[0] && outMaxs[1] > outMins[1] && outMaxs[2] > outMins[2];
}

static srfSurfaceFace_t *R_BspStream_AllocTopFace( const vec3_t wmins, const vec3_t wmaxs ) {
	srfSurfaceFace_t *cv;
	int sfaceSize;
	int *indexes;
	int i, j;
	vec3_t corners[4];

	sfaceSize = (int)( sizeof( *cv ) - sizeof( cv->points ) + sizeof( cv->points[0] ) * 4 );
	sfaceSize += (int)( sizeof( int ) * 6 );
	cv = ri.Hunk_Alloc( sfaceSize, h_low );
	cv->surfaceType = SF_FACE;
	cv->numPoints = 4;
	cv->numIndices = 6;
	cv->ofsIndices = (int)( sizeof( *cv ) - sizeof( cv->points ) + sizeof( cv->points[0] ) * 4 );
	cv->dlightBits = 0;
#ifdef USE_VBO
	cv->vboItemIndex = 0;
#endif

	VectorSet( corners[0], wmins[0], wmins[1], wmaxs[2] );
	VectorSet( corners[1], wmaxs[0], wmins[1], wmaxs[2] );
	VectorSet( corners[2], wmaxs[0], wmaxs[1], wmaxs[2] );
	VectorSet( corners[3], wmins[0], wmaxs[1], wmaxs[2] );

	for ( i = 0; i < 4; i++ ) {
		for ( j = 0; j < 3; j++ ) {
			cv->points[i][j] = corners[i][j];
		}
		cv->points[i][3] = 0.0f;
		cv->points[i][4] = 0.0f;
		cv->points[i][5] = 1.0f;
		cv->points[i][6] = 0.0f;
		cv->points[i][7] = 0.0f;
		cv->points[i][8] = 0.0f;
		cv->points[i][9] = 0.0f;
		cv->points[i][10] = 255.0f;
	}

	indexes = (int *)( (byte *)cv + cv->ofsIndices );
	indexes[0] = 0;
	indexes[1] = 1;
	indexes[2] = 2;
	indexes[3] = 0;
	indexes[4] = 2;
	indexes[5] = 3;

	cv->plane.normal[0] = 0.0f;
	cv->plane.normal[1] = 0.0f;
	cv->plane.normal[2] = 1.0f;
	cv->plane.dist = wmaxs[2];
	SetPlaneSignbits( &cv->plane );
	cv->plane.type = PlaneTypeForNormal( cv->plane.normal );

	return cv;
}

/*
===============
R_BspStream_BakeGridToTris
Bake a subdivided patch grid to static triangle soup (fixed r_lodCurveError LOD).
Avoids per-frame SF_GRID LOD tessellation for streamed sector overlays.
===============
*/
static surfaceType_t *R_BspStream_BakeGridToTris( srfGridMesh_t *grid )
{
	float lodError;
	int widthTable[MAX_GRID_SIZE];
	int heightTable[MAX_GRID_SIZE];
	int lodWidth, lodHeight;
	int numVerts, numIndexes;
	srfTriangles_t *tri;
	srfVert_t *dv;
	int i, w, h;
	int allocSize;

	if ( !grid || grid->surfaceType != SF_GRID ) {
		return NULL;
	}

	lodError = r_lodCurveError ? r_lodCurveError->value : 0.0f;
	if ( lodError < 0.0f ) {
		lodError = 0.0f;
	}

	widthTable[0] = 0;
	lodWidth = 1;
	for ( i = 1; i < grid->width - 1; i++ ) {
		if ( grid->widthLodError[i] <= lodError ) {
			widthTable[lodWidth] = i;
			lodWidth++;
		}
	}
	widthTable[lodWidth] = grid->width - 1;
	lodWidth++;

	heightTable[0] = 0;
	lodHeight = 1;
	for ( i = 1; i < grid->height - 1; i++ ) {
		if ( grid->heightLodError[i] <= lodError ) {
			heightTable[lodHeight] = i;
			lodHeight++;
		}
	}
	heightTable[lodHeight] = grid->height - 1;
	lodHeight++;

	if ( lodWidth < 2 || lodHeight < 2 ) {
		return NULL;
	}

	numVerts = lodWidth * lodHeight;
	numIndexes = ( lodWidth - 1 ) * ( lodHeight - 1 ) * 6;
	allocSize = (int)( sizeof( *tri ) + numVerts * sizeof( tri->verts[0] ) +
		numIndexes * sizeof( tri->indexes[0] ) );
	tri = ri.Hunk_Alloc( allocSize, h_low );
	tri->surfaceType = SF_TRIANGLES;
	tri->dlightBits = 0;
#ifdef USE_VBO
	tri->vboItemIndex = 0;
#endif
	tri->numVerts = numVerts;
	tri->numIndexes = numIndexes;
	tri->verts = (srfVert_t *)( tri + 1 );
	tri->indexes = (int *)( tri->verts + tri->numVerts );

	ClearBounds( tri->bounds[0], tri->bounds[1] );
	VectorCopy( grid->localOrigin, tri->localOrigin );
	tri->radius = grid->meshRadius;

	i = 0;
	for ( h = 0; h < lodHeight; h++ ) {
		for ( w = 0; w < lodWidth; w++ ) {
			dv = &grid->verts[ heightTable[h] * grid->width + widthTable[w] ];
			tri->verts[i] = *dv;
			AddPointToBounds( tri->verts[i].xyz, tri->bounds[0], tri->bounds[1] );
			i++;
		}
	}

	i = 0;
	for ( h = 0; h < lodHeight - 1; h++ ) {
		for ( w = 0; w < lodWidth - 1; w++ ) {
			int v1 = h * lodWidth + w + 1;
			int v2 = v1 - 1;
			int v3 = v2 + lodWidth;
			int v4 = v3 + 1;

			tri->indexes[i++] = v2;
			tri->indexes[i++] = v3;
			tri->indexes[i++] = v1;
			tri->indexes[i++] = v1;
			tri->indexes[i++] = v3;
			tri->indexes[i++] = v4;
		}
	}

	return (surfaceType_t *)tri;
}

static void R_BspStream_ApplyLightmapST( float *lmS, float *lmT, float lmX, float lmY, qboolean mergedAtlas )
{
	if ( mergedAtlas ) {
		*lmS = *lmS * tr.lightmapScale[0] + lmX;
		*lmT = *lmT * tr.lightmapScale[1] + lmY;
	}
}


static qboolean R_BspStream_UsesStreamAtlas( const bspStreamPatch_t *patch, int surfaceLightmapNum )
{
	return ( patch && surfaceLightmapNum >= 0 && patch->numSectorLightmaps > 0 &&
		patch->lightmapAtlasIndex >= 0 &&
		r_bspStreamLightmaps && r_bspStreamLightmaps->integer );
}


#ifdef USE_VK_PBR
static void R_BspStream_GenerateFaceLightDirs( srfSurfaceFace_t *face )
{
	int i;

	if ( !face || !tr.world || !vk.pbrActive ) {
		return;
	}

	face->lightdir = (float *)ri.Hunk_Alloc( face->numPoints * sizeof( tess.lightdir[0] ), h_low );
	for ( i = 0; i < face->numPoints; i++ ) {
		R_LightDirForPoint( face->points[i], face->lightdir + i * 4, face->points[i] + 3, tr.world );
	}
}


static void R_BspStream_FinalizeFacePbr( srfSurfaceFace_t *face, shader_t *shader )
{
	if ( !face ) {
		return;
	}
	if ( shader && shader->numUnfoggedPasses && shader->lightingStage >= 0 ) {
		if ( fabsf( face->plane.normal[0] ) < 0.01f &&
			fabsf( face->plane.normal[1] ) < 0.01f &&
			fabsf( face->plane.normal[2] ) < 0.01f ) {
			R_BspGenerateFaceNormals( face );
		}
	}
	vk_mikkt_bsp_face_generate( face );
	R_BspStream_GenerateFaceLightDirs( face );
}


static void R_BspStream_FinalizeTriPbr( srfTriangles_t *tri )
{
	int i;

	if ( !tri || !tr.world || !vk.pbrActive ) {
		return;
	}
	for ( i = 0; i < tri->numVerts; i++ ) {
		R_LightDirForPoint( tri->verts[i].xyz, tri->verts[i].lightdir, tri->verts[i].normal, tr.world );
	}
}


static void R_BspStream_FinalizeGridPbr( srfGridMesh_t *grid )
{
	int i, numPoints;

	if ( !grid || !tr.world || !vk.pbrActive ) {
		return;
	}
	numPoints = grid->width * grid->height;
	for ( i = 0; i < numPoints; i++ ) {
		R_LightDirForPoint( grid->verts[i].xyz, grid->verts[i].lightdir, grid->verts[i].normal, tr.world );
	}
}
#endif


static void R_BspStream_FinalizeSurfacePbr( surfaceType_t *surface, shader_t *shader )
{
	if ( !surface ) {
		return;
	}
	switch ( *surface ) {
	case SF_FACE:
#ifdef USE_VK_PBR
		R_BspStream_FinalizeFacePbr( (srfSurfaceFace_t *)surface, shader );
#endif
		break;
	case SF_TRIANGLES:
#ifdef USE_VK_PBR
		R_BspStream_FinalizeTriPbr( (srfTriangles_t *)surface );
#endif
		break;
	case SF_GRID:
#ifdef USE_VK_PBR
		R_BspStream_FinalizeGridPbr( (srfGridMesh_t *)surface );
#endif
		break;
	default:
		break;
	}
}


static shader_t *R_BspStream_ShaderForSurface( const dshader_t *dshaders, int shaderNum, int numShaders,
	bspStreamPatch_t *patch, int surfaceLightmapNum, float *outLmX, float *outLmY )
{
	int shaderLm = LIGHTMAP_BY_VERTEX;

	*outLmX = 0.0f;
	*outLmY = 0.0f;

	if ( surfaceLightmapNum >= 0 && patch && patch->numSectorLightmaps > 0 &&
		patch->lightmapAtlasIndex >= 0 &&
		r_bspStreamLightmaps && r_bspStreamLightmaps->integer ) {
		if ( surfaceLightmapNum < patch->numSectorLightmaps ) {
			*outLmX = patch->sectorLightmaps[surfaceLightmapNum].uvX;
			*outLmY = patch->sectorLightmaps[surfaceLightmapNum].uvY;
			shaderLm = patch->lightmapAtlasIndex;
		}
	} else if ( surfaceLightmapNum >= 0 && tr.mergeLightmaps ) {
		shaderLm = R_GetLightmapCoords( surfaceLightmapNum, outLmX, outLmY );
	}

	if ( shaderNum >= 0 && shaderNum < numShaders && dshaders ) {
		return R_FindShader( dshaders[shaderNum].shader, shaderLm, qtrue );
	}

	return tr.defaultShader;
}


static qboolean R_BspStream_LoadSectorLightmaps( bspStreamPatch_t *patch, const dheader_t *header, byte *base )
{
	const lump_t *l;
	int numLm, i, bytesPerLm, lumpStride;
	const byte *buf;

	patch->numSectorLightmaps = 0;
	if ( patch->lightmapAtlasIndex < 0 ) {
		patch->lightmapAtlasIndex = -1;
	}

	if ( !patch || !header || !base ) {
		return qfalse;
	}
	if ( !r_bspStreamLightmaps || !r_bspStreamLightmaps->integer ) {
		return qfalse;
	}

	l = &header->lumps[LUMP_LIGHTMAPS];
	bytesPerLm = BSP_STREAM_LIGHTMAP_BYTES;
	lumpStride = bytesPerLm;
	if ( tr.worldDeluxeMapping ) {
		lumpStride = bytesPerLm * 2;
	}
	if ( l->filelen < bytesPerLm ) {
		return qfalse;
	}

	numLm = l->filelen / lumpStride;
	if ( l->filelen % lumpStride != 0 && l->filelen >= bytesPerLm ) {
		numLm = l->filelen / bytesPerLm;
		lumpStride = bytesPerLm;
		if ( tr.worldDeluxeMapping ) {
			ri.Printf( PRINT_DEVELOPER,
				"[bsp_stream] sector lightmap lump missing deluxe pairs; RGB only\n" );
		}
	}
	if ( numLm > BSP_STREAM_MAX_SECTOR_LIGHTMAPS ) {
		numLm = BSP_STREAM_MAX_SECTOR_LIGHTMAPS;
	}

	if ( patch->lightmapAtlasIndex < 0 ) {
		patch->lightmapAtlasIndex = R_BspStreamLightmap_RegisterAtlas();
	}
	buf = base + l->fileofs;

	for ( i = 0; i < numLm; i++ ) {
		const byte *rgb = buf + i * lumpStride;

		if ( !R_BspStreamLightmap_AllocSlot( &patch->sectorLightmaps[i] ) ) {
			break;
		}
		R_BspStreamLightmap_UploadTile( &patch->sectorLightmaps[i], rgb );
		if ( tr.worldDeluxeMapping && lumpStride >= bytesPerLm * 2 ) {
			R_BspStreamLightmap_UploadDeluxeTile( &patch->sectorLightmaps[i], rgb + bytesPerLm );
		}
		patch->numSectorLightmaps++;
	}

	if ( patch->numSectorLightmaps > 0 ) {
		ri.Printf( PRINT_DEVELOPER,
			"[bsp_stream] uploaded %d sector lightmap(s) to atlas index %d%s\n",
			patch->numSectorLightmaps, patch->lightmapAtlasIndex,
			( tr.worldDeluxeMapping && lumpStride >= bytesPerLm * 2 ) ? " (+deluxe)" : "" );
	}

	return patch->numSectorLightmaps > 0;
}


static qboolean R_BspStream_ParsePlanarFace( const dsurface_t *ds, const drawVert_t *verts,
	const int *indexes, const dshader_t *dshaders, int numShaders, const vec3_t worldOrigin,
	bspStreamPatch_t *patch, bspStreamFace_t *outFace, vec3_t patchMins, vec3_t patchMaxs ) {
	srfSurfaceFace_t *cv;
	int numPoints, numIndexes, sfaceSize, ofsIndexes;
	int i, j;
	int shaderNum;
	int surfaceLightmapNum;
	float lightmapX, lightmapY;
	qboolean mergedAtlas;

	if ( !ds || !verts || !indexes || !outFace ) {
		return qfalse;
	}
	if ( LittleLong( ds->surfaceType ) != MST_PLANAR ) {
		return qfalse;
	}

	numPoints = LittleLong( ds->numVerts );
	if ( numPoints <= 0 || numPoints > MAX_FACE_POINTS ) {
		return qfalse;
	}
	numIndexes = LittleLong( ds->numIndexes );
	if ( numIndexes < 3 || ( numIndexes % 3 ) != 0 ) {
		return qfalse;
	}

	surfaceLightmapNum = LittleLong( ds->lightmapNum );
	shaderNum = LittleLong( ds->shaderNum );
	outFace->shader = R_BspStream_ShaderForSurface( dshaders, shaderNum, numShaders, patch,
		surfaceLightmapNum, &lightmapX, &lightmapY );
	outFace->surfaceLightmapNum = surfaceLightmapNum;
	mergedAtlas = ( surfaceLightmapNum >= 0 &&
		( tr.mergeLightmaps || R_BspStream_UsesStreamAtlas( patch, surfaceLightmapNum ) ) );

	sfaceSize = (int)( sizeof( *cv ) - sizeof( cv->points ) + sizeof( cv->points[0] ) * numPoints );
	ofsIndexes = sfaceSize;
	sfaceSize += (int)( sizeof( int ) * numIndexes );
	cv = ri.Hunk_Alloc( sfaceSize, h_low );
	cv->surfaceType = SF_FACE;
	cv->numPoints = numPoints;
	cv->numIndices = numIndexes;
	cv->ofsIndices = ofsIndexes;
	cv->dlightBits = 0;
#ifdef USE_VBO
	cv->vboItemIndex = 0;
#endif

	verts += LittleLong( ds->firstVert );
	for ( i = 0; i < numPoints; i++ ) {
		for ( j = 0; j < 3; j++ ) {
			cv->points[i][j] = LittleFloat( verts[i].xyz[j] ) + worldOrigin[j];
			cv->points[i][3 + j] = LittleFloat( verts[i].normal[j] );
		}
		for ( j = 0; j < 2; j++ ) {
			cv->points[i][6 + j] = LittleFloat( verts[i].st[j] );
			cv->points[i][8 + j] = LittleFloat( verts[i].lightmap[j] );
		}
		R_BspStream_ApplyLightmapST( &cv->points[i][8], &cv->points[i][9], lightmapX, lightmapY, mergedAtlas );
		R_ColorShiftLightingBytes( verts[i].color.rgba, (byte *)&cv->points[i][10], qtrue );
		AddPointToBounds( cv->points[i], patchMins, patchMaxs );
	}

	indexes += LittleLong( ds->firstIndex );
	for ( i = 0; i < numIndexes; i++ ) {
		( (int *)( (byte *)cv + cv->ofsIndices ) )[i] = LittleLong( indexes[i] );
	}

	for ( i = 0; i < 3; i++ ) {
		cv->plane.normal[i] = LittleFloat( ds->lightmapVecs[2][i] );
	}
	for ( i = 0; i < 3; i++ ) {
		cv->plane.normal[i] = R_ClampDenorm( cv->plane.normal[i] );
	}
	cv->plane.dist = DotProduct( cv->points[0], cv->plane.normal );
	SetPlaneSignbits( &cv->plane );
	cv->plane.type = PlaneTypeForNormal( cv->plane.normal );

	outFace->surface = (surfaceType_t *)cv;
	R_BspStream_FinalizeSurfacePbr( outFace->surface, outFace->shader );
	return qtrue;
}

static qboolean R_BspStream_ParsePatchFace( const dsurface_t *ds, const drawVert_t *verts,
	const dshader_t *dshaders, int numShaders, const vec3_t worldOrigin, bspStreamPatch_t *patch,
	bspStreamFace_t *outFace, vec3_t patchMins, vec3_t patchMaxs ) {
	srfGridMesh_t *grid;
	srfVert_t points[MAX_PATCH_SIZE * MAX_PATCH_SIZE];
	int width, height, numPoints;
	int surfaceLightmapNum;
	float lightmapX, lightmapY;
	qboolean mergedAtlas;
	int shaderNum;
	vec3_t bounds[2];
	vec3_t tmpVec;
	int i, j;

	if ( !ds || !verts || !outFace ) {
		return qfalse;
	}
	if ( LittleLong( ds->surfaceType ) != MST_PATCH ) {
		return qfalse;
	}

	width = LittleLong( ds->patchWidth );
	height = LittleLong( ds->patchHeight );
	if ( width < 2 || height < 2 || width > MAX_PATCH_SIZE || height > MAX_PATCH_SIZE ) {
		return qfalse;
	}

	numPoints = width * height;
	if ( LittleLong( ds->numVerts ) < numPoints ) {
		return qfalse;
	}

	surfaceLightmapNum = LittleLong( ds->lightmapNum );
	shaderNum = LittleLong( ds->shaderNum );
	if ( shaderNum >= 0 && shaderNum < numShaders && dshaders ) {
		if ( dshaders[shaderNum].surfaceFlags & SURF_NODRAW ) {
			return qfalse;
		}
	}
	outFace->shader = R_BspStream_ShaderForSurface( dshaders, shaderNum, numShaders, patch,
		surfaceLightmapNum, &lightmapX, &lightmapY );
	outFace->surfaceLightmapNum = surfaceLightmapNum;
	mergedAtlas = ( surfaceLightmapNum >= 0 &&
		( tr.mergeLightmaps || R_BspStream_UsesStreamAtlas( patch, surfaceLightmapNum ) ) );

	verts += LittleLong( ds->firstVert );
	for ( i = 0; i < numPoints; i++ ) {
		for ( j = 0; j < 3; j++ ) {
			points[i].xyz[j] = LittleFloat( verts[i].xyz[j] ) + worldOrigin[j];
			points[i].normal[j] = R_ClampDenorm( LittleFloat( verts[i].normal[j] ) );
		}
		for ( j = 0; j < 2; j++ ) {
			points[i].st[j] = LittleFloat( verts[i].st[j] );
			points[i].lightmap[j] = LittleFloat( verts[i].lightmap[j] );
		}
		R_ColorShiftLightingBytes( verts[i].color.rgba, points[i].color.rgba, qtrue );
		if ( mergedAtlas ) {
			R_BspStream_ApplyLightmapST( &points[i].lightmap[0], &points[i].lightmap[1],
				lightmapX, lightmapY, qtrue );
		} else if ( surfaceLightmapNum >= 0 && tr.mergeLightmaps ) {
			points[i].lightmap[0] = points[i].lightmap[0] * tr.lightmapScale[0] + lightmapX;
			points[i].lightmap[1] = points[i].lightmap[1] * tr.lightmapScale[1] + lightmapY;
		}
		AddPointToBounds( points[i].xyz, patchMins, patchMaxs );
	}

	grid = R_SubdividePatchToGrid( width, height, points );
	if ( !grid ) {
		return qfalse;
	}

	for ( i = 0; i < 3; i++ ) {
		bounds[0][i] = LittleFloat( ds->lightmapVecs[0][i] ) + worldOrigin[i];
		bounds[1][i] = LittleFloat( ds->lightmapVecs[1][i] ) + worldOrigin[i];
	}
	VectorAdd( bounds[0], bounds[1], bounds[1] );
	VectorScale( bounds[1], 0.5f, grid->lodOrigin );
	VectorSubtract( bounds[0], grid->lodOrigin, tmpVec );
	grid->lodRadius = VectorLength( tmpVec );

	if ( r_bspStreamBake && r_bspStreamBake->integer ) {
		surfaceType_t *baked = R_BspStream_BakeGridToTris( grid );
		if ( baked ) {
			outFace->surface = baked;
			R_BspStream_FinalizeSurfacePbr( outFace->surface, outFace->shader );
			return qtrue;
		}
	}

	outFace->surface = (surfaceType_t *)grid;
	R_BspStream_FinalizeSurfacePbr( outFace->surface, outFace->shader );
	return qtrue;
}

static int R_BspStream_LoadSurfaceLumps( bspStreamPatch_t *patch, dheader_t *header,
	byte *base, const vec3_t worldOrigin ) {
	const dsurface_t *surfs;
	const drawVert_t *verts;
	const int *indexes;
	const dshader_t *dshaders;
	int numSurfaces, numVerts, numIndexes, numShaders;
	int i, loaded;

	if ( header->lumps[LUMP_SURFACES].filelen < (int)sizeof( dsurface_t ) ) {
		return 0;
	}
	if ( header->lumps[LUMP_DRAWVERTS].filelen < (int)sizeof( drawVert_t ) ) {
		return 0;
	}
	if ( header->lumps[LUMP_DRAWINDEXES].filelen < (int)sizeof( int ) ) {
		return 0;
	}

	surfs = (const dsurface_t *)( base + header->lumps[LUMP_SURFACES].fileofs );
	numSurfaces = header->lumps[LUMP_SURFACES].filelen / (int)sizeof( dsurface_t );
	verts = (const drawVert_t *)( base + header->lumps[LUMP_DRAWVERTS].fileofs );
	numVerts = header->lumps[LUMP_DRAWVERTS].filelen / (int)sizeof( drawVert_t );
	indexes = (const int *)( base + header->lumps[LUMP_DRAWINDEXES].fileofs );
	numIndexes = header->lumps[LUMP_DRAWINDEXES].filelen / (int)sizeof( int );
	dshaders = (const dshader_t *)( base + header->lumps[LUMP_SHADERS].fileofs );
	numShaders = header->lumps[LUMP_SHADERS].filelen / (int)sizeof( dshader_t );

	R_BspStream_LoadSectorLightmaps( patch, header, base );

	loaded = 0;
	for ( i = 0; i < numSurfaces && patch->numFaces < BSP_STREAM_MAX_FACES; i++ ) {
		const dsurface_t *ds = &surfs[i];
		int surfaceType = LittleLong( ds->surfaceType );
		int firstVert = LittleLong( ds->firstVert );
		int numSurfVerts = LittleLong( ds->numVerts );
		int firstIndex = LittleLong( ds->firstIndex );
		int numSurfIndexes = LittleLong( ds->numIndexes );

		if ( surfaceType == MST_PATCH ) {
			if ( firstVert < 0 || firstVert + numSurfVerts > numVerts ) {
				continue;
			}
			if ( R_BspStream_ParsePatchFace( ds, verts, dshaders, numShaders, worldOrigin, patch,
				&patch->faces[patch->numFaces], patch->bounds[0], patch->bounds[1] ) ) {
				patch->numFaces++;
				loaded++;
			}
			continue;
		}
		if ( firstVert < 0 || numSurfVerts <= 0 || firstVert + numSurfVerts > numVerts ) {
			continue;
		}
		if ( firstIndex < 0 || numSurfIndexes < 3 || firstIndex + numSurfIndexes > numIndexes ) {
			continue;
		}
		if ( R_BspStream_ParsePlanarFace( ds, verts, indexes, dshaders, numShaders, worldOrigin, patch,
			&patch->faces[patch->numFaces], patch->bounds[0], patch->bounds[1] ) ) {
			patch->numFaces++;
			loaded++;
		}
	}

	/* Distance LOD: far sectors keep fewer faces (id Tech 8-style stream fidelity). */
	if ( r_bspStreamLod && r_bspStreamLod->integer > 0 && patch->numFaces > 1 ) {
		vec3_t center;
		float dist;
		float sectorSize = patch->sectorSize > 1.0f ? patch->sectorSize : 4096.0f;
		float nearDist = sectorSize * 1.5f;
		float farDist = sectorSize * 3.5f;

		center[0] = 0.5f * ( patch->bounds[0][0] + patch->bounds[1][0] );
		center[1] = 0.5f * ( patch->bounds[0][1] + patch->bounds[1][1] );
		center[2] = 0.5f * ( patch->bounds[0][2] + patch->bounds[1][2] );
		dist = Distance( tr.refdef.vieworg, center );
		if ( dist > farDist ) {
			patch->numFaces = ( r_bspStreamLod->integer >= 2 ) ? 1 : ( patch->numFaces > 2 ? 2 : 1 );
			ri.Printf( PRINT_DEVELOPER, "[bsp_stream] lod far %d,%d faces=%d dist=%.0f\n",
				patch->cellX, patch->cellY, patch->numFaces, dist );
		} else if ( dist > nearDist && r_bspStreamLod->integer >= 1 && patch->numFaces > 4 ) {
			patch->numFaces = 4;
			ri.Printf( PRINT_DEVELOPER, "[bsp_stream] lod mid %d,%d faces=%d dist=%.0f\n",
				patch->cellX, patch->cellY, patch->numFaces, dist );
		}
	}
	return loaded;
}


static qboolean R_BspStream_ReloadPatchFromDisk( bspStreamPatch_t *patch )
{
	char mapName[MAX_QPATH];
	void *buf;
	int length;
	dheader_t header;
	byte *base;
	vec3_t worldOrigin;
	int i, loaded;

	if ( !patch || !patch->active ) {
		return qfalse;
	}

	Com_sprintf( mapName, sizeof( mapName ), "maps/sector_%d_%d.bsp", patch->cellX, patch->cellY );
	length = ri.FS_ReadFile( mapName, &buf );
	if ( length <= 0 || !buf ) {
		return qfalse;
	}
	if ( (size_t)length < sizeof( dheader_t ) ) {
		ri.FS_FreeFile( buf );
		return qfalse;
	}

	header = *(dheader_t *)buf;
	for ( i = 0; (size_t)i < sizeof( dheader_t ) / sizeof( int32_t ); i++ ) {
		( (int32_t *)&header )[i] = LittleLong( ( (int32_t *)&header )[i] );
	}
	if ( header.version != BSP_VERSION ) {
		ri.FS_FreeFile( buf );
		return qfalse;
	}

	base = (byte *)buf;
	worldOrigin[0] = (float)patch->cellX * patch->sectorSize;
	worldOrigin[1] = (float)patch->cellY * patch->sectorSize;
	worldOrigin[2] = 0.0f;

	patch->numFaces = 0;
	ClearBounds( patch->bounds[0], patch->bounds[1] );
	loaded = R_BspStream_LoadSurfaceLumps( patch, &header, base, worldOrigin );
	ri.FS_FreeFile( buf );

	return loaded > 0;
}


static void R_BspStream_CompactLightmaps( void )
{
	int i, reloaded = 0;

	if ( !r_bspStreamLightmaps || !r_bspStreamLightmaps->integer ) {
		return;
	}

	R_BspStreamLightmap_ResetTiles();
	for ( i = 0; i < BSP_STREAM_MAX_PATCHES; i++ ) {
		bspStreamPatch_t *patch = &s_stream.patches[i];

		if ( !patch->active ) {
			continue;
		}
		if ( patch->numSectorLightmaps < 1 && patch->lightmapAtlasIndex < 0 ) {
			continue;
		}
		if ( R_BspStream_ReloadPatchFromDisk( patch ) ) {
			reloaded++;
		} else {
			ri.Printf( PRINT_WARNING, "[bsp_stream] lightmap compact: reload failed %d,%d\n",
				patch->cellX, patch->cellY );
		}
	}
	if ( reloaded > 0 ) {
		ri.Printf( PRINT_DEVELOPER, "[bsp_stream] compacted stream lightmap atlas (%d patches)\n", reloaded );
	}
}

void R_BspStream_Init( void ) {
	r_bspStream = ri.Cvar_Get( "r_bspStream", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_bspStream,
		"Merge sector BSP brush tops as visual overlay (open-world renderer streaming)." );
	r_bspStreamResident = ri.Cvar_Get( "r_bspStreamResident", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_bspStreamResident,
		"Log streamed sector face residency (planar, patch, brush-top fallback, stream VBO count, sector lightmaps)." );
	r_bspStreamBake = ri.Cvar_Get( "r_bspStreamBake", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_bspStreamBake,
		"When 1, bake streamed patch grids to static triangle soup at merge (r_lodCurveError LOD)." );
	r_bspStreamVbo = ri.Cvar_Get( "r_bspStreamVbo", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_bspStreamVbo,
		"When 1 and r_vbo 1, upload static streamed sector surfaces to a dedicated GPU stream VBO." );
	r_bspStreamLightmaps = ri.Cvar_Get( "r_bspStreamLightmaps", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_bspStreamLightmaps,
		"When 1, upload sector BSP LUMP_LIGHTMAPS (RGB + deluxe pairs when worldDeluxeMapping) into a stream atlas; tiles compact on unmerge." );
	r_bspStreamLod = ri.Cvar_Get( "r_bspStreamLod", "0", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_bspStreamLod, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_bspStreamLod,
		"Sector visual LOD: 0=full faces, 1=distance (far sectors keep 1 face), 2=aggressive (far=brush-top only)." );
	Com_Memset( &s_stream, 0, sizeof( s_stream ) );
	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "bsp_stream_status", BspStream_Status_f );
		s_cmds = qtrue;
	}
	ri.Printf( PRINT_ALL, "[bsp_stream] visual sector overlay initialized (r_bspStream 1, bake %s, stream VBO %s, lightmaps %s, lod %d)\n",
		( r_bspStreamBake && r_bspStreamBake->integer ) ? "on" : "off",
		( r_bspStreamVbo && r_bspStreamVbo->integer ) ? "on" : "off",
		( r_bspStreamLightmaps && r_bspStreamLightmaps->integer ) ? "on" : "off",
		r_bspStreamLod ? r_bspStreamLod->integer : 0 );
}

void RE_BspStream_ClearAll( void ) {
	Com_Memset( &s_stream, 0, sizeof( s_stream ) );
	R_BspStream_HashClear();
	VBO_StreamClear();
	R_BspStreamLightmap_Reset();
	ri.Printf( PRINT_DEVELOPER, "[bsp_stream] cleared sector visual overlays\n" );
}

static void R_BspStream_SetSurfaceVboIndex( surfaceType_t *surface, int vboItemIndex )
{
	if ( !surface ) {
		return;
	}
	switch ( *surface ) {
	case SF_FACE:
		( (srfSurfaceFace_t *)surface )->vboItemIndex = vboItemIndex;
		break;
	case SF_GRID:
		( (srfGridMesh_t *)surface )->vboItemIndex = vboItemIndex;
		break;
	case SF_TRIANGLES:
		( (srfTriangles_t *)surface )->vboItemIndex = vboItemIndex;
		break;
	default:
		break;
	}
}


static void R_BspStream_UploadPatchVbo( bspStreamPatch_t *patch )
{
	int f, uploaded;

	if ( !patch || !patch->active ) {
		return;
	}
	if ( !r_bspStreamVbo || !r_bspStreamVbo->integer || !r_vbo || !r_vbo->integer || !tr.world ) {
		return;
	}

	uploaded = 0;
	for ( f = 0; f < patch->numFaces; f++ ) {
		int vboItem = 0;
		surfaceType_t *surface = patch->faces[f].surface;
		shader_t *shader = patch->faces[f].shader;

		if ( !surface || !shader ) {
			continue;
		}
		if ( VBO_StreamUploadSurface( surface, shader, &vboItem ) ) {
			R_BspStream_SetSurfaceVboIndex( surface, vboItem );
			uploaded++;
		}
	}
	if ( uploaded > 0 && VBO_StreamFlushGpu() ) {
		ri.Printf( PRINT_DEVELOPER, "[bsp_stream] incremental VBO +%d surfaces for %d,%d\n",
			uploaded, patch->cellX, patch->cellY );
	}
}

static void R_BspStream_RebuildVbo( void )
{
	int i, f, uploaded;

	if ( !r_bspStreamVbo || !r_bspStreamVbo->integer || !r_vbo || !r_vbo->integer || !tr.world ) {
		return;
	}

	VBO_StreamClear();
	uploaded = 0;
	for ( i = 0; i < BSP_STREAM_MAX_PATCHES; i++ ) {
		if ( !s_stream.patches[i].active ) {
			continue;
		}
		for ( f = 0; f < s_stream.patches[i].numFaces; f++ ) {
			int vboItem = 0;
			surfaceType_t *surface = s_stream.patches[i].faces[f].surface;
			shader_t *shader = s_stream.patches[i].faces[f].shader;

			if ( !surface || !shader ) {
				continue;
			}
			if ( VBO_StreamUploadSurface( surface, shader, &vboItem ) ) {
				R_BspStream_SetSurfaceVboIndex( surface, vboItem );
				uploaded++;
			}
		}
	}

	if ( uploaded > 0 && VBO_StreamFlushGpu() ) {
		ri.Printf( PRINT_ALL, "[bsp_stream] stream VBO: %d static surfaces uploaded\n", uploaded );
	}
}


static void R_BspStream_CountVboSurfaces( bspStreamPatch_t *patch, int *vboFaces )
{
	int f;

	if ( !patch || !vboFaces ) {
		return;
	}
	for ( f = 0; f < patch->numFaces; f++ ) {
		surfaceType_t *surface = patch->faces[f].surface;
		int idx = 0;

		if ( !surface ) {
			continue;
		}
		switch ( *surface ) {
		case SF_FACE:
			idx = ( (srfSurfaceFace_t *)surface )->vboItemIndex;
			break;
		case SF_GRID:
			idx = ( (srfGridMesh_t *)surface )->vboItemIndex;
			break;
		case SF_TRIANGLES:
			idx = ( (srfTriangles_t *)surface )->vboItemIndex;
			break;
		default:
			break;
		}
		if ( VBO_ItemIsStream( idx ) ) {
			( *vboFaces )++;
		}
	}
}

static void R_BspStream_CountSurfaceGeometry( const surfaceType_t *surface, int *verts, int *indexes )
{
	if ( !surface || !verts || !indexes ) {
		return;
	}
	switch ( *surface ) {
	case SF_FACE: {
		const srfSurfaceFace_t *cv = (const srfSurfaceFace_t *)surface;
		*verts += cv->numPoints;
		*indexes += cv->numIndices;
		break;
	}
	case SF_GRID: {
		const srfGridMesh_t *grid = (const srfGridMesh_t *)surface;
		*verts += grid->width * grid->height;
		*indexes += ( grid->width - 1 ) * ( grid->height - 1 ) * 6;
		break;
	}
	case SF_TRIANGLES: {
		const srfTriangles_t *tri = (const srfTriangles_t *)surface;
		*verts += tri->numVerts;
		*indexes += tri->numIndexes;
		break;
	}
	default:
		break;
	}
}

static void R_BspStream_LogResidency( int cellX, int cellY, const char *mapName, const char *path,
	bspStreamPatch_t *patch ) {
	int f, verts = 0, indexes = 0, vboFaces = 0;

	if ( !r_bspStreamResident || !r_bspStreamResident->integer || !patch ) {
		return;
	}
	for ( f = 0; f < patch->numFaces; f++ ) {
		R_BspStream_CountSurfaceGeometry( patch->faces[f].surface, &verts, &indexes );
	}
	R_BspStream_CountVboSurfaces( patch, &vboFaces );
	ri.Printf( PRINT_ALL,
		"[bsp_stream] residency %d,%d %s (%s): %d faces, %d verts, %d indexes, %d stream VBO, %d sector LMs\n",
		cellX, cellY, mapName, path, patch->numFaces, verts, indexes, vboFaces, patch->numSectorLightmaps );
}

qboolean RE_BspStream_MergeSector( int cellX, int cellY, float sectorSize ) {
	char mapName[MAX_QPATH];
	void *buf;
	int length;
	dheader_t header;
	byte *base;
	vec3_t worldOrigin;
	int patchIdx;
	bspStreamPatch_t *patch;
	lump_t *l;
	dplane_t *inPlanes;
	dbrushside_t *inSides;
	dbrush_t *inBrushes;
	int numPlanes, numSides, numBrushes;
	int i, firstSide, brushSides;
	vec3_t mins, maxs, wmins, wmaxs;
	qboolean anyBrush;

	if ( !r_bspStream || !r_bspStream->integer ) {
		return qfalse;
	}
	if ( !Cvar_VariableIntegerValue( "r_openWorld" ) ) {
		return qfalse;
	}
	if ( R_BspStream_FindPatch( cellX, cellY ) >= 0 ) {
		return qtrue;
	}
	if ( sectorSize < 256.0f ) {
		sectorSize = 4096.0f;
	}

	patchIdx = R_BspStream_AllocPatch( cellX, cellY );
	if ( patchIdx < 0 ) {
		ri.Printf( PRINT_WARNING, "[bsp_stream] patch table full (%d,%d)\n", cellX, cellY );
		return qfalse;
	}
	patch = &s_stream.patches[patchIdx];
	patch->sectorSize = sectorSize;

	Com_sprintf( mapName, sizeof( mapName ), "maps/sector_%d_%d.bsp", cellX, cellY );
	length = ri.FS_ReadFile( mapName, &buf );
	if ( length <= 0 || !buf ) {
		patch->active = qfalse;
		return qfalse;
	}
	if ( (size_t)length < sizeof( dheader_t ) ) {
		ri.FS_FreeFile( buf );
		patch->active = qfalse;
		return qfalse;
	}

	header = *(dheader_t *)buf;
	for ( i = 0; (size_t)i < sizeof( dheader_t ) / sizeof( int32_t ); i++ ) {
		( (int32_t *)&header )[i] = LittleLong( ( (int32_t *)&header )[i] );
	}
	if ( header.version != BSP_VERSION ) {
		ri.FS_FreeFile( buf );
		patch->active = qfalse;
		return qfalse;
	}

	base = (byte *)buf;
	worldOrigin[0] = (float)cellX * sectorSize;
	worldOrigin[1] = (float)cellY * sectorSize;
	worldOrigin[2] = 0.0f;

	l = &header.lumps[LUMP_PLANES];
	inPlanes = (dplane_t *)( base + l->fileofs );
	numPlanes = l->filelen / sizeof( *inPlanes );
	l = &header.lumps[LUMP_BRUSHSIDES];
	inSides = (dbrushside_t *)( base + l->fileofs );
	numSides = l->filelen / sizeof( *inSides );
	l = &header.lumps[LUMP_BRUSHES];
	inBrushes = (dbrush_t *)( base + l->fileofs );
	numBrushes = l->filelen / sizeof( *inBrushes );

	ClearBounds( patch->bounds[0], patch->bounds[1] );
	patch->numFaces = 0;
	anyBrush = qfalse;

	if ( R_BspStream_LoadSurfaceLumps( patch, &header, base, worldOrigin ) > 0 ) {
		ri.FS_FreeFile( buf );
		R_BspStream_UploadPatchVbo( patch );
		R_BspStream_LogResidency( cellX, cellY, mapName, "surfaces", patch );
		ri.Printf( PRINT_ALL, "[bsp_stream] merged visual sector %d,%d (%s, %d surfaces)\n",
			cellX, cellY, mapName, patch->numFaces );
		return qtrue;
	}

	for ( i = 0; i < numBrushes; i++ ) {
		firstSide = LittleLong( inBrushes[i].firstSide );
		brushSides = LittleLong( inBrushes[i].numSides );
		if ( !R_BspStream_BoundsFromBrush( inPlanes, numPlanes, inSides, numSides,
			firstSide, brushSides, mins, maxs ) ) {
			continue;
		}
		wmins[0] = mins[0] + worldOrigin[0];
		wmins[1] = mins[1] + worldOrigin[1];
		wmins[2] = mins[2] + worldOrigin[2];
		wmaxs[0] = maxs[0] + worldOrigin[0];
		wmaxs[1] = maxs[1] + worldOrigin[1];
		wmaxs[2] = maxs[2] + worldOrigin[2];
		AddPointToBounds( wmins, patch->bounds[0], patch->bounds[1] );
		AddPointToBounds( wmaxs, patch->bounds[0], patch->bounds[1] );
		anyBrush = qtrue;
	}

	ri.FS_FreeFile( buf );

	if ( anyBrush ) {
		vec3_t quadMins, quadMaxs;

		quadMins[0] = patch->bounds[0][0];
		quadMins[1] = patch->bounds[0][1];
		quadMins[2] = patch->bounds[1][2];
		quadMaxs[0] = patch->bounds[1][0];
		quadMaxs[1] = patch->bounds[1][1];
		quadMaxs[2] = patch->bounds[1][2];
		patch->faces[0].surface = (surfaceType_t *)R_BspStream_AllocTopFace( quadMins, quadMaxs );
		if ( R_VT_WantSample() ) {
			shader_t *vtSh = R_GetShaderByHandle( R_VT_AtlasShader() );
			patch->faces[0].shader = vtSh ? vtSh : tr.defaultShader;
		} else {
			patch->faces[0].shader = tr.defaultShader;
		}
		patch->faces[0].surfaceLightmapNum = -1;
		patch->numFaces = patch->faces[0].surface ? 1 : 0;
		if ( patch->numFaces > 0 ) {
			R_BspStream_FinalizeSurfacePbr( patch->faces[0].surface, patch->faces[0].shader );
		}
	}

	if ( !anyBrush || patch->numFaces < 1 ) {
		patch->active = qfalse;
		return qfalse;
	}

	R_BspStream_UploadPatchVbo( patch );
	R_BspStream_LogResidency( cellX, cellY, mapName, "brush-top", patch );
	ri.Printf( PRINT_ALL, "[bsp_stream] merged visual sector %d,%d (%s, brush-top)\n", cellX, cellY, mapName );
	return qtrue;
}

void RE_BspStream_UnmergeSector( int cellX, int cellY ) {
	int idx = R_BspStream_FindPatch( cellX, cellY );

	if ( idx < 0 ) {
		return;
	}
	R_BspStream_HashRemove( cellX, cellY );
	Com_Memset( &s_stream.patches[idx], 0, sizeof( s_stream.patches[idx] ) );
	R_BspStream_CompactLightmaps();
	R_BspStream_RebuildVbo();
}

void R_BspStream_AddSurfaces( void ) {
	int i;

	if ( !r_bspStream || !r_bspStream->integer || !tr.world ) {
		return;
	}
	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return;
	}

	tr.currentEntityNum = REFENTITYNUM_WORLD;
	tr.shiftedEntityNum = tr.currentEntityNum << QSORT_REFENTITYNUM_SHIFT;

	for ( i = 0; i < BSP_STREAM_MAX_PATCHES; i++ ) {
		int f;

		if ( !s_stream.patches[i].active || s_stream.patches[i].numFaces < 1 ) {
			continue;
		}
		if ( R_CullLocalBox( s_stream.patches[i].bounds ) == CULL_OUT ) {
			continue;
		}
		for ( f = 0; f < s_stream.patches[i].numFaces; f++ ) {
			if ( !s_stream.patches[i].faces[f].surface || !s_stream.patches[i].faces[f].shader ) {
				continue;
			}
			/* VT feedback: brush-tops sample full atlas UV space — request a few pages. */
			if ( R_VT_WantSample() ) {
				R_VT_Feedback_RequestUV( 0.1f, 0.1f );
				R_VT_Feedback_RequestUV( 0.5f, 0.5f );
				R_VT_Feedback_RequestUV( 0.9f, 0.9f );
			}
			R_AddDrawSurf( s_stream.patches[i].faces[f].surface,
				s_stream.patches[i].faces[f].shader, 0, 0 );
		}
	}
}
