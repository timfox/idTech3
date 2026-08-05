/*
===========================================================================
Raster Ultra 1.9 — virtualized raster shadow pages.
Virtual addressing + physical pool + page table + demand + eviction.
===========================================================================
*/

#include "tr_local.h"
#include "vk_vshadow.h"
#include "vk_raster_ultra.h"
#include "vk_pass_registry.h"

static cvar_t *r_vshadow;
static cvar_t *r_vshadowPageSize;
static cvar_t *r_vshadowPoolPages;
static cvar_t *r_vshadowClipmapLevels;
static cvar_t *r_vshadowBasePageWorld;
static cvar_t *r_vshadowFallbackCsm;
static cvar_t *r_vshadowAlphaCasters;
static cvar_t *r_vshadowRenderBudget;
static cvar_t *r_vshadowDebug;
static cvar_t *r_vshadowSunDirThreshold;
static cvar_t *r_vshadowLocalLightBudget;
static cvar_t *r_vshadowCasterDrawBudget;
static cvar_t *r_vshadowRequestBudget;

static qboolean s_cmds;
static qboolean s_inited;

static vkVShadowPageMeta_t s_pool[VK_VSHADOW_MAX_POOL_PAGES];
static vkVShadowPageTableEntry_t s_pageTable[VK_VSHADOW_PAGE_TABLE_SLOTS];
static uint32_t s_freeList[VK_VSHADOW_MAX_POOL_PAGES];
static int s_freeCount;
static int s_poolCapacity;
static int s_pageSize;
static int s_atlasGrid; /* ceil(sqrt(capacity)) */

static uint32_t s_dirtyQueue[VK_VSHADOW_MAX_DIRTY_QUEUE];
static int s_dirtyCount;

static vkVShadowStats_t s_stats;
static vkVShadowClipmapState_t s_clip;
static uint32_t s_frame;
static uint32_t s_casterRevision;
static float s_lastSun[3];
static qboolean s_haveSun;
static qboolean s_cameraCut;
static qboolean s_mapChanged;
static qboolean s_healthy;
static vkVShadowBudget_t s_budget;
static uint32_t s_localLightRequests;
static uint32_t s_pageRequestsAccepted;
static worldZoneResidency_t s_worldZones[REF_WORLD_ZONE_MAX];
static int s_worldZoneCount;

static qboolean VShadow_HasResidentShadowZone( void )
{
	int i;
	if ( s_worldZoneCount <= 0 ) return qtrue; /* legacy scenes have no snapshot */
	for ( i = 0; i < s_worldZoneCount; i++ ) {
		if ( s_worldZones[i].resident && ( s_worldZones[i].residencyMask & REF_WORLD_ZONE_RESIDENCY_SHADOW ) ) return qtrue;
	}
	return qfalse;
}

static uint32_t VShadow_Hash( uint32_t virtualId )
{
	virtualId ^= virtualId >> 16;
	virtualId *= 0x7feb352du;
	virtualId ^= virtualId >> 15;
	virtualId *= 0x846ca68bu;
	virtualId ^= virtualId >> 16;
	return virtualId % VK_VSHADOW_PAGE_TABLE_SLOTS;
}

uint32_t vk_vshadow_virtual_id( int clipLevel, int pageX, int pageY, vkVShadowLightKind_t light )
{
	uint32_t id;

	clipLevel = Com_Clamp( 0, VK_VSHADOW_MAX_CLIP_LEVELS - 1, clipLevel );
	/* Pack: light[2] | level[3] | pageY[13] | pageX[13] — signed pages via bias */
	id = ( (uint32_t)light & 3u ) << 30;
	id |= ( (uint32_t)clipLevel & 7u ) << 27;
	id |= ( (uint32_t)( pageY + 4096 ) & 0x1fffu ) << 14;
	id |= ( (uint32_t)( pageX + 4096 ) & 0x1fffu );
	return id;
}

