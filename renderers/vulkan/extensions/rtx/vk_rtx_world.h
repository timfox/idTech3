#pragma once

#include "tr_local.h"

/* Count packable world primitives (SF_FACE + SF_TRIANGLES + SF_GRID). */
uint32_t vk_rtx_world_count_primitives( const world_t *w, uint32_t maxPrimitives );

/*
 * Pack world tris into positions (xyz floats) and indices.
 * If albedoRgb is non-NULL, write maxPrimitives*3 floats (RGB per primitive).
 * Albedo prefers diffuse UV thumbs / avgColor when r_rtxWorldMaterials(+UvSample);
 * otherwise BSP vertex colors.
 * If normalRgb is non-NULL, write maxPrimitives*3 floats (geometric/interpolated normal).
 * If uvHost is non-NULL, write maxPrimitives*6 floats (u0 v0 u1 v1 u2 v2 per prim).
 * If outVertCount is non-NULL, writes number of vertices written (grid shares verts).
 */
uint32_t vk_rtx_world_pack( const world_t *w, uint32_t maxPrimitives,
	float *positions, uint32_t *indices, float *albedoRgb, float *normalRgb, float *uvHost,
	uint32_t *outVertCount );
