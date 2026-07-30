/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Mesh import: STL (binary/ASCII), native static Collada geometry,
and line-based vertex soup for ASCII interchange (.fbx/.usd/.usda/.ma).
===========================================================================
*/

#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include "tr_model_mesh_import.h"
#if defined( RENDERER_VULKAN )
#include "../vulkan/tr_local.h"
#else
#error tr_model_mesh_import.c must be compiled with RENDERER_VULKAN
#endif

#define MIMP_MAX_VERTS   SHADER_MAX_VERTEXES
#define MIMP_MAX_TRIS    (SHADER_MAX_INDEXES / 3)
#define MIMP_SURF_VERTS  MIMP_MAX_VERTS

static float R_Mimp_LeFloatBytes( const byte *p ) {
	union {
		uint32_t u;
		float f;
	} x;
	x.u = (uint32_t)p[0] | ( (uint32_t)p[1] << 8 ) | ( (uint32_t)p[2] << 16 ) | ( (uint32_t)p[3] << 24 );
	return x.f;
}

qhandle_t R_RegisterMeshImport( const char *name, model_t *mod );

static short R_Mimp_LatLong( const vec3_t n ) {
	float lat, lng;
	float nz = Com_Clamp( -1.0f, 1.0f, n[2] );

	if ( VectorLengthSquared( n ) < 0.0001f ) {
		return 0;
	}
	lng = acosf( nz );
	lat = atan2f( n[1], n[0] );
	if ( lat < 0.0f ) {
		lat += (float)( M_PI * 2.0 );
	}
	{
		unsigned la = (unsigned)( lat * ( 255.0f / ( 2.0f * (float)M_PI ) ) ) & 0xff;
		unsigned ln = (unsigned)( lng * ( 255.0f / (float)M_PI ) ) & 0xff;
		return (short)( ( la << 8 ) | ln );
	}
}

qboolean R_MeshImport_FinalizeMD3( model_t *mod, int lod, const char *name,
	float *verts, int numVerts, int *inds, int numIdx ) {
	return R_MeshImport_FinalizeMD3Ex( mod, lod, name, verts, numVerts, inds, numIdx, NULL, NULL, NULL );
}