void vk_vshadow_register_cvars( void )
{
	if ( r_vshadow ) {
		return;
	}
	r_vshadow = ri.Cvar_Get( "r_vshadow", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_vshadow, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_vshadow,
		"Raster Ultra 1.9 virtualized raster shadows (latched).\n"
		"Page addressing + physical pool + page table + demand + eviction.\n"
		"0 off (certified CSM / local atlas). Does not enable RT." );
	ri.Cvar_SetGroup( r_vshadow, CVG_RENDERER );

	r_vshadowPageSize = ri.Cvar_Get( "r_vshadowPageSize", "256", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_vshadowPageSize, "64", "512", CV_INTEGER );
	ri.Cvar_SetDescription( r_vshadowPageSize, "Physical shadow page texel size (latched)." );

	r_vshadowPoolPages = ri.Cvar_Get( "r_vshadowPoolPages", "64", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_vshadowPoolPages, "8", va( "%d", VK_VSHADOW_MAX_POOL_PAGES ), CV_INTEGER );
	ri.Cvar_SetDescription( r_vshadowPoolPages, "Physical page pool capacity (latched; memory budget)." );

	r_vshadowClipmapLevels = ri.Cvar_Get( "r_vshadowClipmapLevels", "4", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vshadowClipmapLevels, "1", va( "%d", VK_VSHADOW_MAX_CLIP_LEVELS ), CV_INTEGER );

	r_vshadowBasePageWorld = ri.Cvar_Get( "r_vshadowBasePageWorld", "512", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vshadowBasePageWorld, "64", "8192", CV_FLOAT );
	ri.Cvar_SetDescription( r_vshadowBasePageWorld,
		"World-space size of finest clipmap page (level 0)." );

	r_vshadowFallbackCsm = ri.Cvar_Get( "r_vshadowFallbackCsm", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vshadowFallbackCsm, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_vshadowFallbackCsm,
		"Keep certified CSM sampling while virtual pages manage residency (default 1)." );

	r_vshadowAlphaCasters = ri.Cvar_Get( "r_vshadowAlphaCasters", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vshadowAlphaCasters, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_vshadowAlphaCasters,
		"Allow alpha-tested / foliage casters in page renders (not solid rectangles)." );

	r_vshadowRenderBudget = ri.Cvar_Get( "r_vshadowRenderBudget", "8", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vshadowRenderBudget, "0", va( "%d", VK_VSHADOW_MAX_DIRTY_QUEUE ), CV_INTEGER );

	r_vshadowDebug = ri.Cvar_Get( "r_vshadowDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vshadowDebug, "0", "3", CV_INTEGER );

	r_vshadowSunDirThreshold = ri.Cvar_Get( "r_vshadowSunDirThreshold", "0.02", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vshadowSunDirThreshold, "0.001", "0.5", CV_FLOAT );

	r_vshadowLocalLightBudget = ri.Cvar_Get( "r_shadowLocalLightBudget", "8", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vshadowLocalLightBudget, "0", "64", CV_INTEGER );
	ri.Cvar_SetDescription( r_vshadowLocalLightBudget,
		"Maximum local lights allowed to request virtual shadow pages per frame; excess lights use the local atlas/fallback." );
	r_vshadowCasterDrawBudget = ri.Cvar_Get( "r_shadowCasterDrawBudget", "200000", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vshadowCasterDrawBudget, "0", "2000000", CV_INTEGER );
	ri.Cvar_SetDescription( r_vshadowCasterDrawBudget,
		"Reserved shadow-caster index budget for atlas/clipmap updates (telemetry and future GPU cull admission)." );
	r_vshadowRequestBudget = ri.Cvar_Get( "r_shadowPageRequestBudget", "64", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vshadowRequestBudget, "1", va( "%d", VK_VSHADOW_MAX_DIRTY_QUEUE ), CV_INTEGER );
	ri.Cvar_SetDescription( r_vshadowRequestBudget,
		"Maximum new virtual shadow page requests admitted per frame." );
}

static void VShadow_ResetPool( void )
{
	int i;

	Com_Memset( s_pool, 0, sizeof( s_pool ) );
	Com_Memset( s_pageTable, 0, sizeof( s_pageTable ) );
	for ( i = 0; i < VK_VSHADOW_PAGE_TABLE_SLOTS; i++ ) {
		s_pageTable[i].physicalIndex = VK_VSHADOW_INVALID_PHYS;
		s_pageTable[i].virtualId = 0;
	}
	s_freeCount = 0;
	for ( i = 0; i < s_poolCapacity; i++ ) {
		s_pool[i].resident = 0;
		s_pool[i].dirty = 0;
		s_pool[i].initialized = 0;
		s_pool[i].virtualId = 0;
		s_pool[i].atlasX = (uint16_t)( i % s_atlasGrid );
		s_pool[i].atlasY = (uint16_t)( i / s_atlasGrid );
		s_freeList[s_freeCount++] = (uint32_t)i;
	}
	s_dirtyCount = 0;
	s_stats.residentPages = 0;
}

void vk_vshadow_init( void )
{
	int n;
	int g;

	vk_vshadow_register_cvars();
	s_pageSize = r_vshadowPageSize ? r_vshadowPageSize->integer : 256;
	n = r_vshadowPoolPages ? r_vshadowPoolPages->integer : 64;
	if ( n < 8 ) {
		n = 8;
	}
	if ( n > VK_VSHADOW_MAX_POOL_PAGES ) {
		n = VK_VSHADOW_MAX_POOL_PAGES;
	}
	s_poolCapacity = n;
	g = 1;
	while ( g * g < s_poolCapacity ) {
		g++;
	}
	s_atlasGrid = g;
	s_stats.pagePoolBytes = (uint32_t)( s_poolCapacity * s_pageSize * s_pageSize * 4 ); /* depth estimate */
	s_budget.physicalPageBudget = (uint32_t)s_poolCapacity;
	s_budget.pageRenderBudget = (uint32_t)( r_vshadowRenderBudget ? r_vshadowRenderBudget->integer : 8 );
	s_budget.localLightBudget = (uint32_t)( r_vshadowLocalLightBudget ? r_vshadowLocalLightBudget->integer : 8 );
	s_budget.casterDrawBudget = (uint32_t)( r_vshadowCasterDrawBudget ? r_vshadowCasterDrawBudget->integer : 200000 );
	s_budget.memoryBudgetBytes = s_stats.pagePoolBytes;
	VShadow_ResetPool();
	Com_Memset( &s_clip, 0, sizeof( s_clip ) );
	s_clip.levels = r_vshadowClipmapLevels ? r_vshadowClipmapLevels->integer : 4;
	s_clip.basePageWorldSize = r_vshadowBasePageWorld ? r_vshadowBasePageWorld->value : 512.0f;
	s_frame = 0;
	s_casterRevision = 1;
	s_haveSun = qfalse;
	s_cameraCut = qfalse;
	s_mapChanged = qfalse;
	s_localLightRequests = 0;
	s_pageRequestsAccepted = 0;
	s_healthy = qtrue;
	s_inited = qtrue;

	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "vshadow_status", vk_vshadow_status_f );
		s_cmds = qtrue;
	}

	ri.Printf( PRINT_ALL,
		"[VK][VShadow] %s page=%d pool=%d clipLevels=%d baseWorld=%.0f fallbackCSM=%d "
		"(virtual pages + physical cache + page table; RT=off)\n",
		( r_vshadow && r_vshadow->integer ) ? "enabled" : "off",
		s_pageSize, s_poolCapacity, s_clip.levels, s_clip.basePageWorldSize,
		r_vshadowFallbackCsm ? r_vshadowFallbackCsm->integer : 1 );
}

