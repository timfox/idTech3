/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Material-blend vertex weight paint: .paint sidecar + brush.
===========================================================================
*/

#include "tr_local.h"
#include "tr_material_paint.h"

cvar_t *r_materialPaint;
cvar_t *r_materialPaintRadius;
cvar_t *r_materialPaintStrength;
cvar_t *r_materialPaintChannels;
cvar_t *r_materialPaintDirty;

static materialPaintVert_t *s_paintVerts;
static int s_paintNumVerts;
static int s_paintCapacity;
static uint32_t s_paintFlags;
static char s_paintMapBase[MAX_QPATH];
static qboolean s_paintDirty;

static void MP_EnsureCapacity( int need ) {
	materialPaintVert_t *n;
	int cap;
	if ( need <= s_paintCapacity ) {
		return;
	}
	cap = s_paintCapacity ? s_paintCapacity * 2 : 256;
	while ( cap < need ) {
		cap *= 2;
	}
	n = (materialPaintVert_t *)ri.Malloc( sizeof( *n ) * (size_t)cap );
	if ( s_paintVerts && s_paintNumVerts > 0 ) {
		Com_Memcpy( n, s_paintVerts, sizeof( *n ) * (size_t)s_paintNumVerts );
	}
	if ( s_paintVerts ) {
		ri.Free( s_paintVerts );
	}
	s_paintVerts = n;
	s_paintCapacity = cap;
}

static materialPaintVert_t *MP_FindOrAdd( uint32_t surf, uint32_t vert ) {
	int i;
	materialPaintVert_t *pv;
	for ( i = 0; i < s_paintNumVerts; i++ ) {
		if ( s_paintVerts[i].surfIndex == surf && s_paintVerts[i].vertIndex == vert ) {
			return &s_paintVerts[i];
		}
	}
	MP_EnsureCapacity( s_paintNumVerts + 1 );
	pv = &s_paintVerts[s_paintNumVerts++];
	Com_Memset( pv, 0, sizeof( *pv ) );
	pv->surfIndex = surf;
	pv->vertIndex = vert;
	pv->rgba[0] = pv->rgba[1] = pv->rgba[2] = pv->rgba[3] = 0;
	pv->rgba2[0] = pv->rgba2[1] = pv->rgba2[2] = pv->rgba2[3] = 0;
	return pv;
}

static void MP_WriteFaceColor( srfSurfaceFace_t *face, int vertIndex, const byte rgba[4] ) {
	byte *dst;
	if ( !face || vertIndex < 0 || vertIndex >= face->numPoints ) {
		return;
	}
	dst = (byte *)&face->points[vertIndex][10];
	dst[0] = rgba[0];
	dst[1] = rgba[1];
	dst[2] = rgba[2];
	dst[3] = rgba[3];
}

static void MP_ReadFaceColor( const srfSurfaceFace_t *face, int vertIndex, byte rgba[4] ) {
	const byte *src;
	if ( !face || vertIndex < 0 || vertIndex >= face->numPoints ) {
		rgba[0] = rgba[1] = rgba[2] = rgba[3] = 0;
		return;
	}
	src = (const byte *)&face->points[vertIndex][10];
	rgba[0] = src[0];
	rgba[1] = src[1];
	rgba[2] = src[2];
	rgba[3] = src[3];
}

static void MP_WriteTriColor( srfTriangles_t *tri, int vertIndex, const byte rgba[4] ) {
	if ( !tri || !tri->verts || vertIndex < 0 || vertIndex >= tri->numVerts ) {
		return;
	}
	tri->verts[vertIndex].color.rgba[0] = rgba[0];
	tri->verts[vertIndex].color.rgba[1] = rgba[1];
	tri->verts[vertIndex].color.rgba[2] = rgba[2];
	tri->verts[vertIndex].color.rgba[3] = rgba[3];
}

static void MP_ApplyRecord( const materialPaintVert_t *pv ) {
	msurface_t *surf;
	if ( !tr.world || !pv ) {
		return;
	}
	if ( (int)pv->surfIndex < 0 || (int)pv->surfIndex >= tr.world->numsurfaces ) {
		return;
	}
	surf = &tr.world->surfaces[pv->surfIndex];
	if ( !surf->data ) {
		return;
	}
	if ( *surf->data == SF_FACE ) {
		MP_WriteFaceColor( (srfSurfaceFace_t *)surf->data, (int)pv->vertIndex, pv->rgba );
	} else if ( *surf->data == SF_TRIANGLES ) {
		MP_WriteTriColor( (srfTriangles_t *)surf->data, (int)pv->vertIndex, pv->rgba );
	}
}

