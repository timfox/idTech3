/*
===========================================================================
World BSP geometry extraction for Vulkan RTX BLAS
(all brush submodels: faces + triangle soups + SF_GRID patches).
Optional per-primitive albedo RGB for Hybrid1 / pathtrace closest-hit.
===========================================================================
*/

#include "tr_local.h"
#include "vk_rtx_world.h"

#ifdef USE_VULKAN_RTX

static const float s_defaultAlbedo[3] = { 0.72f, 0.70f, 0.66f };

static void rtx_store_albedo( float *albedoRgb, uint32_t primIndex, float r, float g, float b )
{
	float *dst;

	if ( !albedoRgb ) {
		return;
	}
	dst = albedoRgb + primIndex * 3u;
	dst[0] = r;
	dst[1] = g;
	dst[2] = b;
}

static void rtx_albedo_from_bytes( const byte *c, float *outRgb )
{
	if ( !c ) {
		outRgb[0] = s_defaultAlbedo[0];
		outRgb[1] = s_defaultAlbedo[1];
		outRgb[2] = s_defaultAlbedo[2];
		return;
	}
	outRgb[0] = (float)c[0] * ( 1.0f / 255.0f );
	outRgb[1] = (float)c[1] * ( 1.0f / 255.0f );
	outRgb[2] = (float)c[2] * ( 1.0f / 255.0f );
}

static void rtx_albedo_from_srfvert( const srfVert_t *v, float *outRgb )
{
	rtx_albedo_from_bytes( v->color.rgba, outRgb );
}

static void rtx_albedo_from_face_vert( const srfSurfaceFace_t *face, unsigned vidx, float *outRgb )
{
	const byte *c;
#ifdef USE_VK_PBR
	c = (const byte *)&face->points[vidx][10];
#else
	c = (const byte *)&face->points[vidx][7];
#endif
	rtx_albedo_from_bytes( c, outRgb );
}

static void rtx_avg3_albedo( const float a[3], const float b[3], const float c[3], float *outRgb )
{
	outRgb[0] = ( a[0] + b[0] + c[0] ) * ( 1.0f / 3.0f );
	outRgb[1] = ( a[1] + b[1] + c[1] ) * ( 1.0f / 3.0f );
	outRgb[2] = ( a[2] + b[2] + c[2] ) * ( 1.0f / 3.0f );
}

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

/*
 * Same LOD selection as R_BspStream_BakeGridToTris (r_lodCurveError).
 * Returns triangle count; fills width/height tables when non-NULL.
 */
static uint32_t rtx_grid_lod_setup( const srfGridMesh_t *grid, int *widthTable, int *heightTable,
	int *outLodWidth, int *outLodHeight )
{
	float lodError;
	int lodWidth, lodHeight;
	int i;
	int localW[MAX_GRID_SIZE];
	int localH[MAX_GRID_SIZE];
	int *wt = widthTable ? widthTable : localW;
	int *ht = heightTable ? heightTable : localH;

	if ( !grid || grid->surfaceType != SF_GRID || grid->width < 2 || grid->height < 2 ) {
		return 0u;
	}

	lodError = r_lodCurveError ? r_lodCurveError->value : 0.0f;
	if ( lodError < 0.0f ) {
		lodError = 0.0f;
	}

	wt[0] = 0;
	lodWidth = 1;
	for ( i = 1; i < grid->width - 1; i++ ) {
		if ( grid->widthLodError[i] <= lodError ) {
			wt[lodWidth++] = i;
		}
	}
	wt[lodWidth++] = grid->width - 1;

	ht[0] = 0;
	lodHeight = 1;
	for ( i = 1; i < grid->height - 1; i++ ) {
		if ( grid->heightLodError[i] <= lodError ) {
			ht[lodHeight++] = i;
		}
	}
	ht[lodHeight++] = grid->height - 1;

	if ( lodWidth < 2 || lodHeight < 2 ) {
		return 0u;
	}

	if ( outLodWidth ) {
		*outLodWidth = lodWidth;
	}
	if ( outLodHeight ) {
		*outLodHeight = lodHeight;
	}
	return (uint32_t)( ( lodWidth - 1 ) * ( lodHeight - 1 ) * 2 );
}

static uint32_t rtx_count_grid_tris( const srfGridMesh_t *grid )
{
	return rtx_grid_lod_setup( grid, NULL, NULL, NULL, NULL );
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
	if ( *st == SF_GRID ) {
		return rtx_count_grid_tris( (const srfGridMesh_t *)st );
	}
	return 0u;
}

uint32_t vk_rtx_world_count_primitives( const world_t *w, uint32_t maxPrimitives )
{
	uint32_t n, i, bi, total;
	int bmCount;
	const bmodel_t *bm;
	const msurface_t *sf;

	if ( !w || !w->bmodels || w->surfaces == NULL ) {
		return 0u;
	}
	bmCount = w->numBModels;
	if ( bmCount <= 0 ) {
		bmCount = 1;
	}
	total = 0u;
	for ( bi = 0u; bi < (uint32_t)bmCount; bi++ ) {
		bm = &w->bmodels[bi];
		if ( bm->numSurfaces <= 0 || bm->firstSurface == NULL ) {
			continue;
		}
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
	}
	return total;
}

