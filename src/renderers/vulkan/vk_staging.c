/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vulkan staging buffer for texture/buffer uploads.
Extracted from vk.c for incremental modularization.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_staging.h"
#include "vk_util.h"

void vk_clean_staging_buffer( void )
{
	if ( vk.device == VK_NULL_HANDLE || qvkDestroyBuffer == NULL ) {
		return;
	}
	if ( vk.staging_buffer.handle != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.staging_buffer.handle, NULL );
		vk.staging_buffer.handle = VK_NULL_HANDLE;
	}

	if ( vk.staging_buffer.memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.staging_buffer.memory, NULL );
		vk.staging_buffer.memory = VK_NULL_HANDLE;
	}

	vk.staging_buffer.ptr = NULL;
	vk.staging_buffer.size = 0;
}


void vk_alloc_staging_buffer( VkDeviceSize size )
{
	VkBufferCreateInfo buffer_desc;
	VkMemoryRequirements memory_requirements;
	VkMemoryAllocateInfo alloc_info;
	uint32_t memory_type;
	void *data;

	vk_clean_staging_buffer();

	vk.staging_buffer.size = MAX( size, STAGING_BUFFER_SIZE );
	vk.staging_buffer.size = PAD( vk.staging_buffer.size, 1024 * 1024 );

	buffer_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_desc.pNext = NULL;
	buffer_desc.flags = 0;
	buffer_desc.size = vk.staging_buffer.size;
	buffer_desc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	buffer_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buffer_desc.queueFamilyIndexCount = 0;
	buffer_desc.pQueueFamilyIndices = NULL;
	VK_CHECK(qvkCreateBuffer(vk.device, &buffer_desc, NULL, &vk.staging_buffer.handle));

	qvkGetBufferMemoryRequirements( vk.device, vk.staging_buffer.handle, &memory_requirements );

	memory_type = vk_find_memory_type( vk.physical_device, memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = memory_type;

	VK_CHECK(qvkAllocateMemory(vk.device, &alloc_info, NULL, &vk.staging_buffer.memory));
	VK_CHECK(qvkBindBufferMemory(vk.device, vk.staging_buffer.handle, vk.staging_buffer.memory, 0));

	VK_CHECK(qvkMapMemory(vk.device, vk.staging_buffer.memory, 0, VK_WHOLE_SIZE, 0, &data));
	vk.staging_buffer.ptr = (byte*)data;
	vk_set_object_name( (uint64_t)vk.staging_buffer.handle, "staging buffer", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	vk_set_object_name( (uint64_t)vk.staging_buffer.memory, "staging buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );
}
