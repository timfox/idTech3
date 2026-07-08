/*
===========================================================================
World mesh vertex extraction for Vertex Features Neural GI (VFGI).
===========================================================================
*/

#include "tr_local.h"
#include "vk_vfgi_world.h"

#define VFGI_HASH_BUCKETS   262144u

typedef struct {
	uint32_t key;
	uint32_t vertIndex;
} vfgiHashEntry_t;

static uint32_t VFGI_QuantKey( const float *p, float quant )
{
	int ix, iy, iz;
	uint32_t k;

	if ( quant < 0.01f ) {
		quant = 0.01f;
	}
	ix = (int)( p[0] / quant );
	iy = (int)( p[1] / quant );
	iz = (int)( p[2] / quant );
	k = (uint32_t)( ix * 73856093 ) ^ (uint32_t)( iy * 19349663 ) ^ (uint32_t)( iz * 83492791 );
	return k % VFGI_HASH_BUCKETS;
}

static void VFGI_FillProceduralFeature( float *feat, const float *pos, const float *n )
{
	float h = sinf( pos[0] * 0.002f ) * cosf( pos[1] * 0.002f ) * sinf( pos[2] * 0.003f );
	feat[0] = 0.5f + 0.5f * h;
	feat[1] = Com_Clamp( 0.0f, 1.0f, n[0] * 0.5f + 0.5f );
	feat[2] = Com_Clamp( 0.0f, 1.0f, n[1] * 0.5f + 0.5f );
	feat[3] = Com_Clamp( 0.0f, 1.0f, n[2] * 0.5f + 0.5f );
}

static qboolean VFGI_AddVertex( vfgiWorldData_t *wd, vfgiHashEntry_t *hashTable,
	uint32_t *hashUsed, const float *pos, const float *n, float quant, uint32_t maxVertices )
{
	uint32_t bucket, probe, key, idx;
	vfgiVertexRecord_t *v;

	if ( wd->vertexCount >= maxVertices ) {
		return qfalse;
	}

	key = VFGI_QuantKey( pos, quant );
	bucket = key;
	for ( probe = 0; probe < VFGI_HASH_BUCKETS; probe++ ) {
		if ( hashTable[bucket].vertIndex == 0xFFFFFFFFu ) {
			idx = wd->vertexCount++;
			hashTable[bucket].key = key;
			hashTable[bucket].vertIndex = idx;
			v = &wd->vertices[idx];
			v->pos[0] = pos[0];
			v->pos[1] = pos[1];
			v->pos[2] = pos[2];
			v->pos[3] = 0.0f;
			VFGI_FillProceduralFeature( v->feat, pos, n );
			if ( wd->vertexCount == 1 ) {
				VectorCopy( pos, wd->worldMin );
				VectorCopy( pos, wd->worldMax );
			} else {
				int c;
				for ( c = 0; c < 3; c++ ) {
					if ( pos[c] < wd->worldMin[c] ) {
						wd->worldMin[c] = pos[c];
					}
					if ( pos[c] > wd->worldMax[c] ) {
						wd->worldMax[c] = pos[c];
					}
				}
			}
			( *hashUsed )++;
			return qtrue;
		}
		if ( hashTable[bucket].key == key ) {
			idx = hashTable[bucket].vertIndex;
			v = &wd->vertices[idx];
			if ( DistanceSquared( v->pos, pos ) < quant * quant * 0.25f ) {
				return qtrue;
			}
		}
		bucket = ( bucket + 1u ) % VFGI_HASH_BUCKETS;
	}
	return qfalse;
}

static void VFGI_EmitFace( vfgiWorldData_t *wd, vfgiHashEntry_t *hash, uint32_t *hashUsed,
	const srfSurfaceFace_t *face, float quant, uint32_t maxVerts )
{
	const unsigned *idxSrc;
	uint32_t t;
	int vi;

	if ( face->numIndices < 3 ) {
		return;
	}
	idxSrc = (const unsigned *)( (const char *)face + face->ofsIndices );
	for ( t = 0u; t < (uint32_t)face->numIndices / 3u; t++ ) {
		float n[3];
		const float *p0, *p1;
		p0 = face->points[idxSrc[t * 3u + 0]];
		p1 = face->points[idxSrc[t * 3u + 1]];
		for ( vi = 0; vi < 3; vi++ ) {
			unsigned vidx = idxSrc[t * 3u + (unsigned)vi];
			const float *p = face->points[vidx];
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
			VFGI_AddVertex( wd, hash, hashUsed, p, n, quant, maxVerts );
		}
	}
}

