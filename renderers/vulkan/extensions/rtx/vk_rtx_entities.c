/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

RTX entity BLAS: MD3 LOD0 mesh, IQM (bind-pose or CPU-skinned), glTF
(static or CPU-skinned), MDR LOD0 (CPU-skinned), with AABB proxy fallback
for unknown types / pack failures.
===========================================================================
*/

#include "tr_local.h"
#include "tr_model_gltf.h"
#include "vk_rtx_entities.h"

#ifdef USE_VULKAN_RTX

static const float s_entity_cube_verts[8][3] = {
	{ 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 },
	{ 0, 0, 1 }, { 1, 0, 1 }, { 0, 1, 1 }, { 1, 1, 1 }
};

static const uint16_t s_entity_cube_indices[36] = {
	0, 2, 1, 1, 2, 3, 4, 5, 6, 5, 7, 6, 0, 1, 4, 1, 5, 4,
	2, 6, 3, 3, 6, 7, 0, 4, 2, 2, 4, 6, 1, 3, 5, 3, 7, 5
};

static void vk_rtx_xform_local_to_world( const orientationr_t *or, const vec3_t local, float *dst )
{
	dst[0] = or->origin[0] + or->axis[0][0] * local[0] + or->axis[1][0] * local[1] + or->axis[2][0] * local[2];
	dst[1] = or->origin[1] + or->axis[0][1] * local[0] + or->axis[1][1] * local[1] + or->axis[2][1] * local[2];
	dst[2] = or->origin[2] + or->axis[0][2] * local[0] + or->axis[1][2] * local[1] + or->axis[2][2] * local[2];
}

static qboolean vk_rtx_pack_aabb( const trRefEntity_t *ent, const viewParms_t *viewParms,
	float *positions, uint32_t maxVerts, uint32_t *indices, uint32_t maxIndices,
	uint32_t *ioVertCount, uint32_t *ioIndexCount )
{
	vec3_t mins, maxs;
	float sx, sy, sz;
	uint32_t vertBase;
	uint32_t v, t;
	const float *ax0, *ax1, *ax2;
	const float *o;

	if ( *ioVertCount + 8u > maxVerts || *ioIndexCount + 36u > maxIndices ) {
		return qfalse;
	}

	R_ModelBounds( ent->e.hModel, mins, maxs );
	R_RotateForEntity( ent, viewParms, &backEnd.or );

	sx = maxs[0] - mins[0];
	sy = maxs[1] - mins[1];
	sz = maxs[2] - mins[2];
	ax0 = backEnd.or.axis[0];
	ax1 = backEnd.or.axis[1];
	ax2 = backEnd.or.axis[2];
	o = backEnd.or.origin;

	vertBase = *ioVertCount;
	for ( v = 0; v < 8; v++ ) {
		float lx = s_entity_cube_verts[v][0];
		float ly = s_entity_cube_verts[v][1];
		float lz = s_entity_cube_verts[v][2];
		float *dst = positions + ( vertBase + v ) * 3u;

		dst[0] = o[0] + ax0[0] * ( mins[0] + lx * sx ) + ax1[0] * ( mins[1] + ly * sy ) + ax2[0] * ( mins[2] + lz * sz );
		dst[1] = o[1] + ax0[1] * ( mins[0] + lx * sx ) + ax1[1] * ( mins[1] + ly * sy ) + ax2[1] * ( mins[2] + lz * sz );
		dst[2] = o[2] + ax0[2] * ( mins[0] + lx * sx ) + ax1[2] * ( mins[1] + ly * sy ) + ax2[2] * ( mins[2] + lz * sz );
	}

	for ( t = 0; t < 36; t++ ) {
		indices[*ioIndexCount + t] = vertBase + (uint32_t)s_entity_cube_indices[t];
	}

	*ioVertCount += 8u;
	*ioIndexCount += 36u;
	return qtrue;
}

