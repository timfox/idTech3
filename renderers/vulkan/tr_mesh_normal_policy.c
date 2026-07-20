/*
===========================================================================
Experimental authored / hard-edge mesh normal policy.
===========================================================================
*/

#include "tr_local.h"
#include "tr_model_gltf.h"
#include "tr_mesh_normal_policy.h"
#include "utils/mikktspace/mikktspace.h"

static cvar_t *r_meshNormalPolicy;
static cvar_t *r_preserveCustomNormals;
static cvar_t *r_hardEdgeAngle;
static cvar_t *r_splitTangentsAtHardEdges;
static cvar_t *r_meshNormalDebug;

static meshNormalStats_t s_lastStats;
static qboolean s_inited;

static void MeshNormal_SmoothIndexed( gltfPrimitive_t *prim );

/*
===============
MeshNormal_Valid
===============
*/
static qboolean MeshNormal_Valid( const vec3_t n )
{
	float lenSq = DotProduct( n, n );
	return ( lenSq > 0.01f && lenSq < 1e10f &&
		n[0] == n[0] && n[1] == n[1] && n[2] == n[2] ) ? qtrue : qfalse;
}

/*
===============
MeshNormal_FaceNormal
===============
*/
static qboolean MeshNormal_FaceNormal( const gltfVertex_t *v0, const gltfVertex_t *v1,
	const gltfVertex_t *v2, vec3_t outN, float *outArea )
{
	vec3_t e1, e2, cross;
	float len;

	VectorSubtract( v1->position, v0->position, e1 );
	VectorSubtract( v2->position, v0->position, e2 );
	CrossProduct( e1, e2, cross );
	len = VectorNormalize( cross );
	if ( len < 1e-12f ) {
		VectorSet( outN, 0.0f, 0.0f, 1.0f );
		if ( outArea ) {
			*outArea = 0.0f;
		}
		return qfalse;
	}
	VectorCopy( cross, outN );
	if ( outArea ) {
		*outArea = len * 0.5f;
	}
	return qtrue;
}

/*
===============
MeshNormal_CornerAngle
===============
*/
static float MeshNormal_CornerAngle( const vec3_t p0, const vec3_t p1, const vec3_t p2 )
{
	vec3_t a, b;
	float la, lb, d;

	VectorSubtract( p0, p1, a );
	VectorSubtract( p2, p1, b );
	la = VectorNormalize( a );
	lb = VectorNormalize( b );
	if ( la < 1e-8f || lb < 1e-8f ) {
		return 0.0f;
	}
	d = DotProduct( a, b );
	d = Com_Clamp( -1.0f, 1.0f, d );
	return acosf( d );
}

/*
===============
MeshNormal_FillMissing

Smooth-generate into a temp buffer, then copy only onto vertices that lack
valid authored normals (preserves hard-edge authored splits).
===============
*/
static void MeshNormal_FillMissing( gltfPrimitive_t *prim )
{
	int i;
	gltfVertex_t *backup;

	if ( !prim || prim->numVertices <= 0 ) {
		return;
	}

	backup = (gltfVertex_t *)ri.Hunk_AllocateTempMemory( (int)( prim->numVertices * sizeof( gltfVertex_t ) ) );
	Com_Memcpy( backup, prim->vertices, (size_t)prim->numVertices * sizeof( gltfVertex_t ) );
	MeshNormal_SmoothIndexed( prim );
	for ( i = 0; i < prim->numVertices; i++ ) {
		if ( MeshNormal_Valid( backup[i].normal ) ) {
			VectorCopy( backup[i].normal, prim->vertices[i].normal );
			VectorNormalize( prim->vertices[i].normal );
		}
	}
	ri.Hunk_FreeTempMemory( backup );
}

/*
===============
MeshNormal_CountAuthored
===============
*/
static int MeshNormal_CountAuthored( const gltfPrimitive_t *prim )
{
	int i;
	int n = 0;

	for ( i = 0; i < prim->numVertices; i++ ) {
		if ( MeshNormal_Valid( prim->vertices[i].normal ) ) {
			n++;
		}
	}
	return n;
}

