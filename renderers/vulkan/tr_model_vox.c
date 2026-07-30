/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

MagicaVoxel .vox → MOD_MESH (exposed-face cube mesh + 16×16 palette shader).
===========================================================================
*/

#include "tr_local.h"
#include "../common/tr_model_mesh_import.h"
#include "../common/tr_vox_parse.h"
#include <stdlib.h>
#include <math.h>

#define VOX_FACE_VERTS_MAX  ( SHADER_MAX_VERTEXES )
#define VOX_FACE_IDX_MAX    SHADER_MAX_VERTEXES

/* Face verts in local voxel space (+X right, +Y forward, +Z up). */
static const float s_voxFaceVerts[6][4][3] = {
	/* +X */ { { 1, 0, 0 }, { 1, 1, 0 }, { 1, 1, 1 }, { 1, 0, 1 } },
	/* -X */ { { 0, 1, 0 }, { 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 1 } },
	/* +Y */ { { 1, 1, 0 }, { 0, 1, 0 }, { 0, 1, 1 }, { 1, 1, 1 } },
	/* -Y */ { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 0, 1 }, { 0, 0, 1 } },
	/* +Z */ { { 0, 0, 1 }, { 1, 0, 1 }, { 1, 1, 1 }, { 0, 1, 1 } },
	/* -Z */ { { 0, 1, 0 }, { 1, 1, 0 }, { 1, 0, 0 }, { 0, 0, 0 } },
};

static const int s_voxFaceNbr[6][3] = {
	{ 1, 0, 0 }, { -1, 0, 0 },
	{ 0, 1, 0 }, { 0, -1, 0 },
	{ 0, 0, 1 }, { 0, 0, -1 },
};

static void R_Vox_PaletteUv( int colorIndex, float *u, float *v )
{
	int idx = colorIndex & 255;
	int x = idx % 16;
	int y = idx / 16;

	*u = ( (float)x + 0.5f ) / 16.0f;
	*v = ( (float)y + 0.5f ) / 16.0f;
}

static qboolean R_Vox_BuildPaletteShader( const char *modelName, const voxModel_t *vox,
	char *shaderOut, int shaderOutSize )
{
	byte pic[16 * 16 * 4];
	image_t *img;
	qhandle_t sh;
	char imgName[MAX_QPATH];
	char base[MAX_QPATH];
	int i;

	COM_StripExtension( modelName, base, sizeof( base ) );
	Com_sprintf( imgName, sizeof( imgName ), "*voxpal/%s", base );
	Com_sprintf( shaderOut, shaderOutSize, "*voxsh/%s", base );

	for ( i = 0; i < 256; i++ ) {
		pic[i * 4 + 0] = vox->palette[i][0];
		pic[i * 4 + 1] = vox->palette[i][1];
		pic[i * 4 + 2] = vox->palette[i][2];
		pic[i * 4 + 3] = ( i == 0 ) ? 0 : 255;
	}

	img = R_CreateImage( imgName, NULL, pic, 16, 16,
		IMGFLAG_CLAMPTOEDGE | IMGFLAG_NOLIGHTSCALE | IMGFLAG_NO_COMPRESSION, 0, 0 );
	if ( !img ) {
		return qfalse;
	}
	sh = RE_RegisterShaderFromImage( shaderOut, LIGHTMAP_NONE, img, qfalse );
	return ( sh != 0 ) ? qtrue : qfalse;
}

