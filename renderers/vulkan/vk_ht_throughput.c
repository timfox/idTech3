/*
===========================================================================
High-Throughput Raster Engine 1.0 — Slice A implementation.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_ht_throughput.h"
#include "vk_gpu_scene.h"
#include "vk_forward_plus.h"

static cvar_t *r_htThroughput;
static cvar_t *r_htDecalBin;
static cvar_t *r_htMergeDraws;
static cvar_t *r_htResValidate;
static cvar_t *r_htDebug;

static vkHtResourceSlot_t s_res[VK_HT_RES_MAX];
static uint32_t s_resNext = VK_HT_RES_USER_BASE;
static uint32_t s_resAlive;
static uint32_t s_resHighWater;

static vkHtDecalProj_t s_decals[VK_HT_DECAL_MAX];
static uint32_t s_decalCount;
static uint32_t s_decalNext;

/* Per-cluster packed indices (host mirror of Forward+ tile grid). */
static uint32_t s_decalBins[256u * 256u * VK_HT_DECAL_MAX_PER_CLUSTER];
static uint32_t s_decalBinCounts[256u * 256u];
static uint32_t s_decalTilesX;
static uint32_t s_decalTilesY;

static vkHtThroughputStats_t s_stats;
static qboolean s_cmds;
static qboolean s_logged;