static void MP_ApplyAll( void ) {
	int i;
	for ( i = 0; i < s_paintNumVerts; i++ ) {
		MP_ApplyRecord( &s_paintVerts[i] );
	}
}

static void MP_BuildPath( char *out, int outSize, const char *mapBase ) {
	if ( !mapBase || !mapBase[0] ) {
		mapBase = s_paintMapBase;
	}
	Com_sprintf( out, outSize, "maps/%s.paint", mapBase );
}

/*
===============
R_MaterialPaint_RegisterCvars
===============
*/
void R_MaterialPaint_RegisterCvars( void ) {
	r_materialPaint = ri.Cvar_Get( "r_materialPaint", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_materialPaint, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_materialPaint,
		"Studio material-weight paint mode. Requires r_studio_tools 1. Brush writes vertex RGBA for materialBlend." );

	r_materialPaintRadius = ri.Cvar_Get( "r_materialPaintRadius", "64", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_materialPaintRadius, "1", "1024", CV_FLOAT );

	r_materialPaintStrength = ri.Cvar_Get( "r_materialPaintStrength", "0.35", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_materialPaintStrength, "0.01", "1", CV_FLOAT );

	r_materialPaintChannels = ri.Cvar_Get( "r_materialPaintChannels", "15", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_materialPaintChannels,
		"Bitmask of channels to paint: bits 0-3 = RGBA stream0 (layers 0-3), bits 4-7 = stream1 (layers 4-7)." );

	r_materialPaintDirty = ri.Cvar_Get( "r_materialPaintDirty", "0", CVAR_ROM );

	ri.Printf( PRINT_ALL, "Material paint: %s (maps/<map>.paint sidecar)\n",
		( r_materialPaint && r_materialPaint->integer ) ? "ON" : "OFF" );
}

void R_MaterialPaint_Init( void ) {
	s_paintVerts = NULL;
	s_paintNumVerts = 0;
	s_paintCapacity = 0;
	s_paintFlags = 0;
	s_paintMapBase[0] = '\0';
	s_paintDirty = qfalse;
}

void R_MaterialPaint_Shutdown( void ) {
	R_MaterialPaint_Clear();
}

void R_MaterialPaint_Clear( void ) {
	if ( s_paintVerts ) {
		ri.Free( s_paintVerts );
	}
	s_paintVerts = NULL;
	s_paintNumVerts = 0;
	s_paintCapacity = 0;
	s_paintFlags = 0;
	s_paintDirty = qfalse;
	if ( r_materialPaintDirty ) {
		ri.Cvar_Set( "r_materialPaintDirty", "0" );
	}
}

int R_MaterialPaint_NumVerts( void ) {
	return s_paintNumVerts;
}

qboolean R_MaterialPaint_HasStream2( void ) {
	return ( s_paintFlags & MATERIAL_PAINT_FLAG_STREAM2 ) ? qtrue : qfalse;
}

qboolean R_MaterialPaint_GetStream2( uint32_t surfIndex, uint32_t vertIndex, byte rgba2[4] ) {
	int i;
	if ( !rgba2 ) {
		return qfalse;
	}
	rgba2[0] = rgba2[1] = rgba2[2] = rgba2[3] = 0;
	for ( i = 0; i < s_paintNumVerts; i++ ) {
		if ( s_paintVerts[i].surfIndex == surfIndex && s_paintVerts[i].vertIndex == vertIndex ) {
			rgba2[0] = s_paintVerts[i].rgba2[0];
			rgba2[1] = s_paintVerts[i].rgba2[1];
			rgba2[2] = s_paintVerts[i].rgba2[2];
			rgba2[3] = s_paintVerts[i].rgba2[3];
			return qtrue;
		}
	}
	return qfalse;
}

