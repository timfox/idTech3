/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vulkan staging buffer for texture/buffer uploads.
Extracted from vk.c for incremental modularization.
===========================================================================
*/

#pragma once

#include "../common/vulkan/vulkan.h"
#include "tr_common.h"

#ifdef __cplusplus
extern "C" {
#endif

void vk_clean_staging_buffer( void );
void vk_alloc_staging_buffer( VkDeviceSize size );

#ifdef __cplusplus
}
#endif