void vk_ht_throughput_register_cvars( void )
{
	if ( r_htThroughput ) {
		return;
	}

	r_htThroughput = ri.Cvar_Get( "r_htThroughput", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_htThroughput, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_htThroughput,
		"High-Throughput Raster Engine 1.0 Slice A (latched).\n"
		"Global resource indices, decal binning, GPU-scene draw merge, ht_status.\n"
		"Opt-in only — does not change certified modern_vulkan.cfg boot." );
	ri.Cvar_SetGroup( r_htThroughput, CVG_RENDERER );

	r_htDecalBin = ri.Cvar_Get( "r_htDecalBin", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_htDecalBin, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_htDecalBin,
		"When r_htThroughput 1: bin decal projections onto the Forward+ cluster grid." );

	r_htMergeDraws = ri.Cvar_Get( "r_htMergeDraws", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_htMergeDraws, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_htMergeDraws,
		"When r_htThroughput 1: merge compatible GPU-scene indirect draws (same index range)." );

	r_htResValidate = ri.Cvar_Get( "r_htResValidate", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_htResValidate, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_htResValidate,
		"Validate global resource indices; out-of-range → documented fallback (never index 0)." );

	r_htDebug = ri.Cvar_Get( "r_htDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_htDebug, "0", "3", CV_INTEGER );
}

qboolean vk_ht_throughput_active( void )
{
	return ( r_htThroughput && r_htThroughput->integer ) ? qtrue : qfalse;
}

const vkHtThroughputStats_t *vk_ht_throughput_stats( void )
{
	return &s_stats;
}

uint32_t vk_ht_res_fallback( vkHtResKind_t kind )
{
	switch ( kind ) {
	case VK_HT_RES_KIND_TEXTURE:
		return VK_HT_RES_FALLBACK_WHITE;
	case VK_HT_RES_KIND_MATERIAL:
		return VK_HT_RES_FALLBACK_MATERIAL;
	case VK_HT_RES_KIND_MESH:
	case VK_HT_RES_KIND_BUFFER:
		return VK_HT_RES_FALLBACK_GEOMETRY;
	case VK_HT_RES_KIND_SKELETON:
		return VK_HT_RES_FALLBACK_ANIM;
	default:
		return VK_HT_RES_FALLBACK_WHITE;
	}
}

static void HT_InitFallbackSlots( void )
{
	uint32_t i;
	static const vkHtResKind_t kinds[] = {
		VK_HT_RES_KIND_NONE,
		VK_HT_RES_KIND_TEXTURE,  /* white */
		VK_HT_RES_KIND_TEXTURE,  /* black */
		VK_HT_RES_KIND_TEXTURE,  /* flat normal */
		VK_HT_RES_KIND_TEXTURE,  /* neutral rough */
		VK_HT_RES_KIND_MATERIAL,
		VK_HT_RES_KIND_MESH,
		VK_HT_RES_KIND_SKELETON
	};

	Com_Memset( s_res, 0, sizeof( s_res ) );
	s_res[VK_HT_RES_INVALID].index = VK_HT_RES_INVALID;
	s_res[VK_HT_RES_INVALID].alive = qfalse;

	for ( i = 1; i <= VK_HT_RES_FALLBACK_ANIM; i++ ) {
		s_res[i].index = i;
		s_res[i].generation = 1;
		s_res[i].kind = kinds[i];
		s_res[i].alive = qtrue;
		s_res[i].flags = 1u; /* fallback */
	}
	s_resAlive = VK_HT_RES_FALLBACK_ANIM;
	s_resHighWater = VK_HT_RES_FALLBACK_ANIM;
	s_resNext = VK_HT_RES_USER_BASE;
}

uint32_t vk_ht_res_alloc( vkHtResKind_t kind )
{
	uint32_t i;
	uint32_t start;

	if ( !vk_ht_throughput_active() || kind == VK_HT_RES_KIND_NONE ) {
		return VK_HT_RES_INVALID;
	}
	start = s_resNext;
	for ( i = 0; i < ( VK_HT_RES_MAX - VK_HT_RES_USER_BASE ); i++ ) {
		uint32_t idx = start + i;
		if ( idx >= VK_HT_RES_MAX ) {
			idx = VK_HT_RES_USER_BASE + ( idx - VK_HT_RES_MAX );
		}
		if ( idx < VK_HT_RES_USER_BASE ) {
			continue;
		}
		if ( !s_res[idx].alive ) {
			s_res[idx].index = idx;
			s_res[idx].generation++;
			if ( s_res[idx].generation == 0u ) {
				s_res[idx].generation = 1u;
			}
			s_res[idx].kind = kind;
			s_res[idx].alive = qtrue;
			s_res[idx].flags = 0u;
			s_resAlive++;
			if ( idx > s_resHighWater ) {
				s_resHighWater = idx;
			}
			s_resNext = idx + 1u;
			if ( s_resNext >= VK_HT_RES_MAX ) {
				s_resNext = VK_HT_RES_USER_BASE;
			}
			return idx;
		}
	}
	s_stats.overflowFlag = qtrue;
	s_stats.resInvalidLookups++;
	return vk_ht_res_fallback( kind );
}

void vk_ht_res_free( uint32_t index )
{
	if ( index < VK_HT_RES_USER_BASE || index >= VK_HT_RES_MAX ) {
		return;
	}
	if ( !s_res[index].alive ) {
		return;
	}
	s_res[index].alive = qfalse;
	s_res[index].kind = VK_HT_RES_KIND_NONE;
	if ( s_resAlive > 0u ) {
		s_resAlive--;
	}
}

uint32_t vk_ht_res_resolve( uint32_t index, vkHtResKind_t expectedKind, uint32_t fallbackIndex )
{
	uint32_t fb = fallbackIndex ? fallbackIndex : vk_ht_res_fallback( expectedKind );

	if ( index == VK_HT_RES_INVALID ) {
		s_stats.resInvalidLookups++;
		s_stats.resFallbackUses++;
		return fb;
	}
	if ( !r_htResValidate || !r_htResValidate->integer ) {
		return index;
	}
	if ( index >= VK_HT_RES_MAX || !s_res[index].alive ) {
		s_stats.resInvalidLookups++;
		s_stats.resFallbackUses++;
		return fb;
	}
	if ( expectedKind != VK_HT_RES_KIND_NONE && s_res[index].kind != expectedKind &&
		index >= VK_HT_RES_USER_BASE ) {
		s_stats.resInvalidLookups++;
		s_stats.resFallbackUses++;
		return fb;
	}
	return index;
}

uint32_t vk_ht_decal_register( const vec3_t origin, float radius, const vec3_t normal,
	float halfExtent, uint32_t materialId, uint32_t objectId )
{
	vkHtDecalProj_t *d;
	uint32_t i;

	if ( !vk_ht_throughput_active() || !origin ) {
		return 0;
	}
	if ( s_decalCount >= VK_HT_DECAL_MAX ) {
		s_stats.decalRejected++;
		s_stats.overflowFlag = qtrue;
		return 0;
	}
	for ( i = 0; i < VK_HT_DECAL_MAX; i++ ) {
		uint32_t slot = ( s_decalNext + i ) % VK_HT_DECAL_MAX;
		if ( !s_decals[slot].alive ) {
			d = &s_decals[slot];
			Com_Memset( d, 0, sizeof( *d ) );
			VectorCopy( origin, d->origin );
			d->radius = radius > 0.0f ? radius : 16.0f;
			if ( normal ) {
				VectorCopy( normal, d->normal );
			} else {
				VectorSet( d->normal, 0.0f, 0.0f, 1.0f );
			}
			d->halfExtent = halfExtent > 0.0f ? halfExtent : d->radius;
			d->materialId = materialId ? materialId : VK_HT_RES_FALLBACK_MATERIAL;
			d->objectId = objectId;
			d->alive = qtrue;
			s_decalCount++;
			s_decalNext = ( slot + 1u ) % VK_HT_DECAL_MAX;
			return slot + 1u; /* 1-based handle; 0 = invalid */
		}
	}
	s_stats.decalRejected++;
	s_stats.overflowFlag = qtrue;
	return 0;
}

void vk_ht_decal_clear( void )
{
	Com_Memset( s_decals, 0, sizeof( s_decals ) );
	s_decalCount = 0;
	s_decalNext = 0;
	Com_Memset( s_decalBins, 0, sizeof( s_decalBins ) );
	Com_Memset( s_decalBinCounts, 0, sizeof( s_decalBinCounts ) );
}

void vk_ht_decal_bin_for_view( void )
{
	uint32_t i;
	uint32_t tilesX, tilesY, flat;
	uint32_t maxClusters;
	float vw, vh;

	s_stats.decalSubmitted = 0;
	s_stats.decalCulled = 0;
	s_stats.decalBinsTouched = 0;
	s_stats.decalListEntries = 0;
	s_stats.decalMaxOccupancy = 0;
	s_stats.decalOverflow = 0;

	if ( !vk_ht_throughput_active() || !r_htDecalBin || !r_htDecalBin->integer ) {
		return;
	}

	tilesX = vk.forward_plus.tiles_x;
	tilesY = vk.forward_plus.tiles_y;
	if ( tilesX < 1u ) {
		tilesX = 1u;
	}
	if ( tilesY < 1u ) {
		tilesY = 1u;
	}
	if ( tilesX > 256u ) {
		tilesX = 256u;
	}
	if ( tilesY > 256u ) {
		tilesY = 256u;
	}
	s_decalTilesX = tilesX;
	s_decalTilesY = tilesY;
	flat = tilesX * tilesY;
	maxClusters = flat;
	if ( maxClusters > ( 256u * 256u ) ) {
		maxClusters = 256u * 256u;
	}

	Com_Memset( s_decalBinCounts, 0, sizeof( uint32_t ) * maxClusters );

	vw = (float)vk.renderWidth;
	vh = (float)vk.renderHeight;
	if ( vw < 1.0f ) {
		vw = 1280.0f;
	}
	if ( vh < 1.0f ) {
		vh = 720.0f;
	}

	for ( i = 0; i < VK_HT_DECAL_MAX; i++ ) {
		const vkHtDecalProj_t *d = &s_decals[i];
		float ndcX, ndcY;
		float px, py;
		float radiusPx;
		int tx0, ty0, tx1, ty1;
		int tx, ty;
		vec3_t viewDelta;
		float dist;

		if ( !d->alive ) {
			continue;
		}
		s_stats.decalSubmitted++;

		VectorSubtract( d->origin, backEnd.viewParms.or.origin, viewDelta );
		dist = VectorLength( viewDelta );
		if ( dist > d->radius + backEnd.viewParms.zFar ) {
			s_stats.decalCulled++;
			continue;
		}
		radiusPx = ( d->radius * ( 0.5f * vw ) ) / MAX( dist, 1.0f );
		if ( radiusPx < 0.5f && dist > d->radius * 4.0f ) {
			s_stats.decalCulled++;
			continue;
		}

		ndcX = DotProduct( viewDelta, backEnd.viewParms.or.axis[1] ) / MAX( dist, 1.0f );
		ndcY = DotProduct( viewDelta, backEnd.viewParms.or.axis[2] ) / MAX( dist, 1.0f );
		px = ( ndcX * 0.5f + 0.5f ) * vw;
		py = ( ndcY * 0.5f + 0.5f ) * vh;

		tx0 = (int)( ( px - radiusPx ) / ( vw / (float)tilesX ) );
		ty0 = (int)( ( py - radiusPx ) / ( vh / (float)tilesY ) );
		tx1 = (int)( ( px + radiusPx ) / ( vw / (float)tilesX ) );
		ty1 = (int)( ( py + radiusPx ) / ( vh / (float)tilesY ) );
		if ( tx0 < 0 ) {
			tx0 = 0;
		}
		if ( ty0 < 0 ) {
			ty0 = 0;
		}
		if ( tx1 >= (int)tilesX ) {
			tx1 = (int)tilesX - 1;
		}
		if ( ty1 >= (int)tilesY ) {
			ty1 = (int)tilesY - 1;
		}
		if ( tx0 > tx1 || ty0 > ty1 ) {
			s_stats.decalCulled++;
			continue;
		}

		for ( ty = ty0; ty <= ty1; ty++ ) {
			for ( tx = tx0; tx <= tx1; tx++ ) {
				uint32_t bin = (uint32_t)ty * tilesX + (uint32_t)tx;
				uint32_t occ;

				if ( bin >= maxClusters ) {
					continue;
				}
				occ = s_decalBinCounts[bin];
				if ( occ == 0u ) {
					s_stats.decalBinsTouched++;
				}
				if ( occ >= VK_HT_DECAL_MAX_PER_CLUSTER ) {
					s_stats.decalOverflow++;
					s_stats.overflowFlag = qtrue;
					continue;
				}
				s_decalBins[bin * VK_HT_DECAL_MAX_PER_CLUSTER + occ] = i;
				s_decalBinCounts[bin] = occ + 1u;
				s_stats.decalListEntries++;
				if ( occ + 1u > s_stats.decalMaxOccupancy ) {
					s_stats.decalMaxOccupancy = occ + 1u;
				}
			}
		}
	}
}

void vk_ht_merge_gpu_scene_draws( void )
{
	uint32_t inC = 0, outC = 0, groups = 0, skip = 0;

	s_stats.mergeCmdsIn = 0;
	s_stats.mergeCmdsOut = 0;
	s_stats.mergeGroups = 0;
	s_stats.mergeSkippedIncompatible = 0;

	if ( !vk_ht_throughput_active() || !r_htMergeDraws || !r_htMergeDraws->integer ) {
		return;
	}
	if ( !vk_gpu_scene_active() ) {
		return;
	}

	vk_gpu_scene_merge_compatible_draws( &inC, &outC, &groups, &skip );
	s_stats.mergeCmdsIn = inC;
	s_stats.mergeCmdsOut = outC;
	s_stats.mergeGroups = groups;
	s_stats.mergeSkippedIncompatible = skip;
}

void vk_ht_throughput_begin_frame( void )
{
	if ( !vk_ht_throughput_active() ) {
		return;
	}
	s_stats.overflowFlag = qfalse;
	s_stats.gpuVisible = 0;
	s_stats.gpuIndirect = 0;
	s_stats.meshletGpuDraws = 0;
}

void vk_ht_throughput_end_frame( void )
{
	if ( !vk_ht_throughput_active() ) {
		return;
	}

	/* Decal bin after lights have a known tile grid. Merge runs earlier (post cull). */
	vk_ht_decal_bin_for_view();

	s_stats.resCapacity = VK_HT_RES_MAX;
	s_stats.resAlive = s_resAlive;
	s_stats.resHighWater = s_resHighWater;
	s_stats.gpuVisible = vk_gpu_scene_visible_count();
	s_stats.gpuIndirect = vk_gpu_scene_indirect_count();
}

void vk_ht_throughput_init( void )
{
	vk_ht_throughput_register_cvars();
	Com_Memset( &s_stats, 0, sizeof( s_stats ) );
	HT_InitFallbackSlots();
	vk_ht_decal_clear();

	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "ht_status", vk_ht_throughput_status_f );
		ri.Cmd_AddCommand( "ht_decal_status", vk_ht_decal_status_f );
		ri.Cmd_AddCommand( "ht_res_status", vk_ht_res_status_f );
		s_cmds = qtrue;
	}

	if ( vk_ht_throughput_active() && !s_logged ) {
		ri.Printf( PRINT_ALL,
			"[VK][HT] High-Throughput 1.0 Slice A: active "
			"(res indices, decalBin=%s, merge=%s). Classic BSP preserved. RT opt-in only.\n",
			( r_htDecalBin && r_htDecalBin->integer ) ? "on" : "off",
			( r_htMergeDraws && r_htMergeDraws->integer ) ? "on" : "off" );
		s_logged = qtrue;
	}
}

