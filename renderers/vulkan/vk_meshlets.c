/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Compute/CPU meshlet bake + frustum cull (chocolate). No mesh shaders required.
Bake-at-load cache stores local-space AABBs; cull transforms to world.
See docs/MESHLETS.md.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_util.h"
#include "vk_meshlets.h"
#include "vk_pass_registry.h"

#define MESHLET_CACHE_SLOTS 256
#define MESHLET_MDI_FRAME_MAX 2048

typedef struct {
	uint64_t key;
	int count;
	meshlet_t meshlets[MESHLET_MAX_PER_SURFACE];
} meshlet_cache_entry_t;

static cvar_t *r_meshlets;
static cvar_t *r_virtualGeometry;
static cvar_t *r_meshletsMdi;
static cvar_t *r_meshletsMdiDraw;
static cvar_t *r_meshletsCompact;
static cvar_t *r_meshletsLod;
static cvar_t *r_meshletsLodPixels;
static qboolean s_cmds;
static int s_bakeCount;
static int s_cacheHits;
static int s_cacheMisses;
static int s_cullVisible;
static int s_cullTotal;
static int s_lodCulled;
static int s_coneCulled;
static int s_mdiCount;
static int s_mdiTris;
static int s_mdiDrawCalls;
static int s_compactIndexes;
static int s_compactSurfaces;
static int s_virtualGeometrySurfaces;
static uint32_t s_cacheGeneration;
static meshlet_cache_entry_t s_cache[MESHLET_CACHE_SLOTS];
static meshlet_draw_cmd_t s_mdiCmds[MESHLET_MAX_PER_SURFACE];
static meshlet_draw_cmd_t s_frameCmds[MESHLET_MDI_FRAME_MAX];
static int s_frameCmdCount;
static VkBuffer s_mdiBuffer;
static VkDeviceMemory s_mdiMemory;
static void *s_mdiMapped;
static qboolean s_mdiDrawLogged;

static void Meshlets_Status_f( void )
{
	int used = 0;
	int i;

	for ( i = 0; i < MESHLET_CACHE_SLOTS; i++ ) {
		if ( s_cache[i].key ) {
			used++;
		}
	}
	ri.Printf( PRINT_ALL,
		"[VK][meshlets] active=%d bakeCalls=%d cache hits=%d misses=%d slots=%d/%d gen=%u\n"
		"  lastCull visible=%d / total=%d lodCulled=%d coneCulled=%d lod=%d mdi=%d cmds=%d tris=%d mdiDraw=%d gpuDraws=%d\n"
		"  compact=%d lastIndexes=%d surfaces=%d (stable uint64 keys — no transient pointers)\n",
		R_Meshlets_Active() ? 1 : 0, s_bakeCount, s_cacheHits, s_cacheMisses,
		used, MESHLET_CACHE_SLOTS, s_cacheGeneration,
		s_cullVisible, s_cullTotal, s_lodCulled, s_coneCulled,
		( r_meshletsLod && r_meshletsLod->integer ) ? 1 : 0,
		( r_meshletsMdi && r_meshletsMdi->integer ) ? 1 : 0,
		s_mdiCount, s_mdiTris,
		( r_meshletsMdiDraw && r_meshletsMdiDraw->integer ) ? 1 : 0,
		s_mdiDrawCalls,
		( r_meshletsCompact && r_meshletsCompact->integer ) ? 1 : 0,
		s_compactIndexes, s_compactSurfaces );
	ri.Printf( PRINT_ALL,
		"  virtualGeometry=%d meshShaderNVReady=%d path=%s\n",
		( r_virtualGeometry && r_virtualGeometry->integer ) ? s_virtualGeometrySurfaces : 0,
		( vk.meshShaderNV && qvkCmdDrawMeshTasksNV ) ? 1 : 0,
		( r_meshletsMdiDraw && r_meshletsMdiDraw->integer ) ? "meshlet_mdi" : "meshlet_compact" );
}