qboolean R_MeshImport_FinalizeMD3Ex( model_t *mod, int lod, const char *name,
	float *verts, int numVerts, int *inds, int numIdx,
	const char *shaderName, const float *vertSt, const float *vertNormals ) {
	const char *surfShader = shaderName && shaderName[0] ? shaderName : "textures/common/white";
	int numTris = numIdx / 3;
	int i, s, v, t;
	vec3_t mins, maxs, localOrigin;
	float radius;
	md3Header_t *md3;
	int numSurfaces;
	int vertOffset;
	md3Frame_t *frame;
	md3Surface_t *surf;
	int surfSize;

	if ( numVerts <= 0 || numTris <= 0 || numIdx % 3 != 0 ) {
		return qfalse;
	}

	mins[0] = maxs[0] = verts[0];
	mins[1] = maxs[1] = verts[1];
	mins[2] = maxs[2] = verts[2];
	for ( i = 1; i < numVerts; i++ ) {
		float *p = verts + i * 3;
		if ( p[0] < mins[0] ) mins[0] = p[0];
		if ( p[1] < mins[1] ) mins[1] = p[1];
		if ( p[2] < mins[2] ) mins[2] = p[2];
		if ( p[0] > maxs[0] ) maxs[0] = p[0];
		if ( p[1] > maxs[1] ) maxs[1] = p[1];
		if ( p[2] > maxs[2] ) maxs[2] = p[2];
	}
	localOrigin[0] = ( mins[0] + maxs[0] ) * 0.5f;
	localOrigin[1] = ( mins[1] + maxs[1] ) * 0.5f;
	localOrigin[2] = ( mins[2] + maxs[2] ) * 0.5f;
	radius = 0.5f * sqrtf(
		( maxs[0] - mins[0] ) * ( maxs[0] - mins[0] ) +
		( maxs[1] - mins[1] ) * ( maxs[1] - mins[1] ) +
		( maxs[2] - mins[2] ) * ( maxs[2] - mins[2] ) );

	numSurfaces = ( numVerts + MIMP_SURF_VERTS - 1 ) / MIMP_SURF_VERTS;
	if ( numSurfaces < 1 ) {
		numSurfaces = 1;
	}

	surfSize = 0;
	for ( s = 0; s < numSurfaces; s++ ) {
		int sv = numVerts - s * MIMP_SURF_VERTS;
		int str;
		if ( sv > MIMP_SURF_VERTS ) {
			sv = MIMP_SURF_VERTS;
		}
		str = sv / 3;
		surfSize += sizeof( md3Surface_t ) + sizeof( md3Shader_t ) +
			str * sizeof( md3Triangle_t ) +
			sv * ( sizeof( md3St_t ) + sizeof( md3XyzNormal_t ) );
	}

	md3 = (md3Header_t *)ri.Hunk_Alloc( sizeof( md3Header_t ) + sizeof( md3Frame_t ) + surfSize, h_low );
	Com_Memset( md3, 0, sizeof( md3Header_t ) + sizeof( md3Frame_t ) + surfSize );

	md3->ident = MD3_IDENT;
	md3->version = MD3_VERSION;
	Q_strncpyz( md3->name, name, sizeof( md3->name ) );
	md3->flags = 0;
	md3->numFrames = 1;
	md3->numTags = 0;
	md3->numSurfaces = numSurfaces;
	md3->numSkins = 0;
	md3->ofsFrames = sizeof( md3Header_t );
	md3->ofsTags = sizeof( md3Header_t ) + sizeof( md3Frame_t );
	md3->ofsSurfaces = md3->ofsTags;
	md3->ofsEnd = sizeof( md3Header_t ) + sizeof( md3Frame_t ) + surfSize;

	frame = (md3Frame_t *)( (byte *)md3 + md3->ofsFrames );
	Q_strncpyz( frame->name, "default", sizeof( frame->name ) );
	VectorCopy( mins, frame->bounds[0] );
	VectorCopy( maxs, frame->bounds[1] );
	VectorCopy( localOrigin, frame->localOrigin );
	frame->radius = radius;

	surf = (md3Surface_t *)( (byte *)md3 + md3->ofsSurfaces );
	vertOffset = 0;
	for ( s = 0; s < numSurfaces; s++ ) {
		md3Shader_t *md3Shader;
		md3Triangle_t *tri;
		md3St_t *st;
		md3XyzNormal_t *xyz;
		int surfVerts = numVerts - s * MIMP_SURF_VERTS;
		int surfTris;
		if ( surfVerts > MIMP_SURF_VERTS ) {
			surfVerts = MIMP_SURF_VERTS;
		}
		surfTris = surfVerts / 3;

		surf->ident = SF_MD3;
		Com_sprintf( surf->name, sizeof( surf->name ), "meshimport_s%d", s );
		surf->name[sizeof( surf->name ) - 1] = '\0';
		Q_strlwr( surf->name );
		surf->flags = 0;
		surf->numFrames = 1;
		surf->numShaders = 1;
		surf->numVerts = surfVerts;
		surf->numTriangles = surfTris;
		{
			int offset = sizeof( md3Surface_t );
			surf->ofsShaders = offset;
			offset += sizeof( md3Shader_t );
			surf->ofsTriangles = offset;
			offset += surfTris * sizeof( md3Triangle_t );
			surf->ofsSt = offset;
			offset += surfVerts * sizeof( md3St_t );
			surf->ofsXyzNormals = offset;
			offset += surfVerts * sizeof( md3XyzNormal_t );
			surf->ofsEnd = offset;
		}

		md3Shader = (md3Shader_t *)( (byte *)surf + surf->ofsShaders );
		Q_strncpyz( md3Shader->name, surfShader, sizeof( md3Shader->name ) );
		{
			shader_t *sh = R_FindShader( md3Shader->name, LIGHTMAP_NONE, qtrue );
			md3Shader->shaderIndex = sh->defaultShader ? 0 : sh->index;
		}

		tri = (md3Triangle_t *)( (byte *)surf + surf->ofsTriangles );
		st = (md3St_t *)( (byte *)surf + surf->ofsSt );
		xyz = (md3XyzNormal_t *)( (byte *)surf + surf->ofsXyzNormals );

		for ( v = 0; v < surfVerts; v++ ) {
			int gi = vertOffset + v;
			float *p = verts + gi * 3;
			if ( vertSt ) {
				st[v].st[0] = vertSt[gi * 2 + 0];
				st[v].st[1] = vertSt[gi * 2 + 1];
			} else {
				st[v].st[0] = 0.0f;
				st[v].st[1] = 0.0f;
			}
			xyz[v].xyz[0] = (short)( p[0] * 64.0f );
			xyz[v].xyz[1] = (short)( p[1] * 64.0f );
			xyz[v].xyz[2] = (short)( p[2] * 64.0f );
			if ( vertNormals ) {
				vec3_t n;
				n[0] = vertNormals[gi * 3 + 0];
				n[1] = vertNormals[gi * 3 + 1];
				n[2] = vertNormals[gi * 3 + 2];
				VectorNormalize( n );
				xyz[v].normal = R_Mimp_LatLong( n );
			} else {
				xyz[v].normal = 0;
			}
		}

		for ( t = 0; t < surfTris; t++ ) {
			int i0 = inds[ vertOffset + t * 3 + 0 ];
			int i1 = inds[ vertOffset + t * 3 + 1 ];
			int i2 = inds[ vertOffset + t * 3 + 2 ];
			vec3_t e1, e2, fn;
			int j;
			if ( i0 < 0 || i1 < 0 || i2 < 0 || i0 >= numVerts || i1 >= numVerts || i2 >= numVerts ) {
				continue;
			}
			VectorSubtract( verts + i1 * 3, verts + i0 * 3, e1 );
			VectorSubtract( verts + i2 * 3, verts + i0 * 3, e2 );
			CrossProduct( e1, e2, fn );
			VectorNormalize( fn );
			if ( !vertNormals ) {
				for ( j = 0; j < 3; j++ ) {
					int vi = inds[ vertOffset + t * 3 + j ] - vertOffset;
					if ( vi >= 0 && vi < surfVerts ) {
						xyz[vi].normal = R_Mimp_LatLong( fn );
					}
				}
			}
			tri[t].indexes[0] = t * 3 + 0;
			tri[t].indexes[1] = t * 3 + 1;
			tri[t].indexes[2] = t * 3 + 2;
		}

		vertOffset += surfVerts;
		surf = (md3Surface_t *)( (byte *)surf + surf->ofsEnd );
	}

	mod->type = MOD_MESH;
	mod->dataSize = 0;
	mod->md3[lod] = md3;
	ri.Printf( PRINT_DEVELOPER, "MeshImport: %s (%d verts, %d tris) shader '%s'\n",
		name, numVerts, numTris, surfShader );
	return qtrue;
}