/*
===============
MeshNormal_SmoothIndexed

Area + corner-angle weighted average into existing indexed vertices.
Does not create hard-edge splits.
===============
*/
static void MeshNormal_SmoothIndexed( gltfPrimitive_t *prim )
{
	int i, t, c;
	int numTris;
	vec3_t *accum;
	float *wsum;

	if ( !prim || prim->numVertices <= 0 || !prim->indices || prim->numIndices < 3 ) {
		return;
	}

	numTris = prim->numIndices / 3;
	accum = (vec3_t *)ri.Hunk_AllocateTempMemory( (int)( prim->numVertices * sizeof( vec3_t ) ) );
	wsum = (float *)ri.Hunk_AllocateTempMemory( (int)( prim->numVertices * sizeof( float ) ) );
	Com_Memset( accum, 0, (size_t)prim->numVertices * sizeof( vec3_t ) );
	Com_Memset( wsum, 0, (size_t)prim->numVertices * sizeof( float ) );

	for ( t = 0; t < numTris; t++ ) {
		uint32_t i0 = prim->indices[t * 3 + 0];
		uint32_t i1 = prim->indices[t * 3 + 1];
		uint32_t i2 = prim->indices[t * 3 + 2];
		vec3_t fn;
		float area;
		uint32_t idx[3];
		gltfVertex_t *pv[3];

		if ( i0 >= (uint32_t)prim->numVertices || i1 >= (uint32_t)prim->numVertices ||
			i2 >= (uint32_t)prim->numVertices ) {
			continue;
		}
		idx[0] = i0;
		idx[1] = i1;
		idx[2] = i2;
		pv[0] = &prim->vertices[i0];
		pv[1] = &prim->vertices[i1];
		pv[2] = &prim->vertices[i2];
		if ( !MeshNormal_FaceNormal( pv[0], pv[1], pv[2], fn, &area ) || area < 1e-12f ) {
			continue;
		}
		for ( c = 0; c < 3; c++ ) {
			float ang = MeshNormal_CornerAngle( pv[( c + 2 ) % 3]->position, pv[c]->position,
				pv[( c + 1 ) % 3]->position );
			float w = area * ang;
			if ( w < 1e-12f ) {
				w = area;
			}
			VectorMA( accum[idx[c]], w, fn, accum[idx[c]] );
			wsum[idx[c]] += w;
		}
	}

	for ( i = 0; i < prim->numVertices; i++ ) {
		if ( wsum[i] > 1e-12f ) {
			VectorNormalize( accum[i] );
			VectorCopy( accum[i], prim->vertices[i].normal );
		} else if ( !MeshNormal_Valid( prim->vertices[i].normal ) ) {
			VectorSet( prim->vertices[i].normal, 0.0f, 0.0f, 1.0f );
		} else {
			VectorNormalize( prim->vertices[i].normal );
		}
	}

	ri.Hunk_FreeTempMemory( wsum );
	ri.Hunk_FreeTempMemory( accum );
}

