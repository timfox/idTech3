/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Post-fog color source selection and descriptor helpers for the Vulkan
FBO pipeline. Used by luminance, gamma, and volumetric fog passes.
===========================================================================
*/

#pragma once

#include "../common/tr_types.h"
#include "../common/vulkan/vulkan.h"

#ifdef __cplusplus
extern "C" {
#endif

const char *vk_post_fog_source_name( VkImageView color_source );
VkImage      vk_post_fog_source_image( VkImageView color_source );
VkImageView  vk_get_post_fog_source( void );
VkImageView  vk_get_luminance_source( void );
void        vk_reset_post_fog_frame_state( void );
void        vk_set_scene_post_fog_source( VkImageView color_source );
void        vk_log_post_fog_rebind( const char *reason, VkImageView color_source );
void        vk_update_color_descriptor_image( VkImageView color_view );
void        vk_update_luminance_descriptor_image( VkImageView color_view );
void        vk_barrier_post_fog_source_for_sampling( VkImageView color_source, const char *reason );
void        vk_barrier_motion_vector_for_sampling( const char *reason );
void        vk_update_post_fog_descriptors( VkImageView color_source );

/* Throttle FBO debug logs to at most once per second. Internal use. */
qboolean    vk_post_fog_fbo_debug_throttle( void );

#ifdef __cplusplus
}
#endif