static qboolean R_Vox_EmitMesh( const voxModel_t *vox, float *verts, float *sts, int *inds,
	int *outNumVerts, int *outNumIdx )
{
	size_t vol;
	byte *occ;
	int i, f;
	int nv = 0;
	int ni = 0;
	float minX, minY, minZ, maxX, maxY, maxZ;
	float cx, cy, cz;

	*outNumVerts = 0;
	*outNumIdx = 0;
	if ( !vox || vox->numVoxels <= 0 || !vox->xyzc ) {
		return qfalse;
	}

	vol = (size_t)vox->sizeX * (size_t)vox->sizeY * (size_t)vox->sizeZ;
	if ( vol == 0 || vol > (size_t)VOX_MAX_DIM * VOX_MAX_DIM * VOX_MAX_DIM ) {
		return qfalse;
	}

	occ = (byte *)calloc( vol, 1 );
	if ( !occ ) {
		return qfalse;
	}

	minX = minY = minZ = 1e9f;
	maxX = maxY = maxZ = -1e9f;

	for ( i = 0; i < vox->numVoxels; i++ ) {
		int x = vox->xyzc[i * 4 + 0];
		int y = vox->xyzc[i * 4 + 1];
		int z = vox->xyzc[i * 4 + 2];
		size_t idx;

		if ( x < 0 || x >= vox->sizeX || y < 0 || y >= vox->sizeY || z < 0 || z >= vox->sizeZ ) {
			continue;
		}
		idx = (size_t)x + (size_t)y * (size_t)vox->sizeX
			+ (size_t)z * (size_t)vox->sizeX * (size_t)vox->sizeY;
		occ[idx] = vox->xyzc[i * 4 + 3] ? vox->xyzc[i * 4 + 3] : 1;
		if ( (float)x < minX ) minX = (float)x;
		if ( (float)y < minY ) minY = (float)y;
		if ( (float)z < minZ ) minZ = (float)z;
		if ( (float)x + 1.0f > maxX ) maxX = (float)x + 1.0f;
		if ( (float)y + 1.0f > maxY ) maxY = (float)y + 1.0f;
		if ( (float)z + 1.0f > maxZ ) maxZ = (float)z + 1.0f;
	}

	cx = ( minX + maxX ) * 0.5f;
	cy = ( minY + maxY ) * 0.5f;
	cz = ( minZ + maxZ ) * 0.5f;

	for ( i = 0; i < vox->numVoxels; i++ ) {
		int x = vox->xyzc[i * 4 + 0];
		int y = vox->xyzc[i * 4 + 1];
		int z = vox->xyzc[i * 4 + 2];
		int ci = vox->xyzc[i * 4 + 3];
		float u, v;

		if ( x < 0 || x >= vox->sizeX || y < 0 || y >= vox->sizeY || z < 0 || z >= vox->sizeZ ) {
			continue;
		}
		R_Vox_PaletteUv( ci, &u, &v );

		for ( f = 0; f < 6; f++ ) {
			int nx = x + s_voxFaceNbr[f][0];
			int ny = y + s_voxFaceNbr[f][1];
			int nz = z + s_voxFaceNbr[f][2];
			int base;
			int k;
			const int triIdx[6] = { 0, 1, 2, 0, 2, 3 };

			if ( nx >= 0 && nx < vox->sizeX && ny >= 0 && ny < vox->sizeY && nz >= 0 && nz < vox->sizeZ ) {
				size_t nidx = (size_t)nx + (size_t)ny * (size_t)vox->sizeX
					+ (size_t)nz * (size_t)vox->sizeX * (size_t)vox->sizeY;
				if ( occ[nidx] ) {
					continue;
				}
			}

			if ( nv + 6 > VOX_FACE_VERTS_MAX || ni + 6 > VOX_FACE_IDX_MAX ) {
				free( occ );
				return qfalse;
			}

			base = nv;
			for ( k = 0; k < 6; k++ ) {
				int qi = triIdx[k];
				float px = (float)x + s_voxFaceVerts[f][qi][0] - cx;
				float py = (float)y + s_voxFaceVerts[f][qi][1] - cy;
				float pz = (float)z + s_voxFaceVerts[f][qi][2] - cz;
				verts[( base + k ) * 3 + 0] = px;
				verts[( base + k ) * 3 + 1] = py;
				verts[( base + k ) * 3 + 2] = pz;
				sts[( base + k ) * 2 + 0] = u;
				sts[( base + k ) * 2 + 1] = v;
				inds[ni++] = base + k;
			}
			nv += 6;
		}
	}

	free( occ );
	*outNumVerts = nv;
	*outNumIdx = ni;
	return ( nv > 0 && ni > 0 ) ? qtrue : qfalse;
}

extern qhandle_t R_RegisterVOX( const char *name, model_t *mod );

qhandle_t R_RegisterVOX( const char *name, model_t *mod )
{
	union {
		byte *b;
		void *v;
	} buf;
	int fileSize;
	voxModel_t vox;
	float *verts = NULL;
	float *sts = NULL;
	int *inds = NULL;
	int numVerts = 0, numIdx = 0;
	char shaderName[MAX_QPATH];
	qboolean ok = qfalse;

	if ( !name || !mod ) {
		return 0;
	}

	fileSize = ri.FS_ReadFile( name, &buf.v );
	if ( !buf.v || fileSize <= 0 ) {
		return 0;
	}

	if ( !R_Vox_Parse( buf.b, fileSize, &vox ) ) {
		ri.Printf( PRINT_WARNING, "R_RegisterVOX: failed to parse '%s'\n", name );
		ri.FS_FreeFile( buf.v );
		return 0;
	}
	ri.FS_FreeFile( buf.v );

	verts = (float *)malloc( (size_t)VOX_FACE_VERTS_MAX * 3u * sizeof( float ) );
	sts = (float *)malloc( (size_t)VOX_FACE_VERTS_MAX * 2u * sizeof( float ) );
	inds = (int *)malloc( (size_t)VOX_FACE_IDX_MAX * sizeof( int ) );
	if ( !verts || !sts || !inds ) {
		free( verts );
		free( sts );
		free( inds );
		R_Vox_Free( &vox );
		return 0;
	}

	if ( !R_Vox_EmitMesh( &vox, verts, sts, inds, &numVerts, &numIdx ) ) {
		ri.Printf( PRINT_WARNING, "R_RegisterVOX: mesh emit failed for '%s' (voxels=%d)\n",
			name, vox.numVoxels );
		free( verts );
		free( sts );
		free( inds );
		R_Vox_Free( &vox );
		return 0;
	}

	if ( !R_Vox_BuildPaletteShader( name, &vox, shaderName, sizeof( shaderName ) ) ) {
		Q_strncpyz( shaderName, "textures/common/white", sizeof( shaderName ) );
	}

	ok = R_MeshImport_FinalizeMD3Ex( mod, 0, name, verts, numVerts, inds, numIdx,
		shaderName, sts, NULL );
	{
		int voxelCount = vox.numVoxels;
		free( verts );
		free( sts );
		free( inds );
		R_Vox_Free( &vox );

		if ( !ok ) {
			ri.Printf( PRINT_WARNING, "R_RegisterVOX: MD3 finalize failed for '%s'\n", name );
			return 0;
		}

		mod->numLods = 1;
		ri.Printf( PRINT_ALL, "[engine][vox] loaded '%s' (%d voxels → %d tris) shader='%s'\n",
			name, voxelCount, numIdx / 3, shaderName );
	}
	return mod->index;
}
