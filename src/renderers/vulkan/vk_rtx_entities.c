/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

RTX entity TLAS scaffolding: world-space AABB proxy boxes per RT_MODEL entity.
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

uint32_t vk_rtx_entities_pack( const refdef_t *refdef, const viewParms_t *viewParms,
	uint32_t maxEntities, float *positions, uint32_t *indices )
{
	int i, n, v, t;
	uint32_t packed = 0u;
	uint32_t vertBase;
	vec3_t mins, maxs;
	float sx, sy, sz;
	const float *ax0, *ax1, *ax2;
	const float *o;
	const trRefEntity_t *ent;

	if ( !refdef || !viewParms || !positions || !indices || maxEntities == 0u ) {
		return 0u;
	}

	n = refdef->num_entities;
	if ( n <= 0 ) {
		return 0u;
	}

	for ( i = 0; i < n && packed < maxEntities; i++ ) {
		ent = &refdef->entities[i];
		if ( ent->e.reType != RT_MODEL || !ent->e.hModel ) {
			continue;
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

		vertBase = packed * 8u;
		for ( v = 0; v < 8; v++ ) {
			float lx = s_entity_cube_verts[v][0];
			float ly = s_entity_cube_verts[v][1];
			float lz = s_entity_cube_verts[v][2];
			float *dst = positions + ( vertBase + (uint32_t)v ) * 3u;

			dst[0] = o[0] + ax0[0] * ( mins[0] + lx * sx ) + ax1[0] * ( mins[1] + ly * sy ) + ax2[0] * ( mins[2] + lz * sz );
			dst[1] = o[1] + ax0[1] * ( mins[0] + lx * sx ) + ax1[1] * ( mins[1] + ly * sy ) + ax2[1] * ( mins[2] + lz * sz );
			dst[2] = o[2] + ax0[2] * ( mins[0] + lx * sx ) + ax1[2] * ( mins[1] + ly * sy ) + ax2[2] * ( mins[2] + lz * sz );
		}

		for ( t = 0; t < 36; t++ ) {
			indices[packed * 36u + (uint32_t)t] = vertBase + (uint32_t)s_entity_cube_indices[t];
		}

		packed++;
	}

	return packed;
}

#else /* !USE_VULKAN_RTX */

uint32_t vk_rtx_entities_pack( const refdef_t *refdef, const viewParms_t *viewParms,
	uint32_t maxEntities, float *positions, uint32_t *indices )
{
	(void)refdef;
	(void)viewParms;
	(void)maxEntities;
	(void)positions;
	(void)indices;
	return 0u;
}

#endif /* USE_VULKAN_RTX */