/*
===============
MeshNormal_GenerateCreaseSplit

Expand to unique corners, then weld only across soft edges (face angle ≤ crease).
Produces real hard-edge normal discontinuities.
===============
*/
static void MeshNormal_GenerateCreaseSplit( gltfPrimitive_t *prim, float creaseDeg,
	meshNormalStats_t *stats )
{
	int t, c, e, i;
	int numTris;
	int cornerCount;
	gltfVertex_t *corners;
	uint32_t *newIdx;
	vec3_t *faceN;
	float *faceArea;
	float creaseCos;
	int *parent;
	vec3_t *accum;
	float *wsum;
	int *remap;
	int uniqueCount;
	gltfVertex_t *outVerts;
	int hardEdges = 0;
	int softEdges = 0;

	if ( !prim || !prim->indices || prim->numIndices < 3 || prim->numVertices <= 0 ) {
		return;
	}

	numTris = prim->numIndices / 3;
	cornerCount = numTris * 3;
	creaseCos = cosf( DEG2RAD( Com_Clamp( 1.0f, 179.0f, creaseDeg ) ) );

	faceN = (vec3_t *)ri.Hunk_AllocateTempMemory( (int)( numTris * sizeof( vec3_t ) ) );
	faceArea = (float *)ri.Hunk_AllocateTempMemory( (int)( numTris * sizeof( float ) ) );
	corners = (gltfVertex_t *)ri.Hunk_AllocateTempMemory( (int)( cornerCount * sizeof( gltfVertex_t ) ) );
	newIdx = (uint32_t *)ri.Hunk_AllocateTempMemory( (int)( cornerCount * sizeof( uint32_t ) ) );
	parent = (int *)ri.Hunk_AllocateTempMemory( (int)( cornerCount * sizeof( int ) ) );

	for ( t = 0; t < numTris; t++ ) {
		uint32_t i0 = prim->indices[t * 3 + 0];
		uint32_t i1 = prim->indices[t * 3 + 1];
		uint32_t i2 = prim->indices[t * 3 + 2];
		gltfVertex_t *pv[3];

		if ( i0 >= (uint32_t)prim->numVertices || i1 >= (uint32_t)prim->numVertices ||
			i2 >= (uint32_t)prim->numVertices ) {
			VectorSet( faceN[t], 0.0f, 0.0f, 1.0f );
			faceArea[t] = 0.0f;
			for ( c = 0; c < 3; c++ ) {
				Com_Memset( &corners[t * 3 + c], 0, sizeof( gltfVertex_t ) );
				newIdx[t * 3 + c] = (uint32_t)( t * 3 + c );
				parent[t * 3 + c] = t * 3 + c;
			}
			continue;
		}
		pv[0] = &prim->vertices[i0];
		pv[1] = &prim->vertices[i1];
		pv[2] = &prim->vertices[i2];
		MeshNormal_FaceNormal( pv[0], pv[1], pv[2], faceN[t], &faceArea[t] );
		for ( c = 0; c < 3; c++ ) {
			uint32_t src = ( c == 0 ) ? i0 : ( c == 1 ) ? i1 : i2;
			corners[t * 3 + c] = prim->vertices[src];
			VectorCopy( faceN[t], corners[t * 3 + c].normal );
			newIdx[t * 3 + c] = (uint32_t)( t * 3 + c );
			parent[t * 3 + c] = t * 3 + c;
		}
	}

	/* Union soft corners that share position and face-normal angle ≤ crease. */
	for ( t = 0; t < numTris; t++ ) {
		for ( e = 0; e < 3; e++ ) {
			int a = t * 3 + e;
			int b = t * 3 + ( ( e + 1 ) % 3 );
			/* Find opposite triangle sharing this edge by position (O(n^2) — OK for props). */
			vec3_t pa, pb;
			int ot, oe;

			VectorCopy( corners[a].position, pa );
			VectorCopy( corners[b].position, pb );
			for ( ot = t + 1; ot < numTris; ot++ ) {
				for ( oe = 0; oe < 3; oe++ ) {
					int oa = ot * 3 + oe;
					int ob = ot * 3 + ( ( oe + 1 ) % 3 );
					float d00, d01, d10, d11;
					int ca, cb;
					float ndot;
					int ra, rb;

					d00 = DistanceSquared( corners[oa].position, pa );
					d01 = DistanceSquared( corners[oa].position, pb );
					d10 = DistanceSquared( corners[ob].position, pa );
					d11 = DistanceSquared( corners[ob].position, pb );
					if ( !( ( d00 < 1e-10f && d11 < 1e-10f ) || ( d01 < 1e-10f && d10 < 1e-10f ) ) ) {
						continue;
					}
					/* Match corners at same positions */
					if ( d00 < 1e-10f && d11 < 1e-10f ) {
						ca = a;
						cb = b;
					} else {
						ca = b;
						cb = a;
					}
					ndot = DotProduct( faceN[t], faceN[ot] );
					if ( ndot >= creaseCos ) {
						softEdges++;
						/* Union-find merge ca with oa, cb with ob */
						ra = ca;
						while ( parent[ra] != ra ) {
							ra = parent[ra];
						}
						rb = oa;
						while ( parent[rb] != rb ) {
							rb = parent[rb];
						}
						if ( ra != rb ) {
							parent[rb] = ra;
						}
						ra = cb;
						while ( parent[ra] != ra ) {
							ra = parent[ra];
						}
						rb = ob;
						while ( parent[rb] != rb ) {
							rb = parent[rb];
						}
						if ( ra != rb ) {
							parent[rb] = ra;
						}
					} else {
						hardEdges++;
					}
					goto next_edge;
				}
			}
next_edge:
			;
		}
	}

	/* Path compress */
	for ( i = 0; i < cornerCount; i++ ) {
		int r = i;
		while ( parent[r] != r ) {
			r = parent[r];
		}
		parent[i] = r;
	}

	accum = (vec3_t *)ri.Hunk_AllocateTempMemory( (int)( cornerCount * sizeof( vec3_t ) ) );
	wsum = (float *)ri.Hunk_AllocateTempMemory( (int)( cornerCount * sizeof( float ) ) );
	Com_Memset( accum, 0, (size_t)cornerCount * sizeof( vec3_t ) );
	Com_Memset( wsum, 0, (size_t)cornerCount * sizeof( float ) );

	for ( t = 0; t < numTris; t++ ) {
		for ( c = 0; c < 3; c++ ) {
			int ci = t * 3 + c;
			int root = parent[ci];
			float ang = MeshNormal_CornerAngle(
				corners[t * 3 + ( ( c + 2 ) % 3 )].position,
				corners[ci].position,
				corners[t * 3 + ( ( c + 1 ) % 3 )].position );
			float w = faceArea[t] * ang;
			if ( w < 1e-12f ) {
				w = faceArea[t];
			}
			if ( w < 1e-12f ) {
				continue;
			}
			VectorMA( accum[root], w, faceN[t], accum[root] );
			wsum[root] += w;
		}
	}

	for ( i = 0; i < cornerCount; i++ ) {
		int root = parent[i];
		if ( wsum[root] > 1e-12f ) {
			VectorNormalize( accum[root] );
			VectorCopy( accum[root], corners[i].normal );
		} else {
			VectorNormalize( corners[i].normal );
		}
	}

	/*
	 * Weld only corners that share soft-normal partition AND matching GPU attrs.
	 * Soft averaging may share a normal across a UV seam, but the seam still needs
	 * distinct vertices (normal continuous, UV/tangent discontinuous).
	 */
	remap = (int *)ri.Hunk_AllocateTempMemory( (int)( cornerCount * sizeof( int ) ) );
	for ( i = 0; i < cornerCount; i++ ) {
		remap[i] = -1;
	}
	uniqueCount = 0;
	for ( i = 0; i < cornerCount; i++ ) {
		int j;
		int root = parent[i];
		if ( remap[i] >= 0 ) {
			continue;
		}
		remap[i] = uniqueCount;
		for ( j = i + 1; j < cornerCount; j++ ) {
			const gltfVertex_t *a;
			const gltfVertex_t *b;
			if ( remap[j] >= 0 || parent[j] != root ) {
				continue;
			}
			a = &corners[i];
			b = &corners[j];
			if ( DistanceSquared( a->position, b->position ) >= 1e-10f ) {
				continue;
			}
			if ( DistanceSquared( a->normal, b->normal ) >= 1e-8f ) {
				continue;
			}
			if ( fabsf( a->texCoord0[0] - b->texCoord0[0] ) > 1e-6f ||
				fabsf( a->texCoord0[1] - b->texCoord0[1] ) > 1e-6f ) {
				continue;
			}
			if ( a->joints[0] != b->joints[0] || a->joints[1] != b->joints[1] ||
				a->joints[2] != b->joints[2] || a->joints[3] != b->joints[3] ) {
				continue;
			}
			if ( fabsf( a->weights[0] - b->weights[0] ) > 1e-5f ||
				fabsf( a->weights[1] - b->weights[1] ) > 1e-5f ||
				fabsf( a->weights[2] - b->weights[2] ) > 1e-5f ||
				fabsf( a->weights[3] - b->weights[3] ) > 1e-5f ) {
				continue;
			}
			remap[j] = uniqueCount;
		}
		uniqueCount++;
	}

	outVerts = (gltfVertex_t *)ri.Hunk_Alloc( (int)( uniqueCount * sizeof( gltfVertex_t ) ), h_low );
	Com_Memset( outVerts, 0, (size_t)uniqueCount * sizeof( gltfVertex_t ) );
	for ( i = 0; i < cornerCount; i++ ) {
		int dst = remap[i];
		if ( dst < 0 || dst >= uniqueCount ) {
			continue;
		}
		/* First writer wins; later matches are identical by key. */
		if ( DotProduct( outVerts[dst].normal, outVerts[dst].normal ) < 1e-12f ) {
			outVerts[dst] = corners[i];
			VectorNormalize( outVerts[dst].normal );
		}
		newIdx[i] = (uint32_t)dst;
	}

	prim->vertices = outVerts;
	prim->numVertices = uniqueCount;
	prim->indices = (uint32_t *)ri.Hunk_Alloc( (int)( cornerCount * sizeof( uint32_t ) ), h_low );
	Com_Memcpy( prim->indices, newIdx, (size_t)cornerCount * sizeof( uint32_t ) );
	prim->numIndices = cornerCount;

	if ( stats ) {
		stats->hardEdges = hardEdges;
		stats->softEdges = softEdges;
		stats->vertsAfter = uniqueCount;
		stats->normalSplits = ( uniqueCount > stats->vertsBefore ) ?
			( uniqueCount - stats->vertsBefore ) : 0;
	}

	ri.Hunk_FreeTempMemory( remap );
	ri.Hunk_FreeTempMemory( wsum );
	ri.Hunk_FreeTempMemory( accum );
	ri.Hunk_FreeTempMemory( parent );
	ri.Hunk_FreeTempMemory( newIdx );
	ri.Hunk_FreeTempMemory( corners );
	ri.Hunk_FreeTempMemory( faceArea );
	ri.Hunk_FreeTempMemory( faceN );
}

