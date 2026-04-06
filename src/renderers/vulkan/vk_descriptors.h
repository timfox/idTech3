/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vulkan descriptor set layout and update helpers.
Extracted from vk.c for incremental modularization.
===========================================================================
*/

#pragma once

#include "../common/vulkan/vulkan.h"
#include "tr_common.h"

#ifdef __cplusplus
extern "C" {
#endif

void vk_create_layout_binding( int binding, VkDescriptorType type, VkShaderStageFlags flags, VkDescriptorSetLayout *layout );
void vk_create_uniform_layout( VkDescriptorSetLayout *layout );
void vk_write_buffer_descriptor( VkWriteDescriptorSet *desc, VkDescriptorBufferInfo *info,
	VkBuffer buffer, VkDescriptorSet descriptor, const uint32_t binding, VkDeviceSize size, VkDescriptorType type );
void vk_update_uniform_descriptor( VkDescriptorSet descriptor, VkBuffer buffer );

#ifdef __cplusplus
}
#endif
