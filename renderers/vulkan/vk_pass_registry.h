/*
===========================================================================
Lightweight Spine pass / resource registry (Renderer Spine 1.0).

Declarative ownership around the existing Vulkan path. Pass entry/exit and
known-image layout transitions are graph-owned; full diagnostics remain
available when r_spineValidate > 0 (or on DEVICE_LOST dump).
===========================================================================
*/
#ifndef VK_PASS_REGISTRY_H
#define VK_PASS_REGISTRY_H

#include "../common/tr_types.h"
#include "../common/vulkan/vulkan.h"
#include "vk_deferred_gbuffer.h"

/* Frame phase order (production spine). */
typedef enum {
	VK_SPINE_PHASE_FRAME_BEGIN = 0,
	VK_SPINE_PHASE_SCENE_PREP,
	VK_SPINE_PHASE_WORLD_DEPTH,
	VK_SPINE_PHASE_WORLD_OPAQUE,
	VK_SPINE_PHASE_OPAQUE_LIGHTING,
	VK_SPINE_PHASE_SCREEN_SPACE,
	VK_SPINE_PHASE_WORLD_TRANSPARENCY,
	VK_SPINE_PHASE_TRANSPARENCY_RESOLVE,
	VK_SPINE_PHASE_TEMPORAL,
	VK_SPINE_PHASE_POST,
	VK_SPINE_PHASE_WEAPON,
	VK_SPINE_PHASE_UI,
	VK_SPINE_PHASE_PRESENT,
	VK_SPINE_PHASE_FRAME_END,
	VK_SPINE_PHASE_COUNT
} vkSpinePhase;

typedef enum {
	VK_SPINE_CAT_SCENE_PREP = 0,
	VK_SPINE_CAT_DEPTH,
	VK_SPINE_CAT_OPAQUE_RASTER,
	VK_SPINE_CAT_DEFERRED,
	VK_SPINE_CAT_FORWARD_PLUS,
	VK_SPINE_CAT_TRANSPARENCY,
	VK_SPINE_CAT_OIT,
	VK_SPINE_CAT_RAY_TRACING,
	VK_SPINE_CAT_TEMPORAL,
	VK_SPINE_CAT_POST,
	VK_SPINE_CAT_WEAPON,
	VK_SPINE_CAT_UI,
	VK_SPINE_CAT_PRESENTATION,
	VK_SPINE_CAT_MAINTENANCE,
	VK_SPINE_CAT_COUNT
} vkSpinePassCategory;

/* Access kinds for declared resource edges. */
typedef enum {
	VK_SPINE_ACCESS_SAMPLED_READ           = 1u << 0,
	VK_SPINE_ACCESS_STORAGE_READ           = 1u << 1,
	VK_SPINE_ACCESS_STORAGE_WRITE          = 1u << 2,
	VK_SPINE_ACCESS_COLOR_WRITE            = 1u << 3,
	VK_SPINE_ACCESS_DEPTH_READ             = 1u << 4,
	VK_SPINE_ACCESS_DEPTH_WRITE            = 1u << 5,
	VK_SPINE_ACCESS_TRANSFER_READ          = 1u << 6,
	VK_SPINE_ACCESS_TRANSFER_WRITE         = 1u << 7,
	VK_SPINE_ACCESS_INDIRECT_READ          = 1u << 8,
	VK_SPINE_ACCESS_AS_READ                = 1u << 9,
	VK_SPINE_ACCESS_HISTORY_READ           = 1u << 10,
	VK_SPINE_ACCESS_HISTORY_WRITE          = 1u << 11
} vkSpineAccess;

