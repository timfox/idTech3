/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

glTF GPU tangent topology build + SSBO pack for Vulkan PBR (mode 2).
===========================================================================
*/

#include "tr_local.h"
#include "tr_gltf_topo.h"

static void append_incident_tri( uint32_t *topoOut, int numVerts, int vert, uint32_t triFirstIndex )
{
	uint32_t *row;
	uint32_t cnt;

	if ( vert < 0 || vert >= numVerts ) {
		return;
	}
	row = topoOut + (size_t)vert * (size_t)GLTF_GPU_TOPO_WORDS_PER_VERT;
	cnt = row[0];
	if ( cnt >= (uint32_t)GLTF_GPU_ADJ_TRIS_MAX ) {
		return;
	}
	row[1u + cnt] = triFirstIndex;
	row[0] = cnt + 1u;
}

void R_BuildGLTFPrimitiveTopo( const uint32_t *indices, int numIndices, int numVerts, uint32_t *topoOut )
{
	int numTris;
	int ti;

	if ( !indices || !topoOut || numVerts <= 0 || numIndices < 3 || ( numIndices % 3 ) != 0 ) {
		return;
	}
	numTris = numIndices / 3;
	Com_Memset( topoOut, 0, (size_t)numVerts * (size_t)GLTF_GPU_TOPO_WORDS_PER_VERT * sizeof( uint32_t ) );

	for ( ti = 0; ti < numTris; ti++ ) {
		uint32_t i0 = indices[ti * 3 + 0];
		uint32_t i1 = indices[ti * 3 + 1];
		uint32_t i2 = indices[ti * 3 + 2];
		uint32_t triFirst = (uint32_t)( ti * 3 );

		if ( i0 == i1 || i1 == i2 || i2 == i0 ) {
			continue;
		}
		if ( (int)i0 >= numVerts || (int)i1 >= numVerts || (int)i2 >= numVerts ) {
			continue;
		}
		append_incident_tri( topoOut, numVerts, (int)i0, triFirst );
		append_incident_tri( topoOut, numVerts, (int)i1, triFirst );
		append_incident_tri( topoOut, numVerts, (int)i2, triFirst );
	}
}

static int align16_uints( int n )
{
	return ( n + 15 ) / 16 * 16;
}

int R_GLTFTopoDrawBlobUints( int numIndices, int numVerts )
{
	int topoWords;
	int topoBase;
	int pullBase;
	int pullWords;

	if ( numIndices <= 0 || numVerts <= 0 ) {
		return 0;
	}
	topoWords = numVerts * GLTF_GPU_TOPO_WORDS_PER_VERT;
	topoBase = align16_uints( 4 + numIndices );
	pullBase = align16_uints( topoBase + topoWords );
	pullWords = numVerts * GLTF_GPU_PULL_UINTS_PER_VERT;
	return pullBase + pullWords;
}

void R_GLTFTopoPackDrawBlob( const uint32_t *indices, int numIndices, int numVerts,
	const uint32_t *topo, const gltfVertex_t *vertices, uint32_t *out, int *outTopoBase )
{
	int topoWords;
	int topoBase;
	int pullBase;
	int vi;
	const float *srcF;
	uint32_t *dstU;

	if ( !indices || !topo || !vertices || !out || !outTopoBase ) {
		return;
	}
	topoWords = numVerts * GLTF_GPU_TOPO_WORDS_PER_VERT;
	topoBase = align16_uints( 4 + numIndices );
	pullBase = align16_uints( topoBase + topoWords );

	out[0] = (uint32_t)numIndices;
	out[1] = (uint32_t)numVerts;
	out[2] = (uint32_t)topoBase;
	out[3] = (uint32_t)pullBase;
	Com_Memcpy( out + 4, indices, (size_t)numIndices * sizeof( uint32_t ) );
	Com_Memset( out + 4 + numIndices, 0, (size_t)( topoBase - 4 - numIndices ) * sizeof( uint32_t ) );
	Com_Memcpy( out + topoBase, topo, (size_t)topoWords * sizeof( uint32_t ) );
	Com_Memset( out + topoBase + topoWords, 0, (size_t)( pullBase - topoBase - topoWords ) * sizeof( uint32_t ) );

	dstU = out + pullBase;
	for ( vi = 0; vi < numVerts; vi++ ) {
		const gltfVertex_t *v = &vertices[vi];
		size_t base = (size_t)vi * (size_t)GLTF_GPU_PULL_UINTS_PER_VERT;
		srcF = v->position;
		Com_Memcpy( dstU + base + 0, srcF, sizeof( float ) * 3u );
		srcF = v->normal;
		Com_Memcpy( dstU + base + 3, srcF, sizeof( float ) * 3u );
		srcF = v->texCoord0;
		Com_Memcpy( dstU + base + 6, srcF, sizeof( float ) * 2u );
		dstU[base + 8] = (uint32_t)v->joints[0];
		dstU[base + 9] = (uint32_t)v->joints[1];
		dstU[base + 10] = (uint32_t)v->joints[2];
		dstU[base + 11] = (uint32_t)v->joints[3];
		Com_Memcpy( dstU + base + 12, v->weights, sizeof( float ) * 4u );
	}
	*outTopoBase = topoBase;
}