static qboolean vk_rtx_pack_md3( const trRefEntity_t *ent, const viewParms_t *viewParms,
	model_t *mod, float *positions, uint32_t maxVerts, uint32_t *indices, uint32_t maxIndices,
	uint32_t *ioVertCount, uint32_t *ioIndexCount )
{
	md3Header_t *header;
	md3Surface_t *surface;
	int frame, oldframe;
	float backlerp;
	int s;
	uint32_t vertsBefore = *ioVertCount;
	uint32_t indicesBefore = *ioIndexCount;

	if ( !mod->md3[0] || mod->md3[0]->numFrames < 1 || mod->md3[0]->numSurfaces < 1 ) {
		return qfalse;
	}

	header = mod->md3[0];
	frame = ent->e.frame;
	oldframe = ent->e.oldframe;
	backlerp = ent->e.backlerp;

	if ( ent->e.renderfx & RF_WRAP_FRAMES ) {
		frame %= header->numFrames;
		oldframe %= header->numFrames;
	}
	if ( frame < 0 || frame >= header->numFrames ) {
		frame = 0;
	}
	if ( oldframe < 0 || oldframe >= header->numFrames ) {
		oldframe = 0;
	}
	if ( backlerp < 0.0f ) {
		backlerp = 0.0f;
	} else if ( backlerp > 1.0f ) {
		backlerp = 1.0f;
	}

	R_RotateForEntity( ent, viewParms, &backEnd.or );

	surface = (md3Surface_t *)( (byte *)header + header->ofsSurfaces );
	for ( s = 0; s < header->numSurfaces; s++ ) {
		const short *newXyz;
		const short *oldXyz;
		const md3Triangle_t *tri;
		float newScale = MD3_XYZ_SCALE * ( 1.0f - backlerp );
		float oldScale = MD3_XYZ_SCALE * backlerp;
		uint32_t vertBase;
		int v, t;

		if ( surface->numVerts < 1 || surface->numTriangles < 1 ) {
			surface = (md3Surface_t *)( (byte *)surface + surface->ofsEnd );
			continue;
		}

		if ( *ioVertCount + (uint32_t)surface->numVerts > maxVerts ||
			*ioIndexCount + (uint32_t)surface->numTriangles * 3u > maxIndices ) {
			*ioVertCount = vertsBefore;
			*ioIndexCount = indicesBefore;
			return qfalse;
		}

		vertBase = *ioVertCount;
		newXyz = (const short *)( (byte *)surface + surface->ofsXyzNormals )
			+ ( frame * surface->numVerts * 4 );
		oldXyz = (const short *)( (byte *)surface + surface->ofsXyzNormals )
			+ ( oldframe * surface->numVerts * 4 );

		for ( v = 0; v < surface->numVerts; v++ ) {
			vec3_t local;
			float *dst = positions + ( vertBase + (uint32_t)v ) * 3u;

			if ( backlerp == 0.0f ) {
				local[0] = newXyz[0] * newScale;
				local[1] = newXyz[1] * newScale;
				local[2] = newXyz[2] * newScale;
			} else {
				local[0] = oldXyz[0] * oldScale + newXyz[0] * newScale;
				local[1] = oldXyz[1] * oldScale + newXyz[1] * newScale;
				local[2] = oldXyz[2] * oldScale + newXyz[2] * newScale;
			}
			vk_rtx_xform_local_to_world( &backEnd.or, local, dst );
			newXyz += 4;
			oldXyz += 4;
		}

		tri = (const md3Triangle_t *)( (byte *)surface + surface->ofsTriangles );
		for ( t = 0; t < surface->numTriangles; t++ ) {
			int i0 = tri[t].indexes[0];
			int i1 = tri[t].indexes[1];
			int i2 = tri[t].indexes[2];
			uint32_t *out;

			if ( i0 < 0 || i0 >= surface->numVerts ||
				i1 < 0 || i1 >= surface->numVerts ||
				i2 < 0 || i2 >= surface->numVerts ) {
				continue;
			}
			out = indices + *ioIndexCount;
			out[0] = vertBase + (uint32_t)i0;
			out[1] = vertBase + (uint32_t)i1;
			out[2] = vertBase + (uint32_t)i2;
			*ioIndexCount += 3u;
		}

		*ioVertCount += (uint32_t)surface->numVerts;
		surface = (md3Surface_t *)( (byte *)surface + surface->ofsEnd );
	}

	if ( *ioVertCount == vertsBefore || *ioIndexCount == indicesBefore ) {
		*ioVertCount = vertsBefore;
		*ioIndexCount = indicesBefore;
		return qfalse;
	}

	return qtrue;
}

