/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vulkan image memory allocation: chunk-based allocator for image memory.
Extracted from vk.c for incremental modularization.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_util.h"

/*
===============
vk_allocate_and_bind_image_memory
===============
Allocates device-local memory for an image using the chunk-based allocator.
Oversized images (e.g. merged lightmaps, large cubemaps) get dedicated chunks.
*/
void vk_allocate_and_bind_image_memory( VkImage image )
{
	VkMemoryRequirements memory_requirements;
	VkDeviceSize alignment;
	ImageChunk *chunk;
	int i;

	qvkGetImageMemoryRequirements( vk.device, image, &memory_requirements );

	if ( vk_world.num_image_chunks >= MAX_IMAGE_CHUNKS ) {
		ri.Error( ERR_FATAL, "Vulkan: image chunk limit has been reached" );
	}

	chunk = NULL;

	/* Oversized image: allocate a dedicated chunk instead of failing.
	 * Merged lightmaps and large cubemaps can exceed IMAGE_CHUNK_SIZE (32MB). */
	if ( memory_requirements.size > vk.image_chunk_size ) {
		VkMemoryAllocateInfo alloc_info;
		VkDeviceMemory memory;

		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.allocationSize = memory_requirements.size;
		alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

		VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &memory ) );

		chunk = &vk_world.image_chunks[vk_world.num_image_chunks];
		chunk->memory = memory;
		chunk->used = memory_requirements.size;

		ri.Printf( PRINT_DEVELOPER, "Vulkan: allocated dedicated %ikB for oversized image\n",
			(int)(memory_requirements.size / 1024) );

		SET_OBJECT_NAME( memory, va( "image memory chunk %i (oversized)", vk_world.num_image_chunks ), VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );

		vk_world.num_image_chunks++;

		VK_CHECK( qvkBindImageMemory( vk.device, image, chunk->memory, 0 ) );
		return;
	}

	/* Try to find an existing chunk of sufficient capacity. */
	alignment = memory_requirements.alignment;
	for ( i = 0; i < vk_world.num_image_chunks; i++ ) {
		/* Skip dedicated oversized chunks; they have no free space. */
		if ( vk_world.image_chunks[i].used > vk.image_chunk_size )
			continue;
		/* Ensure that memory region has proper alignment */
		VkDeviceSize offset = PAD( vk_world.image_chunks[i].used, alignment );

		if ( offset + memory_requirements.size <= vk.image_chunk_size ) {
			chunk = &vk_world.image_chunks[i];
			chunk->used = offset + memory_requirements.size;
			break;
		}
	}

	/* Allocate a new chunk in case we couldn't find suitable existing chunk. */
	if ( chunk == NULL ) {
		VkMemoryAllocateInfo alloc_info;
		VkDeviceMemory memory;

		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.allocationSize = vk.image_chunk_size;
		alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

		VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &memory ) );

		chunk = &vk_world.image_chunks[vk_world.num_image_chunks];
		chunk->memory = memory;
		chunk->used = memory_requirements.size;

		SET_OBJECT_NAME( memory, va( "image memory chunk %i", vk_world.num_image_chunks ), VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );

		vk_world.num_image_chunks++;
	}

	VK_CHECK( qvkBindImageMemory( vk.device, image, chunk->memory, chunk->used - memory_requirements.size ) );
}

/*
===============
vk_image_free_chunks
===============
Frees all image chunk memory and optionally adjusts chunk size for next map load.
Call before Com_Memset( &vk_world, 0, ... ).
*/
void vk_image_free_chunks( void )
{
	int i;

	for ( i = 0; i < vk_world.num_image_chunks; i++ )
		qvkFreeMemory( vk.device, vk_world.image_chunks[i].memory, NULL );

	if ( vk_world.num_image_chunks > 1 ) {
		/* If we allocated more than one chunk, use doubled default size next time */
		vk.image_chunk_size = (IMAGE_CHUNK_SIZE * 2);
	}
#if 0 /* do not reduce chunk size */
	else if ( vk_world.num_image_chunks == 1 ) {
		if ( vk_world.image_chunks[0].used < ( IMAGE_CHUNK_SIZE - (IMAGE_CHUNK_SIZE / 10) ) ) {
			vk.image_chunk_size = IMAGE_CHUNK_SIZE;
		}
	}
#endif
}
