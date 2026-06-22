/*
===========================================================================
Triangle token extraction for RenderFormer-style neural preview.
===========================================================================
*/

#include "tr_local.h"
#include "vk_renderformer_scene.h"

#define RF_HASH_BUCKETS     131072u

typedef struct {
	uint32_t key;
	uint32_t triIndex;
} rfHashEntry_t;

static uint32_t RF_QuantKey( const float *p, float quant )
{
	int ix, iy, iz;

	if ( quant < 1.0f ) {
		quant = 1.0f;
	}
	ix = (int)( p[0] / quant );
	iy = (int)( p[1] / quant );
	iz = (int)( p[2] / quant );
	return (uint32_t)( ix * 73856093 ) ^ (uint32_t)( iy * 19349663 ) ^ (uint32_t)( iz * 83492791 );
}

static void RF_ExpandBoundsTri( rfSceneData_t *wd, const float *c0, const float *c1, const float *c2 )
{
	const float *pts[3] = { c0, c1, c2 };
	int vi, c;

	for ( vi = 0; vi < 3; vi++ ) {
		if ( wd->triangleCount == 0 && vi == 0 ) {
			VectorCopy( pts[0], wd->worldMin );
			VectorCopy( pts[0], wd->worldMax );
			continue;
		}
		for ( c = 0; c < 3; c++ ) {
			if ( pts[vi][c] < wd->worldMin[c] ) {
				wd->worldMin[c] = pts[vi][c];
			}
			if ( pts[vi][c] > wd->worldMax[c] ) {
				wd->worldMax[c] = pts[vi][c];
			}
		}
	}
}

static void RF_FillMaterial( float *albedo, float *roughness, float *metallic, const float *pos, const float *n )
{
	float h = sinf( pos[0] * 0.0015f ) * cosf( pos[1] * 0.0012f );
	float nUp = Com_Clamp( 0.0f, 1.0f, n[2] * 0.5f + 0.5f );

	albedo[0] = Com_Clamp( 0.08f, 0.92f, 0.35f + 0.25f * h + 0.1f * nUp );
	albedo[1] = Com_Clamp( 0.08f, 0.92f, 0.32f + 0.2f * cosf( pos[2] * 0.002f ) );
	albedo[2] = Com_Clamp( 0.08f, 0.92f, 0.38f + 0.15f * sinf( pos[0] * 0.003f ) );
	*roughness = Com_Clamp( 0.05f, 0.95f, 0.55f - 0.2f * nUp );
	*metallic = Com_Clamp( 0.0f, 1.0f, 0.08f + 0.12f * ( 1.0f - nUp ) );
}

static qboolean RF_AddTriangle( rfSceneData_t *wd, rfHashEntry_t *hash, uint32_t *hashUsed,
	const float *c0, const float *c1, const float *c2, const float *n, float quant, uint32_t maxTris )
{
	float center[3];
	float area;
	float albedo[3];
	float roughness;
	float metallic;
	uint32_t bucket, probe, key;
	rfTriangleToken_t *tok;

	if ( wd->triangleCount >= maxTris ) {
		return qfalse;
	}

	VectorAdd( c0, c1, center );
	VectorAdd( center, c2, center );
	VectorScale( center, 1.0f / 3.0f, center );

	key = RF_QuantKey( center, quant ) % RF_HASH_BUCKETS;
	bucket = key;
	for ( probe = 0; probe < RF_HASH_BUCKETS; probe++ ) {
		if ( hash[bucket].triIndex == 0xFFFFFFFFu ) {
			vec3_t e1, e2, cr;

			VectorSubtract( c1, c0, e1 );
			VectorSubtract( c2, c0, e2 );
			CrossProduct( e1, e2, cr );
			area = VectorLength( cr ) * 0.5f;

			tok = &wd->tokens[wd->triangleCount];
			tok->center[0] = center[0];
			tok->center[1] = center[1];
			tok->center[2] = center[2];
			tok->center[3] = area;
			tok->normal[0] = n[0];
			tok->normal[1] = n[1];
			tok->normal[2] = n[2];
			RF_FillMaterial( albedo, &roughness, &metallic, center, n );
			tok->normal[3] = roughness;
			tok->albedo[0] = albedo[0];
			tok->albedo[1] = albedo[1];
			tok->albedo[2] = albedo[2];
			tok->albedo[3] = metallic;

			hash[bucket].key = key;
			hash[bucket].triIndex = wd->triangleCount++;
			( *hashUsed )++;
			RF_ExpandBoundsTri( wd, c0, c1, c2 );
			return qtrue;
		}
		if ( hash[bucket].key == key ) {
			const rfTriangleToken_t *ex = &wd->tokens[hash[bucket].triIndex];
			if ( DistanceSquared( ex->center, center ) < quant * quant ) {
				return qtrue;
			}
		}
		bucket = ( bucket + 1u ) % RF_HASH_BUCKETS;
	}
	return qfalse;
}

