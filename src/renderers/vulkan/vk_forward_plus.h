#pragma once

#ifdef USE_VULKAN

void vk_forward_plus_create_set_layout( void );
void vk_forward_plus_init( void );
void vk_forward_plus_shutdown( void );
/* Packs visible dynamic lights from backEnd.refdef into a host-visible SSBO (scaffolding). */
void vk_forward_plus_update_for_refdef( void );
/* Resize tile SSBO when FBO / r_renderScale resolution changes (no vid_restart). */
void vk_forward_plus_ensure_render_resolution( void );
void vk_forward_plus_dispatch_tile_cull( void );
VkDescriptorSet vk_forward_plus_get_graphics_descriptor_set( void );
/* Teardown order: pipeline before descriptor pool; layout after pool (vk_shutdown). */
void vk_forward_plus_destroy_compute_pipeline( void );
void vk_forward_plus_on_descriptor_pool_destroyed( void );
#ifdef USE_VK_PBR
void vk_forward_plus_init_graphics_descriptors( void );
void vk_forward_plus_destroy_graphics_layout( void );
#endif

#endif /* USE_VULKAN */