void vk_ht_throughput_shutdown( void )
{
	if ( s_cmds ) {
		ri.Cmd_RemoveCommand( "ht_status" );
		ri.Cmd_RemoveCommand( "ht_decal_status" );
		ri.Cmd_RemoveCommand( "ht_res_status" );
		s_cmds = qfalse;
	}
	vk_ht_decal_clear();
	Com_Memset( &s_stats, 0, sizeof( s_stats ) );
	Com_Memset( s_res, 0, sizeof( s_res ) );
	s_logged = qfalse;
}

void vk_ht_res_status_f( void )
{
	ri.Printf( PRINT_ALL, "=== HT Global Resource Indices ===\n" );
	ri.Printf( PRINT_ALL, "capacity     : %u (user base %u)\n", VK_HT_RES_MAX, VK_HT_RES_USER_BASE );
	ri.Printf( PRINT_ALL, "alive        : %u highWater=%u\n", s_resAlive, s_resHighWater );
	ri.Printf( PRINT_ALL, "invalid      : %u fallbackUses=%u\n",
		s_stats.resInvalidLookups, s_stats.resFallbackUses );
	ri.Printf( PRINT_ALL, "fallbacks    : white=%u black=%u flatN=%u rough=%u mat=%u geo=%u anim=%u\n",
		VK_HT_RES_FALLBACK_WHITE, VK_HT_RES_FALLBACK_BLACK, VK_HT_RES_FALLBACK_FLAT_N,
		VK_HT_RES_FALLBACK_ROUGH, VK_HT_RES_FALLBACK_MATERIAL, VK_HT_RES_FALLBACK_GEOMETRY,
		VK_HT_RES_FALLBACK_ANIM );
	ri.Printf( PRINT_ALL, "index 0      : INVALID (never a texture/material)\n" );
}

