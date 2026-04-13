/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Volumetric fog internals: MSAA depth resolve, fluid sim dispatch, perf timers.
Split from vk.c.
===========================================================================
*/

#ifndef VK_VOLUMETRIC_INTERNAL_H
#define VK_VOLUMETRIC_INTERNAL_H

#include "../common/vulkan/vulkan.h"
#include "tr_common.h"

void vk_resolve_volumetric_depth_msaa( void );
void vk_fluid_simulation_pass( float delta_time );
void vk_write_volumetric_timestamp( uint32_t query_index, VkPipelineStageFlagBits stage );
void vk_update_volumetric_perf_queries( void );
void vk_set_volumetric_pass_params( float stage, float x, float y, float z );
void vk_volumetric_stage_barrier( VkImage image );

#endif