void R_Meshlets_Init( void )
{
	r_virtualGeometry = ri.Cvar_Get( "r_virtualGeometry", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_virtualGeometry, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_virtualGeometry,
		"Master switch for the virtual geometry renderer path. Uses portable meshlet compact/MDI draws; optional NV mesh shaders stay behind r_vk_meshShaderNV." );
	ri.Cvar_SetGroup( r_virtualGeometry, CVG_RENDERER );

	r_meshlets = ri.Cvar_Get( "r_meshlets", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_meshlets, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_meshlets,
		"Virtual geometry meshlet path: bake clusters, cull/LOD visible clusters, compact/MDI draw static dense meshes. Default 0." );
	ri.Cvar_SetGroup( r_meshlets, CVG_RENDERER );

	r_meshletsMdi = ri.Cvar_Get( "r_meshletsMdi", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_meshletsMdi, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_meshletsMdi,
		"Pack VkDrawIndexedIndirectCommand list from visible meshlets (metrics). Pair with r_meshletsMdiDraw for GPU draws." );
	ri.Cvar_SetGroup( r_meshletsMdi, CVG_RENDERER );

	r_meshletsMdiDraw = ri.Cvar_Get( "r_meshletsMdiDraw", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_meshletsMdiDraw, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_meshletsMdiDraw,
		"When r_meshlets 1: issue vkCmdDrawIndexedIndirect for visible meshlet ranges in the tess index buffer (2027 P2). "
		"Falls back to a single vkCmdDrawIndexed if the indirect entry point is missing. Implies compact index emit." );
	ri.Cvar_SetGroup( r_meshletsMdiDraw, CVG_RENDERER );

	r_meshletsCompact = ri.Cvar_Get( "r_meshletsCompact", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_meshletsCompact, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_meshletsCompact,
		"When r_meshlets 1: emit only visible meshlet triangles into tess (partial draw). Default 1." );
	ri.Cvar_SetGroup( r_meshletsCompact, CVG_RENDERER );

	r_meshletsLod = ri.Cvar_Get( "r_meshletsLod", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_meshletsLod, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_meshletsLod,
		"When r_meshlets 1: cull meshlets whose projected AABB diagonal is below r_meshletsLodPixels (screen-space LOD)." );
	ri.Cvar_SetGroup( r_meshletsLod, CVG_RENDERER );

	r_meshletsLodPixels = ri.Cvar_Get( "r_meshletsLodPixels", "2", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_meshletsLodPixels, "0.25", "64", CV_FLOAT );
	ri.Cvar_SetDescription( r_meshletsLodPixels,
		"Minimum projected AABB diagonal in pixels to keep a meshlet when r_meshletsLod 1. Default 2." );
	ri.Cvar_SetGroup( r_meshletsLodPixels, CVG_RENDERER );

	Com_Memset( s_cache, 0, sizeof( s_cache ) );
	s_bakeCount = s_cacheHits = s_cacheMisses = 0;
	s_cullVisible = s_cullTotal = s_lodCulled = s_coneCulled = 0;
	s_mdiCount = s_mdiTris = s_mdiDrawCalls = 0;
	s_compactIndexes = s_compactSurfaces = 0;
	s_virtualGeometrySurfaces = 0;
	s_frameCmdCount = 0;
	s_mdiDrawLogged = qfalse;
	s_cacheGeneration = 1;

	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "meshlet_status", Meshlets_Status_f );
		s_cmds = qtrue;
	}
	if ( r_meshlets->integer ) {
		ri.Printf( PRINT_ALL, "[VK][meshlets] r_meshlets=1 (bake-at-load + CPU cull%s%s%s%s)\n",
			( r_meshletsCompact && r_meshletsCompact->integer ) ? "; compact draw" : "",
			( r_meshletsMdi && r_meshletsMdi->integer ) ? "; MDI pack" : "",
			( r_meshletsMdiDraw && r_meshletsMdiDraw->integer ) ? "; MDI GPU draw" : "",
			( r_meshletsLod && r_meshletsLod->integer ) ? "; screen LOD" : "" );
	}
}

void R_Meshlets_Shutdown( void )
{
	if ( s_mdiMapped && s_mdiMemory != VK_NULL_HANDLE ) {
		qvkUnmapMemory( vk.device, s_mdiMemory );
		s_mdiMapped = NULL;
	}
	if ( s_mdiBuffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, s_mdiBuffer, NULL );
		s_mdiBuffer = VK_NULL_HANDLE;
	}
	if ( s_mdiMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, s_mdiMemory, NULL );
		s_mdiMemory = VK_NULL_HANDLE;
	}
	Com_Memset( s_cache, 0, sizeof( s_cache ) );
	s_frameCmdCount = 0;
}

qboolean R_Meshlets_Active( void )
{
	if ( r_virtualGeometry && !r_virtualGeometry->integer ) {
		return qfalse;
	}
	return ( r_meshlets && r_meshlets->integer ) ? qtrue : qfalse;
}

