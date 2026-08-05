#pragma once

#ifdef USE_VULKAN

/*
 * Raster Ultra 1.9 — virtualized raster shadows.
 *
 * Genuine page system (not a renamed CSM):
 *   virtual page addressing, physical page cache, residency,
 *   page-table lookup, receiver demand, invalidation,
 *   allocation / eviction, cached shadow pages.
 *
 * Certified CSM + local atlas remain fallbacks. RT stays off.
 */

#define VK_VSHADOW_MAX_POOL_PAGES   256
#define VK_VSHADOW_MAX_CLIP_LEVELS  6
#define VK_VSHADOW_MAX_DIRTY_QUEUE  64
#define VK_VSHADOW_MAX_LOCAL_REQ    32
#define VK_VSHADOW_INVALID_PHYS     0xffffffffu
#define VK_VSHADOW_PAGE_TABLE_SLOTS 512

typedef enum {
	VK_VSHADOW_LIGHT_SUN = 0,
	VK_VSHADOW_LIGHT_SPOT,
	VK_VSHADOW_LIGHT_POINT,
	VK_VSHADOW_LIGHT_AREA
} vkVShadowLightKind_t;

typedef struct vkVShadowPageMeta_s {
	uint32_t virtualId;
	uint32_t lastUseFrame;
	uint32_t casterRevision;
	uint32_t allocFrame;
	float    worldOrigin[2]; /* snapped page origin XY */
	float    worldSize;
	uint16_t atlasX;
	uint16_t atlasY;
	uint8_t  clipLevel;
	uint8_t  lightKind;
	uint8_t  dirty;
	uint8_t  resident;
	uint8_t  pinned;
	uint8_t  staticCached;
	uint8_t  initialized; /* cleared before first render — never sample if 0 */
	uint8_t  alphaCasters;
} vkVShadowPageMeta_t;

typedef struct vkVShadowPageTableEntry_s {
	uint32_t virtualId;
	uint32_t physicalIndex; /* VK_VSHADOW_INVALID_PHYS if empty */
	uint32_t lastUseFrame;
} vkVShadowPageTableEntry_t;

typedef struct vkVShadowStats_s {
	uint32_t requestedPages;
	uint32_t residentPages;
	uint32_t renderedPages;
	uint32_t reusedPages;
	uint32_t evictions;
	uint32_t allocationFailures;
	uint32_t dirtyQueued;
	uint32_t invalidations;
	uint32_t localRequests;
	uint32_t localAtlasFallbacks;
	uint32_t missingPageFallbacks;
	uint32_t budgetDrops;
	uint32_t localLightsAccepted;
	uint32_t casterDrawBudget;
	uint32_t pagePoolBytes;
	uint32_t frameNumber;
	float    fallbackPercent; /* 0..100 CSM/atlas share */
} vkVShadowStats_t;

/* One budget surface shared by CSM/atlas fallback and virtual clipmap pages. */
typedef struct vkVShadowBudget_s {
	uint32_t physicalPageBudget;
	uint32_t pageRenderBudget;
	uint32_t localLightBudget;
	uint32_t casterDrawBudget;
	uint32_t memoryBudgetBytes;
	uint32_t pagesClaimed;
	uint32_t localLightsAccepted;
	uint32_t budgetDrops;
} vkVShadowBudget_t;

typedef struct vkVShadowClipmapState_s {
	int      levels;
	float    basePageWorldSize;
	float    cameraSnap[3];
	float    sunDir[3];
	uint32_t originRevision;
	qboolean valid;
} vkVShadowClipmapState_t;

void vk_vshadow_register_cvars( void );
void vk_vshadow_init( void );
void vk_vshadow_shutdown( void );

qboolean vk_vshadow_active( void );
qboolean vk_vshadow_healthy( void );
qboolean vk_vshadow_fallback_csm( void );
qboolean vk_vshadow_owns_sun_pages( void ); /* pages manage sun residency; CSM may still sample */
qboolean vk_vshadow_alpha_casters( void );

/* Frame lifecycle */
void vk_vshadow_begin_frame( void );
void vk_vshadow_on_camera_cut( void );
void vk_vshadow_on_map_change( void );
void vk_vshadow_on_sun_direction( const float sunDir[3] );

/*
 * Receiver-driven demand from view + clipmap, then alloc/evict/invalidate.
 * Does not require CPU readback of GPU request buffers (CPU demand path for 1.9;
 * GPU compact requests remain extension point).
 */
void vk_vshadow_update( const float viewOrigin[3], const float sunDir[3],
	float viewNear, float viewFar );

/* Page render scheduling — returns number of dirty pages claimed this frame. */
int vk_vshadow_claim_dirty_pages( uint32_t *outPhysIndices, int maxOut );

/* Mark a physical page as rendered/initialized after depth fill. */
void vk_vshadow_mark_page_rendered( uint32_t physIndex );

/* Page-table lookup: virtual id -> physical index or INVALID. */
uint32_t vk_vshadow_page_table_lookup( uint32_t virtualId );

/* Build clipmap virtual id for level + snapped grid coords. */
uint32_t vk_vshadow_virtual_id( int clipLevel, int pageX, int pageY, vkVShadowLightKind_t light );

const vkVShadowPageMeta_t *vk_vshadow_page_meta( uint32_t physIndex );
const vkVShadowStats_t *vk_vshadow_stats( void );
const vkVShadowBudget_t *vk_vshadow_budget( void );
const vkVShadowClipmapState_t *vk_vshadow_clipmap_state( void );

/* Local-light demand; may fall back to atlas when pool exhausted. */
qboolean vk_vshadow_request_local( vkVShadowLightKind_t kind, int lightIndex,
	float importance, float distance );

void vk_vshadow_status_f( void );
void vk_vshadow_set_world_zone_residency( const worldZoneResidency_t *zones, int count );

#endif /* USE_VULKAN */