void vk_vshadow_shutdown( void )
{
	s_worldZoneCount = 0;
	VShadow_ResetPool();
	s_inited = qfalse;
	s_healthy = qfalse;
}

void vk_vshadow_set_world_zone_residency( const worldZoneResidency_t *zones, int count )
{
	if ( !zones || count <= 0 ) {
		s_worldZoneCount = 0;
		return;
	}
	count = MIN( count, REF_WORLD_ZONE_MAX );
	Com_Memcpy( s_worldZones, zones, (size_t)count * sizeof( s_worldZones[0] ) );
	s_worldZoneCount = count;
}

qboolean vk_vshadow_active( void )
{
	return ( s_inited && r_vshadow && r_vshadow->integer ) ? qtrue : qfalse;
}

qboolean vk_vshadow_healthy( void )
{
	return ( vk_vshadow_active() && s_healthy ) ? qtrue : qfalse;
}

qboolean vk_vshadow_fallback_csm( void )
{
	if ( !vk_vshadow_active() ) {
		return qtrue;
	}
	if ( !r_vshadowFallbackCsm || r_vshadowFallbackCsm->integer ) {
		return qtrue;
	}
	if ( !s_healthy || s_stats.allocationFailures > 0 ) {
		return qtrue;
	}
	return qfalse;
}

