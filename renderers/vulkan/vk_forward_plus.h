#pragma once

#ifdef USE_VULKAN

/* GPU Forward+ light records (decoupled from surface dlightBits, which stays MAX_DLIGHTS). */
#define VK_FP_MAX_GPU_LIGHTS 64

/* Shared cluster aliases (docs/RENDERER_PATH_OWNERSHIP.md) — same SSBOs as Forward+. */
#define VK_CLUSTER_TILE_SIZE 16u

/* Tile list capacity (SSBO stride = max cap uint32s per tile); must match forward_plus_tile_cull.comp MAX_PER_TILE. */
uint32_t vk_forward_plus_get_min_per_tile_cap( void );
uint32_t vk_forward_plus_get_max_per_tile_cap( void );

/* Contract: deferred lighting + Forward+ fragment bind the same tile buffer generation. */
void vk_cluster_assert_shared_consumers( const char *consumer );
uint32_t vk_cluster_list_generation( void );
void vk_cluster_register_commands( void );
void vk_cluster_unregister_commands( void );
/* Transparent submission lifecycle; geometry marking remains a later GPU stage. */
void vk_cluster_transparent_begin_frame( void );
void vk_cluster_transparent_note_submission( const char *owner );
void vk_cluster_transparent_note_candidate( qboolean additive );
void vk_cluster_transparent_note_accepted( qboolean additive );
void vk_cluster_transparent_print_status( void );

/* Thin wrappers naming the shared cluster API (call existing Forward+ implementations). */
void vk_cluster_dispatch_tile_cull( void );
VkBuffer vk_cluster_tile_buffer( void );
VkBuffer vk_cluster_light_buffer( void );

void vk_forward_plus_create_set_layout( void );
void vk_forward_plus_init( void );
void vk_forward_plus_shutdown( void );
void vk_forward_plus_ensure_runtime( void );
/* Packs dynamic lights from backEnd.refdef; staging buffer — call vk_forward_plus_upload_refdef after (GPU copy to device). */
void vk_forward_plus_update_for_refdef( void );
/* After vk_begin_frame (command buffer valid), before main pass: copy staging -> device-local light SSBO. */
void vk_forward_plus_upload_refdef( void );
/* Resize tile SSBO when FBO / r_renderScale resolution changes (no vid_restart). */
void vk_forward_plus_ensure_render_resolution( void );
void vk_forward_plus_dispatch_tile_cull( void );
void vk_forward_plus_dispatch_tile_cull_after_opaque( void );
/* Stamp OIT/accum viewport into Forward+ param SSBO (no re-cull). */
void vk_forward_plus_refresh_viewport_params( uint32_t width, uint32_t height );
VkDescriptorSet vk_forward_plus_get_graphics_descriptor_set( void );
/* Teardown order: pipeline before descriptor pool; layout after pool (vk_shutdown). */
void vk_forward_plus_destroy_compute_pipeline( void );
void vk_forward_plus_on_descriptor_pool_destroyed( void );
void vk_forward_plus_update_depth_descriptor( void );
void vk_forward_plus_update_sun_shadow_descriptor( void );
#ifdef USE_VK_PBR
void vk_forward_plus_init_graphics_descriptors( void );
void vk_forward_plus_destroy_graphics_layout( void );
#endif

#endif /* USE_VULKAN */