void R_MaterialPaint_FillStream2ForSurface( int surfIndex, int firstVert, int numVerts ) {
	int v;
	if ( surfIndex < 0 || numVerts <= 0 || !( s_paintFlags & MATERIAL_PAINT_FLAG_STREAM2 ) ) {
		return;
	}
	for ( v = 0; v < numVerts; v++ ) {
		byte rgba2[4];
		if ( R_MaterialPaint_GetStream2( (uint32_t)surfIndex, (uint32_t)v, rgba2 ) ) {
			tess.vertexColors1[firstVert + v].rgba[0] = rgba2[0];
			tess.vertexColors1[firstVert + v].rgba[1] = rgba2[1];
			tess.vertexColors1[firstVert + v].rgba[2] = rgba2[2];
			tess.vertexColors1[firstVert + v].rgba[3] = rgba2[3];
		} else {
			tess.vertexColors1[firstVert + v].rgba[0] = 0;
			tess.vertexColors1[firstVert + v].rgba[1] = 0;
			tess.vertexColors1[firstVert + v].rgba[2] = 0;
			tess.vertexColors1[firstVert + v].rgba[3] = 0;
		}
	}
}

static int MP_SurfIndexFromData( const void *data ) {
	int i;
	if ( !tr.world || !data ) {
		return -1;
	}
	for ( i = 0; i < tr.world->numsurfaces; i++ ) {
		if ( tr.world->surfaces[i].data == data ) {
			return i;
		}
	}
	return -1;
}

void R_MaterialPaint_FillStream2FromSurfaceData( const void *surfData, int firstVert, int numVerts ) {
	int idx = MP_SurfIndexFromData( surfData );
	if ( idx >= 0 ) {
		R_MaterialPaint_FillStream2ForSurface( idx, firstVert, numVerts );
	}
}

void R_MaterialPaint_InvalidateWorldVBO( void ) {
#ifdef USE_VBO
	if ( tr.world && tr.world->numsurfaces > 0 ) {
		R_BuildWorldVBO( tr.world->surfaces, tr.world->numsurfaces );
	}
#endif
}

/*
===============
R_MaterialPaint_OnMapLoad
===============
*/
void R_MaterialPaint_OnMapLoad( const char *mapBaseName ) {
	char path[MAX_QPATH];

	R_MaterialPaint_Clear();
	if ( !mapBaseName || !mapBaseName[0] ) {
		return;
	}
	Q_strncpyz( s_paintMapBase, mapBaseName, sizeof( s_paintMapBase ) );
	MP_BuildPath( path, sizeof( path ), mapBaseName );
	if ( R_MaterialPaint_Load( path ) ) {
		ri.Printf( PRINT_ALL, "Material paint: loaded %s (%d verts)%s\n",
			path, s_paintNumVerts,
			( s_paintFlags & MATERIAL_PAINT_FLAG_STREAM2 ) ? " +stream2" : "" );
	}
}

qboolean R_MaterialPaint_Load( const char *pathOrNull ) {
	char path[MAX_QPATH];
	byte *buf = NULL;
	int32_t len;
	const byte *p;
	const byte *end;
	uint32_t magic, version, flags, count, i;

	if ( pathOrNull && pathOrNull[0] ) {
		Q_strncpyz( path, pathOrNull, sizeof( path ) );
	} else {
		MP_BuildPath( path, sizeof( path ), s_paintMapBase );
	}

	len = ri.FS_ReadFile( path, (void **)&buf );
	if ( !buf || len < 16 ) {
		if ( buf ) {
			ri.FS_FreeFile( buf );
		}
		return qfalse;
	}

	p = buf;
	end = buf + len;
	Com_Memcpy( &magic, p, 4 ); p += 4;
	Com_Memcpy( &version, p, 4 ); p += 4;
	Com_Memcpy( &flags, p, 4 ); p += 4;
	Com_Memcpy( &count, p, 4 ); p += 4;

	if ( magic != MATERIAL_PAINT_MAGIC || ( version != 1 && version != MATERIAL_PAINT_VERSION ) ) {
		ri.Printf( PRINT_WARNING, "Material paint: bad header in %s\n", path );
		ri.FS_FreeFile( buf );
		return qfalse;
	}

	R_MaterialPaint_Clear();
	s_paintFlags = flags;
	MP_EnsureCapacity( (int)count );

	for ( i = 0; i < count; i++ ) {
		materialPaintVert_t *pv;
		size_t recSize = ( version >= 2 || ( flags & MATERIAL_PAINT_FLAG_STREAM2 ) ) ? 16 : 12;
		if ( p + recSize > end ) {
			break;
		}
		MP_EnsureCapacity( s_paintNumVerts + 1 );
		pv = &s_paintVerts[s_paintNumVerts++];
		Com_Memcpy( &pv->surfIndex, p, 4 ); p += 4;
		Com_Memcpy( &pv->vertIndex, p, 4 ); p += 4;
		Com_Memcpy( pv->rgba, p, 4 ); p += 4;
		if ( recSize >= 16 ) {
			Com_Memcpy( pv->rgba2, p, 4 ); p += 4;
			s_paintFlags |= MATERIAL_PAINT_FLAG_STREAM2;
		} else {
			Com_Memset( pv->rgba2, 0, 4 );
		}
	}

	ri.FS_FreeFile( buf );
	MP_ApplyAll();
	s_paintDirty = qfalse;
	return qtrue;
}