void vk_ht_decal_status_f( void )
{
	ri.Printf( PRINT_ALL, "=== HT Decal Binning (shared Forward+ grid) ===\n" );
	ri.Printf( PRINT_ALL, "active       : %s\n",
		( vk_ht_throughput_active() && r_htDecalBin && r_htDecalBin->integer ) ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "projections  : alive=%u / %u\n", s_decalCount, VK_HT_DECAL_MAX );
	ri.Printf( PRINT_ALL, "grid         : %u x %u (maxPerCluster=%u)\n",
		s_decalTilesX, s_decalTilesY, VK_HT_DECAL_MAX_PER_CLUSTER );
	ri.Printf( PRINT_ALL, "submitted    : %u culled=%u\n", s_stats.decalSubmitted, s_stats.decalCulled );
	ri.Printf( PRINT_ALL, "binsTouched  : %u listEntries=%u maxOcc=%u\n",
		s_stats.decalBinsTouched, s_stats.decalListEntries, s_stats.decalMaxOccupancy );
	ri.Printf( PRINT_ALL, "overflow     : %u rejected=%u flag=%s\n",
		s_stats.decalOverflow, s_stats.decalRejected, s_stats.overflowFlag ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "policy       : overflow keeps earliest entries; never OOB shader read\n" );
}

