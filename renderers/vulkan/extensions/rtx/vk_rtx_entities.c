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
#include "vk_rtx_material.h"

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
	uint32_t *ioVertCount, uint32_t *ioIndexCount, qboolean *outCpuSkinned )
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

	if ( outCpuSkinned ) {
		*outCpuSkinned = qfalse;
	}
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

	if ( outCpuSkinned ) {
		*outCpuSkinned = needSkin;
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
	uint32_t *ioVertCount, uint32_t *ioIndexCount, qboolean *outCpuSkinned )
{
	const gltfModel_t *gltf;
	uint32_t vertsBefore = *ioVertCount;
	uint32_t indicesBefore = *ioIndexCount;
	int mi, pi;
	float jointMats[GLTF_MAX_JOINTS * 12];
	qboolean needSkin;

	if ( outCpuSkinned ) {
		*outCpuSkinned = qfalse;
	}
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

	if ( outCpuSkinned ) {
		*outCpuSkinned = needSkin;
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
	uint32_t *ioVertCount, uint32_t *ioIndexCount, qboolean *outCpuSkinned )
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

	if ( outCpuSkinned ) {
		*outCpuSkinned = qfalse;
	}
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

	if ( outCpuSkinned ) {
		*outCpuSkinned = qtrue; /* MDR path always CPU-skins LOD0 surfaces. */
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

static void vk_rtx_entity_fill_prim_attrs_rgb( const float *positions, const uint32_t *indices,
	uint32_t primBegin, uint32_t primEnd, const float rgb[3],
	float *albedoRgb, float *normalNxyz )
{
	uint32_t p;
	float ar = 0.72f, ag = 0.70f, ab = 0.66f;

	if ( !albedoRgb || !normalNxyz || !positions || !indices || primEnd <= primBegin ) {
		return;
	}
	if ( rgb ) {
		ar = rgb[0];
		ag = rgb[1];
		ab = rgb[2];
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

static void vk_rtx_entity_tint_rgb( const byte *rgba, float out[3] )
{
	vk_rtx_material_tint_rgb( rgba, out );
}

static shader_t *vk_rtx_entity_shader_for_surface( const trRefEntity_t *ent, const char *surfName,
	shader_t *fallback )
{
	if ( !ent ) {
		return fallback ? fallback : tr.defaultShader;
	}
	if ( ent->e.customShader ) {
		return R_GetShaderByHandle( ent->e.customShader );
	}
	if ( ent->e.customSkin > 0 && ent->e.customSkin < tr.numSkins && surfName && surfName[0] ) {
		const skin_t *skin = R_GetSkinByHandle( ent->e.customSkin );
		int j;

		if ( skin ) {
			for ( j = 0; j < skin->numSurfaces; j++ ) {
				if ( !strcmp( skin->surfaces[j].name, surfName ) ) {
					return skin->surfaces[j].shader;
				}
			}
		}
		return tr.defaultShader;
	}
	return fallback ? fallback : tr.defaultShader;
}

static void vk_rtx_entity_albedo_from_shader( const shader_t *shader, const byte *tintRgba,
	qboolean useMaterials, float out[3] )
{
	float fallback[3];

	vk_rtx_material_tint_rgb( tintRgba, fallback );
	vk_rtx_material_resolve_albedo( shader, useMaterials, qfalse, 0.0f, 0.0f,
		fallback, tintRgba, out );
}

static void vk_rtx_entity_write_prim( float *albedoRgb, float *normalNxyz, uint32_t prim,
	const float *a, const float *b, const float *c, const float rgb[3] )
{
	float n[3];

	if ( albedoRgb ) {
		albedoRgb[prim * 3u + 0u] = rgb[0];
		albedoRgb[prim * 3u + 1u] = rgb[1];
		albedoRgb[prim * 3u + 2u] = rgb[2];
	}
	if ( normalNxyz ) {
		vk_rtx_entity_geo_normal( a, b, c, n );
		normalNxyz[prim * 3u + 0u] = n[0];
		normalNxyz[prim * 3u + 1u] = n[1];
		normalNxyz[prim * 3u + 2u] = n[2];
	}
}

static qboolean vk_rtx_entity_uv_enabled( void )
{
	return ( r_rtxEntityUvSample && r_rtxEntityUvSample->integer
		&& r_rtxEntityMaterials && r_rtxEntityMaterials->integer ) ? qtrue : qfalse;
}

static void vk_rtx_entity_prim_rgb( const shader_t *shader, const byte *tintRgba,
	qboolean useMaterials, qboolean useUv, float u, float v, float out[3] )
{
	float fallback[3];

	vk_rtx_material_tint_rgb( tintRgba, fallback );
	vk_rtx_material_resolve_albedo( shader, useMaterials, useUv, u, v,
		fallback, tintRgba, out );
}

static void vk_rtx_entity_fill_md3_materials( const trRefEntity_t *ent, model_t *mod,
	const float *positions, const uint32_t *indices, uint32_t indexBefore, uint32_t indexAfter,
	float *albedoRgb, float *normalNxyz, qboolean useMaterials )
{
	md3Header_t *header;
	md3Surface_t *surface;
	int frame, oldframe, s;
	uint32_t prim = indexBefore / 3u;
	qboolean useUv = ( useMaterials && vk_rtx_entity_uv_enabled() ) ? qtrue : qfalse;

	if ( !mod->md3[0] || indexAfter <= indexBefore ) {
		return;
	}
	header = mod->md3[0];
	frame = ent->e.frame;
	oldframe = ent->e.oldframe;
	if ( ent->e.renderfx & RF_WRAP_FRAMES ) {
		frame %= header->numFrames;
		oldframe %= header->numFrames;
	}
	if ( frame < 0 || frame >= header->numFrames ) {
		frame = 0;
	}
	(void)oldframe;

	surface = (md3Surface_t *)( (byte *)header + header->ofsSurfaces );
	for ( s = 0; s < header->numSurfaces; s++ ) {
		const md3Triangle_t *tri;
		const md3St_t *st;
		shader_t *shader;
		md3Shader_t *md3Shader;
		float rgb[3];
		int t;

		if ( surface->numVerts < 1 || surface->numTriangles < 1 ) {
			surface = (md3Surface_t *)( (byte *)surface + surface->ofsEnd );
			continue;
		}

		if ( ent->e.customShader || ( ent->e.customSkin > 0 && ent->e.customSkin < tr.numSkins ) ) {
			shader = vk_rtx_entity_shader_for_surface( ent, surface->name, tr.defaultShader );
		} else if ( surface->numShaders <= 0 ) {
			shader = tr.defaultShader;
		} else {
			md3Shader = (md3Shader_t *)( (byte *)surface + surface->ofsShaders );
			md3Shader += ent->e.skinNum % surface->numShaders;
			shader = tr.shaders[ md3Shader->shaderIndex ];
		}

		tri = (const md3Triangle_t *)( (byte *)surface + surface->ofsTriangles );
		st = (const md3St_t *)( (byte *)surface + surface->ofsSt );
		vk_rtx_entity_albedo_from_shader( shader, ent->e.shader.rgba, useMaterials, rgb );

		for ( t = 0; t < surface->numTriangles; t++ ) {
			int i0 = tri[t].indexes[0];
			int i1 = tri[t].indexes[1];
			int i2 = tri[t].indexes[2];
			float u, v, primRgb[3];
			const float *pa, *pb, *pc;

			if ( i0 < 0 || i0 >= surface->numVerts ||
				i1 < 0 || i1 >= surface->numVerts ||
				i2 < 0 || i2 >= surface->numVerts ) {
				continue;
			}
			if ( prim * 3u >= indexAfter ) {
				break;
			}
			pa = positions + indices[prim * 3u + 0u] * 3u;
			pb = positions + indices[prim * 3u + 1u] * 3u;
			pc = positions + indices[prim * 3u + 2u] * 3u;
			u = ( st[i0].st[0] + st[i1].st[0] + st[i2].st[0] ) * ( 1.0f / 3.0f );
			v = ( st[i0].st[1] + st[i1].st[1] + st[i2].st[1] ) * ( 1.0f / 3.0f );
			primRgb[0] = rgb[0];
			primRgb[1] = rgb[1];
			primRgb[2] = rgb[2];
			vk_rtx_entity_prim_rgb( shader, ent->e.shader.rgba, useMaterials, useUv, u, v, primRgb );
			vk_rtx_entity_write_prim( albedoRgb, normalNxyz, prim, pa, pb, pc, primRgb );
			prim++;
		}

		surface = (md3Surface_t *)( (byte *)surface + surface->ofsEnd );
	}
}

static void vk_rtx_entity_fill_iqm_materials( const trRefEntity_t *ent, model_t *mod,
	const float *positions, const uint32_t *indices, uint32_t indexBefore, uint32_t indexAfter,
	float *albedoRgb, float *normalNxyz, qboolean useMaterials )
{
	iqmData_t *data;
	srfIQModel_t *surface;
	int s;
	uint32_t prim = indexBefore / 3u;
	qboolean useUv = ( useMaterials && vk_rtx_entity_uv_enabled() ) ? qtrue : qfalse;

	if ( !mod->modelData || indexAfter <= indexBefore ) {
		return;
	}
	data = (iqmData_t *)mod->modelData;
	if ( !data->surfaces || data->num_surfaces < 1 ) {
		return;
	}

	surface = data->surfaces;
	for ( s = 0; s < data->num_surfaces; s++, surface++ ) {
		shader_t *shader;
		float rgb[3];
		int t;

		shader = vk_rtx_entity_shader_for_surface( ent, surface->name, surface->shader );
		vk_rtx_entity_albedo_from_shader( shader, ent->e.shader.rgba, useMaterials, rgb );

		for ( t = 0; t < surface->num_triangles; t++ ) {
			int tri = surface->first_triangle + t;
			int i0, i1, i2;
			float u = 0.5f, v = 0.5f, primRgb[3];
			const float *pa, *pb, *pc;

			if ( tri < 0 || tri >= data->num_triangles ) {
				continue;
			}
			i0 = data->triangles[tri * 3 + 0];
			i1 = data->triangles[tri * 3 + 1];
			i2 = data->triangles[tri * 3 + 2];
			if ( i0 < 0 || i0 >= data->num_vertexes ||
				i1 < 0 || i1 >= data->num_vertexes ||
				i2 < 0 || i2 >= data->num_vertexes ) {
				continue;
			}
			if ( prim * 3u >= indexAfter ) {
				break;
			}
			pa = positions + indices[prim * 3u + 0u] * 3u;
			pb = positions + indices[prim * 3u + 1u] * 3u;
			pc = positions + indices[prim * 3u + 2u] * 3u;
			if ( data->texcoords ) {
				u = ( data->texcoords[i0 * 2 + 0] + data->texcoords[i1 * 2 + 0] + data->texcoords[i2 * 2 + 0] ) * ( 1.0f / 3.0f );
				v = ( data->texcoords[i0 * 2 + 1] + data->texcoords[i1 * 2 + 1] + data->texcoords[i2 * 2 + 1] ) * ( 1.0f / 3.0f );
			}
			primRgb[0] = rgb[0];
			primRgb[1] = rgb[1];
			primRgb[2] = rgb[2];
			vk_rtx_entity_prim_rgb( shader, ent->e.shader.rgba, useMaterials, useUv, u, v, primRgb );
			vk_rtx_entity_write_prim( albedoRgb, normalNxyz, prim, pa, pb, pc, primRgb );
			prim++;
		}
	}
}

static void vk_rtx_entity_fill_mdr_materials( const trRefEntity_t *ent, model_t *mod,
	const float *positions, const uint32_t *indices, uint32_t indexBefore, uint32_t indexAfter,
	float *albedoRgb, float *normalNxyz, qboolean useMaterials )
{
	mdrHeader_t *header;
	mdrLOD_t *lod;
	mdrSurface_t *surface;
	int s;
	uint32_t prim = indexBefore / 3u;
	qboolean useUv = ( useMaterials && vk_rtx_entity_uv_enabled() ) ? qtrue : qfalse;

	if ( !mod->modelData || indexAfter <= indexBefore ) {
		return;
	}
	header = (mdrHeader_t *)mod->modelData;
	if ( header->numLODs < 1 ) {
		return;
	}
	lod = (mdrLOD_t *)( (byte *)header + header->ofsLODs );
	surface = (mdrSurface_t *)( (byte *)lod + lod->ofsSurfaces );

	for ( s = 0; s < lod->numSurfaces; s++ ) {
		shader_t *shader;
		float rgb[3];
		int t, numTris;
		const int *tri;

		if ( surface->numVerts < 3 || surface->numTriangles < 1 ) {
			surface = (mdrSurface_t *)( (byte *)surface + surface->ofsEnd );
			continue;
		}
		numTris = surface->numTriangles;
		shader = vk_rtx_entity_shader_for_surface( ent, surface->name, tr.defaultShader );
		tri = (const int *)( (byte *)surface + surface->ofsTriangles );
		vk_rtx_entity_albedo_from_shader( shader, ent->e.shader.rgba, useMaterials, rgb );

		/* Build UV table — MDR verts are variable-sized. */
		{
			float *uvTab = (float *)ri.Hunk_AllocateTempMemory( surface->numVerts * 2 * (int)sizeof( float ) );
			const mdrVertex_t *v;
			int vi;

			if ( !uvTab ) {
				surface = (mdrSurface_t *)( (byte *)surface + surface->ofsEnd );
				continue;
			}
			v = (const mdrVertex_t *)( (byte *)surface + surface->ofsVerts );
			for ( vi = 0; vi < surface->numVerts; vi++ ) {
				uvTab[vi * 2 + 0] = v->texCoords[0];
				uvTab[vi * 2 + 1] = v->texCoords[1];
				v = (const mdrVertex_t *)&v->weights[v->numWeights];
			}

			for ( t = 0; t < numTris; t++ ) {
				int i0 = tri[t * 3 + 0];
				int i1 = tri[t * 3 + 1];
				int i2 = tri[t * 3 + 2];
				float u, vuv, primRgb[3];
				const float *pa, *pb, *pc;

				if ( i0 < 0 || i0 >= surface->numVerts ||
					i1 < 0 || i1 >= surface->numVerts ||
					i2 < 0 || i2 >= surface->numVerts ) {
					continue;
				}
				if ( prim * 3u >= indexAfter ) {
					break;
				}
				pa = positions + indices[prim * 3u + 0u] * 3u;
				pb = positions + indices[prim * 3u + 1u] * 3u;
				pc = positions + indices[prim * 3u + 2u] * 3u;
				u = ( uvTab[i0 * 2 + 0] + uvTab[i1 * 2 + 0] + uvTab[i2 * 2 + 0] ) * ( 1.0f / 3.0f );
				vuv = ( uvTab[i0 * 2 + 1] + uvTab[i1 * 2 + 1] + uvTab[i2 * 2 + 1] ) * ( 1.0f / 3.0f );
				primRgb[0] = rgb[0];
				primRgb[1] = rgb[1];
				primRgb[2] = rgb[2];
				vk_rtx_entity_prim_rgb( shader, ent->e.shader.rgba, useMaterials, useUv, u, vuv, primRgb );
				vk_rtx_entity_write_prim( albedoRgb, normalNxyz, prim, pa, pb, pc, primRgb );
				prim++;
			}
			ri.Hunk_FreeTempMemory( uvTab );
		}
		surface = (mdrSurface_t *)( (byte *)surface + surface->ofsEnd );
	}
}

static void vk_rtx_entity_fill_gltf_materials( const trRefEntity_t *ent, model_t *mod,
	const float *positions, const uint32_t *indices, uint32_t indexBefore, uint32_t indexAfter,
	float *albedoRgb, float *normalNxyz, qboolean useMaterials )
{
	const gltfModel_t *gltf;
	int mi, pi;
	uint32_t prim = indexBefore / 3u;
	shader_t *custom = NULL;
	qboolean useUv = ( useMaterials && vk_rtx_entity_uv_enabled() ) ? qtrue : qfalse;

	gltf = R_GetGLTFModelFromModelData( mod->modelData );
	if ( !gltf || indexAfter <= indexBefore ) {
		return;
	}
	if ( ent->e.customShader ) {
		custom = R_GetShaderByHandle( ent->e.customShader );
	}

	for ( mi = 0; mi < gltf->numMeshes; mi++ ) {
		const gltfMesh_t *mesh = &gltf->meshes[mi];
		if ( !mesh->primitives || mesh->numPrimitives < 1 ) {
			continue;
		}
		for ( pi = 0; pi < mesh->numPrimitives; pi++ ) {
			const gltfPrimitive_t *gprim = &mesh->primitives[pi];
			float rgb[3];
			int t, numTris;
			const uint32_t *idx;
			image_t *baseImg = NULL;

			if ( !gprim->vertices || gprim->numVertices < 3 || !gprim->indices || gprim->numIndices < 3 ) {
				continue;
			}
			numTris = gprim->numIndices / 3;
			idx = gprim->indices;

			if ( custom ) {
				vk_rtx_entity_albedo_from_shader( custom, ent->e.shader.rgba, useMaterials, rgb );
			} else if ( useMaterials && gprim->materialIndex >= 0 && gprim->materialIndex < gltf->numMaterials ) {
				const gltfMaterial_t *mat = &gltf->materials[gprim->materialIndex];
				float tint[3];

				vk_rtx_entity_tint_rgb( ent->e.shader.rgba, tint );
				rgb[0] = mat->baseColorFactor[0];
				rgb[1] = mat->baseColorFactor[1];
				rgb[2] = mat->baseColorFactor[2];
				if ( mat->baseColorTexture[0] ) {
					baseImg = R_FindImageFile( mat->baseColorTexture,
						IMGFLAG_CLAMPTOEDGE | IMGFLAG_NOLIGHTSCALE, 0 );
					/* UV path samples thumbs per-prim; fall back to average when no thumb. */
					if ( baseImg && baseImg != tr.defaultImage
						&& !( useUv && baseImg->hasThumb ) ) {
						rgb[0] *= baseImg->avgColor[0];
						rgb[1] *= baseImg->avgColor[1];
						rgb[2] *= baseImg->avgColor[2];
					}
				}
				if ( ( ent->e.shader.rgba[0] | ent->e.shader.rgba[1] | ent->e.shader.rgba[2] ) != 0 ) {
					rgb[0] *= tint[0];
					rgb[1] *= tint[1];
					rgb[2] *= tint[2];
				}
			} else {
				vk_rtx_entity_tint_rgb( ent->e.shader.rgba, rgb );
			}

			for ( t = 0; t < numTris; t++ ) {
				uint32_t i0 = idx[t * 3 + 0];
				uint32_t i1 = idx[t * 3 + 1];
				uint32_t i2 = idx[t * 3 + 2];
				float u, v, primRgb[3];
				const float *pa, *pb, *pc;

				if ( i0 >= (uint32_t)gprim->numVertices ||
					i1 >= (uint32_t)gprim->numVertices ||
					i2 >= (uint32_t)gprim->numVertices ) {
					continue;
				}
				if ( prim * 3u >= indexAfter ) {
					break;
				}
				pa = positions + indices[prim * 3u + 0u] * 3u;
				pb = positions + indices[prim * 3u + 1u] * 3u;
				pc = positions + indices[prim * 3u + 2u] * 3u;
				u = ( gprim->vertices[i0].texCoord0[0] + gprim->vertices[i1].texCoord0[0]
					+ gprim->vertices[i2].texCoord0[0] ) * ( 1.0f / 3.0f );
				v = ( gprim->vertices[i0].texCoord0[1] + gprim->vertices[i1].texCoord0[1]
					+ gprim->vertices[i2].texCoord0[1] ) * ( 1.0f / 3.0f );
				primRgb[0] = rgb[0];
				primRgb[1] = rgb[1];
				primRgb[2] = rgb[2];
				if ( useUv && baseImg && baseImg->hasThumb ) {
					float sampled[3];
					float tint[3];
					vk_rtx_material_sample_thumb_uv( baseImg, u, v, sampled );
					primRgb[0] = sampled[0] * ( ( custom || !useMaterials ) ? 1.0f :
						gltf->materials[gprim->materialIndex].baseColorFactor[0] );
					primRgb[1] = sampled[1] * ( ( custom || !useMaterials ) ? 1.0f :
						gltf->materials[gprim->materialIndex].baseColorFactor[1] );
					primRgb[2] = sampled[2] * ( ( custom || !useMaterials ) ? 1.0f :
						gltf->materials[gprim->materialIndex].baseColorFactor[2] );
					if ( ( ent->e.shader.rgba[0] | ent->e.shader.rgba[1] | ent->e.shader.rgba[2] ) != 0 ) {
						vk_rtx_entity_tint_rgb( ent->e.shader.rgba, tint );
						primRgb[0] *= tint[0];
						primRgb[1] *= tint[1];
						primRgb[2] *= tint[2];
					}
				} else if ( custom ) {
					vk_rtx_entity_prim_rgb( custom, ent->e.shader.rgba, useMaterials, useUv, u, v, primRgb );
				}
				vk_rtx_entity_write_prim( albedoRgb, normalNxyz, prim, pa, pb, pc, primRgb );
				prim++;
			}
		}
	}
}

static void vk_rtx_entity_fill_attrs( const trRefEntity_t *ent, model_t *mod, int meshKind,
	qboolean usedMesh, const float *positions, const uint32_t *indices,
	uint32_t indexBefore, uint32_t indexAfter, float *albedoRgb, float *normalNxyz )
{
	qboolean useMaterials;
	float rgb[3];

	if ( !albedoRgb || !normalNxyz || indexAfter <= indexBefore ) {
		return;
	}

	useMaterials = ( r_rtxEntityMaterials && r_rtxEntityMaterials->integer ) ? qtrue : qfalse;

	/* Baseline tint so any unpainted prims (pack/fill mismatch) stay valid. */
	vk_rtx_entity_tint_rgb( ent->e.shader.rgba, rgb );
	vk_rtx_entity_fill_prim_attrs_rgb( positions, indices, indexBefore / 3u, indexAfter / 3u,
		rgb, albedoRgb, normalNxyz );

	if ( usedMesh && useMaterials ) {
		if ( meshKind == 1 ) {
			vk_rtx_entity_fill_md3_materials( ent, mod, positions, indices, indexBefore, indexAfter,
				albedoRgb, normalNxyz, useMaterials );
			return;
		}
		if ( meshKind == 2 ) {
			vk_rtx_entity_fill_iqm_materials( ent, mod, positions, indices, indexBefore, indexAfter,
				albedoRgb, normalNxyz, useMaterials );
			return;
		}
		if ( meshKind == 3 ) {
			vk_rtx_entity_fill_gltf_materials( ent, mod, positions, indices, indexBefore, indexAfter,
				albedoRgb, normalNxyz, useMaterials );
			return;
		}
		if ( meshKind == 4 ) {
			vk_rtx_entity_fill_mdr_materials( ent, mod, positions, indices, indexBefore, indexAfter,
				albedoRgb, normalNxyz, useMaterials );
			return;
		}
	}

	/* AABB / materials off: prefer customShader average when enabled. */
	if ( useMaterials && ent->e.customShader ) {
		vk_rtx_entity_albedo_from_shader( R_GetShaderByHandle( ent->e.customShader ),
			ent->e.shader.rgba, qtrue, rgb );
		vk_rtx_entity_fill_prim_attrs_rgb( positions, indices, indexBefore / 3u, indexAfter / 3u,
			rgb, albedoRgb, normalNxyz );
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
	uint32_t proxyMd3Fail = 0u;
	uint32_t proxyIqmFail = 0u;
	uint32_t proxyGltfFail = 0u;
	uint32_t proxyMdrFail = 0u;
	uint32_t meshCpuSkinned = 0u;

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
		qboolean cpuSkinned = qfalse;
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
				&vertCount, &indexCount, &cpuSkinned );
			if ( usedMesh ) {
				meshKind = 2;
			} else {
				triedFailKind = 2;
			}
		} else if ( mod->type == MOD_GLTF ) {
			usedMesh = vk_rtx_pack_gltf( ent, viewParms, mod, refdef->time, positions, maxVerts, indices, maxIndices,
				&vertCount, &indexCount, &cpuSkinned );
			if ( usedMesh ) {
				meshKind = 3;
			} else {
				triedFailKind = 3;
			}
		} else if ( mod->type == MOD_MDR ) {
			usedMesh = vk_rtx_pack_mdr( ent, viewParms, mod, positions, maxVerts, indices, maxIndices,
				&vertCount, &indexCount, &cpuSkinned );
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
			if ( cpuSkinned ) {
				meshCpuSkinned++;
			}
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
			vk_rtx_entity_fill_attrs( ent, mod, meshKind, usedMesh, positions, indices,
				indexBefore, indexCount, albedoRgb, normalNxyz );
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
		/* Rollup: skinned-format pack failures that fell back to AABB. */
		stats->proxySkinnedCount = proxyIqmFail + proxyGltfFail + proxyMdrFail;
		stats->proxyMd3FailCount = proxyMd3Fail;
		stats->proxyIqmFailCount = proxyIqmFail;
		stats->proxyGltfFailCount = proxyGltfFail;
		stats->proxyMdrFailCount = proxyMdrFail;
		stats->meshCpuSkinnedCount = meshCpuSkinned;
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