qboolean R_MaterialPaint_Save( const char *pathOrNull ) {
	char path[MAX_QPATH];
	byte *buf;
	byte *p;
	int i;
	size_t size;
	uint32_t magic = MATERIAL_PAINT_MAGIC;
	uint32_t version = MATERIAL_PAINT_VERSION;
	uint32_t flags = s_paintFlags;
	uint32_t count = (uint32_t)s_paintNumVerts;

	if ( pathOrNull && pathOrNull[0] ) {
		Q_strncpyz( path, pathOrNull, sizeof( path ) );
	} else {
		MP_BuildPath( path, sizeof( path ), s_paintMapBase );
	}

	if ( s_paintNumVerts > 0 ) {
		flags |= MATERIAL_PAINT_FLAG_STREAM2; /* always write stream2 slots in v2 */
	}

	size = 16 + (size_t)s_paintNumVerts * 16;
	buf = (byte *)ri.Hunk_AllocateTempMemory( (int)size );
	p = buf;
	Com_Memcpy( p, &magic, 4 ); p += 4;
	Com_Memcpy( p, &version, 4 ); p += 4;
	Com_Memcpy( p, &flags, 4 ); p += 4;
	Com_Memcpy( p, &count, 4 ); p += 4;
	for ( i = 0; i < s_paintNumVerts; i++ ) {
		Com_Memcpy( p, &s_paintVerts[i].surfIndex, 4 ); p += 4;
		Com_Memcpy( p, &s_paintVerts[i].vertIndex, 4 ); p += 4;
		Com_Memcpy( p, s_paintVerts[i].rgba, 4 ); p += 4;
		Com_Memcpy( p, s_paintVerts[i].rgba2, 4 ); p += 4;
	}

	ri.FS_WriteFile( path, buf, (int)( p - buf ) );
	ri.Hunk_FreeTempMemory( buf );
	s_paintDirty = qfalse;
	if ( r_materialPaintDirty ) {
		ri.Cvar_Set( "r_materialPaintDirty", "0" );
	}
	ri.Printf( PRINT_ALL, "Material paint: saved %s (%d verts)\n", path, s_paintNumVerts );
	return qtrue;
}

static void MP_BlendChannel( byte *dst, byte target, float t, qboolean active ) {
	float v;
	if ( !active ) {
		return;
	}
	v = (float)(*dst) * ( 1.0f - t ) + (float)target * t;
	if ( v < 0.0f ) v = 0.0f;
	if ( v > 255.0f ) v = 255.0f;
	*dst = (byte)( v + 0.5f );
}