void vk_ht_throughput_status_f( void )
{
	ri.Printf( PRINT_ALL, "======== High-Throughput Raster 1.0 (Slice A) ========\n" );
	ri.Printf( PRINT_ALL, "active         : %s\n", vk_ht_throughput_active() ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "profile        : modern_high_throughput.cfg (NOT boot default)\n" );
	ri.Printf( PRINT_ALL, "resources      : alive=%u / %u invalid=%u fallbacks=%u\n",
		s_stats.resAlive ? s_stats.resAlive : s_resAlive, VK_HT_RES_MAX,
		s_stats.resInvalidLookups, s_stats.resFallbackUses );
	ri.Printf( PRINT_ALL, "gpu_scene      : visible=%u indirect=%u bufferReady=%s\n",
		vk_gpu_scene_visible_count(), vk_gpu_scene_indirect_count(),
		vk_gpu_scene_indirect_buffer_ready() ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "merge          : in=%u out=%u groups=%u skip=%u\n",
		s_stats.mergeCmdsIn, s_stats.mergeCmdsOut, s_stats.mergeGroups,
		s_stats.mergeSkippedIncompatible );
	ri.Printf( PRINT_ALL, "decal_bin      : sub=%u touch=%u entries=%u overflow=%u\n",
		s_stats.decalSubmitted, s_stats.decalBinsTouched, s_stats.decalListEntries,
		s_stats.decalOverflow );
	ri.Printf( PRINT_ALL, "lights         : Forward+ clusters (shared grid owner)\n" );
	ri.Printf( PRINT_ALL, "meshlets/MDI   : companion r_meshlets (see meshlet_status)\n" );
	ri.Printf( PRINT_ALL, "overflow_flag  : %s\n", s_stats.overflowFlag ? "SET" : "clear" );
	ri.Printf( PRINT_ALL, "ownership      : opaque=deferred mode3 | transparent/weapon=Forward+\n" );
	ri.Printf( PRINT_ALL, "classic_bsp    : preserved | RT: optional/off under this profile\n" );
	ri.Printf( PRINT_ALL, "slice_b..e     : animation/materials/water/cert — not started\n" );
	ri.Printf( PRINT_ALL, "=====================================================\n" );
}
