/*
===========================================================================
High-Throughput Raster Engine 1.0 — Slice A (GPU throughput).

Global resource indices, decal binning on the Forward+ cluster grid,
compatible geometry-merge metrics, and an ht_status dashboard.

Does not replace classic BSP / mode-2 certified boot. RT stays optional/off.
===========================================================================
*/

#pragma once

#ifdef USE_VULKAN

#include "../common/tr_types.h"

/* Index 0 is always INVALID — never a white texture or default material. */
#define VK_HT_RES_INVALID            0u
#define VK_HT_RES_FALLBACK_WHITE     1u
#define VK_HT_RES_FALLBACK_BLACK     2u
#define VK_HT_RES_FALLBACK_FLAT_N    3u
#define VK_HT_RES_FALLBACK_ROUGH     4u
#define VK_HT_RES_FALLBACK_MATERIAL  5u
#define VK_HT_RES_FALLBACK_GEOMETRY  6u
#define VK_HT_RES_FALLBACK_ANIM      7u
#define VK_HT_RES_USER_BASE          64u
#define VK_HT_RES_MAX                4096u

#define VK_HT_DECAL_MAX              2048u
#define VK_HT_DECAL_MAX_PER_CLUSTER  8u

typedef enum {
	VK_HT_RES_KIND_NONE = 0,
	VK_HT_RES_KIND_TEXTURE,
	VK_HT_RES_KIND_MATERIAL,
	VK_HT_RES_KIND_BUFFER,
	VK_HT_RES_KIND_MESH,
	VK_HT_RES_KIND_SKELETON,
	VK_HT_RES_KIND_COUNT
} vkHtResKind_t;

typedef struct vkHtResourceSlot_s {
	uint32_t      index;
	uint32_t      generation;
	vkHtResKind_t kind;
	uint32_t      flags;
	qboolean      alive;
} vkHtResourceSlot_t;

typedef struct vkHtDecalProj_s {
	float    origin[3];
	float    radius;
	float    normal[3];
	float    halfExtent;
	uint32_t materialId;
	uint32_t objectId;   /* 0 = world */
	uint32_t flags;
	qboolean alive;
} vkHtDecalProj_t;

typedef struct vkHtThroughputStats_s {
	/* Global resources */
	uint32_t resCapacity;
	uint32_t resAlive;
	uint32_t resHighWater;
	uint32_t resInvalidLookups;
	uint32_t resFallbackUses;

	/* Decal binning (shared cluster grid with Forward+) */
	uint32_t decalSubmitted;
	uint32_t decalCulled;
	uint32_t decalBinsTouched;
	uint32_t decalListEntries;
	uint32_t decalMaxOccupancy;
	uint32_t decalOverflow;
	uint32_t decalRejected;

	/* Geometry merge (compatible GPU-scene draws) */
	uint32_t mergeCmdsIn;
	uint32_t mergeCmdsOut;
	uint32_t mergeGroups;
	uint32_t mergeSkippedIncompatible;

	/* GPU scene / meshlet companions (copied each status refresh) */
	uint32_t gpuVisible;
	uint32_t gpuIndirect;
	uint32_t meshletGpuDraws;

	qboolean overflowFlag;
} vkHtThroughputStats_t;

void vk_ht_throughput_register_cvars( void );
void vk_ht_throughput_init( void );
void vk_ht_throughput_shutdown( void );
void vk_ht_throughput_begin_frame( void );
void vk_ht_throughput_end_frame( void );

qboolean vk_ht_throughput_active( void );
const vkHtThroughputStats_t *vk_ht_throughput_stats( void );

/* Global resource indices — safe fallbacks; index 0 is never valid. */
uint32_t vk_ht_res_alloc( vkHtResKind_t kind );
void vk_ht_res_free( uint32_t index );
uint32_t vk_ht_res_resolve( uint32_t index, vkHtResKind_t expectedKind, uint32_t fallbackIndex );
uint32_t vk_ht_res_fallback( vkHtResKind_t kind );

/* Decal projections → shared Forward+ tile/cluster bins (host pack, overflow-safe). */
uint32_t vk_ht_decal_register( const vec3_t origin, float radius, const vec3_t normal,
	float halfExtent, uint32_t materialId, uint32_t objectId );
void vk_ht_decal_clear( void );
void vk_ht_decal_bin_for_view( void );

/* Compatible GPU-scene draw merge (same mesh/material/index range → instanceCount). */
void vk_ht_merge_gpu_scene_draws( void );

void vk_ht_throughput_status_f( void );
void vk_ht_decal_status_f( void );
void vk_ht_res_status_f( void );

#endif /* USE_VULKAN */