/*
===============
vk_rtx_pack_iqm

Bind-pose IQM when num_joints == 0 (or no poses). Jointed IQM CPU-skins via
R_IQMSkinPositions; returns qfalse on budget/skin failure (AABB fallback).
===============
*/
static qboolean vk_rtx_pack_iqm( const trRefEntity_t *ent, const viewParms_t *viewParms,
	model_t *mod, float *positions, uint32_t maxVerts, uint32_t *indices, uint32_t maxIndices,
	uint32_t *ioVertCount, uint32_t *ioIndexCount )
{
	iqmData_t *data;
	uint32_t vertsBefore = *ioVertCount;
	uint32_t indicesBefore = *ioIndexCount;
	uint32_t vertBase;
	int v, t, numTris, frame, oldframe;
	float backlerp;
	const int *tri;
	float *skinned = NULL;
	const float *srcPositions;
	qboolean needSkin;

	if ( !mod->modelData ) {
		return qfalse;
	}
	data = (iqmData_t *)mod->modelData;
	if ( !data->positions || !data->triangles || data->num_vertexes < 3 || data->num_triangles < 1 ) {
		return qfalse;
	}

	numTris = data->num_triangles;
	if ( *ioVertCount + (uint32_t)data->num_vertexes > maxVerts ||
		*ioIndexCount + (uint32_t)numTris * 3u > maxIndices ) {
		return qfalse;
	}

	needSkin = (qboolean)( data->num_joints > 0 && data->num_poses > 0 );
	frame = ent->e.frame;
	oldframe = ent->e.oldframe;
	backlerp = ent->e.backlerp;

	if ( needSkin ) {
		skinned = (float *)ri.Hunk_AllocateTempMemory( data->num_vertexes * 3 * (int)sizeof( float ) );
		if ( !skinned ) {
			return qfalse;
		}
		if ( !R_IQMSkinPositions( data, frame, oldframe, backlerp, skinned ) ) {
			ri.Hunk_FreeTempMemory( skinned );
			return qfalse;
		}
		srcPositions = skinned;
	} else {
		srcPositions = data->positions;
	}

	R_RotateForEntity( ent, viewParms, &backEnd.or );
	vertBase = *ioVertCount;

	for ( v = 0; v < data->num_vertexes; v++ ) {
		vec3_t local;
		float *dst = positions + ( vertBase + (uint32_t)v ) * 3u;
		const float *src = srcPositions + v * 3;

		local[0] = src[0];
		local[1] = src[1];
		local[2] = src[2];
		vk_rtx_xform_local_to_world( &backEnd.or, local, dst );
	}

	tri = data->triangles;
	for ( t = 0; t < numTris; t++ ) {
		int i0 = tri[t * 3 + 0];
		int i1 = tri[t * 3 + 1];
		int i2 = tri[t * 3 + 2];
		uint32_t *out;

		if ( i0 < 0 || i0 >= data->num_vertexes ||
			i1 < 0 || i1 >= data->num_vertexes ||
			i2 < 0 || i2 >= data->num_vertexes ) {
			continue;
		}
		out = indices + *ioIndexCount;
		out[0] = vertBase + (uint32_t)i0;
		out[1] = vertBase + (uint32_t)i1;
		out[2] = vertBase + (uint32_t)i2;
		*ioIndexCount += 3u;
	}

	*ioVertCount += (uint32_t)data->num_vertexes;

	if ( skinned ) {
		ri.Hunk_FreeTempMemory( skinned );
	}

	if ( *ioIndexCount == indicesBefore ) {
		*ioVertCount = vertsBefore;
		*ioIndexCount = indicesBefore;
		return qfalse;
	}

	return qtrue;
}