qboolean vk_vshadow_owns_sun_pages( void )
{
	return vk_vshadow_active() ? qtrue : qfalse;
}

qboolean vk_vshadow_alpha_casters( void )
{
	return ( !r_vshadowAlphaCasters || r_vshadowAlphaCasters->integer ) ? qtrue : qfalse;
}

uint32_t vk_vshadow_page_table_lookup( uint32_t virtualId )
{
	uint32_t slot = VShadow_Hash( virtualId );
	uint32_t i;

	for ( i = 0; i < VK_VSHADOW_PAGE_TABLE_SLOTS; i++ ) {
		uint32_t idx = ( slot + i ) % VK_VSHADOW_PAGE_TABLE_SLOTS;
		if ( s_pageTable[idx].physicalIndex == VK_VSHADOW_INVALID_PHYS ) {
			return VK_VSHADOW_INVALID_PHYS;
		}
		if ( s_pageTable[idx].virtualId == virtualId ) {
			s_pageTable[idx].lastUseFrame = s_frame;
			if ( s_pageTable[idx].physicalIndex < (uint32_t)s_poolCapacity ) {
				s_pool[s_pageTable[idx].physicalIndex].lastUseFrame = s_frame;
			}
			return s_pageTable[idx].physicalIndex;
		}
	}
	return VK_VSHADOW_INVALID_PHYS;
}

static void VShadow_PageTableUnmap( uint32_t virtualId )
{
	uint32_t slot = VShadow_Hash( virtualId );
	uint32_t i;

	for ( i = 0; i < VK_VSHADOW_PAGE_TABLE_SLOTS; i++ ) {
		uint32_t idx = ( slot + i ) % VK_VSHADOW_PAGE_TABLE_SLOTS;
		if ( s_pageTable[idx].physicalIndex == VK_VSHADOW_INVALID_PHYS ) {
			return;
		}
		if ( s_pageTable[idx].virtualId == virtualId ) {
			s_pageTable[idx].physicalIndex = VK_VSHADOW_INVALID_PHYS;
			s_pageTable[idx].virtualId = 0;
			return;
		}
	}
}

static qboolean VShadow_PageTableMap( uint32_t virtualId, uint32_t phys )
{
	uint32_t slot = VShadow_Hash( virtualId );
	uint32_t i;

	for ( i = 0; i < VK_VSHADOW_PAGE_TABLE_SLOTS; i++ ) {
		uint32_t idx = ( slot + i ) % VK_VSHADOW_PAGE_TABLE_SLOTS;
		if ( s_pageTable[idx].physicalIndex == VK_VSHADOW_INVALID_PHYS ||
			s_pageTable[idx].virtualId == virtualId ) {
			s_pageTable[idx].virtualId = virtualId;
			s_pageTable[idx].physicalIndex = phys;
			s_pageTable[idx].lastUseFrame = s_frame;
			return qtrue;
		}
	}
	return qfalse;
}

static uint32_t VShadow_EvictLRU( void )
{
	uint32_t best = VK_VSHADOW_INVALID_PHYS;
	uint32_t bestAge = 0;
	int i;

	for ( i = 0; i < s_poolCapacity; i++ ) {
		if ( !s_pool[i].resident || s_pool[i].pinned ) {
			continue;
		}
		if ( best == VK_VSHADOW_INVALID_PHYS || s_pool[i].lastUseFrame < bestAge ) {
			best = (uint32_t)i;
			bestAge = s_pool[i].lastUseFrame;
		}
	}
	if ( best == VK_VSHADOW_INVALID_PHYS ) {
		s_stats.allocationFailures++;
		s_healthy = qfalse;
		return VK_VSHADOW_INVALID_PHYS;
	}
	VShadow_PageTableUnmap( s_pool[best].virtualId );
	s_pool[best].resident = 0;
	s_pool[best].dirty = 0;
	s_pool[best].initialized = 0;
	s_pool[best].staticCached = 0;
	s_pool[best].virtualId = 0;
	if ( s_stats.residentPages > 0 ) {
		s_stats.residentPages--;
	}
	s_stats.evictions++;
	s_freeList[s_freeCount++] = best;
	return best;
}