void R_Meshlets_InvalidateCache( void )
{
	Com_Memset( s_cache, 0, sizeof( s_cache ) );
	s_cacheHits = s_cacheMisses = 0;
	s_cacheGeneration++;
	if ( s_cacheGeneration == 0 ) {
		s_cacheGeneration = 1;
	}
}

uint64_t R_Meshlets_StableKey( const char *modelName, const char *surfaceName, int surfaceIndex )
{
	uint64_t h = 14695981039346656037ull;
	const char *p;

	if ( modelName ) {
		for ( p = modelName; *p; p++ ) {
			h ^= (uint64_t)(unsigned char)*p;
			h *= 1099511628211ull;
		}
	}
	h ^= 0x9e3779b97f4a7c15ull;
	if ( surfaceName ) {
		for ( p = surfaceName; *p; p++ ) {
			h ^= (uint64_t)(unsigned char)*p;
			h *= 1099511628211ull;
		}
	}
	h ^= (uint64_t)(uint32_t)surfaceIndex * 0x100000001b3ull;
	h ^= (uint64_t)s_cacheGeneration << 32;
	if ( h == 0 ) {
		h = 1;
	}
	return h;
}

int R_Meshlets_Bake( const vec3_t *positions, int numVerts, const int *indexes, int numIndexes,
	meshlet_t *out, int maxOut )
{
	int mcount = 0;
	int cursor = 0;

	if ( !positions || !indexes || !out || maxOut <= 0 || numIndexes < 3 ) {
		return 0;
	}

	s_bakeCount++;
	while ( cursor + 3 <= numIndexes && mcount < maxOut ) {
		meshlet_t *m = &out[mcount];
		int triBudget = MESHLET_MAX_TRIS;
		int vertBudget = MESHLET_MAX_VERTS;
		int localVerts = 0;
		int localTris = 0;
		int start = cursor;
		vec3_t coneAcc;
		int coneN = 0;

		ClearBounds( m->mins, m->maxs );
		VectorClear( coneAcc );
		m->firstIndex = (uint16_t)cursor;
		m->firstVert = 0;
		m->vertCount = 0;
		m->coneCutoff = -2.0f;
		m->materialClass = 0;
		VectorClear( m->coneAxis );

		while ( cursor + 2 < numIndexes && localTris < triBudget && localVerts < vertBudget ) {
			int a = indexes[cursor];
			int b = indexes[cursor + 1];
			int c = indexes[cursor + 2];
			vec3_t e0, e1, n;
			if ( a < 0 || b < 0 || c < 0 || a >= numVerts || b >= numVerts || c >= numVerts ) {
				cursor += 3;
				continue;
			}
			AddPointToBounds( positions[a], m->mins, m->maxs );
			AddPointToBounds( positions[b], m->mins, m->maxs );
			AddPointToBounds( positions[c], m->mins, m->maxs );
			VectorSubtract( positions[b], positions[a], e0 );
			VectorSubtract( positions[c], positions[a], e1 );
			CrossProduct( e0, e1, n );
			if ( VectorNormalize( n ) > 0.0f ) {
				VectorAdd( coneAcc, n, coneAcc );
				coneN++;
			}
			localTris++;
			localVerts += 3;
			cursor += 3;
		}
		m->indexCount = (uint16_t)( cursor - start );
		m->vertCount = (uint16_t)( localVerts > MESHLET_MAX_VERTS ? MESHLET_MAX_VERTS : localVerts );
		if ( coneN > 0 && VectorNormalize( coneAcc ) > 0.0f ) {
			float minDot = 1.0f;
			int t;
			VectorCopy( coneAcc, m->coneAxis );
			for ( t = start; t + 2 < cursor; t += 3 ) {
				int a = indexes[t], b = indexes[t + 1], c = indexes[t + 2];
				vec3_t e0, e1, n;
				float d;
				if ( a < 0 || b < 0 || c < 0 || a >= numVerts || b >= numVerts || c >= numVerts ) {
					continue;
				}
				VectorSubtract( positions[b], positions[a], e0 );
				VectorSubtract( positions[c], positions[a], e1 );
				CrossProduct( e0, e1, n );
				if ( VectorNormalize( n ) <= 0.0f ) {
					continue;
				}
				d = DotProduct( m->coneAxis, n );
				if ( d < minDot ) {
					minDot = d;
				}
			}
			m->coneCutoff = minDot - 0.05f;
		}
		if ( m->indexCount >= 3 ) {
			mcount++;
		} else {
			break;
		}
	}
	return mcount;
}