static void RF_EmitFace( rfSceneData_t *wd, rfHashEntry_t *hash, uint32_t *hashUsed,
	const srfSurfaceFace_t *face, float quant, uint32_t maxTris )
{
	const unsigned *idxSrc;
	uint32_t t;

	if ( face->numIndices < 3 ) {
		return;
	}
	idxSrc = (const unsigned *)( (const char *)face + face->ofsIndices );
	for ( t = 0u; t < (uint32_t)face->numIndices / 3u; t++ ) {
		const float *p0 = face->points[idxSrc[t * 3u + 0]];
		const float *p1 = face->points[idxSrc[t * 3u + 1]];
		const float *p2 = face->points[idxSrc[t * 3u + 2]];
		float n[3];

		CrossProduct( p1, p0, n );
		{
			float len = VectorLength( n );
			if ( len > 1e-6f ) {
				VectorScale( n, 1.0f / len, n );
			} else {
				n[0] = 0.0f;
				n[1] = 0.0f;
				n[2] = 1.0f;
			}
		}
		RF_AddTriangle( wd, hash, hashUsed, p0, p1, p2, n, quant, maxTris );
	}
}

static void RF_EmitTris( rfSceneData_t *wd, rfHashEntry_t *hash, uint32_t *hashUsed,
	const srfTriangles_t *surf, float quant, uint32_t maxTris )
{
	uint32_t t;

	if ( surf->numIndexes < 3 ) {
		return;
	}
	for ( t = 0u; t < (uint32_t)surf->numIndexes / 3u; t++ ) {
		float n[3];
		const float *p0, *p1, *p2;

		p0 = surf->verts[surf->indexes[(int)( t * 3u ) + 0]].xyz;
		p1 = surf->verts[surf->indexes[(int)( t * 3u ) + 1]].xyz;
		p2 = surf->verts[surf->indexes[(int)( t * 3u ) + 2]].xyz;
		CrossProduct( p1, p0, n );
		{
			float len = VectorLength( n );
			if ( len > 1e-6f ) {
				VectorScale( n, 1.0f / len, n );
			} else {
				n[0] = 0.0f;
				n[1] = 0.0f;
				n[2] = 1.0f;
			}
		}
		RF_AddTriangle( wd, hash, hashUsed, p0, p1, p2, n, quant, maxTris );
	}
}

static void RF_BuildGrid( rfSceneData_t *wd, uint32_t gx, uint32_t gy, uint32_t gz )
{
	uint32_t i, cellCount;
	float span[3];

	if ( gx < 4 ) {
		gx = 4;
	}
	if ( gy < 4 ) {
		gy = 4;
	}
	if ( gz < 4 ) {
		gz = 4;
	}

	wd->gridX = gx;
	wd->gridY = gy;
	wd->gridZ = gz;
	cellCount = gx * gy * gz;

	wd->cells = (rfGridCell_t *)ri.Malloc( cellCount * sizeof( rfGridCell_t ) );
	for ( i = 0; i < cellCount; i++ ) {
		wd->cells[i].count = 0;
		wd->cells[i].indices[0] = 0xFFFFFFFFu;
		wd->cells[i].indices[1] = 0xFFFFFFFFu;
		wd->cells[i].indices[2] = 0xFFFFFFFFu;
		wd->cells[i].indices[3] = 0xFFFFFFFFu;
	}

	VectorSubtract( wd->worldMax, wd->worldMin, span );
	if ( span[0] < 8.0f ) {
		span[0] = 8.0f;
	}
	if ( span[1] < 8.0f ) {
		span[1] = 8.0f;
	}
	if ( span[2] < 8.0f ) {
		span[2] = 8.0f;
	}
	wd->cellSize[0] = span[0] / (float)gx;
	wd->cellSize[1] = span[1] / (float)gy;
	wd->cellSize[2] = span[2] / (float)gz;

	for ( i = 0; i < wd->triangleCount; i++ ) {
		const float *p = wd->tokens[i].center;
		uint32_t cx, cy, cz, cid;

		cx = (uint32_t)( ( p[0] - wd->worldMin[0] ) / wd->cellSize[0] );
		cy = (uint32_t)( ( p[1] - wd->worldMin[1] ) / wd->cellSize[1] );
		cz = (uint32_t)( ( p[2] - wd->worldMin[2] ) / wd->cellSize[2] );
		if ( cx >= gx ) {
			cx = gx - 1;
		}
		if ( cy >= gy ) {
			cy = gy - 1;
		}
		if ( cz >= gz ) {
			cz = gz - 1;
		}
		cid = cx + cy * gx + cz * gx * gy;
		if ( wd->cells[cid].count < 4u ) {
			wd->cells[cid].indices[wd->cells[cid].count++] = i;
		}
	}
}