/* Production-spine attachment / buffer identities. */
typedef enum {
	VK_SPINE_RES_NONE = 0,
	VK_SPINE_RES_SWAPCHAIN_COLOR,
	VK_SPINE_RES_DEPTH,
	VK_SPINE_RES_HDR_COLOR,
	VK_SPINE_RES_MOTION_VECTORS,
	VK_SPINE_RES_GBUFFER_ALBEDO,
	VK_SPINE_RES_GBUFFER_NORMAL,
	VK_SPINE_RES_GBUFFER_MATERIAL,
	VK_SPINE_RES_DEFERRED_LIGHTING,
	VK_SPINE_RES_SSAO,
	VK_SPINE_RES_AV_HISTORY,
	VK_SPINE_RES_AV_FILTERED,
	VK_SPINE_RES_SSR,
	VK_SPINE_RES_OIT_ACCUM,
	VK_SPINE_RES_OIT_REVEAL,
	VK_SPINE_RES_OIT_MOMENTS,
	VK_SPINE_RES_OIT_B0,
	VK_SPINE_RES_TAA_HISTORY,
	VK_SPINE_RES_REACTIVE_MASK,
	VK_SPINE_RES_BLOOM_CHAIN,
	VK_SPINE_RES_EXPOSURE_LUMINANCE,
	VK_SPINE_RES_SHADOW_SUN,
	VK_SPINE_RES_FROXEL_SCATTER,
	VK_SPINE_RES_FORWARD_PLUS_LIGHTS,
	VK_SPINE_RES_PROBE_GRID,
	VK_SPINE_RES_PROBE_IRRADIANCE,
	VK_SPINE_RES_SSGI_RADIANCE,
	VK_SPINE_RES_SSGI_CONFIDENCE,
	VK_SPINE_RES_RADIANCE_CLIPMAP,
	VK_SPINE_RES_RADIANCE_CACHE_IRRADIANCE,
	VK_SPINE_RES_INDIRECT_DIFFUSE,
	VK_SPINE_RES_VISIBILITY_IDS,
	VK_SPINE_RES_VISIBILITY_BARY,
	VK_SPINE_RES_VISIBILITY_CLASS,
	VK_SPINE_RES_VIRTUAL_GEOMETRY_MESHLETS,
	VK_SPINE_RES_VIRTUAL_GEOMETRY_INDIRECT,
	VK_SPINE_RES_SURFEL_POOL,
	VK_SPINE_RES_SURFEL_HASH,
	VK_SPINE_RES_SURFEL_IRRADIANCE,
	/* Raster Ultra 1.14 terrain / vegetation */
	VK_SPINE_RES_TERRAIN_HEIGHT,
	VK_SPINE_RES_TERRAIN_CHUNK_META,
	VK_SPINE_RES_TERRAIN_LOD_STATE,
	VK_SPINE_RES_BIOME_MAP,
	VK_SPINE_RES_TERRAIN_LAYER_MAP,
	VK_SPINE_RES_VEG_INSTANCE_BUFFER,
	VK_SPINE_RES_VEG_VISIBLE_LIST,
	VK_SPINE_RES_VEG_INDIRECT,
	VK_SPINE_RES_VEG_WIND_FIELD,
	VK_SPINE_RES_VEG_INTERACTION_FIELD,
	VK_SPINE_RES_TERRAIN_DEFORM,
	VK_SPINE_RES_TERRAIN_RESIDENCY,
	VK_SPINE_RES_COUNT
} vkSpineResourceId;

/* Minimum registered production passes (Spine 1.0). */
typedef enum {
	VK_SPINE_PASS_NONE = 0,
	VK_SPINE_PASS_FRAME_PREP,
	VK_SPINE_PASS_LIGHT_PACK,
	VK_SPINE_PASS_TILE_CONSTRUCT,
	VK_SPINE_PASS_SUN_SHADOW,
	VK_SPINE_PASS_WORLD_OPAQUE,
	VK_SPINE_PASS_GBUFFER_FILL,
	VK_SPINE_PASS_DEFERRED_LIGHTING,
	VK_SPINE_PASS_FORWARD_PLUS_OPAQUE,
	VK_SPINE_PASS_SSR,
	VK_SPINE_PASS_AMBIENT_VISIBILITY,
	VK_SPINE_PASS_RASTER_GI,
	VK_SPINE_PASS_FROXEL_VOLUME,
	VK_SPINE_PASS_TRANSPARENT_FORWARD_PLUS,
	VK_SPINE_PASS_WBOIT_ACCUM,
	VK_SPINE_PASS_MBOIT_MOMENTS,
	VK_SPINE_PASS_MBOIT_ACCUM,
	VK_SPINE_PASS_OIT_RESOLVE,
	VK_SPINE_PASS_REACTIVE_MASK,
	VK_SPINE_PASS_TEMPORAL_RECON,
	VK_SPINE_PASS_SPATIAL_AA,
	VK_SPINE_PASS_SMAA,
	VK_SPINE_PASS_BLOOM,
	VK_SPINE_PASS_EYE_ADAPTATION,
	VK_SPINE_PASS_WEAPON,
	VK_SPINE_PASS_HUD_2D,
	VK_SPINE_PASS_PRESENTATION,
	VK_SPINE_PASS_HISTORY_MAINT,
	VK_SPINE_PASS_VISIBILITY_FILL,
	VK_SPINE_PASS_MATERIAL_CLASSIFY,
	VK_SPINE_PASS_VIRTUAL_GEOMETRY_CULL,
	VK_SPINE_PASS_VIRTUAL_GEOMETRY_DRAW,
	VK_SPINE_PASS_SURFEL_GI_UPDATE,
	VK_SPINE_PASS_SURFEL_GI_HASH,
	VK_SPINE_PASS_SURFEL_GI_RESOLVE,
	VK_SPINE_PASS_SURFEL_GI_COMPOSITE,
	/* Raster Ultra 1.14 terrain / vegetation */
	VK_SPINE_PASS_TERRAIN_LOD,
	VK_SPINE_PASS_TERRAIN_CULL,
	VK_SPINE_PASS_TERRAIN_DRAW,
	VK_SPINE_PASS_BIOME_EVAL,
	VK_SPINE_PASS_VEG_GENERATE,
	VK_SPINE_PASS_VEG_CULL,
	VK_SPINE_PASS_VEG_DRAW,
	VK_SPINE_PASS_VEG_WIND,
	VK_SPINE_PASS_VEG_INTERACTION,
	VK_SPINE_PASS_TERRAIN_DEFORM,
	VK_SPINE_PASS_TERRAIN_RESIDENCY,
	VK_SPINE_PASS_COUNT
} vkSpinePassId;