static void rtx_emit_face_tris( const srfSurfaceFace_t *face, float *positions, uint32_t *indices,
	float *albedoRgb, uint32_t *outVert, uint32_t *outIdx, uint32_t maxPrimitives, uint32_t *primCount )
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
		float ca[3], cb[3], cc[3], avg[3];
		unsigned i0, i1, i2;

		if ( *primCount >= maxPrimitives ) {
			return;
		}
		i0 = idxSrc[t * 3u + 0u];
		i1 = idxSrc[t * 3u + 1u];
		i2 = idxSrc[t * 3u + 2u];
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
		rtx_albedo_from_face_vert( face, i0, ca );
		rtx_albedo_from_face_vert( face, i1, cb );
		rtx_albedo_from_face_vert( face, i2, cc );
		rtx_avg3_albedo( ca, cb, cc, avg );
		rtx_store_albedo( albedoRgb, *primCount, avg[0], avg[1], avg[2] );
		baseV += 3u;
		baseI += 3u;
		( *primCount )++;
	}
	*outVert = baseV;
	*outIdx = baseI;
}

static void rtx_emit_triangles_tris( const srfTriangles_t *surf, float *positions, uint32_t *indices,
	float *albedoRgb, uint32_t *outVert, uint32_t *outIdx, uint32_t maxPrimitives, uint32_t *primCount )
{
	uint32_t t, baseV, baseI;
	int vi;

	if ( surf->numIndexes < 3 || ( surf->numIndexes % 3 ) != 0 ) {
		return;
	}
	baseV = *outVert;
	baseI = *outIdx;

	for ( t = 0u; t < (uint32_t)surf->numIndexes / 3u; t++ ) {
		float ca[3], cb[3], cc[3], avg[3];
		int i0, i1, i2;

		if ( *primCount >= maxPrimitives ) {
			return;
		}
		i0 = surf->indexes[(int)( t * 3u ) + 0];
		i1 = surf->indexes[(int)( t * 3u ) + 1];
		i2 = surf->indexes[(int)( t * 3u ) + 2];
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
		rtx_albedo_from_srfvert( &surf->verts[i0], ca );
		rtx_albedo_from_srfvert( &surf->verts[i1], cb );
		rtx_albedo_from_srfvert( &surf->verts[i2], cc );
		rtx_avg3_albedo( ca, cb, cc, avg );
		rtx_store_albedo( albedoRgb, *primCount, avg[0], avg[1], avg[2] );
		baseV += 3u;
		baseI += 3u;
		( *primCount )++;
	}
	*outVert = baseV;
	*outIdx = baseI;
}

static void rtx_emit_grid_tris( const srfGridMesh_t *grid, float *positions, uint32_t *indices,
	float *albedoRgb, uint32_t *outVert, uint32_t *outIdx, uint32_t maxPrimitives, uint32_t *primCount )
{
	int widthTable[MAX_GRID_SIZE];
	int heightTable[MAX_GRID_SIZE];
	int lodWidth, lodHeight;
	int h, w;
	uint32_t baseV, baseI;
	uint32_t vertBase;

	if ( rtx_grid_lod_setup( grid, widthTable, heightTable, &lodWidth, &lodHeight ) == 0u ) {
		return;
	}

	baseV = *outVert;
	baseI = *outIdx;
	vertBase = baseV;

	/* Emit LOD verts once, then index them (unlike face path which duplicates). */
	for ( h = 0; h < lodHeight; h++ ) {
		for ( w = 0; w < lodWidth; w++ ) {
			const srfVert_t *dv = &grid->verts[heightTable[h] * grid->width + widthTable[w]];
			float *dst = positions + baseV * 3u;
			dst[0] = dv->xyz[0];
			dst[1] = dv->xyz[1];
			dst[2] = dv->xyz[2];
			baseV++;
		}
	}

	for ( h = 0; h < lodHeight - 1; h++ ) {
		for ( w = 0; w < lodWidth - 1; w++ ) {
			int v1 = h * lodWidth + w + 1;
			int v2 = v1 - 1;
			int v3 = v2 + lodWidth;
			int v4 = v3 + 1;
			float ca[3], cb[3], cc[3], avg[3];
			const srfVert_t *dvA, *dvB, *dvC;

			/* Tri 1: v2, v3, v1 */
			if ( *primCount >= maxPrimitives ) {
				*outVert = baseV;
				*outIdx = baseI;
				return;
			}
			indices[baseI + 0u] = vertBase + (uint32_t)v2;
			indices[baseI + 1u] = vertBase + (uint32_t)v3;
			indices[baseI + 2u] = vertBase + (uint32_t)v1;
			dvA = &grid->verts[heightTable[h] * grid->width + widthTable[w]];
			dvB = &grid->verts[heightTable[h + 1] * grid->width + widthTable[w]];
			dvC = &grid->verts[heightTable[h] * grid->width + widthTable[w + 1]];
			rtx_albedo_from_srfvert( dvA, ca );
			rtx_albedo_from_srfvert( dvB, cb );
			rtx_albedo_from_srfvert( dvC, cc );
			rtx_avg3_albedo( ca, cb, cc, avg );
			rtx_store_albedo( albedoRgb, *primCount, avg[0], avg[1], avg[2] );
			baseI += 3u;
			( *primCount )++;

			/* Tri 2: v1, v3, v4 */
			if ( *primCount >= maxPrimitives ) {
				*outVert = baseV;
				*outIdx = baseI;
				return;
			}
			indices[baseI + 0u] = vertBase + (uint32_t)v1;
			indices[baseI + 1u] = vertBase + (uint32_t)v3;
			indices[baseI + 2u] = vertBase + (uint32_t)v4;
			dvA = &grid->verts[heightTable[h] * grid->width + widthTable[w + 1]];
			dvB = &grid->verts[heightTable[h + 1] * grid->width + widthTable[w]];
			dvC = &grid->verts[heightTable[h + 1] * grid->width + widthTable[w + 1]];
			rtx_albedo_from_srfvert( dvA, ca );
			rtx_albedo_from_srfvert( dvB, cb );
			rtx_albedo_from_srfvert( dvC, cc );
			rtx_avg3_albedo( ca, cb, cc, avg );
			rtx_store_albedo( albedoRgb, *primCount, avg[0], avg[1], avg[2] );
			baseI += 3u;
			( *primCount )++;
		}
	}

	*outVert = baseV;
	*outIdx = baseI;
}