static qboolean R_LoadSTL( model_t *mod, int lod, const char *name, const byte *data, int size ) {
	float verts[MIMP_MAX_VERTS * 3];
	int inds[MIMP_MAX_VERTS];
	int nv = 0;
	int ni = 0;

	if ( size >= 84 && strncmp( (const char *)data, "solid", 5 ) != 0 ) {
		uint32_t ntri = LittleLong( *(const uint32_t *)( data + 80 ) );
		int t;
		if ( ntri > (uint32_t)MIMP_MAX_TRIS || size < 84 + (int)ntri * 50 ) {
			return qfalse;
		}
		for ( t = 0; t < (int)ntri; t++ ) {
			const byte *tri = data + 84 + t * 50;
			int k, base = nv;
			for ( k = 0; k < 3; k++ ) {
				const byte *pv = tri + 12 + k * 12;
				verts[( base + k ) * 3 + 0] = R_Mimp_LeFloatBytes( pv );
				verts[( base + k ) * 3 + 1] = R_Mimp_LeFloatBytes( pv + 4 );
				verts[( base + k ) * 3 + 2] = R_Mimp_LeFloatBytes( pv + 8 );
			}
			nv += 3;
			inds[ni++] = base;
			inds[ni++] = base + 1;
			inds[ni++] = base + 2;
		}
	} else {
		const char *p = (const char *)data;
		const char *end = p + size;
		float v0[3] = { 0.0f, 0.0f, 0.0f };
		float v1[3] = { 0.0f, 0.0f, 0.0f };
		float v2[3] = { 0.0f, 0.0f, 0.0f };
		int state = 0;

		while ( p < end ) {
			char line[512];
			const char *le = p;
			int len;
			while ( le < end && *le != '\n' && *le != '\r' ) {
				le++;
			}
			len = (int)( le - p );
			if ( len >= (int)sizeof( line ) ) {
				len = (int)sizeof( line ) - 1;
			}
			Com_Memcpy( line, p, (size_t)len );
			line[len] = '\0';
			p = le;
			if ( p < end ) {
				p++;
			}

			if ( Q_stristr( line, "vertex" ) ) {
				float x, y, z;
				const char *vp = Q_stristr( line, "vertex" );
				if ( vp && sscanf( vp, "vertex %f %f %f", &x, &y, &z ) == 3 ) {
					if ( state == 0 ) {
						v0[0] = x; v0[1] = y; v0[2] = z; state = 1;
					} else if ( state == 1 ) {
						v1[0] = x; v1[1] = y; v1[2] = z; state = 2;
					} else if ( state == 2 ) {
						int base = nv;
						v2[0] = x; v2[1] = y; v2[2] = z;
						if ( nv + 3 > MIMP_MAX_VERTS ) {
							return qfalse;
						}
						VectorCopy( v0, verts + base * 3 );
						VectorCopy( v1, verts + ( base + 1 ) * 3 );
						VectorCopy( v2, verts + ( base + 2 ) * 3 );
						nv += 3;
						inds[ni++] = base;
						inds[ni++] = base + 1;
						inds[ni++] = base + 2;
						state = 0;
					}
				}
			} else if ( Q_stristr( line, "endfacet" ) ) {
				state = 0;
			}
		}
	}

	if ( nv < 3 || ni < 3 ) {
		return qfalse;
	}
	return R_MeshImport_FinalizeMD3( mod, lod, name, verts, nv, inds, ni );
}