/* ---- MikkTSpace for glTF after normal finalization ---- */

typedef struct {
	gltfPrimitive_t *prim;
} mikktGltfCtx_t;

static int mikkt_gltf_GetNumFaces( const SMikkTSpaceContext *pContext )
{
	mikktGltfCtx_t *ctx = (mikktGltfCtx_t *)pContext->m_pUserData;
	return ctx->prim->numIndices / 3;
}

static int mikkt_gltf_GetNumVerticesOfFace( const SMikkTSpaceContext *pContext, const int iFace )
{
	(void)pContext;
	(void)iFace;
	return 3;
}

static void mikkt_gltf_GetPosition( const SMikkTSpaceContext *pContext, float fvPosOut[],
	const int iFace, const int iVert )
{
	mikktGltfCtx_t *ctx = (mikktGltfCtx_t *)pContext->m_pUserData;
	uint32_t idx = ctx->prim->indices[iFace * 3 + iVert];
	const float *p = ctx->prim->vertices[idx].position;
	fvPosOut[0] = p[0];
	fvPosOut[1] = p[1];
	fvPosOut[2] = p[2];
}

static void mikkt_gltf_GetNormal( const SMikkTSpaceContext *pContext, float fvNormOut[],
	const int iFace, const int iVert )
{
	mikktGltfCtx_t *ctx = (mikktGltfCtx_t *)pContext->m_pUserData;
	uint32_t idx = ctx->prim->indices[iFace * 3 + iVert];
	const float *n = ctx->prim->vertices[idx].normal;
	fvNormOut[0] = n[0];
	fvNormOut[1] = n[1];
	fvNormOut[2] = n[2];
}