/*
===============
vk_rtx_pack_gltf

Static glTF (no joints) or CPU-skinned glTF via R_GLTFSkinPositions.
Returns qfalse on budget/skin failure (AABB fallback). Morph omitted.
===============
*/
static qboolean vk_rtx_pack_gltf( const trRefEntity_t *ent, const viewParms_t *viewParms,
	model_t *mod, int refdefTimeMs, float *positions, uint32_t maxVerts, uint32_t *indices, uint32_t maxIndices,
	uint32_t *ioVertCount, uint32_t *ioIndexCount )
{
	const gltfModel_t *gltf;
	uint32_t vertsBefore = *ioVertCount;
	uint32_t indicesBefore = *ioIndexCount;
	int mi, pi;
	float jointMats[GLTF_MAX_JOINTS * 12];
	qboolean needSkin;

	gltf = R_GetGLTFModelFromModelData( mod->modelData );
	if ( !gltf || gltf->numMeshes < 1 ) {
		return qfalse;
	}

	needSkin = (qboolean)( gltf->skeleton.numJoints > 0 );
	if ( needSkin ) {
		if ( !R_GLTFComputeEntityJointMatrices( gltf, ent, refdefTimeMs, jointMats ) ) {
			return qfalse;
		}
	}

	R_RotateForEntity( ent, viewParms, &backEnd.or );

	for ( mi = 0; mi < gltf->numMeshes; mi++ ) {
		const gltfMesh_t *mesh = &gltf->meshes[mi];
		if ( !mesh->primitives || mesh->numPrimitives < 1 ) {
			continue;
		}
		for ( pi = 0; pi < mesh->numPrimitives; pi++ ) {
			const gltfPrimitive_t *prim = &mesh->primitives[pi];
			uint32_t vertBase;
			int v, t, numTris;
			const uint32_t *idx;
			float *skinned = NULL;
			const float *srcPositions = NULL;

			if ( !prim->vertices || prim->numVertices < 3 || !prim->indices || prim->numIndices < 3 ) {
				continue;
			}
			numTris = prim->numIndices / 3;
			if ( numTris < 1 ) {
				continue;
			}
			if ( *ioVertCount + (uint32_t)prim->numVertices > maxVerts ||
				*ioIndexCount + (uint32_t)numTris * 3u > maxIndices ) {
				*ioVertCount = vertsBefore;
				*ioIndexCount = indicesBefore;
				return qfalse;
			}

			if ( needSkin ) {
				skinned = (float *)ri.Hunk_AllocateTempMemory( prim->numVertices * 3 * (int)sizeof( float ) );
				if ( !skinned ) {
					*ioVertCount = vertsBefore;
					*ioIndexCount = indicesBefore;
					return qfalse;
				}
				if ( !R_GLTFSkinPositions( gltf, prim->vertices, prim->numVertices, jointMats, skinned ) ) {
					ri.Hunk_FreeTempMemory( skinned );
					*ioVertCount = vertsBefore;
					*ioIndexCount = indicesBefore;
					return qfalse;
				}
				srcPositions = skinned;
			}

			vertBase = *ioVertCount;
			for ( v = 0; v < prim->numVertices; v++ ) {
				vec3_t local;
				float *dst = positions + ( vertBase + (uint32_t)v ) * 3u;

				if ( srcPositions ) {
					local[0] = srcPositions[v * 3 + 0];
					local[1] = srcPositions[v * 3 + 1];
					local[2] = srcPositions[v * 3 + 2];
				} else {
					VectorCopy( prim->vertices[v].position, local );
				}
				vk_rtx_xform_local_to_world( &backEnd.or, local, dst );
			}

			if ( skinned ) {
				ri.Hunk_FreeTempMemory( skinned );
			}

			idx = prim->indices;
			for ( t = 0; t < numTris; t++ ) {
				uint32_t i0 = idx[t * 3 + 0];
				uint32_t i1 = idx[t * 3 + 1];
				uint32_t i2 = idx[t * 3 + 2];
				uint32_t *out;

				if ( i0 >= (uint32_t)prim->numVertices ||
					i1 >= (uint32_t)prim->numVertices ||
					i2 >= (uint32_t)prim->numVertices ) {
					continue;
				}
				out = indices + *ioIndexCount;
				out[0] = vertBase + i0;
				out[1] = vertBase + i1;
				out[2] = vertBase + i2;
				*ioIndexCount += 3u;
			}

			*ioVertCount += (uint32_t)prim->numVertices;
		}
	}

	if ( *ioVertCount == vertsBefore || *ioIndexCount == indicesBefore ) {
		*ioVertCount = vertsBefore;
		*ioIndexCount = indicesBefore;
		return qfalse;
	}

	return qtrue;
}