static const char *R_DAE_FindAttr( const char *tagStart, const char *tagEnd, const char *attr ) {
	char pat[64];
	const char *p;

	Com_sprintf( pat, sizeof( pat ), "%s=\"", attr );
	p = Q_stristr( tagStart, pat );
	if ( !p || p >= tagEnd ) {
		return NULL;
	}
	p += strlen( pat );
	return p < tagEnd ? p : NULL;
}

static qboolean R_DAE_CopyAttr( const char *tagStart, const char *tagEnd, const char *attr, char *out, int outSize ) {
	const char *p = R_DAE_FindAttr( tagStart, tagEnd, attr );
	const char *e;
	int n;

	if ( !p || outSize <= 0 ) {
		return qfalse;
	}
	e = strchr( p, '"' );
	if ( !e || e > tagEnd ) {
		return qfalse;
	}
	n = (int)( e - p );
	if ( n >= outSize ) {
		n = outSize - 1;
	}
	Com_Memcpy( out, p, (size_t)n );
	out[n] = '\0';
	return qtrue;
}

static const char *R_DAE_FindElementWithId( const char *text, const char *element, const char *id ) {
	char openPat[64];
	const char *p = text;

	Com_sprintf( openPat, sizeof( openPat ), "<%s", element );
	while ( ( p = Q_stristr( p, openPat ) ) != NULL ) {
		const char *gt = strchr( p, '>' );
		char found[128];
		if ( !gt ) {
			return NULL;
		}
		if ( R_DAE_CopyAttr( p, gt, "id", found, sizeof( found ) ) && !Q_stricmp( found, id ) ) {
			return p;
		}
		p = gt + 1;
	}
	return NULL;
}

static int R_DAE_ReadFloatArray( const char *sourceElem, float *out, int maxFloats ) {
	const char *fa = Q_stristr( sourceElem, "<float_array" );
	const char *gt;
	const char *close;
	int n = 0;

	if ( !fa ) {
		return 0;
	}
	gt = strchr( fa, '>' );
	close = Q_stristr( fa, "</float_array>" );
	if ( !gt || !close || close <= gt ) {
		return 0;
	}
	while ( gt < close && n < maxFloats ) {
		char *endp = NULL;
		double d = strtod( gt, &endp );
		if ( endp == gt ) {
			gt++;
			continue;
		}
		out[n++] = (float)d;
		gt = endp;
	}
	return n;
}

static int R_DAE_SourceStride( const char *sourceElem ) {
	const char *acc = Q_stristr( sourceElem, "<accessor" );
	const char *gt;
	char stride[32];

	if ( !acc ) {
		return 3;
	}
	gt = strchr( acc, '>' );
	if ( !gt || !R_DAE_CopyAttr( acc, gt, "stride", stride, sizeof( stride ) ) ) {
		return 3;
	}
	return MAX( 1, atoi( stride ) );
}

static qboolean R_DAE_ResolveVertexPositionSource( const char *text, const char *verticesId,
	char *positionSource, int positionSourceSize ) {
	const char *vertices = R_DAE_FindElementWithId( text, "vertices", verticesId );
	const char *end = vertices ? Q_stristr( vertices, "</vertices>" ) : NULL;
	const char *input;

	if ( !vertices || !end ) {
		return qfalse;
	}
	input = vertices;
	while ( ( input = Q_stristr( input, "<input" ) ) != NULL && input < end ) {
		const char *gt = strchr( input, '>' );
		char semantic[64];
		char source[128];
		if ( !gt || gt > end ) {
			break;
		}
		if ( R_DAE_CopyAttr( input, gt, "semantic", semantic, sizeof( semantic ) ) &&
			!Q_stricmp( semantic, "POSITION" ) &&
			R_DAE_CopyAttr( input, gt, "source", source, sizeof( source ) ) ) {
			Q_strncpyz( positionSource, source[0] == '#' ? source + 1 : source, positionSourceSize );
			return qtrue;
		}
		input = gt + 1;
	}
	return qfalse;
}

