/*
===========================================================================
World BSP geometry extraction for Vulkan RTX BLAS (faces + triangle soups).
===========================================================================
*/

#include "tr_local.h"
#include "vk_rtx_world.h"

#ifdef USE_VULKAN_RTX

static uint32_t rtx_count_face_tris( const srfSurfaceFace_t *face )
{
	if ( face->numIndices < 3 || ( face->numIndices % 3u ) != 0u ) {
		return 0u;
	}
	return (uint32_t)( face->numIndices / 3 );
}

static uint32_t rtx_count_triangles_tris( const srfTriangles_t *surf )
{
	if ( surf->numIndexes < 3 || ( surf->numIndexes % 3 ) != 0 ) {
		return 0u;
	}
	return (uint32_t)( surf->numIndexes / 3 );
}

static uint32_t rtx_count_surface_primitives( const msurface_t *sf )
{
	const surfaceType_t *st;

	if ( !sf || !sf->data ) {
		return 0u;
	}
	st = sf->data;
	if ( *st == SF_FACE ) {
		return rtx_count_face_tris( (const srfSurfaceFace_t *)st );
	}
	if ( *st == SF_TRIANGLES ) {
		return rtx_count_triangles_tris( (const srfTriangles_t *)st );
	}
	return 0u;
}

uint32_t vk_rtx_world_count_primitives( const world_t *w, uint32_t maxPrimitives )
{
	uint32_t n, i, total;
	const bmodel_t *bm;
	const msurface_t *sf;

	if ( !w || !w->bmodels || w->surfaces == NULL ) {
		return 0u;
	}
	bm = &w->bmodels[0];
	total = 0u;
	for ( i = 0; i < (uint32_t)bm->numSurfaces; i++ ) {
		sf = bm->firstSurface + i;
		n = rtx_count_surface_primitives( sf );
		if ( n == 0u ) {
			continue;
		}
		if ( total + n > maxPrimitives ) {
			return maxPrimitives;
		}
		total += n;
	}
	return total;
}

static void rtx_emit_face_tris( const srfSurfaceFace_t *face, float *positions, uint32_t *indices,
	uint32_t *outVert, uint32_t *outIdx, uint32_t maxPrimitives, uint32_t *primCount )
{
	const unsigned *idxSrc;
	uint32_t t, baseV, baseI;
	int vi;

	if ( face->numIndices < 3 || ( face->numIndices % 3u ) != 0u ) {
		return;
	}
	idxSrc = (const unsigned *)( (const char *)face + face->ofsIndices );
	baseV = *outVert;
	baseI = *outIdx;

	for ( t = 0u; t < (uint32_t)face->numIndices / 3u; t++ ) {
		if ( *primCount >= maxPrimitives ) {
			return;
		}
		for ( vi = 0; vi < 3; vi++ ) {
			unsigned vidx = idxSrc[t * 3u + (unsigned)vi];
			const float *p = face->points[vidx];
			positions[( baseV + (uint32_t)vi ) * 3u + 0u] = p[0];
			positions[( baseV + (uint32_t)vi ) * 3u + 1u] = p[1];
			positions[( baseV + (uint32_t)vi ) * 3u + 2u] = p[2];
		}
		indices[baseI + 0u] = baseV + 0u;
		indices[baseI + 1u] = baseV + 1u;
		indices[baseI + 2u] = baseV + 2u;
		baseV += 3u;
		baseI += 3u;
		( *primCount )++;
	}
	*outVert = baseV;
	*outIdx = baseI;
}

static void rtx_emit_triangles_tris( const srfTriangles_t *surf, float *positions, uint32_t *indices,
	uint32_t *outVert, uint32_t *outIdx, uint32_t maxPrimitives, uint32_t *primCount )
{
	uint32_t t, baseV, baseI;
	int vi;

	if ( surf->numIndexes < 3 || ( surf->numIndexes % 3 ) != 0 ) {
		return;
	}
	baseV = *outVert;
	baseI = *outIdx;

	for ( t = 0u; t < (uint32_t)surf->numIndexes / 3u; t++ ) {
		if ( *primCount >= maxPrimitives ) {
			return;
		}
		for ( vi = 0; vi < 3; vi++ ) {
			int vidx = surf->indexes[(int)( t * 3u ) + vi];
			const float *p = surf->verts[vidx].xyz;
			positions[( baseV + (uint32_t)vi ) * 3u + 0u] = p[0];
			positions[( baseV + (uint32_t)vi ) * 3u + 1u] = p[1];
			positions[( baseV + (uint32_t)vi ) * 3u + 2u] = p[2];
		}
		indices[baseI + 0u] = baseV + 0u;
		indices[baseI + 1u] = baseV + 1u;
		indices[baseI + 2u] = baseV + 2u;
		baseV += 3u;
		baseI += 3u;
		( *primCount )++;
	}
	*outVert = baseV;
	*outIdx = baseI;
}

uint32_t vk_rtx_world_pack( const world_t *w, uint32_t maxPrimitives,
	float *positions, uint32_t *indices )
{
	uint32_t i, primCount, vertPos, idxPos;
	const bmodel_t *bm;
	const msurface_t *sf;
	const surfaceType_t *st;

	if ( !w || !w->bmodels || w->surfaces == NULL || maxPrimitives == 0u ) {
		return 0u;
	}
	bm = &w->bmodels[0];
	primCount = 0u;
	vertPos = 0u;
	idxPos = 0u;

	for ( i = 0; i < (uint32_t)bm->numSurfaces; i++ ) {
		sf = bm->firstSurface + i;
		if ( !sf->data ) {
			continue;
		}
		st = sf->data;
		if ( *st == SF_FACE ) {
			rtx_emit_face_tris( (const srfSurfaceFace_t *)st, positions, indices, &vertPos, &idxPos, maxPrimitives, &primCount );
		} else if ( *st == SF_TRIANGLES ) {
			rtx_emit_triangles_tris( (const srfTriangles_t *)st, positions, indices, &vertPos, &idxPos, maxPrimitives, &primCount );
		}
		if ( primCount >= maxPrimitives ) {
			break;
		}
	}
	return primCount;
}

#else /* !USE_VULKAN_RTX */

uint32_t vk_rtx_world_count_primitives( const world_t *w, uint32_t maxPrimitives )
{
	(void)w;
	(void)maxPrimitives;
	return 0u;
}

uint32_t vk_rtx_world_pack( const world_t *w, uint32_t maxPrimitives, float *positions, uint32_t *indices )
{
	(void)w;
	(void)maxPrimitives;
	(void)positions;
	(void)indices;
	return 0u;
}

#endif /* USE_VULKAN_RTX */
