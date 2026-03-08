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
void        vk_log_post_fog_rebind( const char *reason, VkImageView color_source );

/* Throttle FBO debug logs to at most once per second. Internal use. */
qboolean    vk_post_fog_fbo_debug_throttle( void );

#ifdef __cplusplus
}
#endif