static qboolean R_DAE_FindInput( const char *start, const char *end, const char *semantic,
	char *sourceOut, int sourceOutSize, int *offsetOut ) {
	const char *input = start;

	while ( ( input = Q_stristr( input, "<input" ) ) != NULL && input < end ) {
		const char *gt = strchr( input, '>' );
		char sem[64];
		char source[128];
		char offset[32];
		if ( !gt || gt > end ) {
			break;
		}
		if ( R_DAE_CopyAttr( input, gt, "semantic", sem, sizeof( sem ) ) &&
			!Q_stricmp( sem, semantic ) &&
			R_DAE_CopyAttr( input, gt, "source", source, sizeof( source ) ) ) {
			Q_strncpyz( sourceOut, source[0] == '#' ? source + 1 : source, sourceOutSize );
			if ( offsetOut ) {
				*offsetOut = 0;
				if ( R_DAE_CopyAttr( input, gt, "offset", offset, sizeof( offset ) ) ) {
					*offsetOut = atoi( offset );
				}
			}
			return qtrue;
		}
		input = gt + 1;
	}
	return qfalse;
}

static int R_DAE_ReadInts( const char *start, const char *end, int *out, int maxInts ) {
	int n = 0;

	while ( start < end && n < maxInts ) {
		char *endp = NULL;
		long v = strtol( start, &endp, 10 );
		if ( endp == start ) {
			start++;
			continue;
		}
		out[n++] = (int)v;
		start = endp;
	}
	return n;
}