void R_MaterialPaint_Brush( const vec3_t worldPos, float radius, float strength, uint32_t channelMask, const byte targetRGBA[4] ) {
	int s, v;
	float r2;

	if ( !tr.world || radius <= 0.0f ) {
		return;
	}
	r2 = radius * radius;

	for ( s = 0; s < tr.world->numsurfaces; s++ ) {
		msurface_t *surf = &tr.world->surfaces[s];
		if ( !surf->data ) {
			continue;
		}
		if ( *surf->data == SF_FACE ) {
			srfSurfaceFace_t *face = (srfSurfaceFace_t *)surf->data;
			for ( v = 0; v < face->numPoints; v++ ) {
				vec3_t delta;
				float d2, t;
				byte rgba[4];
				materialPaintVert_t *pv;
				VectorSubtract( face->points[v], worldPos, delta );
				d2 = DotProduct( delta, delta );
				if ( d2 > r2 ) {
					continue;
				}
				t = 1.0f - sqrtf( d2 ) / radius;
				t *= strength;
				if ( t <= 0.0f ) {
					continue;
				}
				MP_ReadFaceColor( face, v, rgba );
				MP_BlendChannel( &rgba[0], targetRGBA[0], t, ( channelMask & 1 ) != 0 );
				MP_BlendChannel( &rgba[1], targetRGBA[1], t, ( channelMask & 2 ) != 0 );
				MP_BlendChannel( &rgba[2], targetRGBA[2], t, ( channelMask & 4 ) != 0 );
				MP_BlendChannel( &rgba[3], targetRGBA[3], t, ( channelMask & 8 ) != 0 );
				MP_WriteFaceColor( face, v, rgba );
				pv = MP_FindOrAdd( (uint32_t)s, (uint32_t)v );
				Com_Memcpy( pv->rgba, rgba, 4 );
				if ( channelMask & 0xf0 ) {
					MP_BlendChannel( &pv->rgba2[0], targetRGBA[0], t, ( channelMask & 16 ) != 0 );
					MP_BlendChannel( &pv->rgba2[1], targetRGBA[1], t, ( channelMask & 32 ) != 0 );
					MP_BlendChannel( &pv->rgba2[2], targetRGBA[2], t, ( channelMask & 64 ) != 0 );
					MP_BlendChannel( &pv->rgba2[3], targetRGBA[3], t, ( channelMask & 128 ) != 0 );
					s_paintFlags |= MATERIAL_PAINT_FLAG_STREAM2;
				}
				s_paintDirty = qtrue;
			}
		} else if ( *surf->data == SF_TRIANGLES ) {
			srfTriangles_t *tri = (srfTriangles_t *)surf->data;
			for ( v = 0; v < tri->numVerts; v++ ) {
				vec3_t delta;
				float d2, t;
				byte rgba[4];
				materialPaintVert_t *pv;
				VectorSubtract( tri->verts[v].xyz, worldPos, delta );
				d2 = DotProduct( delta, delta );
				if ( d2 > r2 ) {
					continue;
				}
				t = ( 1.0f - sqrtf( d2 ) / radius ) * strength;
				if ( t <= 0.0f ) {
					continue;
				}
				Com_Memcpy( rgba, tri->verts[v].color.rgba, 4 );
				MP_BlendChannel( &rgba[0], targetRGBA[0], t, ( channelMask & 1 ) != 0 );
				MP_BlendChannel( &rgba[1], targetRGBA[1], t, ( channelMask & 2 ) != 0 );
				MP_BlendChannel( &rgba[2], targetRGBA[2], t, ( channelMask & 4 ) != 0 );
				MP_BlendChannel( &rgba[3], targetRGBA[3], t, ( channelMask & 8 ) != 0 );
				MP_WriteTriColor( tri, v, rgba );
				pv = MP_FindOrAdd( (uint32_t)s, (uint32_t)v );
				Com_Memcpy( pv->rgba, rgba, 4 );
				s_paintDirty = qtrue;
			}
		}
	}

	if ( s_paintDirty ) {
		if ( r_materialPaintDirty ) {
			ri.Cvar_Set( "r_materialPaintDirty", "1" );
		}
		R_MaterialPaint_InvalidateWorldVBO();
	}
}

qboolean R_MaterialPaint_BrushFromScreen( float ndcX, float ndcY, float radius, float strength, uint32_t channelMask, const byte targetRGBA[4] ) {
	vec3_t forward, right, up, start, dir, sample;
	float tanFov;
	float bestDist = 1e30f;
	vec3_t bestPos;
	int step;
	const float farDist = 4096.0f;
	const int steps = 64;

	if ( !tr.world ) {
		return qfalse;
	}

	VectorCopy( backEnd.viewParms.or.origin, start );
	VectorCopy( backEnd.viewParms.or.axis[0], forward );
	VectorCopy( backEnd.viewParms.or.axis[1], right );
	VectorCopy( backEnd.viewParms.or.axis[2], up );

	tanFov = tanf( backEnd.viewParms.fovX * (float)( M_PI / 360.0 ) );
	VectorCopy( forward, dir );
	VectorMA( dir, ndcX * tanFov, right, dir );
	VectorMA( dir, ndcY * tanFov * ( backEnd.viewParms.fovY / ( backEnd.viewParms.fovX > 1.0f ? backEnd.viewParms.fovX : 1.0f ) ), up, dir );
	VectorNormalize( dir );

	/* Sample along view ray; pick closest world vertex as brush center. */
	VectorCopy( start, bestPos );
	for ( step = 1; step <= steps; step++ ) {
		int s, v;
		float t = ( (float)step / (float)steps ) * farDist;
		VectorMA( start, t, dir, sample );
		for ( s = 0; s < tr.world->numsurfaces; s++ ) {
			msurface_t *surf = &tr.world->surfaces[s];
			if ( !surf->data ) {
				continue;
			}
			if ( *surf->data == SF_FACE ) {
				srfSurfaceFace_t *face = (srfSurfaceFace_t *)surf->data;
				for ( v = 0; v < face->numPoints; v++ ) {
					vec3_t delta;
					float d2;
					VectorSubtract( face->points[v], sample, delta );
					d2 = DotProduct( delta, delta );
					if ( d2 < bestDist && d2 < radius * radius * 4.0f ) {
						bestDist = d2;
						VectorCopy( face->points[v], bestPos );
					}
				}
			}
		}
	}
	if ( bestDist > radius * radius * 16.0f ) {
		return qfalse;
	}
	R_MaterialPaint_Brush( bestPos, radius, strength, channelMask, targetRGBA );
	return qtrue;
}