static meshlet_cache_entry_t *Meshlets_FindSlotKey( uint64_t key, qboolean create )
{
	int i;

	if ( key == 0 ) {
		return NULL;
	}
	for ( i = 0; i < MESHLET_CACHE_SLOTS; i++ ) {
		int idx = (int)( ( key + (uint64_t)i ) % (uint64_t)MESHLET_CACHE_SLOTS );
		if ( s_cache[idx].key == key ) {
			return &s_cache[idx];
		}
		if ( create && s_cache[idx].key == 0 ) {
			s_cache[idx].key = key;
			s_cache[idx].count = 0;
			return &s_cache[idx];
		}
	}
	if ( create ) {
		int idx = (int)( key % (uint64_t)MESHLET_CACHE_SLOTS );
		s_cache[idx].key = key;
		s_cache[idx].count = 0;
		return &s_cache[idx];
	}
	return NULL;
}

int R_Meshlets_CacheLocalKey( uint64_t key, const vec3_t *positions, int numVerts,
	const int *indexes, int numIndexes )
{
	meshlet_cache_entry_t *e;

	if ( !R_Meshlets_Active() || key == 0 ) {
		return 0;
	}
	e = Meshlets_FindSlotKey( key, qtrue );
	if ( !e ) {
		return 0;
	}
	e->count = R_Meshlets_Bake( positions, numVerts, indexes, numIndexes,
		e->meshlets, MESHLET_MAX_PER_SURFACE );
	s_cacheMisses++;
	return e->count;
}

int R_Meshlets_LookupKey( uint64_t key, const meshlet_t **outMeshlets )
{
	meshlet_cache_entry_t *e = Meshlets_FindSlotKey( key, qfalse );

	if ( !e || e->count <= 0 ) {
		s_cacheMisses++;
		if ( outMeshlets ) {
			*outMeshlets = NULL;
		}
		return 0;
	}
	s_cacheHits++;
	if ( outMeshlets ) {
		*outMeshlets = e->meshlets;
	}
	return e->count;
}

int R_Meshlets_CacheLocal( const void *key, const vec3_t *positions, int numVerts,
	const int *indexes, int numIndexes )
{
	uint64_t sk;
	if ( !key ) {
		return 0;
	}
	sk = (uint64_t)(uintptr_t)key ^ ( (uint64_t)s_cacheGeneration << 17 );
	if ( sk == 0 ) {
		sk = 1;
	}
	return R_Meshlets_CacheLocalKey( sk, positions, numVerts, indexes, numIndexes );
}

int R_Meshlets_Lookup( const void *key, const meshlet_t **outMeshlets )
{
	uint64_t sk;
	if ( !key ) {
		return 0;
	}
	sk = (uint64_t)(uintptr_t)key ^ ( (uint64_t)s_cacheGeneration << 17 );
	if ( sk == 0 ) {
		sk = 1;
	}
	return R_Meshlets_LookupKey( sk, outMeshlets );
}

static qboolean Meshlet_AABBInPlane( const vec3_t mins, const vec3_t maxs, const cplane_t *plane )
{
	int k;
	float maxDot = -1e30f;
	for ( k = 0; k < 8; k++ ) {
		vec3_t p;
		float d;
		p[0] = ( k & 1 ) ? maxs[0] : mins[0];
		p[1] = ( k & 2 ) ? maxs[1] : mins[1];
		p[2] = ( k & 4 ) ? maxs[2] : mins[2];
		d = DotProduct( p, plane->normal ) - plane->dist;
		if ( d > maxDot ) {
			maxDot = d;
		}
	}
	return ( maxDot >= 0.0f ) ? qtrue : qfalse;
}

/*
 * Screen-space LOD: keep meshlets whose world AABB projects to at least
 * r_meshletsLodPixels along the diagonal (distance / fov approx).
 */