static qboolean R_LoadDAE_NativeStatic( model_t *mod, int lod, const char *name, char *text ) {
	const char *prim = Q_stristr( text, "<triangles" );
	const char *primClose = NULL;
	const char *primTagEnd;
	const char *pElem;
	const char *pClose;
	const char *vcountElem = NULL;
	const char *vcountClose = NULL;
	qboolean isPolylist = qfalse;
	char vertexSource[128] = "";
	char positionSource[128] = "";
	char texcoordSource[128] = "";
	char normalSource[128] = "";
	char material[128] = "";
	float *positions;
	float *texcoords = NULL;
	float *normals = NULL;
	float *verts;
	float *vertNormals;
	float *st;
	int *raw;
	int *inds;
	int posCount, posStride;
	int texCount = 0, texStride = 2;
	int normCount = 0, normStride = 3;
	int vertexOffset = -1;
	int texOffset = -1;
	int normalOffset = -1;
	int maxOffset = 0;
	int rawCount;
	int nv = 0;
	int i;
	qboolean ok = qfalse;

	if ( !prim ) {
		prim = Q_stristr( text, "<polylist" );
		isPolylist = prim ? qtrue : qfalse;
	}
	if ( !prim ) {
		return qfalse;
	}
	primTagEnd = strchr( prim, '>' );
	primClose = Q_stristr( prim, isPolylist ? "</polylist>" : "</triangles>" );
	if ( !primTagEnd || !primClose || primClose <= primTagEnd ) {
		return qfalse;
	}

	if ( R_DAE_CopyAttr( prim, primTagEnd, "material", material, sizeof( material ) ) ) {
		/* Common Q3 content uses shader-like material symbols; keep plain symbols as-is. */
	}

	if ( !R_DAE_FindInput( primTagEnd, primClose, "VERTEX", vertexSource, sizeof( vertexSource ), &vertexOffset ) ) {
		if ( !R_DAE_FindInput( primTagEnd, primClose, "POSITION", positionSource, sizeof( positionSource ), &vertexOffset ) ) {
			return qfalse;
		}
	} else if ( !R_DAE_ResolveVertexPositionSource( text, vertexSource, positionSource, sizeof( positionSource ) ) ) {
		return qfalse;
	}
	R_DAE_FindInput( primTagEnd, primClose, "TEXCOORD", texcoordSource, sizeof( texcoordSource ), &texOffset );
	R_DAE_FindInput( primTagEnd, primClose, "NORMAL", normalSource, sizeof( normalSource ), &normalOffset );

	{
		const char *posElem = R_DAE_FindElementWithId( text, "source", positionSource );
		if ( !posElem ) {
			return qfalse;
		}
		positions = (float *)ri.Malloc( sizeof( float ) * MIMP_MAX_VERTS * 3 );
		if ( !positions ) {
			return qfalse;
		}
		posCount = R_DAE_ReadFloatArray( posElem, positions, MIMP_MAX_VERTS * 3 );
		posStride = R_DAE_SourceStride( posElem );
	}
	if ( posCount < posStride || posStride < 3 ) {
		ri.Free( positions );
		return qfalse;
	}

	if ( texcoordSource[0] ) {
		const char *texElem = R_DAE_FindElementWithId( text, "source", texcoordSource );
		if ( texElem ) {
			texcoords = (float *)ri.Malloc( sizeof( float ) * MIMP_MAX_VERTS * 2 );
			if ( texcoords ) {
				texCount = R_DAE_ReadFloatArray( texElem, texcoords, MIMP_MAX_VERTS * 2 );
				texStride = R_DAE_SourceStride( texElem );
				if ( texStride < 2 || texCount < texStride ) {
					texCount = 0;
				}
			}
		}
	}

	if ( normalSource[0] ) {
		const char *normElem = R_DAE_FindElementWithId( text, "source", normalSource );
		if ( normElem ) {
			normals = (float *)ri.Malloc( sizeof( float ) * MIMP_MAX_VERTS * 3 );
			if ( normals ) {
				normCount = R_DAE_ReadFloatArray( normElem, normals, MIMP_MAX_VERTS * 3 );
				normStride = R_DAE_SourceStride( normElem );
				if ( normStride < 3 || normCount < normStride ) {
					normCount = 0;
				}
			}
		}
	}

	pElem = Q_stristr( primTagEnd, "<p>" );
	pClose = pElem ? Q_stristr( pElem, "</p>" ) : NULL;
	if ( !pElem || !pClose || pClose > primClose ) {
		ri.Free( positions );
		if ( texcoords ) {
			ri.Free( texcoords );
		}
		return qfalse;
	}
	pElem = strchr( pElem, '>' ) + 1;
	raw = (int *)ri.Malloc( sizeof( int ) * MIMP_MAX_VERTS * 4 );
	verts = (float *)ri.Malloc( sizeof( float ) * MIMP_MAX_VERTS * 3 );
	vertNormals = (float *)ri.Malloc( sizeof( float ) * MIMP_MAX_VERTS * 3 );
	st = (float *)ri.Malloc( sizeof( float ) * MIMP_MAX_VERTS * 2 );
	inds = (int *)ri.Malloc( sizeof( int ) * MIMP_MAX_VERTS );
	if ( !raw || !verts || !vertNormals || !st || !inds ) {
		goto done;
	}
	rawCount = R_DAE_ReadInts( pElem, pClose, raw, MIMP_MAX_VERTS * 4 );

	if ( texOffset > maxOffset ) {
		maxOffset = texOffset;
	}
	if ( normalOffset > maxOffset ) {
		maxOffset = normalOffset;
	}
	if ( vertexOffset > maxOffset ) {
		maxOffset = vertexOffset;
	}
	if ( maxOffset < 0 || rawCount <= 0 ) {
		goto done;
	}
	maxOffset++;

	if ( isPolylist ) {
		vcountElem = Q_stristr( primTagEnd, "<vcount>" );
		vcountClose = vcountElem ? Q_stristr( vcountElem, "</vcount>" ) : NULL;
		if ( vcountElem && vcountClose && vcountClose < primClose ) {
			vcountElem = strchr( vcountElem, '>' ) + 1;
		} else {
			vcountElem = vcountClose = NULL;
		}
	}

	if ( vcountElem ) {
		int *vcounts = (int *)ri.Malloc( sizeof( int ) * MIMP_MAX_TRIS );
		int vc = vcounts ? R_DAE_ReadInts( vcountElem, vcountClose, vcounts, MIMP_MAX_TRIS ) : 0;
		int rawAt = 0;
		int poly;
		if ( !vcounts ) {
			goto done;
		}
		for ( poly = 0; poly < vc && nv + 3 <= MIMP_MAX_VERTS; ++poly ) {
			int corner;
			if ( vcounts[poly] != 3 ) {
				rawAt += vcounts[poly] * maxOffset;
				continue;
			}
			for ( corner = 0; corner < 3 && rawAt + maxOffset <= rawCount; ++corner, rawAt += maxOffset ) {
				int pi = raw[rawAt + vertexOffset];
				int ti = texOffset >= 0 ? raw[rawAt + texOffset] : -1;
				int ni = normalOffset >= 0 ? raw[rawAt + normalOffset] : -1;
				if ( pi < 0 || pi * posStride + 2 >= posCount ) {
					ri.Free( vcounts );
					goto done;
				}
				verts[nv * 3 + 0] = positions[pi * posStride + 0];
				verts[nv * 3 + 1] = positions[pi * posStride + 1];
				verts[nv * 3 + 2] = positions[pi * posStride + 2];
				st[nv * 2 + 0] = ( texcoords && ti >= 0 && ti * texStride + 1 < texCount ) ? texcoords[ti * texStride + 0] : 0.0f;
				st[nv * 2 + 1] = ( texcoords && ti >= 0 && ti * texStride + 1 < texCount ) ? 1.0f - texcoords[ti * texStride + 1] : 0.0f;
				vertNormals[nv * 3 + 0] = ( normals && ni >= 0 && ni * normStride + 2 < normCount ) ? normals[ni * normStride + 0] : 0.0f;
				vertNormals[nv * 3 + 1] = ( normals && ni >= 0 && ni * normStride + 2 < normCount ) ? normals[ni * normStride + 1] : 0.0f;
				vertNormals[nv * 3 + 2] = ( normals && ni >= 0 && ni * normStride + 2 < normCount ) ? normals[ni * normStride + 2] : 1.0f;
				inds[nv] = nv;
				nv++;
			}
		}
		ri.Free( vcounts );
	} else {
		for ( i = 0; i + maxOffset <= rawCount && nv < MIMP_MAX_VERTS; i += maxOffset ) {
			int pi = raw[i + vertexOffset];
			int ti = texOffset >= 0 ? raw[i + texOffset] : -1;
			int ni = normalOffset >= 0 ? raw[i + normalOffset] : -1;
			if ( pi < 0 || pi * posStride + 2 >= posCount ) {
				goto done;
			}
			verts[nv * 3 + 0] = positions[pi * posStride + 0];
			verts[nv * 3 + 1] = positions[pi * posStride + 1];
			verts[nv * 3 + 2] = positions[pi * posStride + 2];
			st[nv * 2 + 0] = ( texcoords && ti >= 0 && ti * texStride + 1 < texCount ) ? texcoords[ti * texStride + 0] : 0.0f;
			st[nv * 2 + 1] = ( texcoords && ti >= 0 && ti * texStride + 1 < texCount ) ? 1.0f - texcoords[ti * texStride + 1] : 0.0f;
			vertNormals[nv * 3 + 0] = ( normals && ni >= 0 && ni * normStride + 2 < normCount ) ? normals[ni * normStride + 0] : 0.0f;
			vertNormals[nv * 3 + 1] = ( normals && ni >= 0 && ni * normStride + 2 < normCount ) ? normals[ni * normStride + 1] : 0.0f;
			vertNormals[nv * 3 + 2] = ( normals && ni >= 0 && ni * normStride + 2 < normCount ) ? normals[ni * normStride + 2] : 1.0f;
			inds[nv] = nv;
			nv++;
		}
	}

	if ( nv >= 3 && nv % 3 == 0 ) {
		ok = R_MeshImport_FinalizeMD3Ex( mod, lod, name, verts, nv, inds, nv,
			material[0] ? material : NULL, texcoords ? st : NULL, normals ? vertNormals : NULL );
	}

done:
	ri.Free( positions );
	if ( texcoords ) {
		ri.Free( texcoords );
	}
	if ( normals ) {
		ri.Free( normals );
	}
	if ( raw ) {
		ri.Free( raw );
	}
	if ( verts ) {
		ri.Free( verts );
	}
	if ( vertNormals ) {
		ri.Free( vertNormals );
	}
	if ( st ) {
		ri.Free( st );
	}
	if ( inds ) {
		ri.Free( inds );
	}
	return ok;
}