static void VFGI_EmitTris( vfgiWorldData_t *wd, vfgiHashEntry_t *hash, uint32_t *hashUsed,
	const srfTriangles_t *surf, float quant, uint32_t maxVerts )
{
	uint32_t t;
	int vi;

	if ( surf->numIndexes < 3 ) {
		return;
	}
	for ( t = 0u; t < (uint32_t)surf->numIndexes / 3u; t++ ) {
		float n[3];
		const float *p0, *p1;
		p0 = surf->verts[surf->indexes[(int)( t * 3u ) + 0]].xyz;
		p1 = surf->verts[surf->indexes[(int)( t * 3u ) + 1]].xyz;
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
		for ( vi = 0; vi < 3; vi++ ) {
			const float *p = surf->verts[surf->indexes[(int)( t * 3u ) + vi]].xyz;
			VFGI_AddVertex( wd, hash, hashUsed, p, n, quant, maxVerts );
		}
	}
}

static void VFGI_BuildGrid( vfgiWorldData_t *wd, uint32_t gx, uint32_t gy, uint32_t gz )
{
	uint32_t i, cx, cy, cz, cellId;
	vec3_t span;

	if ( wd->vertexCount == 0 ) {
		return;
	}

	span[0] = wd->worldMax[0] - wd->worldMin[0];
	span[1] = wd->worldMax[1] - wd->worldMin[1];
	span[2] = wd->worldMax[2] - wd->worldMin[2];
	if ( span[0] < 1.0f ) {
		span[0] = 1.0f;
	}
	if ( span[1] < 1.0f ) {
		span[1] = 1.0f;
	}
	if ( span[2] < 1.0f ) {
		span[2] = 1.0f;
	}

	wd->gridX = gx;
	wd->gridY = gy;
	wd->gridZ = gz;
	wd->cellSize[0] = span[0] / (float)gx;
	wd->cellSize[1] = span[1] / (float)gy;
	wd->cellSize[2] = span[2] / (float)gz;

	wd->cells = (vfgiGridCell_t *)ri.Malloc( (size_t)( gx * gy * gz ) * sizeof( vfgiGridCell_t ) );
	for ( i = 0; i < gx * gy * gz; i++ ) {
		wd->cells[i].count = 0;
		wd->cells[i].indices[0] = 0xFFFFFFFFu;
		wd->cells[i].indices[1] = 0xFFFFFFFFu;
		wd->cells[i].indices[2] = 0xFFFFFFFFu;
		wd->cells[i].indices[3] = 0xFFFFFFFFu;
	}

	for ( i = 0; i < wd->vertexCount; i++ ) {
		const float *p = wd->vertices[i].pos;
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
		cellId = cx + cy * gx + cz * gx * gy;
		if ( wd->cells[cellId].count < VFGI_CELL_MAX_INDICES ) {
			wd->cells[cellId].indices[wd->cells[cellId].count] = i;
			wd->cells[cellId].count++;
		}
	}
}

qboolean VFGI_World_BuildFromMap( vfgiWorldData_t *out, const world_t *w, uint32_t maxVertices,
	float quantUnits, uint32_t gridX, uint32_t gridY, uint32_t gridZ, qboolean proceduralFeatures )
{
	vfgiHashEntry_t *hashTable;
	uint32_t hashUsed = 0;
	uint32_t bi, i;
	int bmCount;
	const bmodel_t *bm;
	const msurface_t *sf;
	const surfaceType_t *st;

	(void)proceduralFeatures;

	if ( !out || !w || !w->bmodels || w->surfaces == NULL || maxVertices == 0 ) {
		return qfalse;
	}

	Com_Memset( out, 0, sizeof( *out ) );
	out->vertices = (vfgiVertexRecord_t *)ri.Malloc( (size_t)maxVertices * sizeof( vfgiVertexRecord_t ) );

	hashTable = (vfgiHashEntry_t *)ri.Malloc( VFGI_HASH_BUCKETS * sizeof( vfgiHashEntry_t ) );
	for ( i = 0; i < VFGI_HASH_BUCKETS; i++ ) {
		hashTable[i].key = 0;
		hashTable[i].vertIndex = 0xFFFFFFFFu;
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
				VFGI_EmitFace( out, hashTable, &hashUsed, (const srfSurfaceFace_t *)st, quantUnits, maxVertices );
			} else if ( *st == SF_TRIANGLES ) {
				VFGI_EmitTris( out, hashTable, &hashUsed, (const srfTriangles_t *)st, quantUnits, maxVertices );
			}
			if ( out->vertexCount >= maxVertices ) {
				break;
			}
		}
	}

	ri.Free( hashTable );

	if ( out->vertexCount == 0 ) {
		ri.Free( out->vertices );
		out->vertices = NULL;
		return qfalse;
	}

	if ( gridX < 4 ) {
		gridX = 4;
	}
	if ( gridY < 4 ) {
		gridY = 4;
	}
	if ( gridZ < 4 ) {
		gridZ = 4;
	}
	VFGI_BuildGrid( out, gridX, gridY, gridZ );
	out->valid = qtrue;
	return qtrue;
}

void VFGI_World_Free( vfgiWorldData_t *wd )
{
	if ( !wd ) {
		return;
	}
	if ( wd->vertices ) {
		ri.Free( wd->vertices );
	}
	if ( wd->cells ) {
		ri.Free( wd->cells );
	}
	Com_Memset( wd, 0, sizeof( *wd ) );
}
