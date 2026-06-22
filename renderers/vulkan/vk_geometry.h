/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vulkan geometry and storage buffer creation.
Extracted from vk.c for incremental modularization.
===========================================================================
*/

#pragma once

#include "../common/tr_types.h"
#include "../common/vulkan/vulkan.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Release per-frame geometry buffers (vertex_buffer) and shared memory. */
void vk_release_geometry_buffers( void );

/* Create per-frame geometry buffers. Call with vk.geometry_buffer_size_new. */
void vk_create_geometry_buffers( VkDeviceSize size );

/* Create storage buffer for flare/feedback. Call with MAX_FLARES * vk.storage_alignment. */
void vk_create_storage_buffer( uint32_t size );

#ifdef __cplusplus
}
#endif