static void mikkt_gltf_GetTexCoord( const SMikkTSpaceContext *pContext, float fvTexcOut[],
	const int iFace, const int iVert )
{
	mikktGltfCtx_t *ctx = (mikktGltfCtx_t *)pContext->m_pUserData;
	uint32_t idx = ctx->prim->indices[iFace * 3 + iVert];
	const float *st = ctx->prim->vertices[idx].texCoord0;
	fvTexcOut[0] = st[0];
	fvTexcOut[1] = st[1];
}

static void mikkt_gltf_SetTSpaceBasic( const SMikkTSpaceContext *pContext, const float fvTangent[],
	const float fSign, const int iFace, const int iVert )
{
	mikktGltfCtx_t *ctx = (mikktGltfCtx_t *)pContext->m_pUserData;
	uint32_t idx = ctx->prim->indices[iFace * 3 + iVert];
	gltfVertex_t *v = &ctx->prim->vertices[idx];
	v->tangent[0] = fvTangent[0];
	v->tangent[1] = fvTangent[1];
	v->tangent[2] = fvTangent[2];
	v->tangent[3] = fSign;
}

static qboolean MeshNormal_GenerateTangentsMikk( gltfPrimitive_t *prim )
{
	SMikkTSpaceInterface iface;
	SMikkTSpaceContext ctx;
	mikktGltfCtx_t user;

	if ( !prim || !prim->indices || prim->numIndices < 3 ) {
		return qfalse;
	}

	Com_Memset( &iface, 0, sizeof( iface ) );
	iface.m_getNumFaces = mikkt_gltf_GetNumFaces;
	iface.m_getNumVerticesOfFace = mikkt_gltf_GetNumVerticesOfFace;
	iface.m_getPosition = mikkt_gltf_GetPosition;
	iface.m_getNormal = mikkt_gltf_GetNormal;
	iface.m_getTexCoord = mikkt_gltf_GetTexCoord;
	iface.m_setTSpaceBasic = mikkt_gltf_SetTSpaceBasic;

	user.prim = prim;
	Com_Memset( &ctx, 0, sizeof( ctx ) );
	ctx.m_pInterface = &iface;
	ctx.m_pUserData = &user;

	return genTangSpaceDefault( &ctx ) ? qtrue : qfalse;
}

