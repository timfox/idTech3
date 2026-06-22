/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Host-visible uniform buffers for volumetric params and per-CB postfx params.
Extracted from vk.c for incremental modularization.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_attachments.h"
#include "vk_postfx_params.h"
#include "vk_temporal.h"
#include "vk_util.h"
#include "vk_volumetric_params.h"

void vk_destroy_volumetric_params_buffer( void )
{
	if ( vk.volumetric_params_ptr ) {
		qvkUnmapMemory( vk.device, vk.volumetric_params_memory );
		vk.volumetric_params_ptr = NULL;
	}

	if ( vk.volumetric_params_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.volumetric_params_buffer, NULL );
		vk.volumetric_params_buffer = VK_NULL_HANDLE;
		vk.volumetric_params_buffer_size = 0;
	}

	if ( vk.volumetric_params_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.volumetric_params_memory, NULL );
		vk.volumetric_params_memory = VK_NULL_HANDLE;
	}
	vk_reset_volumetric_history();
	vk_temporal_request_sticky_reset( VK_TEMPORAL_RESET_MISSING_PREV_DATA );
}

void vk_create_volumetric_params_buffer( void )
{
	if ( vk.volumetric_params_buffer != VK_NULL_HANDLE ) {
		return;
	}

	VkBufferCreateInfo desc;
	VkMemoryRequirements mem_req;
	VkMemoryAllocateInfo alloc_info;

	desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.size = sizeof( volumetric_params_t );
	desc.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;

	VK_CHECK( qvkCreateBuffer( vk.device, &desc, NULL, &vk.volumetric_params_buffer ) );

	qvkGetBufferMemoryRequirements( vk.device, vk.volumetric_params_buffer, &mem_req );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.volumetric_params_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.volumetric_params_buffer, vk.volumetric_params_memory, 0 ) );

	vk.volumetric_params_buffer_size = mem_req.size;

	VK_CHECK( qvkMapMemory( vk.device, vk.volumetric_params_memory, 0, vk.volumetric_params_buffer_size, 0, &vk.volumetric_params_ptr ) );
	vk_reset_volumetric_history();
	vk_temporal_request_sticky_reset( VK_TEMPORAL_RESET_MISSING_PREV_DATA );
}

void vk_destroy_postfx_params_buffers( void )
{
	uint32_t i;

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		if ( vk.postfx_params_memory[i] != VK_NULL_HANDLE ) {
			if ( vk.postfx_params_ptr[i] ) {
				qvkUnmapMemory( vk.device, vk.postfx_params_memory[i] );
				vk.postfx_params_ptr[i] = NULL;
			}
			qvkFreeMemory( vk.device, vk.postfx_params_memory[i], NULL );
			vk.postfx_params_memory[i] = VK_NULL_HANDLE;
		}
		if ( vk.postfx_params_buffer[i] != VK_NULL_HANDLE ) {
			qvkDestroyBuffer( vk.device, vk.postfx_params_buffer[i], NULL );
			vk.postfx_params_buffer[i] = VK_NULL_HANDLE;
		}
		vk.postfx_params_descriptor[i] = VK_NULL_HANDLE;
	}
}

void vk_create_postfx_params_buffers( void )
{
	uint32_t i;

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		VkBufferCreateInfo desc;
		VkMemoryRequirements mem_req;
		VkMemoryAllocateInfo alloc_info;

		if ( vk.postfx_params_buffer[i] != VK_NULL_HANDLE ) {
			continue;
		}

		Com_Memset( &desc, 0, sizeof( desc ) );
		desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		desc.size = sizeof( VkPostFXParams );
		desc.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK( qvkCreateBuffer( vk.device, &desc, NULL, &vk.postfx_params_buffer[i] ) );
		qvkGetBufferMemoryRequirements( vk.device, vk.postfx_params_buffer[i], &mem_req );

		Com_Memset( &alloc_info, 0, sizeof( alloc_info ) );
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = mem_req.size;
		alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device,
			mem_req.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

		VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.postfx_params_memory[i] ) );
		VK_CHECK( qvkBindBufferMemory( vk.device, vk.postfx_params_buffer[i], vk.postfx_params_memory[i], 0 ) );
		VK_CHECK( qvkMapMemory( vk.device, vk.postfx_params_memory[i], 0, mem_req.size, 0, &vk.postfx_params_ptr[i] ) );
		Com_Memset( vk.postfx_params_ptr[i], 0, sizeof( VkPostFXParams ) );
	}
}
