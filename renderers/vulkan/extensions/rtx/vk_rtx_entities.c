/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

RTX entity BLAS: MD3 LOD0 mesh (frame-lerp, world space) with AABB proxy fallback.
===========================================================================
*/

#include "tr_local.h"
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
			/* Surface does not fit — abort mesh pack for this entity (caller may AABB). */
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

uint32_t vk_rtx_entities_pack( const trRefdef_t *refdef, const viewParms_t *viewParms,
	uint32_t maxEntities, float *positions, uint32_t maxVerts,
	uint32_t *indices, uint32_t maxIndices, vkRtxEntityPackStats_t *stats )
{
	int i, n;
	uint32_t packed = 0u;
	uint32_t vertCount = 0u;
	uint32_t indexCount = 0u;
	uint32_t meshCount = 0u;
	uint32_t proxyCount = 0u;

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
		}

		if ( !usedMesh ) {
			if ( !vk_rtx_pack_aabb( ent, viewParms, positions, maxVerts, indices, maxIndices,
					&vertCount, &indexCount ) ) {
				break;
			}
			proxyCount++;
		} else {
			meshCount++;
		}

		packed++;
	}

	if ( stats ) {
		stats->entityCount = packed;
		stats->vertexCount = vertCount;
		stats->primitiveCount = indexCount / 3u;
		stats->meshEntityCount = meshCount;
		stats->proxyEntityCount = proxyCount;
	}

	return packed;
}

#else /* !USE_VULKAN_RTX */

uint32_t vk_rtx_entities_pack( const trRefdef_t *refdef, const viewParms_t *viewParms,
	uint32_t maxEntities, float *positions, uint32_t maxVerts,
	uint32_t *indices, uint32_t maxIndices, vkRtxEntityPackStats_t *stats )
{
	(void)refdef;
	(void)viewParms;
	(void)maxEntities;
	(void)positions;
	(void)maxVerts;
	(void)indices;
	(void)maxIndices;
	if ( stats ) {
		Com_Memset( stats, 0, sizeof( *stats ) );
	}
	return 0u;
}

#endif /* USE_VULKAN_RTX */