static qboolean Meshlet_PassesScreenLod( const vec3_t mins, const vec3_t maxs )
{
	vec3_t center, delta;
	float halfDiag, dist, tanHalf, pixels;
	float thr;
	int vh;

	if ( !r_meshletsLod || !r_meshletsLod->integer ) {
		return qtrue;
	}

	center[0] = 0.5f * ( mins[0] + maxs[0] );
	center[1] = 0.5f * ( mins[1] + maxs[1] );
	center[2] = 0.5f * ( mins[2] + maxs[2] );
	delta[0] = maxs[0] - mins[0];
	delta[1] = maxs[1] - mins[1];
	delta[2] = maxs[2] - mins[2];
	halfDiag = 0.5f * (float)sqrt( (double)( delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2] ) );

	VectorSubtract( center, tr.viewParms.or.origin, delta );
	dist = VectorLength( delta );
	if ( dist < 1.0f ) {
		dist = 1.0f;
	}

	tanHalf = (float)tan( (double)( tr.viewParms.fovY * (float)( M_PI / 360.0 ) ) );
	if ( tanHalf < 1e-4f ) {
		tanHalf = 1e-4f;
	}
	vh = tr.viewParms.viewportHeight;
	if ( vh < 1 ) {
		vh = glConfig.vidHeight > 0 ? glConfig.vidHeight : 720;
	}
	pixels = ( halfDiag / dist ) * ( (float)vh / ( 2.0f * tanHalf ) );
	thr = ( r_meshletsLodPixels && r_meshletsLodPixels->value > 0.0f ) ?
		r_meshletsLodPixels->value : 2.0f;
	if ( pixels < thr ) {
		s_lodCulled++;
		return qfalse;
	}
	return qtrue;
}

int R_Meshlets_CullViewFrustum( const meshlet_t *meshlets, int count, int *visible, int maxVisible )
{
	int i, n = 0;
	int p;

	s_cullTotal = count;
	s_cullVisible = 0;
	s_lodCulled = 0;
	if ( !meshlets || !visible || maxVisible <= 0 ) {
		return 0;
	}
	for ( i = 0; i < count && n < maxVisible; i++ ) {
		qboolean inside = qtrue;
		for ( p = 0; p < 5; p++ ) {
			if ( !Meshlet_AABBInPlane( meshlets[i].mins, meshlets[i].maxs, &tr.viewParms.frustum[p] ) ) {
				inside = qfalse;
				break;
			}
		}
		if ( inside && Meshlet_PassesScreenLod( meshlets[i].mins, meshlets[i].maxs ) ) {
			visible[n++] = i;
		}
	}
	s_cullVisible = n;
	return n;
}

int R_Meshlets_CullViewFrustumXform( const meshlet_t *meshlets, int count,
	const float entityAxis[3][3], const vec3_t entityOrigin, int *visible, int maxVisible )
{
	int i, n = 0;
	int p, c;

	s_cullTotal = count;
	s_cullVisible = 0;
	s_lodCulled = 0;
	s_coneCulled = 0;
	if ( !meshlets || !visible || maxVisible <= 0 ) {
		return 0;
	}
	for ( i = 0; i < count && n < maxVisible; i++ ) {
		vec3_t wmins, wmaxs, corners[8];
		qboolean inside = qtrue;

		ClearBounds( wmins, wmaxs );
		for ( c = 0; c < 8; c++ ) {
			vec3_t local, world;
			local[0] = ( c & 1 ) ? meshlets[i].maxs[0] : meshlets[i].mins[0];
			local[1] = ( c & 2 ) ? meshlets[i].maxs[1] : meshlets[i].mins[1];
			local[2] = ( c & 4 ) ? meshlets[i].maxs[2] : meshlets[i].mins[2];
			VectorRotate( local, (const vec3_t *)entityAxis, world );
			VectorAdd( world, entityOrigin, world );
			AddPointToBounds( world, wmins, wmaxs );
			VectorCopy( world, corners[c] );
		}
		for ( p = 0; p < 5; p++ ) {
			if ( !Meshlet_AABBInPlane( wmins, wmaxs, &tr.viewParms.frustum[p] ) ) {
				inside = qfalse;
				break;
			}
		}
		/* Normal cone cull: back-facing clusters (preserves hard-edge partitions via cutoff). */
		if ( inside && meshlets[i].coneCutoff > -1.5f ) {
			vec3_t coneWorld, viewDir, center;
			float d;
			center[0] = 0.5f * ( wmins[0] + wmaxs[0] );
			center[1] = 0.5f * ( wmins[1] + wmaxs[1] );
			center[2] = 0.5f * ( wmins[2] + wmaxs[2] );
			VectorRotate( meshlets[i].coneAxis, (const vec3_t *)entityAxis, coneWorld );
			VectorSubtract( center, tr.viewParms.or.origin, viewDir );
			if ( VectorNormalize( viewDir ) > 0.0f ) {
				d = DotProduct( coneWorld, viewDir );
				if ( d < meshlets[i].coneCutoff ) {
					s_coneCulled++;
					inside = qfalse;
				}
			}
		}
		if ( inside && Meshlet_PassesScreenLod( wmins, wmaxs ) ) {
			visible[n++] = i;
		}
		(void)corners;
	}
	s_cullVisible = n;
	return n;
}