/*
===============
vk_rtx_pack_mdr

MDR LOD0: CPU-skin each surface via R_MDRSkinSurfacePositions.
Returns qfalse on budget/skin failure (AABB fallback).
===============
*/
static qboolean vk_rtx_pack_mdr( const trRefEntity_t *ent, const viewParms_t *viewParms,
	model_t *mod, float *positions, uint32_t maxVerts, uint32_t *indices, uint32_t maxIndices,
	uint32_t *ioVertCount, uint32_t *ioIndexCount )
{
	mdrHeader_t *header;
	mdrLOD_t *lod;
	mdrSurface_t *surface;
	uint32_t vertsBefore = *ioVertCount;
	uint32_t indicesBefore = *ioIndexCount;
	int frame, oldframe;
	float backlerp;
	int s;
	float *skinned = NULL;

	if ( !mod->modelData ) {
		return qfalse;
	}
	header = (mdrHeader_t *)mod->modelData;
	if ( header->numFrames < 1 || header->numBones < 1 || header->numLODs < 1 || header->numBones > MDR_MAX_BONES ) {
		return qfalse;
	}

	frame = ent->e.frame;
	oldframe = ent->e.oldframe;
	backlerp = ent->e.backlerp;
	if ( ent->e.renderfx & RF_WRAP_FRAMES ) {
		frame %= header->numFrames;
		oldframe %= header->numFrames;
	}
	if ( frame < 0 || frame >= header->numFrames ) {
		frame = 0;
	}
	if ( oldframe < 0 || oldframe >= header->numFrames ) {
		oldframe = 0;
	}
	if ( backlerp < 0.0f ) {
		backlerp = 0.0f;
	} else if ( backlerp > 1.0f ) {
		backlerp = 1.0f;
	}

	lod = (mdrLOD_t *)( (byte *)header + header->ofsLODs );
	if ( lod->numSurfaces < 1 ) {
		return qfalse;
	}

	R_RotateForEntity( ent, viewParms, &backEnd.or );

	surface = (mdrSurface_t *)( (byte *)lod + lod->ofsSurfaces );
	for ( s = 0; s < lod->numSurfaces; s++ ) {
		uint32_t vertBase;
		int v, t, numTris;
		const int *tri;

		if ( surface->numVerts < 3 || surface->numTriangles < 1 ) {
			surface = (mdrSurface_t *)( (byte *)surface + surface->ofsEnd );
			continue;
		}
		numTris = surface->numTriangles;
		if ( *ioVertCount + (uint32_t)surface->numVerts > maxVerts ||
			*ioIndexCount + (uint32_t)numTris * 3u > maxIndices ) {
			*ioVertCount = vertsBefore;
			*ioIndexCount = indicesBefore;
			return qfalse;
		}

		skinned = (float *)ri.Hunk_AllocateTempMemory( surface->numVerts * 3 * (int)sizeof( float ) );
		if ( !skinned ) {
			*ioVertCount = vertsBefore;
			*ioIndexCount = indicesBefore;
			return qfalse;
		}
		if ( !R_MDRSkinSurfacePositions( surface, frame, oldframe, backlerp, skinned ) ) {
			ri.Hunk_FreeTempMemory( skinned );
			*ioVertCount = vertsBefore;
			*ioIndexCount = indicesBefore;
			return qfalse;
		}

		vertBase = *ioVertCount;
		for ( v = 0; v < surface->numVerts; v++ ) {
			vec3_t local;
			float *dst = positions + ( vertBase + (uint32_t)v ) * 3u;
			local[0] = skinned[v * 3 + 0];
			local[1] = skinned[v * 3 + 1];
			local[2] = skinned[v * 3 + 2];
			vk_rtx_xform_local_to_world( &backEnd.or, local, dst );
		}
		ri.Hunk_FreeTempMemory( skinned );
		skinned = NULL;

		tri = (const int *)( (byte *)surface + surface->ofsTriangles );
		for ( t = 0; t < numTris; t++ ) {
			int i0 = tri[t * 3 + 0];
			int i1 = tri[t * 3 + 1];
			int i2 = tri[t * 3 + 2];
			uint32_t *out;

			if ( i0 < 0 || i0 >= surface->numVerts ||
				i1 < 0 || i1 >= surface->numVerts ||
				i2 < 0 || i2 >= surface->numVerts ) {
				continue;
			}
			out = indices + *ioIndexCount;
			out[0] = vertBase + (uint32_t)i0;
			out[1] = vertBase + (uint32_t)i1;
			out[2] = vertBase + (uint32_t)i2;
			*ioIndexCount += 3u;
		}

		*ioVertCount += (uint32_t)surface->numVerts;
		surface = (mdrSurface_t *)( (byte *)surface + surface->ofsEnd );
	}

	if ( *ioVertCount == vertsBefore || *ioIndexCount == indicesBefore ) {
		*ioVertCount = vertsBefore;
		*ioIndexCount = indicesBefore;
		return qfalse;
	}

	return qtrue;
}