static uint32_t VShadow_AllocPhysical( void )
{
	uint32_t phys;

	if ( s_freeCount > 0 ) {
		phys = s_freeList[--s_freeCount];
		return phys;
	}
	return VShadow_EvictLRU();
}

static void VShadow_QueueDirty( uint32_t phys )
{
	int i;

	if ( phys >= (uint32_t)s_poolCapacity ) {
		return;
	}
	if ( !s_pool[phys].dirty ) {
		s_pool[phys].dirty = 1;
		s_pool[phys].initialized = 0; /* never sample uninitialized */
	}
	for ( i = 0; i < s_dirtyCount; i++ ) {
		if ( s_dirtyQueue[i] == phys ) {
			return;
		}
	}
	if ( s_dirtyCount < VK_VSHADOW_MAX_DIRTY_QUEUE ) {
		s_dirtyQueue[s_dirtyCount++] = phys;
		s_stats.dirtyQueued = (uint32_t)s_dirtyCount;
	}
}

static uint32_t VShadow_EnsurePage( uint32_t virtualId, int clipLevel, float originX, float originY,
	float worldSize, vkVShadowLightKind_t light, qboolean pinNear )
{
	uint32_t phys = vk_vshadow_page_table_lookup( virtualId );

	s_stats.requestedPages++;
	if ( phys != VK_VSHADOW_INVALID_PHYS ) {
		s_stats.reusedPages++;
		s_pool[phys].lastUseFrame = s_frame;
		if ( pinNear ) {
			s_pool[phys].pinned = 1;
		}
		return phys;
	}
	if ( s_pageRequestsAccepted >= (uint32_t)( r_vshadowRequestBudget ? r_vshadowRequestBudget->integer : VK_VSHADOW_MAX_DIRTY_QUEUE ) ) {
		s_stats.budgetDrops++;
		s_budget.budgetDrops++;
		s_stats.missingPageFallbacks++;
		return VK_VSHADOW_INVALID_PHYS;
	}
	s_pageRequestsAccepted++;

	phys = VShadow_AllocPhysical();
	if ( phys == VK_VSHADOW_INVALID_PHYS ) {
		s_stats.missingPageFallbacks++;
		return VK_VSHADOW_INVALID_PHYS;
	}

	if ( !VShadow_PageTableMap( virtualId, phys ) ) {
		s_freeList[s_freeCount++] = phys;
		s_stats.allocationFailures++;
		s_stats.missingPageFallbacks++;
		return VK_VSHADOW_INVALID_PHYS;
	}

	s_pool[phys].virtualId = virtualId;
	s_pool[phys].clipLevel = (uint8_t)clipLevel;
	s_pool[phys].lightKind = (uint8_t)light;
	s_pool[phys].worldOrigin[0] = originX;
	s_pool[phys].worldOrigin[1] = originY;
	s_pool[phys].worldSize = worldSize;
	s_pool[phys].resident = 1;
	s_pool[phys].pinned = pinNear ? 1 : 0;
	s_pool[phys].staticCached = 1;
	s_pool[phys].alphaCasters = vk_vshadow_alpha_casters() ? 1 : 0;
	s_pool[phys].casterRevision = s_casterRevision;
	s_pool[phys].allocFrame = s_frame;
	s_pool[phys].lastUseFrame = s_frame;
	s_stats.residentPages++;
	VShadow_QueueDirty( phys );
	return phys;
}

static void VShadow_InvalidateAll( void )
{
	int i;

	for ( i = 0; i < s_poolCapacity; i++ ) {
		if ( s_pool[i].resident ) {
			VShadow_QueueDirty( (uint32_t)i );
			s_pool[i].staticCached = 0;
		}
	}
	s_casterRevision++;
	s_stats.invalidations++;
}