int R_Meshlets_PackIndirect( const meshlet_t *meshlets, const int *visible, int visibleCount,
	meshlet_draw_cmd_t *outCmds, int maxCmds, int32_t vertexOffset )
{
	int i;
	int n = 0;
	int tris = 0;

	s_mdiCount = 0;
	s_mdiTris = 0;
	if ( !meshlets || !visible || !outCmds || maxCmds <= 0 || visibleCount <= 0 ) {
		return 0;
	}
	if ( !r_meshletsMdi || !r_meshletsMdi->integer ) {
		return 0;
	}

	for ( i = 0; i < visibleCount && n < maxCmds; i++ ) {
		int idx = visible[i];
		meshlet_draw_cmd_t *cmd;

		if ( idx < 0 ) {
			continue;
		}
		cmd = &outCmds[n];
		cmd->indexCount = meshlets[idx].indexCount;
		cmd->instanceCount = 1u;
		cmd->firstIndex = meshlets[idx].firstIndex;
		cmd->vertexOffset = vertexOffset;
		cmd->firstInstance = 0;
		tris += (int)( meshlets[idx].indexCount / 3u );
		n++;
	}
	s_mdiCount = n;
	s_mdiTris = tris;
	if ( n > 0 && n <= MESHLET_MAX_PER_SURFACE ) {
		Com_Memcpy( s_mdiCmds, outCmds, (size_t)n * sizeof( meshlet_draw_cmd_t ) );
	}
	return n;
}

qboolean R_Meshlets_WantMdi( void )
{
	return ( R_Meshlets_Active() && r_meshletsMdi && r_meshletsMdi->integer ) ? qtrue : qfalse;
}

qboolean R_Meshlets_WantMdiDraw( void )
{
	return ( R_Meshlets_Active() && r_meshletsMdiDraw && r_meshletsMdiDraw->integer ) ? qtrue : qfalse;
}

qboolean R_Meshlets_WantCompact( void )
{
	/* MDI GPU draw reuses compact tess index emit as the IBO content. */
	if ( R_Meshlets_WantMdiDraw() ) {
		return qtrue;
	}
	return ( R_Meshlets_Active() && r_meshletsCompact && r_meshletsCompact->integer ) ? qtrue : qfalse;
}

void R_Meshlets_BeginSurface( void )
{
	s_frameCmdCount = 0;
}

static qboolean Meshlets_EnsureMdiBuffer( void )
{
	VkBufferCreateInfo bci;
	VkMemoryRequirements mr;
	VkMemoryAllocateInfo mai;
	VkResult res;
	uint32_t mem_type;
	VkDeviceSize bytes;

	if ( s_mdiBuffer != VK_NULL_HANDLE && s_mdiMapped != NULL ) {
		return qtrue;
	}
	if ( !vk.device || vk.device_lost ) {
		return qfalse;
	}

	bytes = (VkDeviceSize)MESHLET_MDI_FRAME_MAX * sizeof( meshlet_draw_cmd_t );
	Com_Memset( &bci, 0, sizeof( bci ) );
	bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.size = bytes;
	bci.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	res = qvkCreateBuffer( vk.device, &bci, NULL, &s_mdiBuffer );
	if ( res != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][meshlets] MDI buffer create failed (%d)\n" S_COLOR_WHITE, (int)res );
		s_mdiBuffer = VK_NULL_HANDLE;
		return qfalse;
	}
	qvkGetBufferMemoryRequirements( vk.device, s_mdiBuffer, &mr );
	mem_type = vk_find_memory_type( vk.physical_device, mr.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	Com_Memset( &mai, 0, sizeof( mai ) );
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.allocationSize = mr.size;
	mai.memoryTypeIndex = mem_type;
	res = qvkAllocateMemory( vk.device, &mai, NULL, &s_mdiMemory );
	if ( res != VK_SUCCESS ) {
		qvkDestroyBuffer( vk.device, s_mdiBuffer, NULL );
		s_mdiBuffer = VK_NULL_HANDLE;
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][meshlets] MDI memory alloc failed (%d)\n" S_COLOR_WHITE, (int)res );
		return qfalse;
	}
	res = qvkBindBufferMemory( vk.device, s_mdiBuffer, s_mdiMemory, 0 );
	if ( res != VK_SUCCESS ) {
		qvkFreeMemory( vk.device, s_mdiMemory, NULL );
		qvkDestroyBuffer( vk.device, s_mdiBuffer, NULL );
		s_mdiBuffer = VK_NULL_HANDLE;
		s_mdiMemory = VK_NULL_HANDLE;
		return qfalse;
	}
	res = qvkMapMemory( vk.device, s_mdiMemory, 0, bytes, 0, &s_mdiMapped );
	if ( res != VK_SUCCESS || !s_mdiMapped ) {
		qvkFreeMemory( vk.device, s_mdiMemory, NULL );
		qvkDestroyBuffer( vk.device, s_mdiBuffer, NULL );
		s_mdiBuffer = VK_NULL_HANDLE;
		s_mdiMemory = VK_NULL_HANDLE;
		s_mdiMapped = NULL;
		return qfalse;
	}
	SET_OBJECT_NAME( s_mdiBuffer, "meshlet MDI commands", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	return qtrue;
}