/*
===============
MD3 paint sidecar: models/<name>.md3.paint
Header: ID3P + version + flags + count; then count * rgba (bind-pose, sequential verts).
===============
*/
qboolean R_MaterialPaint_LoadMD3( const char *modelName, byte **outColors, int *outNumVerts ) {
	char path[MAX_QPATH];
	char base[MAX_QPATH];
	byte *buf = NULL;
	int32_t len;
	uint32_t magic, version, flags, count;
	byte *colors;

	*outColors = NULL;
	*outNumVerts = 0;
	if ( !modelName || !modelName[0] ) {
		return qfalse;
	}
	Q_strncpyz( base, modelName, sizeof( base ) );
	COM_StripExtension( base, base, sizeof( base ) );
	Com_sprintf( path, sizeof( path ), "%s.md3.paint", base );

	len = ri.FS_ReadFile( path, (void **)&buf );
	if ( !buf || len < 16 ) {
		if ( buf ) {
			ri.FS_FreeFile( buf );
		}
		return qfalse;
	}
	Com_Memcpy( &magic, buf, 4 );
	Com_Memcpy( &version, buf + 4, 4 );
	Com_Memcpy( &flags, buf + 8, 4 );
	Com_Memcpy( &count, buf + 12, 4 );
	if ( magic != MATERIAL_PAINT_MAGIC || count == 0 || (int)( 16 + count * 4 ) > len ) {
		ri.FS_FreeFile( buf );
		return qfalse;
	}
	colors = (byte *)ri.Malloc( (int)count * 4 );
	Com_Memcpy( colors, buf + 16, (size_t)count * 4 );
	ri.FS_FreeFile( buf );
	*outColors = colors;
	*outNumVerts = (int)count;
	ri.Printf( PRINT_DEVELOPER, "Material paint: MD3 overlay %s (%u verts)\n", path, count );
	return qtrue;
}

void R_MaterialPaint_FreeMD3( byte *colors ) {
	if ( colors ) {
		ri.Free( colors );
	}
}

/* Console commands */
static void MP_Save_f( void ) {
	const char *p = ( ri.Cmd_Argc() > 1 ) ? ri.Cmd_Argv( 1 ) : NULL;
	R_MaterialPaint_Save( p );
}

static void MP_Load_f( void ) {
	const char *p = ( ri.Cmd_Argc() > 1 ) ? ri.Cmd_Argv( 1 ) : NULL;
	if ( R_MaterialPaint_Load( p ) ) {
		R_MaterialPaint_InvalidateWorldVBO();
	} else {
		ri.Printf( PRINT_ALL, "Material paint: load failed\n" );
	}
}

static void MP_Clear_f( void ) {
	R_MaterialPaint_Clear();
	ri.Printf( PRINT_ALL, "Material paint: cleared in-memory weights (reload map to restore BSP colors)\n" );
}

static void MP_Status_f( void ) {
	ri.Printf( PRINT_ALL, "Material paint: map=%s verts=%d dirty=%d stream2=%d enabled=%d\n",
		s_paintMapBase[0] ? s_paintMapBase : "<none>",
		s_paintNumVerts,
		s_paintDirty ? 1 : 0,
		( s_paintFlags & MATERIAL_PAINT_FLAG_STREAM2 ) ? 1 : 0,
		( r_materialPaint && r_materialPaint->integer ) ? 1 : 0 );
}

void R_MaterialPaint_RegisterCommands( void ) {
	ri.Cmd_AddCommand( "paint_save", MP_Save_f );
	ri.Cmd_AddCommand( "paint_load", MP_Load_f );
	ri.Cmd_AddCommand( "paint_clear", MP_Clear_f );
	ri.Cmd_AddCommand( "paint_status", MP_Status_f );
}