void vk_vshadow_begin_frame( void )
{
	int i;

	if ( !vk_vshadow_active() ) {
		return;
	}
	s_frame++;
	s_stats.frameNumber = s_frame;
	s_stats.requestedPages = 0;
	s_stats.renderedPages = 0;
	s_stats.reusedPages = 0;
	s_stats.dirtyQueued = 0;
	s_stats.localRequests = 0;
	s_stats.localAtlasFallbacks = 0;
	s_stats.missingPageFallbacks = 0;
	s_stats.budgetDrops = 0;
	s_stats.localLightsAccepted = 0;
	s_stats.casterDrawBudget = s_budget.casterDrawBudget;
	s_stats.zoneGatedUpdates = 0;
	s_localLightRequests = 0;
	s_pageRequestsAccepted = 0;
	s_budget.pagesClaimed = 0;
	s_budget.localLightsAccepted = 0;
	s_budget.budgetDrops = 0;
	s_dirtyCount = 0;
	/* Unpin non-critical pages each frame; near pages re-pin on demand. */
	for ( i = 0; i < s_poolCapacity; i++ ) {
		if ( s_pool[i].clipLevel > 0 ) {
			s_pool[i].pinned = 0;
		}
	}
	s_healthy = qtrue;
}

void vk_vshadow_on_camera_cut( void )
{
	s_cameraCut = qtrue;
}

void vk_vshadow_on_map_change( void )
{
	s_mapChanged = qtrue;
}

void vk_vshadow_on_sun_direction( const float sunDir[3] )
{
	float d;
	float thr;

	if ( !sunDir ) {
		return;
	}
	if ( !s_haveSun ) {
		VectorCopy( sunDir, s_lastSun );
		s_haveSun = qtrue;
		return;
	}
	d = fabsf( sunDir[0] - s_lastSun[0] ) + fabsf( sunDir[1] - s_lastSun[1] ) +
		fabsf( sunDir[2] - s_lastSun[2] );
	thr = r_vshadowSunDirThreshold ? r_vshadowSunDirThreshold->value : 0.02f;
	if ( d > thr ) {
		VectorCopy( sunDir, s_lastSun );
		VShadow_InvalidateAll();
	}
}