static qboolean R_LoadDAE_FloatSoup( model_t *mod, int lod, const char *name, char *text ) {
	float pos[49152];
	int np = 0;
	const char *fa = Q_stristr( text, "<float_array" );

	while ( fa && np == 0 ) {
		const char *cnt = Q_stristr( fa, "count=\"" );
		const char *gt = strchr( fa, '>' );
		int count = 0;
		const char *close;
		if ( !cnt || !gt || cnt > gt ) {
			fa = Q_stristr( fa + 1, "<float_array" );
			continue;
		}
		if ( sscanf( cnt, "count=\"%d\"", &count ) != 1 || count < 9 || count > (int)( sizeof( pos ) / sizeof( pos[0] ) ) ) {
			fa = Q_stristr( fa + 1, "<float_array" );
			continue;
		}
		close = Q_stristr( gt + 1, "</float_array>" );
		if ( !close ) {
			break;
		}
		{
			const char *q = gt + 1;
			while ( q < close && np < count ) {
				char *endp = NULL;
				double d = strtod( q, &endp );
				if ( endp == q ) {
					q++;
					continue;
				}
				pos[np++] = (float)d;
				q = endp;
			}
		}
		if ( np != count || np % 9 != 0 ) {
			np = 0;
		}
		fa = Q_stristr( fa + 1, "<float_array" );
	}

	if ( np < 9 || np % 9 != 0 ) {
		return qfalse;
	}
	{
		int nv = np / 3;
		int i;
		int *inds = (int *)ri.Malloc( (int)( sizeof( int ) * (size_t)nv ) );
		if ( !inds ) {
			return qfalse;
		}
		for ( i = 0; i < nv; i++ ) {
			inds[i] = i;
		}
		{
			qboolean ok = R_MeshImport_FinalizeMD3( mod, lod, name, pos, nv, inds, nv );
			ri.Free( inds );
			return ok;
		}
	}
}