typedef struct {
	vkSpineResourceId resource;
	uint32_t access; /* vkSpineAccess bits */
} vkSpineResourceEdge;

void vk_spine_registry_init( void );
void vk_spine_registry_shutdown( void );

void vk_spine_frame_begin( void );
void vk_spine_frame_end( void );

void vk_spine_pass_begin( vkSpinePassId pass );
void vk_spine_pass_end( vkSpinePassId pass );
/* Map sticky pass_diag / scene-pass string names onto registry IDs. */
void vk_spine_pass_begin_named( const char *name, uint32_t width, uint32_t height );
void vk_spine_pass_end_named( const char *name );

void vk_spine_note_write( vkSpineResourceId res, vkSpinePassId pass, uint32_t access );
void vk_spine_note_read( vkSpineResourceId res, vkSpinePassId pass, uint32_t access );
/* Attachment clear / layout-barrier contracts (cheap stamps; validated when r_spineValidate > 0). */
void vk_spine_note_clear( vkSpineResourceId res, vkSpinePassId pass );
void vk_spine_note_barrier( vkSpineResourceId res, vkSpinePassId pass, const char *reason );
/* Authoritative image transition hook used by the single Vulkan transition helper. */
void vk_spine_transition_image( VkImage image, VkImageLayout old_layout, VkImageLayout new_layout,
	vkSpinePassId pass, const char *reason );
/* Resolve a live Vulkan image to its Spine resource, or VK_SPINE_RES_NONE. */
vkSpineResourceId vk_spine_resource_for_image( VkImage image );
vkSpinePassId vk_spine_current_pass( void );
/* Track expected VkImageLayout for Spine attachments (cheap stamp; expect checks when validating). */
void vk_spine_note_layout( vkSpineResourceId res, VkImageLayout layout );
void vk_spine_expect_layout( vkSpineResourceId res, VkImageLayout expected, vkSpinePassId pass, const char *where );
VkImageLayout vk_spine_resource_layout( vkSpineResourceId res );
const char *vk_spine_layout_name( VkImageLayout layout );
qboolean vk_spine_resource_cleared_this_frame( vkSpineResourceId res );
qboolean vk_spine_resource_barriered_this_frame( vkSpineResourceId res );

void vk_spine_attachments_created( uint32_t width, uint32_t height );
void vk_spine_attachments_destroyed( void );
/* Call after vk_update_attachment_descriptors rebinds views to live attachments. */
void vk_spine_note_descriptors_rebound( void );

void vk_spine_note_temporal_history( vkSpineResourceId res, qboolean valid );
void vk_spine_validate_feature_combos( void );
/* Illegal OIT×TAA without weapon-after: suppress world TAA for this frame. */
qboolean vk_spine_combo_suppress_taa( void );
const char *vk_spine_combo_fallback( void );

/* Spine 1.1: mode3 + WBOIT + Temporal Reconstruction + weapon-after. */
qboolean vk_spine_is_spine_1_1_combo( void );
qboolean vk_spine_cert_active( void );
/* Skipped OIT: HDR color remains the valid scene-color producer (single-frame OIT never becomes history). */
void vk_spine_note_oit_skipped( void );
/* Cert: TAA current must be resolved world HDR — never raw OIT accum/reveal/moments. */
void vk_spine_cert_check_taa_input( VkImageView taa_src );
void vk_spine_cert_check_black_frame( void );
void vk_spine_cert_check_resource_growth( void );
/* After sticky temporal resets (resize/vid_restart/focus/map), history must be invalid. */
void vk_spine_cert_check_history_invalidated( uint32_t resetReasons );
/* Weapon flush must not precede Temporal Reconstruction when world TAA ran this frame. */
void vk_spine_cert_check_weapon_flush_order( qboolean taaRanThisFrame );

qboolean vk_spine_validate_enabled( void );
uint32_t vk_spine_attachment_generation( void );
uint32_t vk_spine_descriptor_generation( void );
uint32_t vk_spine_violation_count( void );
void vk_spine_reset_cert_counters( void );
const char *vk_spine_pass_name( vkSpinePassId pass );
const char *vk_spine_resource_name( vkSpineResourceId res );
const char *vk_spine_phase_name( vkSpinePhase phase );
vkSpinePassId vk_spine_last_writer( vkSpineResourceId res );

void vk_spine_dump_device_lost( void );
void vk_spine_status_f( void );

/* Raster Ultra 2.0 — production frame-contract checks (opt-in via r_spineValidate / Ultra). */
void vk_spine_validate_ultra_frame_contract( void );
qboolean vk_spine_ultra_contract_ok( void );
const char *vk_spine_ultra_contract_reason( void );

#endif /* VK_PASS_REGISTRY_H */
