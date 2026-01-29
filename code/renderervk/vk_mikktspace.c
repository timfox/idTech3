/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "tr_local.h"
#include "utils/mikktspace/mikktspace.h"

/*
================
R_FixMikktVertIndex

Swaps second index to third and the other way around.
When baking normal maps for models, the mesh importer will swap face indices to change the face orientation.
To generate exactly the same tangents, we have to swap indices to the same order.
Change when mesh importers swaps different indices!
(modelling tool standard is backface culling vs frontface culling in id tech 3)
================
*/
int R_FixMikktVertIndex(const int index)
{
	switch (index % 3)
	{
		case 2: return 1;
		case 1: return 2;
		default: return index;
	}
	return index;
}

//
// BSP triangle soup
//

int mikkt_bsp_tri_GetNumFaces( const SMikkTSpaceContext *context ) 
{
	srfTriangles_t *tri = (srfTriangles_t *)context->m_pUserData;
	return tri->numIndexes / 3;
}

int mikkt_bsp_tri_GetNumVerticesOfFace( const SMikkTSpaceContext *context, const int face )
{
	return 3;
}

void mikkt_bsp_tri_GetPosition( const SMikkTSpaceContext *context, float position[], const int face, const int vert )
{
	srfTriangles_t *tri = (srfTriangles_t *)context->m_pUserData;
	const int vert_index = R_FixMikktVertIndex(vert);
	int idx = tri->indexes[face * 3 + vert_index];
	Com_Memcpy( position, tri->verts[idx].xyz, sizeof(float) * 3);
}

void mikkt_bsp_tri_GetNormal( const SMikkTSpaceContext *context, float normal[], const int face, const int vert )
{
	srfTriangles_t *tri = (srfTriangles_t *)context->m_pUserData;
	const int vert_index = R_FixMikktVertIndex(vert);
	int idx = tri->indexes[face * 3 + vert_index];
	Com_Memcpy( normal, tri->verts[idx].normal, sizeof(float) * 3);
}

void mikkt_bsp_tri_GetTexCoord( const SMikkTSpaceContext *context, float st[], const int face, const int vert )
{
	srfTriangles_t *tri = (srfTriangles_t *)context->m_pUserData;
	const int vert_index = R_FixMikktVertIndex(vert);
	int idx = tri->indexes[face * 3 + vert_index];
	Com_Memcpy( st, tri->verts[idx].st, sizeof(float) * 2 );
}

void mikkt_bsp_tri_SetTangent( const SMikkTSpaceContext *context, const float tangent[], const float sign, const int face, const int vert )
{
	srfTriangles_t *tri = (srfTriangles_t *)context->m_pUserData;
	const int vert_index = R_FixMikktVertIndex(vert);
	int idx = tri->indexes[face * 3 + vert_index];

	Com_Memcpy( tri->verts[idx].qtangent, tangent, sizeof(vec3_t) );
	tri->verts[idx].qtangent[3] = sign;
}

void vk_mikkt_bsp_tri_generate( srfTriangles_t *tri )
{
	SMikkTSpaceInterface info;
	info.m_getNumFaces			= mikkt_bsp_tri_GetNumFaces;
	info.m_getNumVerticesOfFace = mikkt_bsp_tri_GetNumVerticesOfFace;
	info.m_getPosition			= mikkt_bsp_tri_GetPosition;
	info.m_getNormal			= mikkt_bsp_tri_GetNormal;
	info.m_getTexCoord			= mikkt_bsp_tri_GetTexCoord;
	info.m_setTSpaceBasic		= mikkt_bsp_tri_SetTangent;
	info.m_setTSpace			= NULL;

	SMikkTSpaceContext context;
	context.m_pInterface = &info;
	context.m_pUserData = tri;

	genTangSpaceDefault( &context );
}

//
// BSP face
//
int mikkt_bsp_face_GetNumFaces( const SMikkTSpaceContext *context ) 
{
	srfSurfaceFace_t *cv = (srfSurfaceFace_t *)context->m_pUserData;
	return cv->numIndices / 3;
}

int mikkt_bsp_face_GetNumVerticesOfFace(const SMikkTSpaceContext *context, const int face) 
{
	return 3;
}

void mikkt_bsp_face_GetPosition( const SMikkTSpaceContext *context, float position[], const int face, const int vert )
{
	srfSurfaceFace_t *cv = (srfSurfaceFace_t *)context->m_pUserData;
	int *indices = (int *)((byte *)cv + cv->ofsIndices);
	const int vert_index = R_FixMikktVertIndex(vert);
	int idx = indices[face * 3 + vert_index];

	Com_Memcpy( position, cv->points[idx], sizeof(float) * 3); // xyz: [0, 1, 2]
}

void mikkt_bsp_face_GetNormal( const SMikkTSpaceContext *context, float normal[], const int face, const int vert )
{
	srfSurfaceFace_t *cv = (srfSurfaceFace_t *)context->m_pUserData;
	int *indices = (int *)((byte *)cv + cv->ofsIndices);
	const int vert_index = R_FixMikktVertIndex(vert);
	int idx = indices[face * 3 + vert_index];

	Com_Memcpy( normal, cv->points[idx] + 3, sizeof(float) * 3); // normal: [3, 4, 5]
}

void mikkt_bsp_face_GetTexCoord( const SMikkTSpaceContext *context, float st[], const int face, const int vert )
{
	srfSurfaceFace_t *cv = (srfSurfaceFace_t *)context->m_pUserData;
	int *indices = (int *)((byte *)cv + cv->ofsIndices);
	const int vert_index = R_FixMikktVertIndex(vert);
	int idx = indices[face * 3 + vert_index];

	Com_Memcpy( st, cv->points[idx] + 6, sizeof(float) * 2 ); // st: [6, 7]
}

void mikkt_bsp_face_SetTangent( const SMikkTSpaceContext *context, const float tangent[], const float sign, const int face, const int vert )
{
	srfSurfaceFace_t *cv = (srfSurfaceFace_t *)context->m_pUserData;
	int *indices = (int *)((byte *)cv + cv->ofsIndices);
	const int vert_index = R_FixMikktVertIndex(vert);
	int idx = indices[face * 3 + vert_index];

	float *out = cv->qtangents + idx * 4;
	Com_Memcpy( out, tangent, sizeof(float) * 3 );
	out[3] = sign;
}

void vk_mikkt_bsp_face_generate( srfSurfaceFace_t *cv )
{
	cv->qtangents = (float *)ri.Hunk_Alloc(cv->numPoints * sizeof(vec4_t), h_low);

	SMikkTSpaceInterface info;
	info.m_getNumFaces			= mikkt_bsp_face_GetNumFaces;
	info.m_getNumVerticesOfFace = mikkt_bsp_face_GetNumVerticesOfFace;
	info.m_getPosition			= mikkt_bsp_face_GetPosition;
	info.m_getNormal			= mikkt_bsp_face_GetNormal;
	info.m_getTexCoord			= mikkt_bsp_face_GetTexCoord;
	info.m_setTSpaceBasic		= mikkt_bsp_face_SetTangent;
	info.m_setTSpace			= NULL;

	SMikkTSpaceContext context;
	context.m_pInterface = &info;
	context.m_pUserData = cv;

	genTangSpaceDefault( &context );
}