uint32_t vk_rtx_world_pack( const world_t *w, uint32_t maxPrimitives,
	float *positions, uint32_t *indices, float *albedoRgb, uint32_t *outVertCount )
{
	uint32_t i, bi, primCount, vertPos, idxPos, gridPrims;
	int bmCount;
	const bmodel_t *bm;
	const msurface_t *sf;
	const surfaceType_t *st;

	if ( outVertCount ) {
		*outVertCount = 0u;
	}
	if ( !w || !w->bmodels || w->surfaces == NULL || maxPrimitives == 0u || !positions || !indices ) {
		return 0u;
	}
	bmCount = w->numBModels;
	if ( bmCount <= 0 ) {
		bmCount = 1;
	}
	primCount = 0u;
	vertPos = 0u;
	idxPos = 0u;
	gridPrims = 0u;

	for ( bi = 0u; bi < (uint32_t)bmCount; bi++ ) {
		bm = &w->bmodels[bi];
		if ( bm->numSurfaces <= 0 || bm->firstSurface == NULL ) {
			continue;
		}
		for ( i = 0; i < (uint32_t)bm->numSurfaces; i++ ) {
			uint32_t before = primCount;

			sf = bm->firstSurface + i;
			if ( !sf->data ) {
				continue;
			}
			st = sf->data;
			if ( *st == SF_FACE ) {
				rtx_emit_face_tris( (const srfSurfaceFace_t *)st, positions, indices, albedoRgb,
					&vertPos, &idxPos, maxPrimitives, &primCount );
			} else if ( *st == SF_TRIANGLES ) {
				rtx_emit_triangles_tris( (const srfTriangles_t *)st, positions, indices, albedoRgb,
					&vertPos, &idxPos, maxPrimitives, &primCount );
			} else if ( *st == SF_GRID ) {
				rtx_emit_grid_tris( (const srfGridMesh_t *)st, positions, indices, albedoRgb,
					&vertPos, &idxPos, maxPrimitives, &primCount );
				gridPrims += ( primCount - before );
			}
			if ( primCount >= maxPrimitives ) {
				if ( gridPrims > 0u ) {
					ri.Printf( PRINT_DEVELOPER, "[VK][RTX] world pack: %u grid tris (capped at %u total)\n",
						gridPrims, maxPrimitives );
				}
				if ( outVertCount ) {
					*outVertCount = vertPos;
				}
				return primCount;
			}
		}
	}
	if ( gridPrims > 0u ) {
		ri.Printf( PRINT_DEVELOPER, "[VK][RTX] world pack: %u tris from SF_GRID patches\n", gridPrims );
	}
	if ( outVertCount ) {
		*outVertCount = vertPos;
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

uint32_t vk_rtx_world_pack( const world_t *w, uint32_t maxPrimitives,
	float *positions, uint32_t *indices, float *albedoRgb, uint32_t *outVertCount )
{
	(void)w;
	(void)maxPrimitives;
	(void)positions;
	(void)indices;
	(void)albedoRgb;
	if ( outVertCount ) {
		*outVertCount = 0u;
	}
	return 0u;
}

#endif /* USE_VULKAN_RTX */