void vk_vshadow_update( const float viewOrigin[3], const float sunDir[3],
	float viewNear, float viewFar )
{
	int level;
	int levels;
	float baseSize;
	float radius;
	(void)viewNear;
	(void)viewFar;

	if ( !vk_vshadow_active() || !viewOrigin ) {
		return;
	}

	if ( s_mapChanged ) {
		VShadow_ResetPool();
		s_mapChanged = qfalse;
		s_stats.invalidations++;
	}
	if ( s_cameraCut ) {
		VShadow_InvalidateAll();
		s_cameraCut = qfalse;
	}
	if ( sunDir ) {
		vk_vshadow_on_sun_direction( sunDir );
	}
	if ( !VShadow_HasResidentShadowZone() ) {
		s_stats.zoneGatedUpdates++;
		s_clip.valid = qfalse;
		return;
	}

	levels = r_vshadowClipmapLevels ? r_vshadowClipmapLevels->integer : 4;
	if ( levels < 1 ) {
		levels = 1;
	}
	if ( levels > VK_VSHADOW_MAX_CLIP_LEVELS ) {
		levels = VK_VSHADOW_MAX_CLIP_LEVELS;
	}
	baseSize = r_vshadowBasePageWorld ? r_vshadowBasePageWorld->value : 512.0f;
	s_clip.levels = levels;
	s_clip.basePageWorldSize = baseSize;
	s_clip.valid = qtrue;
	VectorCopy( viewOrigin, s_clip.cameraSnap );
	if ( sunDir ) {
		VectorCopy( sunDir, s_clip.sunDir );
	}

	/*
	 * Receiver-driven demand: cover a radius around the camera per clipmap level.
	 * Pages snap to world grid for stable incremental scrolling (no full redraw
	 * on small motion — only newly entered page cells allocate/dirty).
	 */
	radius = baseSize * 1.5f;
	for ( level = 0; level < levels; level++ ) {
		float pageSize = baseSize * (float)( 1 << level );
		float snapX = floorf( viewOrigin[0] / pageSize ) * pageSize;
		float snapY = floorf( viewOrigin[1] / pageSize ) * pageSize;
		int extent = ( level == 0 ) ? 2 : 1;
		int px, py;

		s_clip.cameraSnap[0] = snapX;
		s_clip.cameraSnap[1] = snapY;

		for ( py = -extent; py <= extent; py++ ) {
			for ( px = -extent; px <= extent; px++ ) {
				float ox = snapX + (float)px * pageSize;
				float oy = snapY + (float)py * pageSize;
				float cx = ox + pageSize * 0.5f;
				float cy = oy + pageSize * 0.5f;
				float dx = cx - viewOrigin[0];
				float dy = cy - viewOrigin[1];
				int gridX = (int)floorf( ox / pageSize );
				int gridY = (int)floorf( oy / pageSize );
				uint32_t vid;
				qboolean pin;

				if ( ( dx * dx + dy * dy ) > ( radius * radius * (float)( 1 << ( level * 2 ) ) ) ) {
					continue;
				}
				vid = vk_vshadow_virtual_id( level, gridX, gridY, VK_VSHADOW_LIGHT_SUN );
				pin = ( level == 0 && abs( px ) <= 1 && abs( py ) <= 1 ) ? qtrue : qfalse;
				VShadow_EnsurePage( vid, level, ox, oy, pageSize, VK_VSHADOW_LIGHT_SUN, pin );
			}
		}
		radius *= 2.0f;
	}

	if ( s_stats.requestedPages > 0 ) {
		s_stats.fallbackPercent = vk_vshadow_fallback_csm() ? 100.0f :
			( 100.0f * (float)s_stats.missingPageFallbacks / (float)s_stats.requestedPages );
	} else {
		s_stats.fallbackPercent = vk_vshadow_fallback_csm() ? 100.0f : 0.0f;
	}

	/* Spine: demand + alloc conceptually owned by sun-shadow phase extension. */
	(void)VK_SPINE_PASS_SUN_SHADOW;
}

int vk_vshadow_claim_dirty_pages( uint32_t *outPhysIndices, int maxOut )
{
	int budget;
	int claimed = 0;
	int i;

	if ( !vk_vshadow_active() || !outPhysIndices || maxOut <= 0 ) {
		return 0;
	}
	budget = r_vshadowRenderBudget ? r_vshadowRenderBudget->integer : 8;
	if ( budget < 0 ) {
		budget = 0;
	}
	if ( budget > maxOut ) {
		budget = maxOut;
	}
	if ( budget > (int)s_budget.pageRenderBudget ) {
		budget = (int)s_budget.pageRenderBudget;
	}
	for ( i = 0; i < s_dirtyCount && claimed < budget; i++ ) {
		uint32_t phys = s_dirtyQueue[i];
		if ( phys >= (uint32_t)s_poolCapacity || !s_pool[phys].dirty ) {
			continue;
		}
		outPhysIndices[claimed++] = phys;
	}
	s_budget.pagesClaimed += (uint32_t)claimed;
	if ( s_dirtyCount > claimed ) {
		s_budget.budgetDrops += (uint32_t)( s_dirtyCount - claimed );
		s_stats.budgetDrops += (uint32_t)( s_dirtyCount - claimed );
	}
	return claimed;
}

void vk_vshadow_mark_page_rendered( uint32_t physIndex )
{
	if ( physIndex >= (uint32_t)s_poolCapacity ) {
		return;
	}
	s_pool[physIndex].dirty = 0;
	s_pool[physIndex].initialized = 1;
	s_pool[physIndex].staticCached = 1;
	s_pool[physIndex].lastUseFrame = s_frame;
	s_stats.renderedPages++;
}

const vkVShadowPageMeta_t *vk_vshadow_page_meta( uint32_t physIndex )
{
	if ( physIndex >= (uint32_t)s_poolCapacity ) {
		return NULL;
	}
	return &s_pool[physIndex];
}

const vkVShadowStats_t *vk_vshadow_stats( void )
{
	return &s_stats;
}

