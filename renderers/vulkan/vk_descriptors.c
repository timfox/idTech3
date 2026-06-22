/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vulkan descriptor set layout and update helpers.
Extracted from vk.c for incremental modularization.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_descriptors.h"
#include "vk_util.h"

void vk_create_layout_binding( int binding, VkDescriptorType type, VkShaderStageFlags flags, VkDescriptorSetLayout *layout )
{
	VkDescriptorSetLayoutBinding bind[1];
	VkDescriptorSetLayoutCreateInfo desc;

	bind[0].binding = binding;
	bind[0].descriptorType = type;
	bind[0].descriptorCount = 1;
	bind[0].stageFlags = flags;
	bind[0].pImmutableSamplers = NULL;

	desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.bindingCount = ARRAY_LEN( bind );
	desc.pBindings = bind;

	VK_CHECK( qvkCreateDescriptorSetLayout(vk.device, &desc, NULL, layout ) );
}

void vk_create_uniform_layout( VkDescriptorSetLayout *layout )
{
	VkDescriptorSetLayoutBinding bindings[VK_DESC_UNIFORM_COUNT];
	VkDescriptorSetLayoutCreateInfo desc;

	Com_Memset( bindings, 0, sizeof( bindings ) );

	bindings[VK_DESC_UNIFORM_MAIN_BINDING].binding = VK_DESC_UNIFORM_MAIN_BINDING;
	bindings[VK_DESC_UNIFORM_MAIN_BINDING].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	bindings[VK_DESC_UNIFORM_MAIN_BINDING].descriptorCount = 1;
	bindings[VK_DESC_UNIFORM_MAIN_BINDING].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;

	bindings[VK_DESC_UNIFORM_CAMERA_BINDING].binding = VK_DESC_UNIFORM_CAMERA_BINDING;
	bindings[VK_DESC_UNIFORM_CAMERA_BINDING].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	bindings[VK_DESC_UNIFORM_CAMERA_BINDING].descriptorCount = 1;
	bindings[VK_DESC_UNIFORM_CAMERA_BINDING].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	bindings[VK_DESC_UNIFORM_IQM_SKIN_BINDING].binding = VK_DESC_UNIFORM_IQM_SKIN_BINDING;
	bindings[VK_DESC_UNIFORM_IQM_SKIN_BINDING].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
	bindings[VK_DESC_UNIFORM_IQM_SKIN_BINDING].descriptorCount = 1;
	bindings[VK_DESC_UNIFORM_IQM_SKIN_BINDING].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	bindings[VK_DESC_UNIFORM_IQM_MORPH_BINDING].binding = VK_DESC_UNIFORM_IQM_MORPH_BINDING;
	bindings[VK_DESC_UNIFORM_IQM_MORPH_BINDING].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
	bindings[VK_DESC_UNIFORM_IQM_MORPH_BINDING].descriptorCount = 1;
	bindings[VK_DESC_UNIFORM_IQM_MORPH_BINDING].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	bindings[VK_DESC_UNIFORM_GLTF_TOPO_BINDING].binding = VK_DESC_UNIFORM_GLTF_TOPO_BINDING;
	bindings[VK_DESC_UNIFORM_GLTF_TOPO_BINDING].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
	bindings[VK_DESC_UNIFORM_GLTF_TOPO_BINDING].descriptorCount = 1;
	bindings[VK_DESC_UNIFORM_GLTF_TOPO_BINDING].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.bindingCount = ARRAY_LEN( bindings );
	desc.pBindings = bindings;

	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &desc, NULL, layout ) );
}

void vk_write_buffer_descriptor( VkWriteDescriptorSet *desc, VkDescriptorBufferInfo *info,
	VkBuffer buffer, VkDescriptorSet descriptor, const uint32_t binding, VkDeviceSize size, VkDescriptorType type )
{
	info[binding].buffer = buffer;
	info[binding].offset = 0;
	info[binding].range = size;

	desc[binding].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	desc[binding].dstSet = descriptor;
	desc[binding].dstBinding = binding;
	desc[binding].dstArrayElement = 0;
	desc[binding].descriptorCount = 1;
	desc[binding].pNext = NULL;
	desc[binding].descriptorType = type;
	desc[binding].pImageInfo = NULL;
	desc[binding].pBufferInfo = &info[binding];
	desc[binding].pTexelBufferView = NULL;
}

void vk_update_uniform_descriptor( VkDescriptorSet descriptor, VkBuffer buffer )
{
	VkDescriptorBufferInfo info[VK_DESC_UNIFORM_COUNT];
	VkWriteDescriptorSet desc[VK_DESC_UNIFORM_COUNT];

	vk_write_buffer_descriptor( desc, info, buffer, descriptor, VK_DESC_UNIFORM_MAIN_BINDING,
		(VkDeviceSize)sizeof( vkUniform_t ), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC );
	vk_write_buffer_descriptor( desc, info, buffer, descriptor, VK_DESC_UNIFORM_CAMERA_BINDING,
		(VkDeviceSize)sizeof( vkUniformCamera_t ), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC );
	vk_write_buffer_descriptor( desc, info, buffer, descriptor, VK_DESC_UNIFORM_IQM_SKIN_BINDING,
		VK_WHOLE_SIZE, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC );
	vk_write_buffer_descriptor( desc, info, buffer, descriptor, VK_DESC_UNIFORM_IQM_MORPH_BINDING,
		VK_WHOLE_SIZE, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC );
	vk_write_buffer_descriptor( desc, info, buffer, descriptor, VK_DESC_UNIFORM_GLTF_TOPO_BINDING,
		VK_WHOLE_SIZE, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC );

	qvkUpdateDescriptorSets(vk.device, VK_DESC_UNIFORM_COUNT, desc, 0, NULL);
}
