/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Small graphics pipeline factory helpers shared by post-processing and the
main pipeline creator (split from vk.c).
===========================================================================
*/

#ifndef VK_PIPELINE_HELPERS_H
#define VK_PIPELINE_HELPERS_H

#include "../common/vulkan/vulkan.h"
#include "tr_common.h"

#ifdef __cplusplus
extern "C" {
#endif

void vk_set_shader_stage_desc( VkPipelineShaderStageCreateInfo *desc, VkShaderStageFlagBits stage, VkShaderModule shader_module, const char *entry );

void vk_create_atmosphere_pipeline( void );
void vk_create_oit_accum_pipeline( void );
void vk_create_blur_pipeline( uint32_t index, uint32_t width, uint32_t height, qboolean horizontal_pass );

#ifdef __cplusplus
}
#endif

#endif
