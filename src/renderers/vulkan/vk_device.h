/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vulkan device and format selection helpers.
Extracted from vk.c for incremental modularization.
===========================================================================
*/

#pragma once

#include "../common/tr_types.h"
#include "../common/vulkan/vulkan.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Check if format has all required features (optimal tiling). */
qboolean vk_format_has_features( VkPhysicalDevice physical_device, VkFormat format, VkFormatFeatureFlags required );

/* Select surface format and store in vk.base_format, vk.present_format.
 * Returns qfalse on failure. */
qboolean vk_select_surface_format( VkPhysicalDevice physical_device, VkSurfaceKHR surface );

/* Set vk.depth_format, vk.color_format, vk.bloom_format, vk.blitEnabled, etc.
 * Call after vk_select_surface_format. */
void vk_setup_surface_formats( VkPhysicalDevice physical_device );

/* Format physical device properties as human-readable string (e.g. "Discrete NVIDIA GeForce RTX 3080, 0x2206"). */
const char *vk_device_renderer_name( const VkPhysicalDeviceProperties *props );

#ifdef __cplusplus
}
#endif