/*
===============
R_MeshNormalPolicy_ProcessGLTFPrimitive
===============
*/
void R_MeshNormalPolicy_ProcessGLTFPrimitive( gltfPrimitive_t *prim, const char *modelName,
	meshNormalStats_t *outStats )
{
	meshNormalPolicy_t policy;
	meshNormalStats_t stats;
	int authored;
	int invalid = 0;
	int i;
	qboolean needGenerate;
	qboolean forceCrease;
	float crease;

	Com_Memset( &stats, 0, sizeof( stats ) );
	Q_strncpyz( stats.sourceHint, "gltf", sizeof( stats.sourceHint ) );

	if ( !prim || !prim->vertices || prim->numVertices <= 0 ) {
		if ( outStats ) {
			*outStats = stats;
		}
		return;
	}

	/* Expanding topology would invalidate morph target delta arrays — skip those prims. */
	if ( prim->numMorphTargets > 0 && prim->morphTargets ) {
		stats.vertsAfter = prim->numVertices;
		Q_strncpyz( stats.sourceHint, "gltf_morph_skip", sizeof( stats.sourceHint ) );
		if ( outStats ) {
			*outStats = stats;
		}
		return;
	}

	policy = R_MeshNormalPolicy_Get();
	stats.effectivePolicy = policy;
	stats.creaseAngleDeg = R_MeshNormalPolicy_HardEdgeAngleDeg();
	stats.vertsBefore = prim->numVertices;
	stats.vertsAfter = prim->numVertices;
	authored = MeshNormal_CountAuthored( prim );
	stats.authoredNormals = authored;

	for ( i = 0; i < prim->numVertices; i++ ) {
		if ( !MeshNormal_Valid( prim->vertices[i].normal ) ) {
			invalid++;
		}
	}
	stats.invalidNormals = invalid;

	if ( policy == MESH_NORMAL_POLICY_LEGACY ) {
		if ( outStats ) {
			*outStats = stats;
		}
		return;
	}

	crease = R_MeshNormalPolicy_HardEdgeAngleDeg();
	needGenerate = qfalse;
	forceCrease = qfalse;

	if ( policy == MESH_NORMAL_POLICY_FORCE_ANGLE ) {
		needGenerate = qtrue;
		forceCrease = qtrue;
	} else if ( policy == MESH_NORMAL_POLICY_PRESERVE || policy == MESH_NORMAL_POLICY_DEBUG ) {
		if ( r_preserveCustomNormals && r_preserveCustomNormals->integer && authored > 0 ) {
			/* Normalize authored; fill holes without destroying splits. */
			for ( i = 0; i < prim->numVertices; i++ ) {
				if ( MeshNormal_Valid( prim->vertices[i].normal ) ) {
					VectorNormalize( prim->vertices[i].normal );
				}
			}
			if ( invalid > 0 ) {
				MeshNormal_FillMissing( prim );
			}
		} else if ( authored == 0 ) {
			needGenerate = qtrue;
			forceCrease = qfalse;
		}
	} else if ( policy == MESH_NORMAL_POLICY_PRESERVE_OR_ANGLE ) {
		if ( r_preserveCustomNormals && r_preserveCustomNormals->integer && authored > prim->numVertices / 2 ) {
			for ( i = 0; i < prim->numVertices; i++ ) {
				if ( MeshNormal_Valid( prim->vertices[i].normal ) ) {
					VectorNormalize( prim->vertices[i].normal );
				}
			}
			if ( invalid > 0 ) {
				MeshNormal_FillMissing( prim );
			}
		} else {
			needGenerate = qtrue;
			forceCrease = qtrue;
		}
	}

	if ( needGenerate ) {
		if ( forceCrease ) {
			MeshNormal_GenerateCreaseSplit( prim, crease, &stats );
		} else {
			MeshNormal_SmoothIndexed( prim );
			stats.vertsAfter = prim->numVertices;
		}
	}

	if ( R_MeshNormalPolicy_SplitTangentsAtHardEdges() && prim->numIndices >= 3 ) {
		if ( MeshNormal_GenerateTangentsMikk( prim ) ) {
			stats.tangentGenerated = 1;
		}
	}

	if ( policy == MESH_NORMAL_POLICY_DEBUG && modelName ) {
		ri.Printf( PRINT_DEVELOPER,
			"[VK][meshNormal] %s verts %d→%d authored=%d invalid=%d hardE=%d softE=%d mikkt=%d policy=%d crease=%.1f\n",
			modelName, stats.vertsBefore, stats.vertsAfter, stats.authoredNormals, stats.invalidNormals,
			stats.hardEdges, stats.softEdges, stats.tangentGenerated, (int)policy, crease );
	}

	s_lastStats = stats;
	if ( outStats ) {
		*outStats = stats;
	}
}

