#pragma once

#ifdef USE_VULKAN

void vk_forward_plus_init( void );
void vk_forward_plus_shutdown( void );
/* Packs visible dynamic lights from backEnd.refdef into a host-visible SSBO (scaffolding). */
void vk_forward_plus_update_for_refdef( void );
void vk_forward_plus_dispatch_tile_cull( void );
/* Teardown order: pipeline before descriptor pool; layout after pool (vk_shutdown). */
void vk_forward_plus_destroy_compute_pipeline( void );
void vk_forward_plus_destroy_descriptor_layout( void );

#endif /* USE_VULKAN */
