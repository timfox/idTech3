/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vulkan glTF buffer creation: device-local vertex/index buffers for glTF primitives.
Extracted from vk.c for incremental modularization.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_cmd.h"
#include "vk_staging.h"
#include "vk_util.h"

/*
===============
vk_create_gltf_buffers
===============
Creates device-local vertex and index buffers for a glTF primitive.
vboData: packed vertex data (xyz vec4, rgba u8, st vec2, normal vec4 per vertex)
vboSize: total bytes
idxData/idxCount: index buffer
*/
qboolean vk_create_gltf_buffers( const byte *vboData, int vboSize, const uint32_t *idxData, int idxCount,
	VkBuffer *outVertexBuffer, VkBuffer *outIndexBuffer )
{
	VkBufferCreateInfo desc;
	VkMemoryRequirements memReq;
	VkMemoryAllocateInfo allocInfo;
	VkDeviceMemory vertMem = VK_NULL_HANDLE, idxMem = VK_NULL_HANDLE;
	VkBuffer vertBuf = VK_NULL_HANDLE, idxBuf = VK_NULL_HANDLE;
	VkCommandBuffer cmd;
	VkBufferCopy copyRegion;
	VkDeviceSize idxSize;
	uint32_t memType;

	if ( !vboData || vboSize <= 0 || !idxData || idxCount <= 0 || !outVertexBuffer || !outIndexBuffer ) {
		return qfalse;
	}
	idxSize = (VkDeviceSize)idxCount * sizeof( uint32_t );

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	desc.size = (VkDeviceSize)vboSize;
	desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &desc, NULL, &vertBuf ) );

	desc.size = idxSize;
	desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &desc, NULL, &idxBuf ) );

	qvkGetBufferMemoryRequirements( vk.device, vertBuf, &memReq );
	memType = vk_find_memory_type( vk.physical_device, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	Com_Memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = memType;
	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vertMem ) );
	qvkBindBufferMemory( vk.device, vertBuf, vertMem, 0 );

	qvkGetBufferMemoryRequirements( vk.device, idxBuf, &memReq );
	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = vk_find_memory_type( vk.physical_device, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &idxMem ) );
	qvkBindBufferMemory( vk.device, idxBuf, idxMem, 0 );

	/* Upload via staging - need space for both vbo and index data */
	if ( !vk.device || !vk.staging_buffer.ptr ||
	     (VkDeviceSize)vboSize + idxSize > vk.staging_buffer.size ) {
		qvkDestroyBuffer( vk.device, vertBuf, NULL );
		qvkDestroyBuffer( vk.device, idxBuf, NULL );
		qvkFreeMemory( vk.device, vertMem, NULL );
		qvkFreeMemory( vk.device, idxMem, NULL );
		return qfalse;
	}
	Com_Memcpy( vk.staging_buffer.ptr, vboData, vboSize );
	Com_Memcpy( vk.staging_buffer.ptr + vboSize, idxData, (size_t)idxSize );

	cmd = vk_begin_command_buffer();
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = (VkDeviceSize)vboSize;
	qvkCmdCopyBuffer( cmd, vk.staging_buffer.handle, vertBuf, 1, &copyRegion );
	copyRegion.srcOffset = (VkDeviceSize)vboSize;
	copyRegion.dstOffset = 0;
	copyRegion.size = idxSize;
	qvkCmdCopyBuffer( cmd, vk.staging_buffer.handle, idxBuf, 1, &copyRegion );
	vk_end_command_buffer( cmd, __func__ );

	*outVertexBuffer = vertBuf;
	*outIndexBuffer = idxBuf;
	return qtrue;
}