/*
===============
R_MeshNormalPolicy_ProcessGLTFModel
===============
*/
void R_MeshNormalPolicy_ProcessGLTFModel( gltfModel_t *model, const char *modelName )
{
	int mi, pi;
	meshNormalStats_t total;
	meshNormalStats_t local;

	if ( !model || !R_MeshNormalPolicy_Active() ) {
		return;
	}

	Com_Memset( &total, 0, sizeof( total ) );
	Q_strncpyz( total.sourceHint, "gltf", sizeof( total.sourceHint ) );
	total.effectivePolicy = R_MeshNormalPolicy_Get();
	total.creaseAngleDeg = R_MeshNormalPolicy_HardEdgeAngleDeg();

	for ( mi = 0; mi < model->numMeshes; mi++ ) {
		gltfMesh_t *mesh = &model->meshes[mi];
		for ( pi = 0; pi < mesh->numPrimitives; pi++ ) {
			R_MeshNormalPolicy_ProcessGLTFPrimitive( &mesh->primitives[pi], modelName, &local );
			total.vertsBefore += local.vertsBefore;
			total.vertsAfter += local.vertsAfter;
			total.authoredNormals += local.authoredNormals;
			total.invalidNormals += local.invalidNormals;
			total.hardEdges += local.hardEdges;
			total.softEdges += local.softEdges;
			total.normalSplits += local.normalSplits;
			total.tangentGenerated += local.tangentGenerated;
		}
	}

	s_lastStats = total;
	if ( r_meshNormalDebug && r_meshNormalDebug->integer ) {
		ri.Printf( PRINT_ALL,
			"[VK][meshNormal] model=%s policy=%d verts %d→%d authored=%d hardE=%d softE=%d mikktPrims=%d\n",
			modelName ? modelName : "?", (int)total.effectivePolicy, total.vertsBefore, total.vertsAfter,
			total.authoredNormals, total.hardEdges, total.softEdges, total.tangentGenerated );
	}
}

meshNormalPolicy_t R_MeshNormalPolicy_Get( void )
{
	int v = r_meshNormalPolicy ? r_meshNormalPolicy->integer : 0;
	if ( v < 0 ) {
		v = 0;
	}
	if ( v > 4 ) {
		v = 4;
	}
	return (meshNormalPolicy_t)v;
}

qboolean R_MeshNormalPolicy_PreserveAuthored( void )
{
	return ( r_preserveCustomNormals && r_preserveCustomNormals->integer ) ? qtrue : qfalse;
}

float R_MeshNormalPolicy_HardEdgeAngleDeg( void )
{
	return r_hardEdgeAngle ? r_hardEdgeAngle->value : 60.0f;
}

qboolean R_MeshNormalPolicy_SplitTangentsAtHardEdges( void )
{
	return ( r_splitTangentsAtHardEdges && r_splitTangentsAtHardEdges->integer ) ? qtrue : qfalse;
}

qboolean R_MeshNormalPolicy_Active( void )
{
	return ( R_MeshNormalPolicy_Get() != MESH_NORMAL_POLICY_LEGACY ) ? qtrue : qfalse;
}