qboolean R_Meshlets_TryDrawIndirect( void )
{
	VkMemoryBarrier barrier;
	uint32_t drawCount;

	if ( !R_Meshlets_WantMdiDraw() || s_frameCmdCount <= 0 ) {
		return qfalse;
	}
	if ( !qvkCmdDrawIndexedIndirect ) {
		if ( !s_mdiDrawLogged ) {
			ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
				"[VK][meshlets] vkCmdDrawIndexedIndirect unavailable; falling back to single draw\n" S_COLOR_WHITE );
			s_mdiDrawLogged = qtrue;
		}
		s_frameCmdCount = 0;
		return qfalse;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE || !vk.inRenderPass ) {
		return qfalse;
	}
	if ( !Meshlets_EnsureMdiBuffer() ) {
		s_frameCmdCount = 0;
		return qfalse;
	}

	drawCount = (uint32_t)s_frameCmdCount;
	if ( drawCount > (uint32_t)MESHLET_MDI_FRAME_MAX ) {
		drawCount = (uint32_t)MESHLET_MDI_FRAME_MAX;
	}
	Com_Memcpy( s_mdiMapped, s_frameCmds, (size_t)drawCount * sizeof( meshlet_draw_cmd_t ) );

	Com_Memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
	qvkCmdPipelineBarrier( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
		0, 1, &barrier, 0, NULL, 0, NULL );

	vk_spine_pass_begin( VK_SPINE_PASS_VIRTUAL_GEOMETRY_DRAW );
	vk_spine_note_read( VK_SPINE_RES_VIRTUAL_GEOMETRY_INDIRECT,
		VK_SPINE_PASS_VIRTUAL_GEOMETRY_DRAW, VK_SPINE_ACCESS_INDIRECT_READ );
	vk_spine_note_read( VK_SPINE_RES_VIRTUAL_GEOMETRY_MESHLETS,
		VK_SPINE_PASS_VIRTUAL_GEOMETRY_DRAW, VK_SPINE_ACCESS_STORAGE_READ );
	vk_spine_note_write( VK_SPINE_RES_HDR_COLOR,
		VK_SPINE_PASS_VIRTUAL_GEOMETRY_DRAW, VK_SPINE_ACCESS_COLOR_WRITE );
	vk_spine_note_write( VK_SPINE_RES_DEPTH,
		VK_SPINE_PASS_VIRTUAL_GEOMETRY_DRAW, VK_SPINE_ACCESS_DEPTH_WRITE );

	qvkCmdDrawIndexedIndirect( vk.cmd->command_buffer, s_mdiBuffer, 0, drawCount,
		(uint32_t)sizeof( meshlet_draw_cmd_t ) );

	vk_spine_pass_end( VK_SPINE_PASS_VIRTUAL_GEOMETRY_DRAW );

	s_mdiDrawCalls += (int)drawCount;
	if ( !s_mdiDrawLogged ) {
		ri.Printf( PRINT_ALL,
			"[VK][meshlets] r_meshletsMdiDraw=1 (vkCmdDrawIndexedIndirect; tess-relative ranges)\n" );
		s_mdiDrawLogged = qtrue;
	}
	s_frameCmdCount = 0;
	return qtrue;
}