qboolean RF_Scene_BuildFromWorld( rfSceneData_t *out, const world_t *w, uint32_t maxTriangles,
	uint32_t gridX, uint32_t gridY, uint32_t gridZ )
{
	rfHashEntry_t *hash;
	uint32_t hashUsed = 0;
	uint32_t bi, i;
	int bmCount;
	const bmodel_t *bm;
	const msurface_t *sf;
	const surfaceType_t *st;
	float quant = 16.0f;

	if ( !out || !w || !w->bmodels || w->surfaces == NULL || maxTriangles == 0 ) {
		return qfalse;
	}
	RF_Scene_Free( out );
	Com_Memset( out, 0, sizeof( *out ) );

	if ( maxTriangles > RF_MAX_TRIANGLES ) {
		maxTriangles = RF_MAX_TRIANGLES;
	}

	out->tokens = (rfTriangleToken_t *)ri.Malloc( (size_t)maxTriangles * sizeof( rfTriangleToken_t ) );
	hash = (rfHashEntry_t *)ri.Malloc( RF_HASH_BUCKETS * sizeof( rfHashEntry_t ) );
	for ( i = 0; i < RF_HASH_BUCKETS; i++ ) {
		hash[i].key = 0;
		hash[i].triIndex = 0xFFFFFFFFu;
	}

	bmCount = w->numBModels;
	if ( bmCount <= 0 ) {
		bmCount = 1;
	}

	for ( bi = 0u; bi < (uint32_t)bmCount; bi++ ) {
		bm = &w->bmodels[bi];
		if ( bm->numSurfaces <= 0 || bm->firstSurface == NULL ) {
			continue;
		}
		for ( i = 0; i < (uint32_t)bm->numSurfaces; i++ ) {
			sf = bm->firstSurface + i;
			if ( !sf->data ) {
				continue;
			}
			st = sf->data;
			if ( *st == SF_FACE ) {
				RF_EmitFace( out, hash, &hashUsed, (const srfSurfaceFace_t *)st, quant, maxTriangles );
			} else if ( *st == SF_TRIANGLES ) {
				RF_EmitTris( out, hash, &hashUsed, (const srfTriangles_t *)st, quant, maxTriangles );
			}
			if ( out->triangleCount >= maxTriangles ) {
				break;
			}
		}
		if ( out->triangleCount >= maxTriangles ) {
			break;
		}
	}

	ri.Free( hash );

	if ( out->triangleCount == 0 ) {
		ri.Free( out->tokens );
		out->tokens = NULL;
		ri.Printf( PRINT_WARNING, "[RenderFormer] No triangles extracted from world\n" );
		return qfalse;
	}

	RF_BuildGrid( out, gridX, gridY, gridZ );
	out->valid = qtrue;
	return qtrue;
}

void RF_Scene_Free( rfSceneData_t *sd )
{
	if ( !sd ) {
		return;
	}
	if ( sd->tokens ) {
		ri.Free( sd->tokens );
	}
	if ( sd->cells ) {
		ri.Free( sd->cells );
	}
	Com_Memset( sd, 0, sizeof( *sd ) );
}
