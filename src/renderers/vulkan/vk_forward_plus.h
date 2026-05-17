#pragma once

#ifdef USE_VULKAN

/* Tile list capacity (SSBO stride = max cap uint32s per tile); must match forward_plus_tile_cull.comp MAX_PER_TILE. */
uint32_t vk_forward_plus_get_min_per_tile_cap( void );
uint32_t vk_forward_plus_get_max_per_tile_cap( void );

void vk_forward_plus_create_set_layout( void );
void vk_forward_plus_init( void );
void vk_forward_plus_shutdown( void );
/* Packs dynamic lights from backEnd.refdef; staging buffer — call vk_forward_plus_upload_refdef after (GPU copy to device). */
void vk_forward_plus_update_for_refdef( void );
/* After vk_begin_frame (command buffer valid), before main pass: copy staging -> device-local light SSBO. */
void vk_forward_plus_upload_refdef( void );
/* Resize tile SSBO when FBO / r_renderScale resolution changes (no vid_restart). */
void vk_forward_plus_ensure_render_resolution( void );
void vk_forward_plus_dispatch_tile_cull( void );
void vk_forward_plus_dispatch_tile_cull_after_opaque( void );
VkDescriptorSet vk_forward_plus_get_graphics_descriptor_set( void );
/* Teardown order: pipeline before descriptor pool; layout after pool (vk_shutdown). */
void vk_forward_plus_destroy_compute_pipeline( void );
void vk_forward_plus_on_descriptor_pool_destroyed( void );
#ifdef USE_VK_PBR
void vk_forward_plus_init_graphics_descriptors( void );
void vk_forward_plus_destroy_graphics_layout( void );
#endif

#endif /* USE_VULKAN */