const vkVShadowBudget_t *vk_vshadow_budget( void )
{
	return &s_budget;
}

const vkVShadowClipmapState_t *vk_vshadow_clipmap_state( void )
{
	return &s_clip;
}

qboolean vk_vshadow_request_local( vkVShadowLightKind_t kind, int lightIndex,
	float importance, float distance )
{
	uint32_t vid;
	uint32_t phys;
	int grid;

	(void)distance;
	if ( !vk_vshadow_active() ) {
		return qfalse;
	}
	s_stats.localRequests++;
	if ( s_localLightRequests >= s_budget.localLightBudget ) {
		s_stats.localAtlasFallbacks++;
		s_stats.budgetDrops++;
		s_budget.budgetDrops++;
		return qfalse;
	}
	if ( importance < 0.05f || ( s_freeCount == 0 && s_stats.residentPages >= (uint32_t)s_poolCapacity ) ) {
		s_stats.localAtlasFallbacks++;
		return qfalse; /* caller keeps local atlas */
	}
	grid = lightIndex & 0xff;
	vid = vk_vshadow_virtual_id( 0, grid, (int)kind + 1, kind );
	phys = VShadow_EnsurePage( vid, 0, 0.0f, 0.0f,
		s_clip.basePageWorldSize > 0.0f ? s_clip.basePageWorldSize : 512.0f,
		kind, importance > 0.75f ? qtrue : qfalse );
	if ( phys == VK_VSHADOW_INVALID_PHYS ) {
		s_stats.localAtlasFallbacks++;
		return qfalse;
	}
	s_localLightRequests++;
	s_stats.localLightsAccepted++;
	s_budget.localLightsAccepted++;
	return qtrue;
}

void vk_vshadow_status_f( void )
{
	const vkVShadowStats_t *st = &s_stats;

	ri.Printf( PRINT_ALL, "=== Virtual Shadow Pages (Raster Ultra 1.9) ===\n" );
	ri.Printf( PRINT_ALL, "active           : %s healthy=%s\n",
		vk_vshadow_active() ? "yes" : "no",
		vk_vshadow_healthy() ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "world zones      : %d (texture/shadow residency snapshot)\n", s_worldZoneCount );
	ri.Printf( PRINT_ALL, "zone gated       : %u shadow updates skipped\n", st->zoneGatedUpdates );
	ri.Printf( PRINT_ALL, "pageSize         : %d pool=%d atlasGrid=%d budgetBytes~%u\n",
		s_pageSize, s_poolCapacity, s_atlasGrid, st->pagePoolBytes );
	ri.Printf( PRINT_ALL, "clipmap          : levels=%d baseWorld=%.0f valid=%s\n",
		s_clip.levels, s_clip.basePageWorldSize, s_clip.valid ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "requested        : %u resident=%u reused=%u rendered=%u\n",
		st->requestedPages, st->residentPages, st->reusedPages, st->renderedPages );
	ri.Printf( PRINT_ALL, "evictions        : %u allocFail=%u dirty=%u inval=%u\n",
		st->evictions, st->allocationFailures, st->dirtyQueued, st->invalidations );
	ri.Printf( PRINT_ALL, "local            : req=%u atlasFallback=%u missing=%u\n",
		st->localRequests, st->localAtlasFallbacks, st->missingPageFallbacks );
	ri.Printf( PRINT_ALL, "budget           : pages=%u/%u render=%u local=%u/%u caster=%u drops=%u bytes=%u\n",
		s_budget.pagesClaimed, s_budget.physicalPageBudget, s_budget.pageRenderBudget,
		s_budget.localLightsAccepted, s_budget.localLightBudget,
		s_budget.casterDrawBudget, s_budget.budgetDrops, s_budget.memoryBudgetBytes );
	ri.Printf( PRINT_ALL, "fallbackCSM      : %s percent=%.1f\n",
		vk_vshadow_fallback_csm() ? "yes" : "no", st->fallbackPercent );
	ri.Printf( PRINT_ALL, "alphaCasters     : %s\n", vk_vshadow_alpha_casters() ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "policy           : never sample uninitialized pages; CSM/atlas remain certified fallbacks\n" );
}
