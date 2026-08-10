/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Per-frame tessellation upload, vertex/index binding, descriptor binding,
pipeline binding, and immediate draws (split from vk.c).
Included from vk.h after vk_create_pipeline.h.
===========================================================================
*/

#ifndef VK_DRAW_STATE_H
#define VK_DRAW_STATE_H

#include <stddef.h>
#include "../common/vulkan/vulkan.h"
#include "tr_common.h"

void vk_bind_pipeline( uint32_t pipeline );
void vk_bind_index( void );
void vk_bind_index_ext( const int numIndexes, const uint32_t *indexes );
void vk_bind_geometry( uint32_t flags );
void vk_bind_lighting( int stage, int bundle );
void vk_draw_geometry( Vk_Depth_Range depth_range, qboolean indexed );
void vk_draw_dot( uint32_t storage_offset );

uint32_t vk_tess_index( uint32_t numIndexes, const void *src );
void *vk_alloc_storage( size_t size, uint32_t *offset );
void vk_set_iqm_storage_offsets( uint32_t skin_offset, uint32_t morph_offset, uint32_t topo_offset );
void vk_reset_iqm_storage_offsets( void );
void vk_bind_index_buffer( VkBuffer buffer, uint32_t offset );
void vk_draw_indexed( uint32_t indexCount, uint32_t firstIndex );
void vk_reset_descriptor( int index );
void vk_update_descriptor( int index, VkDescriptorSet descriptor );
void vk_update_descriptor_offset( int index, uint32_t offset );
void vk_bind_descriptor_sets( void );

#endif
