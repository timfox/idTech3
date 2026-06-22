/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Main-pass MSAA sample count (vkSamples / vkMaxSamples) shared by vk_initialize
and vk_get_main_rasterization_samples.
===========================================================================
*/

#pragma once

#include "../common/vulkan/vulkan.h"
#include "../common/tr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void vk_raster_samples_configure( const VkPhysicalDeviceProperties *props, qboolean msaaActive );

#ifdef __cplusplus
}
#endif