static qboolean R_LoadVertexLines( model_t *mod, int lod, const char *name, const char *text ) {
	float verts[MIMP_MAX_VERTS * 3];
	int inds[MIMP_MAX_VERTS];
	int nv = 0;
	const char *p = text;
	char line[512];

	while ( *p && nv < MIMP_MAX_VERTS - 2 ) {
		const char *eol = p;
		int len;
		float x, y, z;
		while ( *eol && *eol != '\n' && *eol != '\r' ) {
			eol++;
		}
		len = (int)( eol - p );
		if ( len >= (int)sizeof( line ) ) {
			len = (int)sizeof( line ) - 1;
		}
		Com_Memcpy( line, p, (size_t)len );
		line[len] = '\0';
		p = eol;
		if ( *p ) {
			p++;
		}

		if ( sscanf( line, "v %f %f %f", &x, &y, &z ) == 3 ||
			sscanf( line, "vertex %f %f %f", &x, &y, &z ) == 3 ) {
			verts[nv * 3 + 0] = x;
			verts[nv * 3 + 1] = y;
			verts[nv * 3 + 2] = z;
			nv++;
		}
	}

	if ( nv < 3 || nv % 3 != 0 ) {
		return qfalse;
	}
	{
		int i;
		for ( i = 0; i < nv; i++ ) {
			inds[i] = i;
		}
		return R_MeshImport_FinalizeMD3( mod, lod, name, verts, nv, inds, nv );
	}
}

static qboolean R_LoadMeshFile( model_t *mod, int lod, const char *path, const char *ext ) {
	void *buf = NULL;
	int sz = ri.FS_ReadFile( path, &buf );
	qboolean ok = qfalse;

	if ( sz <= 0 || !buf ) {
		return qfalse;
	}

	if ( !Q_stricmp( ext, "stl" ) ) {
		ok = R_LoadSTL( mod, lod, path, (const byte *)buf, sz );
	} else if ( !Q_stricmp( ext, "dae" ) ) {
		char *text = (char *)ri.Malloc( sz + 1 );
		if ( text ) {
			Com_Memcpy( text, buf, (size_t)sz );
			text[sz] = '\0';
			ok = R_LoadDAE_NativeStatic( mod, lod, path, text );
			if ( !ok ) {
				ok = R_LoadDAE_FloatSoup( mod, lod, path, text );
			}
			ri.Free( text );
		}
	} else {
		char *text = (char *)ri.Malloc( sz + 1 );
		if ( text ) {
			Com_Memcpy( text, buf, (size_t)sz );
			text[sz] = '\0';
			ok = R_LoadVertexLines( mod, lod, path, text );
			ri.Free( text );
		}
	}

	ri.FS_FreeFile( buf );
	return ok;
}

qhandle_t R_RegisterMeshImport( const char *name, model_t *mod ) {
	char filename[MAX_QPATH];
	char *dot;
	const char *fext;
	int lod;
	int numLoaded = 0;

	Q_strncpyz( filename, name, sizeof( filename ) );
	dot = strrchr( filename, '.' );
	if ( !dot ) {
		mod->type = MOD_BAD;
		return 0;
	}
	fext = dot + 1;

	for ( lod = MD3_MAX_LODS - 1; lod >= 0; lod-- ) {
		char namebuf[MAX_QPATH + 32];
		if ( lod ) {
			Com_sprintf( namebuf, sizeof( namebuf ), "%.*s_%d.%s",
				(int)( dot - filename ), filename, lod, fext );
		} else {
			Q_strncpyz( namebuf, name, sizeof( namebuf ) );
		}

		if ( ri.FS_ReadFile( namebuf, NULL ) <= 0 ) {
			continue;
		}
		if ( R_LoadMeshFile( mod, lod, namebuf, fext ) ) {
			mod->numLods++;
			numLoaded++;
		} else {
			break;
		}
	}

	if ( numLoaded ) {
		for ( lod--; lod >= 0; lod-- ) {
			mod->numLods++;
			mod->md3[lod] = mod->md3[lod + 1];
		}
		return mod->index;
	}

	mod->type = MOD_BAD;
	return 0;
}