static void vk_rtx_entity_geo_normal( const float *a, const float *b, const float *c, float *out )
{
	vec3_t e1, e2, n;

	VectorSubtract( b, a, e1 );
	VectorSubtract( c, a, e2 );
	CrossProduct( e1, e2, n );
	if ( VectorNormalize( n ) == 0.0f ) {
		out[0] = 0.0f;
		out[1] = 0.0f;
		out[2] = 1.0f;
	} else {
		VectorCopy( n, out );
	}
}

static void vk_rtx_entity_fill_prim_attrs( const float *positions, const uint32_t *indices,
	uint32_t primBegin, uint32_t primEnd, const byte *rgba,
	float *albedoRgb, float *normalNxyz )
{
	uint32_t p;
	float ar = 0.72f, ag = 0.70f, ab = 0.66f;

	if ( !albedoRgb || !normalNxyz || !positions || !indices || primEnd <= primBegin ) {
		return;
	}
	if ( rgba && ( rgba[0] | rgba[1] | rgba[2] ) != 0 ) {
		ar = rgba[0] * ( 1.0f / 255.0f );
		ag = rgba[1] * ( 1.0f / 255.0f );
		ab = rgba[2] * ( 1.0f / 255.0f );
	}

	for ( p = primBegin; p < primEnd; p++ ) {
		uint32_t i0 = indices[p * 3u + 0u];
		uint32_t i1 = indices[p * 3u + 1u];
		uint32_t i2 = indices[p * 3u + 2u];
		const float *a = positions + i0 * 3u;
		const float *b = positions + i1 * 3u;
		const float *c = positions + i2 * 3u;
		float n[3];

		albedoRgb[p * 3u + 0u] = ar;
		albedoRgb[p * 3u + 1u] = ag;
		albedoRgb[p * 3u + 2u] = ab;
		vk_rtx_entity_geo_normal( a, b, c, n );
		normalNxyz[p * 3u + 0u] = n[0];
		normalNxyz[p * 3u + 1u] = n[1];
		normalNxyz[p * 3u + 2u] = n[2];
	}
}

