#pragma once


/*
 * Raster Ultra 1.8 — versioned processed-material cache.
 * Tracks source/graph/compiler versions; invalidates on dependency change.
 */

#define VK_MAT_CACHE_MAX       512
#define VK_MAT_CACHE_COMPILER  1

typedef struct vkMaterialCacheEntry_s {
	qboolean active;
	uint32_t key;
	uint32_t permutationKey;
	uint32_t sourceHash;
	uint32_t graphVersion;
	uint32_t compilerVersion;
	uint32_t irCacheVersion;
	qboolean hit;
} vkMaterialCacheEntry_t;

typedef struct vkMaterialCacheStats_s {
	uint32_t lookups;
	uint32_t hits;
	uint32_t misses;
	uint32_t invalidations;
	uint32_t entries;
	uint32_t runtimeCompiles; /* should stay 0 — offline / load-time only */
} vkMaterialCacheStats_t;

void vk_material_cache_register_cvars( void );
void vk_material_cache_init( void );
void vk_material_cache_shutdown( void );

const vkMaterialCacheEntry_t *vk_material_cache_lookup( uint32_t sourceHash, uint32_t permutationKey );
void vk_material_cache_store( uint32_t sourceHash, uint32_t permutationKey, uint32_t graphVersion );
void vk_material_cache_invalidate_all( void );

const vkMaterialCacheStats_t *vk_material_cache_stats( void );
void vk_material_cache_status_f( void );