int R_Meshlets_AppendVisibleIndexes( md3Surface_t *surface, int vertexBase,
	const float entityAxis[3][3], const vec3_t entityOrigin )
{
	const meshlet_t *meshlets = NULL;
	const int *tri;
	int visible[MESHLET_MAX_PER_SURFACE];
	int mcount, vcount, i, k, written;
	int Bob;
	qboolean enqueueMdi;
	qboolean passOpen = qfalse;

	s_compactIndexes = 0;
	if ( !surface || !R_Meshlets_WantCompact() ) {
		return -1;
	}
	if ( surface->numVerts > 512 || surface->numTriangles > 1024 || surface->numTriangles < 1 ) {
		return -1;
	}
	/* Animated MD3: bind-pose meshlet AABBs are unsafe for cull/LOD — full index emit. */
	if ( backEnd.currentEntity &&
		( backEnd.currentEntity->e.frame != 0 || backEnd.currentEntity->e.oldframe != 0 ) ) {
		return -1;
	}

	mcount = R_Meshlets_Lookup( surface, &meshlets );
	if ( mcount <= 0 || !meshlets ) {
		return -1;
	}

	vk_spine_pass_begin( VK_SPINE_PASS_VIRTUAL_GEOMETRY_CULL );
	passOpen = qtrue;
	vcount = R_Meshlets_CullViewFrustumXform( meshlets, mcount, entityAxis, entityOrigin,
		visible, MESHLET_MAX_PER_SURFACE );
	vk_spine_note_read( VK_SPINE_RES_DEPTH,
		VK_SPINE_PASS_VIRTUAL_GEOMETRY_CULL, VK_SPINE_ACCESS_DEPTH_READ );
	vk_spine_note_write( VK_SPINE_RES_VIRTUAL_GEOMETRY_MESHLETS,
		VK_SPINE_PASS_VIRTUAL_GEOMETRY_CULL, VK_SPINE_ACCESS_STORAGE_WRITE );
	if ( vcount <= 0 ) {
		vk_spine_note_write( VK_SPINE_RES_VIRTUAL_GEOMETRY_INDIRECT,
			VK_SPINE_PASS_VIRTUAL_GEOMETRY_CULL, VK_SPINE_ACCESS_STORAGE_WRITE );
		vk_spine_pass_end( VK_SPINE_PASS_VIRTUAL_GEOMETRY_CULL );
		s_compactSurfaces++;
		s_virtualGeometrySurfaces++;
		return 0;
	}

	if ( R_Meshlets_WantMdi() ) {
		meshlet_draw_cmd_t cmds[MESHLET_MAX_PER_SURFACE];
		R_Meshlets_PackIndirect( meshlets, visible, vcount, cmds, MESHLET_MAX_PER_SURFACE,
			(int32_t)vertexBase );
	}
	vk_spine_note_write( VK_SPINE_RES_VIRTUAL_GEOMETRY_INDIRECT,
		VK_SPINE_PASS_VIRTUAL_GEOMETRY_CULL, VK_SPINE_ACCESS_STORAGE_WRITE );
	if ( passOpen ) {
		vk_spine_pass_end( VK_SPINE_PASS_VIRTUAL_GEOMETRY_CULL );
		passOpen = qfalse;
	}

	enqueueMdi = R_Meshlets_WantMdiDraw();
	tri = (const int *)( (const byte *)surface + surface->ofsTriangles );
	Bob = tess.numIndexes;
	written = 0;
	for ( i = 0; i < vcount; i++ ) {
		const meshlet_t *m = &meshlets[visible[i]];
		int first = (int)m->firstIndex;
		int count = (int)m->indexCount;

		if ( first < 0 || count < 3 || first + count > surface->numTriangles * 3 ) {
			continue;
		}
		if ( Bob + written + count > SHADER_MAX_INDEXES ) {
			break;
		}
		if ( enqueueMdi && s_frameCmdCount < MESHLET_MDI_FRAME_MAX ) {
			meshlet_draw_cmd_t *cmd = &s_frameCmds[s_frameCmdCount++];
			cmd->indexCount = (uint32_t)count;
			cmd->instanceCount = 1u;
			cmd->firstIndex = (uint32_t)( Bob + written );
			cmd->vertexOffset = 0; /* already remapped into tess indexes */
			cmd->firstInstance = 0;
		}
		for ( k = 0; k < count; k++ ) {
			tess.indexes[Bob + written + k] = vertexBase + tri[first + k];
		}
		written += count;
	}

	tess.numIndexes += written;
	s_compactIndexes = written;
	s_compactSurfaces++;
	s_virtualGeometrySurfaces++;
	return written;
}