uint32_t vk_rtx_entities_pack( const trRefdef_t *refdef, const viewParms_t *viewParms,
	uint32_t maxEntities, float *positions, uint32_t maxVerts,
	uint32_t *indices, uint32_t maxIndices,
	float *albedoRgb, float *normalNxyz, vkRtxEntityPackStats_t *stats )
{
	int i, n;
	uint32_t packed = 0u;
	uint32_t vertCount = 0u;
	uint32_t indexCount = 0u;
	uint32_t meshCount = 0u;
	uint32_t meshMd3 = 0u;
	uint32_t meshIqm = 0u;
	uint32_t meshGltf = 0u;
	uint32_t meshMdr = 0u;
	uint32_t proxyCount = 0u;
	uint32_t proxyNonMesh = 0u;
	uint32_t proxySkinned = 0u;
	uint32_t proxyMd3Fail = 0u;
	uint32_t proxyIqmFail = 0u;
	uint32_t proxyGltfFail = 0u;
	uint32_t proxyMdrFail = 0u;

	if ( stats ) {
		Com_Memset( stats, 0, sizeof( *stats ) );
	}

	if ( !refdef || !viewParms || !positions || !indices || maxEntities == 0u ||
		maxVerts < 8u || maxIndices < 36u ) {
		return 0u;
	}

	n = refdef->num_entities;
	if ( n <= 0 ) {
		return 0u;
	}

	for ( i = 0; i < n && packed < maxEntities; i++ ) {
		const trRefEntity_t *ent = &refdef->entities[i];
		model_t *mod;
		qboolean usedMesh = qfalse;
		int meshKind = 0; /* 1=md3 2=iqm 3=gltf 4=mdr */
		int triedFailKind = 0; /* 1=md3 2=iqm 3=gltf 4=mdr */
		uint32_t indexBefore = indexCount;

		if ( ent->e.reType != RT_MODEL || !ent->e.hModel ) {
			continue;
		}

		mod = R_GetModelByHandle( ent->e.hModel );
		if ( !mod || mod->type == MOD_BAD ) {
			continue;
		}

		if ( mod->type == MOD_MESH ) {
			usedMesh = vk_rtx_pack_md3( ent, viewParms, mod, positions, maxVerts, indices, maxIndices,
				&vertCount, &indexCount );
			if ( usedMesh ) {
				meshKind = 1;
			} else {
				triedFailKind = 1;
			}
		} else if ( mod->type == MOD_IQM ) {
			usedMesh = vk_rtx_pack_iqm( ent, viewParms, mod, positions, maxVerts, indices, maxIndices,
				&vertCount, &indexCount );
			if ( usedMesh ) {
				meshKind = 2;
			} else {
				triedFailKind = 2;
			}
		} else if ( mod->type == MOD_GLTF ) {
			usedMesh = vk_rtx_pack_gltf( ent, viewParms, mod, refdef->time, positions, maxVerts, indices, maxIndices,
				&vertCount, &indexCount );
			if ( usedMesh ) {
				meshKind = 3;
			} else {
				triedFailKind = 3;
			}
		} else if ( mod->type == MOD_MDR ) {
			usedMesh = vk_rtx_pack_mdr( ent, viewParms, mod, positions, maxVerts, indices, maxIndices,
				&vertCount, &indexCount );
			if ( usedMesh ) {
				meshKind = 4;
			} else {
				triedFailKind = 4;
			}
		}

		if ( !usedMesh ) {
			if ( !vk_rtx_pack_aabb( ent, viewParms, positions, maxVerts, indices, maxIndices,
					&vertCount, &indexCount ) ) {
				break;
			}
			proxyCount++;
			if ( triedFailKind == 1 ) {
				proxyMd3Fail++;
			} else if ( triedFailKind == 2 ) {
				proxyIqmFail++;
			} else if ( triedFailKind == 3 ) {
				proxyGltfFail++;
			} else if ( triedFailKind == 4 ) {
				proxyMdrFail++;
			} else {
				proxyNonMesh++;
			}
		} else {
			meshCount++;
			if ( meshKind == 1 ) {
				meshMd3++;
			} else if ( meshKind == 2 ) {
				meshIqm++;
			} else if ( meshKind == 3 ) {
				meshGltf++;
			} else if ( meshKind == 4 ) {
				meshMdr++;
			}
		}

		if ( ( albedoRgb || normalNxyz ) && indexCount > indexBefore ) {
			vk_rtx_entity_fill_prim_attrs( positions, indices,
				indexBefore / 3u, indexCount / 3u, ent->e.shader.rgba,
				albedoRgb, normalNxyz );
		}

		packed++;
	}

	if ( stats ) {
		stats->entityCount = packed;
		stats->vertexCount = vertCount;
		stats->primitiveCount = indexCount / 3u;
		stats->meshEntityCount = meshCount;
		stats->meshMd3Count = meshMd3;
		stats->meshIqmCount = meshIqm;
		stats->meshGltfCount = meshGltf;
		stats->meshMdrCount = meshMdr;
		stats->proxyEntityCount = proxyCount;
		stats->proxyNonMeshCount = proxyNonMesh;
		stats->proxySkinnedCount = proxySkinned;
		stats->proxyMd3FailCount = proxyMd3Fail;
		stats->proxyIqmFailCount = proxyIqmFail;
		stats->proxyGltfFailCount = proxyGltfFail;
		stats->proxyMdrFailCount = proxyMdrFail;
	}

	return packed;
}

#else /* !USE_VULKAN_RTX */

uint32_t vk_rtx_entities_pack( const trRefdef_t *refdef, const viewParms_t *viewParms,
	uint32_t maxEntities, float *positions, uint32_t maxVerts,
	uint32_t *indices, uint32_t maxIndices,
	float *albedoRgb, float *normalNxyz, vkRtxEntityPackStats_t *stats )
{
	(void)refdef;
	(void)viewParms;
	(void)maxEntities;
	(void)positions;
	(void)maxVerts;
	(void)indices;
	(void)maxIndices;
	(void)albedoRgb;
	(void)normalNxyz;
	if ( stats ) {
		Com_Memset( stats, 0, sizeof( *stats ) );
	}
	return 0u;
}

#endif /* USE_VULKAN_RTX */
