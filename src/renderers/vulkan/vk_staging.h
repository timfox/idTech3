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

#ifdef USE_UPLOAD_QUEUE
qboolean vk_wait_staging_buffer( void );
void vk_flush_staging_buffer( qboolean final );
#endif

#ifdef __cplusplus
}
#endif
