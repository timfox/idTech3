/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "tr_local.h"
#include "tr_bsp_stream.h"
#include "../../qcommon/qfiles.h"

#define BSP_STREAM_MAX_PATCHES 64
#define BSP_STREAM_MAX_FACES 16

typedef struct {
	srfSurfaceFace_t *face;
	shader_t        *shader;
} bspStreamFace_t;

typedef struct {
	qboolean        active;
	int             cellX;
	int             cellY;
	vec3_t          bounds[2];
	bspStreamFace_t faces[BSP_STREAM_MAX_FACES];
	int             numFaces;
} bspStreamPatch_t;

static bspStreamPatch_t s_bspPatches[BSP_STREAM_MAX_PATCHES];
static cvar_t *r_bspStream;

static int R_BspStream_FindPatch( int cellX, int cellY ) {
	int i;

	for ( i = 0; i < BSP_STREAM_MAX_PATCHES; i++ ) {
		if ( s_bspPatches[i].active && s_bspPatches[i].cellX == cellX &&
			s_bspPatches[i].cellY == cellY ) {
			return i;
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
		if ( !s_bspPatches[i].active ) {
			Com_Memset( &s_bspPatches[i], 0, sizeof( s_bspPatches[i] ) );
			s_bspPatches[i].active = qtrue;
			s_bspPatches[i].cellX = cellX;
			s_bspPatches[i].cellY = cellY;
			return i;
		}
	}
	return -1;
}

static qboolean R_BspStream_BoundsFromBrush( dplane_t *planes, int numPlanes,
	dbrushside_t *sides, int numSides, int firstSide, int brushSides,
	vec3_t outMins, vec3_t outMaxs ) {
	int i, planeNum;
	qboolean haveMins[3], haveMaxs[3];
	float nx, ny, nz, pdist;

	if ( firstSide < 0 || brushSides < 6 || firstSide + brushSides > numSides ) {
		return qfalse;
	}

	VectorSet( outMins, 0, 0, 0 );
	VectorSet( outMaxs, 0, 0, 0 );
	memset( haveMins, 0, sizeof( haveMins ) );
	memset( haveMaxs, 0, sizeof( haveMaxs ) );

	for ( i = 0; i < brushSides; i++ ) {
		dbrushside_t *side = &sides[firstSide + i];
		dplane_t *plane;

		planeNum = LittleLong( side->planeNum );
		if ( planeNum < 0 || planeNum >= numPlanes ) {
			return qfalse;
		}
		plane = &planes[planeNum];
		nx = LittleFloat( plane->normal[0] );
		ny = LittleFloat( plane->normal[1] );
		nz = LittleFloat( plane->normal[2] );
		pdist = LittleFloat( plane->dist );
		if ( nx == 1.0f && !haveMins[0] ) {
			outMins[0] = pdist;
			haveMins[0] = qtrue;
		} else if ( nx == -1.0f && !haveMaxs[0] ) {
			outMaxs[0] = -pdist;
			haveMaxs[0] = qtrue;
		} else if ( ny == 1.0f && !haveMins[1] ) {
			outMins[1] = pdist;
			haveMins[1] = qtrue;
		} else if ( ny == -1.0f && !haveMaxs[1] ) {
			outMaxs[1] = -pdist;
			haveMaxs[1] = qtrue;
		} else if ( nz == 1.0f && !haveMins[2] ) {
			outMins[2] = pdist;
			haveMins[2] = qtrue;
		} else if ( nz == -1.0f && !haveMaxs[2] ) {
			outMaxs[2] = -pdist;
			haveMaxs[2] = qtrue;
		}
	}

	return haveMins[0] && haveMaxs[0] && haveMins[1] && haveMaxs[1] &&
		haveMins[2] && haveMaxs[2] &&
		outMaxs[0] > outMins[0] && outMaxs[1] > outMins[1] && outMaxs[2] > outMins[2];
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
#ifdef USE_VK_PBR
		cv->points[i][3] = 0.0f;
		cv->points[i][4] = 0.0f;
		cv->points[i][5] = 1.0f;
		cv->points[i][6] = 0.0f;
		cv->points[i][7] = 0.0f;
		cv->points[i][8] = 0.0f;
		cv->points[i][9] = 0.0f;
		cv->points[i][10] = 255.0f;
#else
		cv->points[i][3] = 0.0f;
		cv->points[i][4] = 0.0f;
		cv->points[i][5] = 0.0f;
		cv->points[i][6] = 0.0f;
		cv->points[i][7] = 255.0f;
#endif
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

static qboolean R_BspStream_ParsePlanarFace( const dsurface_t *ds, const drawVert_t *verts,
	const int *indexes, const dshader_t *dshaders, int numShaders, const vec3_t worldOrigin,
	bspStreamFace_t *outFace, vec3_t patchMins, vec3_t patchMaxs ) {
	srfSurfaceFace_t *cv;
	int numPoints, numIndexes, sfaceSize, ofsIndexes;
	int i, j;
	int shaderNum;
	int lightmapNum;
	float lightmapX, lightmapY;

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

	lightmapNum = LittleLong( ds->lightmapNum );
	if ( lightmapNum >= 0 && tr.mergeLightmaps ) {
		lightmapNum = R_GetLightmapCoords( lightmapNum, &lightmapX, &lightmapY );
	} else {
		lightmapX = lightmapY = 0.0f;
	}

	shaderNum = LittleLong( ds->shaderNum );
	if ( shaderNum >= 0 && shaderNum < numShaders && dshaders ) {
		outFace->shader = R_FindShader( dshaders[shaderNum].shader, LIGHTMAP_NONE, qtrue );
	} else {
		outFace->shader = tr.defaultShader;
	}

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
#ifdef USE_VK_PBR
			cv->points[i][3 + j] = LittleFloat( verts[i].normal[j] );
#endif
		}
#ifdef USE_VK_PBR
		for ( j = 0; j < 2; j++ ) {
			cv->points[i][6 + j] = LittleFloat( verts[i].st[j] );
			cv->points[i][8 + j] = LittleFloat( verts[i].lightmap[j] );
		}
		R_ColorShiftLightingBytes( verts[i].color.rgba, (byte *)&cv->points[i][10], qtrue );
#else
		for ( j = 0; j < 2; j++ ) {
			cv->points[i][3 + j] = LittleFloat( verts[i].st[j] );
			cv->points[i][5 + j] = LittleFloat( verts[i].lightmap[j] );
		}
		R_ColorShiftLightingBytes( verts[i].color.rgba, (byte *)&cv->points[i][7], qtrue );
#endif
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

	outFace->face = cv;
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

	loaded = 0;
	for ( i = 0; i < numSurfaces && patch->numFaces < BSP_STREAM_MAX_FACES; i++ ) {
		const dsurface_t *ds = &surfs[i];
		int firstVert = LittleLong( ds->firstVert );
		int numSurfVerts = LittleLong( ds->numVerts );
		int firstIndex = LittleLong( ds->firstIndex );
		int numSurfIndexes = LittleLong( ds->numIndexes );

		if ( firstVert < 0 || numSurfVerts <= 0 || firstVert + numSurfVerts > numVerts ) {
			continue;
		}
		if ( firstIndex < 0 || numSurfIndexes < 3 || firstIndex + numSurfIndexes > numIndexes ) {
			continue;
		}
		if ( R_BspStream_ParsePlanarFace( ds, verts, indexes, dshaders, numShaders, worldOrigin,
			&patch->faces[patch->numFaces], patch->bounds[0], patch->bounds[1] ) ) {
			patch->numFaces++;
			loaded++;
		}
	}
	return loaded;
}

void R_BspStream_Init( void ) {
	r_bspStream = ri.Cvar_Get( "r_bspStream", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_bspStream,
		"Merge sector BSP brush tops as visual overlay (open-world renderer streaming)." );
	Com_Memset( s_bspPatches, 0, sizeof( s_bspPatches ) );
	ri.Printf( PRINT_ALL, "[bsp_stream] visual sector overlay initialized (r_bspStream 1)\n" );
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
	patch = &s_bspPatches[patchIdx];

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
		patch->faces[0].face = R_BspStream_AllocTopFace( quadMins, quadMaxs );
		patch->faces[0].shader = tr.defaultShader;
		patch->numFaces = patch->faces[0].face ? 1 : 0;
	}

	if ( !anyBrush || patch->numFaces < 1 ) {
		patch->active = qfalse;
		return qfalse;
	}

	ri.Printf( PRINT_ALL, "[bsp_stream] merged visual sector %d,%d (%s, brush-top)\n", cellX, cellY, mapName );
	return qtrue;
}

void RE_BspStream_UnmergeSector( int cellX, int cellY ) {
	int idx = R_BspStream_FindPatch( cellX, cellY );

	if ( idx < 0 ) {
		return;
	}
	Com_Memset( &s_bspPatches[idx], 0, sizeof( s_bspPatches[idx] ) );
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

		if ( !s_bspPatches[i].active || s_bspPatches[i].numFaces < 1 ) {
			continue;
		}
		if ( R_CullLocalBox( s_bspPatches[i].bounds ) == CULL_OUT ) {
			continue;
		}
		for ( f = 0; f < s_bspPatches[i].numFaces; f++ ) {
			if ( !s_bspPatches[i].faces[f].face || !s_bspPatches[i].faces[f].shader ) {
				continue;
			}
			R_AddDrawSurf( (surfaceType_t *)s_bspPatches[i].faces[f].face,
				s_bspPatches[i].faces[f].shader, 0, 0 );
		}
	}
}