void R_MeshNormalStatus_f( void )
{
	const char *name = ( Cmd_Argc() >= 2 ) ? Cmd_Argv( 1 ) : "(last)";
	ri.Printf( PRINT_ALL,
		"[VK][meshNormal] model=%s policy=%d crease=%.1f preserve=%d splitTangents=%d\n"
		"  verts %d→%d authored=%d invalid=%d hardEdges=%d softEdges=%d splits=%d mikkt=%d\n"
		"  hint=%s (MD3/BSP untouched; glTF/GLB processed when policy!=0)\n",
		name,
		(int)R_MeshNormalPolicy_Get(),
		R_MeshNormalPolicy_HardEdgeAngleDeg(),
		R_MeshNormalPolicy_PreserveAuthored() ? 1 : 0,
		R_MeshNormalPolicy_SplitTangentsAtHardEdges() ? 1 : 0,
		s_lastStats.vertsBefore, s_lastStats.vertsAfter,
		s_lastStats.authoredNormals, s_lastStats.invalidNormals,
		s_lastStats.hardEdges, s_lastStats.softEdges, s_lastStats.normalSplits,
		s_lastStats.tangentGenerated,
		s_lastStats.sourceHint[0] ? s_lastStats.sourceHint : "none" );
}

void R_MeshNormalPolicy_Init( void )
{
	if ( s_inited ) {
		return;
	}

	r_meshNormalPolicy = ri.Cvar_Get( "r_meshNormalPolicy", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_meshNormalPolicy, "0", "4", CV_INTEGER );
	ri.Cvar_SetDescription( r_meshNormalPolicy,
		"Experimental mesh normal policy (latched; vid_restart / model reload):\n"
		" 0 - legacy certified (default; no post-process)\n"
		" 1 - preserve authored glTF normals; fill missing with smooth\n"
		" 2 - preserve authored; else crease-angle generate with hard-edge splits\n"
		" 3 - force crease-angle generate + splits\n"
		" 4 - preserve + developer comparison logs\n"
		"Does not change MD3/BSP certified paths." );
	ri.Cvar_SetGroup( r_meshNormalPolicy, CVG_RENDERER );

	r_preserveCustomNormals = ri.Cvar_Get( "r_preserveCustomNormals", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_preserveCustomNormals, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_preserveCustomNormals,
		"When mesh normal policy ≥ 1, keep valid authored NORMAL attributes from glTF." );
	ri.Cvar_SetGroup( r_preserveCustomNormals, CVG_RENDERER );

	r_hardEdgeAngle = ri.Cvar_Get( "r_hardEdgeAngle", "60", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_hardEdgeAngle, "1", "179", CV_FLOAT );
	ri.Cvar_SetDescription( r_hardEdgeAngle,
		"Crease angle in degrees for generated hard edges (policies 2–3)." );
	ri.Cvar_SetGroup( r_hardEdgeAngle, CVG_RENDERER );

	r_splitTangentsAtHardEdges = ri.Cvar_Get( "r_splitTangentsAtHardEdges", "1",
		CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_splitTangentsAtHardEdges, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_splitTangentsAtHardEdges,
		"After normal policy processing, regenerate glTF tangents with MikkTSpace "
		"(uses final normals; UV seams create tangent discontinuities)." );
	ri.Cvar_SetGroup( r_splitTangentsAtHardEdges, CVG_RENDERER );

	r_meshNormalDebug = ri.Cvar_Get( "r_meshNormalDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_meshNormalDebug, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_meshNormalDebug, "Log mesh normal policy stats on glTF load." );
	ri.Cvar_SetGroup( r_meshNormalDebug, CVG_RENDERER );

	ri.Cmd_AddCommand( "mesh_normal_status", R_MeshNormalStatus_f );

	Com_Memset( &s_lastStats, 0, sizeof( s_lastStats ) );
	s_inited = qtrue;

	ri.Printf( PRINT_ALL,
		"[VK][meshNormal] policy=%d (0=legacy certified; experimental hard-edge path opt-in)\n",
		r_meshNormalPolicy->integer );
}

void R_MeshNormalPolicy_Shutdown( void )
{
	if ( !s_inited ) {
		return;
	}
	ri.Cmd_RemoveCommand( "mesh_normal_status" );
	Com_Memset( &s_lastStats, 0, sizeof( s_lastStats ) );
	s_inited = qfalse;
}
