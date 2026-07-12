/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Sparse VkImage residency: create sparse images, page memory pool, bind/unbind
via vkQueueBindSparse. See docs/VIRTUAL_TEXTURE.md.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_sparse.h"
#include "vk_util.h"
#include "vk_texture_image.h"

qboolean vk_sparse_available( void )
{
	return ( vk.device && vk.sparseBinding && vk.sparseResidencyImage2D
		&& qvkQueueBindSparse && qvkGetImageSparseMemoryRequirements ) ? qtrue : qfalse;
}

static qboolean vk_sparse_submit_binds( vkSparsePool_t *pool, VkSparseImageMemoryBind *binds, uint32_t bindCount )
{
	VkBindSparseInfo bindInfo;
	VkSparseImageMemoryBindInfo imageBind;
	VkFenceCreateInfo fenceInfo;
	VkResult res;

	if ( !pool || !binds || bindCount == 0 ) {
		return qtrue;
	}

	Com_Memset( &imageBind, 0, sizeof( imageBind ) );
	imageBind.image = pool->owner->image;
	imageBind.bindCount = bindCount;
	imageBind.pBinds = binds;

	Com_Memset( &bindInfo, 0, sizeof( bindInfo ) );
	bindInfo.sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
	bindInfo.imageBindCount = 1;
	bindInfo.pImageBinds = &imageBind;

	if ( pool->fence == VK_NULL_HANDLE ) {
		Com_Memset( &fenceInfo, 0, sizeof( fenceInfo ) );
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		VK_CHECK( qvkCreateFence( vk.device, &fenceInfo, NULL, &pool->fence ) );
	} else {
		VK_CHECK( qvkResetFences( vk.device, 1, &pool->fence ) );
	}

	res = qvkQueueBindSparse( vk.queue, 1, &bindInfo, pool->fence );
	if ( res != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, "[VK][sparse] vkQueueBindSparse failed (%d)\n", (int)res );
		return qfalse;
	}
	VK_CHECK( qvkWaitForFences( vk.device, 1, &pool->fence, VK_TRUE, UINT64_MAX ) );
	return qtrue;
}

qboolean vk_sparse_create_image2d( vkSparseImage_t *out, int width, int height, VkFormat format,
	VkSamplerAddressMode wrapClampMode, const char *debugName )
{
	VkImageCreateInfo desc;
	VkMemoryRequirements memReqs;
	uint32_t reqCount = 0;
	VkSparseImageMemoryRequirements *reqs = NULL;
	VkImageViewCreateInfo viewDesc;
	VkDescriptorSetAllocateInfo setAlloc;
	uint32_t i;
	qboolean foundColor = qfalse;

	if ( !out || width < 1 || height < 1 || !vk_sparse_available() ) {
		return qfalse;
	}

	Com_Memset( out, 0, sizeof( *out ) );
	out->format = format;
	out->width = width;
	out->height = height;
	out->wrapClampMode = wrapClampMode;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	desc.flags = VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;
	desc.imageType = VK_IMAGE_TYPE_2D;
	desc.format = format;
	desc.extent.width = (uint32_t)width;
	desc.extent.height = (uint32_t)height;
	desc.extent.depth = 1;
	desc.mipLevels = 1;
	desc.arrayLayers = 1;
	desc.samples = VK_SAMPLE_COUNT_1_BIT;
	desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	if ( qvkCreateImage( vk.device, &desc, NULL, &out->image ) != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, "[VK][sparse] vkCreateImage failed\n" );
		return qfalse;
	}

	qvkGetImageMemoryRequirements( vk.device, out->image, &memReqs );
	out->memoryTypeBits = memReqs.memoryTypeBits;
	out->alignment = memReqs.alignment ? memReqs.alignment : 256;

	qvkGetImageSparseMemoryRequirements( vk.device, out->image, &reqCount, NULL );
	if ( reqCount == 0 ) {
		ri.Printf( PRINT_WARNING, "[VK][sparse] no sparse memory requirements\n" );
		qvkDestroyImage( vk.device, out->image, NULL );
		out->image = VK_NULL_HANDLE;
		return qfalse;
	}
	reqs = (VkSparseImageMemoryRequirements *)ri.Hunk_AllocateTempMemory( reqCount * sizeof( *reqs ) );
	qvkGetImageSparseMemoryRequirements( vk.device, out->image, &reqCount, reqs );

	for ( i = 0; i < reqCount; i++ ) {
		if ( reqs[i].formatProperties.aspectMask & VK_IMAGE_ASPECT_COLOR_BIT ) {
			out->granW = reqs[i].formatProperties.imageGranularity.width;
			out->granH = reqs[i].formatProperties.imageGranularity.height;
			foundColor = qtrue;
			break;
		}
	}
	ri.Hunk_FreeTempMemory( reqs );

	if ( !foundColor || out->granW < 1 || out->granH < 1 ) {
		ri.Printf( PRINT_WARNING, "[VK][sparse] missing color granularity\n" );
		qvkDestroyImage( vk.device, out->image, NULL );
		out->image = VK_NULL_HANDLE;
		return qfalse;
	}

	out->pageBytes = (VkDeviceSize)out->granW * (VkDeviceSize)out->granH * 4;
	if ( out->pageBytes < out->alignment ) {
		out->pageBytes = out->alignment;
	}
	out->pageBytes = ( out->pageBytes + out->alignment - 1 ) / out->alignment * out->alignment;

	Com_Memset( &viewDesc, 0, sizeof( viewDesc ) );
	viewDesc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewDesc.image = out->image;
	viewDesc.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewDesc.format = format;
	viewDesc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewDesc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewDesc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewDesc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewDesc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewDesc.subresourceRange.baseMipLevel = 0;
	viewDesc.subresourceRange.levelCount = 1;
	viewDesc.subresourceRange.baseArrayLayer = 0;
	viewDesc.subresourceRange.layerCount = 1;

	if ( qvkCreateImageView( vk.device, &viewDesc, NULL, &out->view ) != VK_SUCCESS ) {
		qvkDestroyImage( vk.device, out->image, NULL );
		Com_Memset( out, 0, sizeof( *out ) );
		return qfalse;
	}

	Com_Memset( &setAlloc, 0, sizeof( setAlloc ) );
	setAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAlloc.descriptorPool = vk.descriptor_pool;
	setAlloc.descriptorSetCount = 1;
	setAlloc.pSetLayouts = &vk.set_layout_sampler;
	if ( qvkAllocateDescriptorSets( vk.device, &setAlloc, &out->descriptor ) != VK_SUCCESS ) {
		qvkDestroyImageView( vk.device, out->view, NULL );
		qvkDestroyImage( vk.device, out->image, NULL );
		Com_Memset( out, 0, sizeof( *out ) );
		return qfalse;
	}

	{
		image_t tmp;
		char nameBuf[MAX_QPATH];
		Com_Memset( &tmp, 0, sizeof( tmp ) );
		tmp.handle = out->image;
		tmp.view = out->view;
		tmp.descriptor = out->descriptor;
		tmp.wrapClampMode = wrapClampMode;
		Q_strncpyz( nameBuf, debugName ? debugName : "*sparse", sizeof( nameBuf ) );
		tmp.imgName = nameBuf;
		vk_update_descriptor_set( &tmp, qfalse );
	}

	if ( debugName ) {
		SET_OBJECT_NAME( out->image, debugName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		SET_OBJECT_NAME( out->view, debugName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}

	ri.Printf( PRINT_ALL, "[VK][sparse] image %dx%d gran=%ux%u pageBytes=%u\n",
		width, height, out->granW, out->granH, (unsigned)out->pageBytes );
	return qtrue;
}

void vk_sparse_destroy_image( vkSparseImage_t *img, vkSparsePool_t *pool )
{
	if ( pool ) {
		vk_sparse_pool_shutdown( pool );
	}
	if ( !img ) {
		return;
	}
	if ( img->view ) {
		qvkDestroyImageView( vk.device, img->view, NULL );
		img->view = VK_NULL_HANDLE;
	}
	if ( img->image ) {
		qvkDestroyImage( vk.device, img->image, NULL );
		img->image = VK_NULL_HANDLE;
	}
	/* descriptor sets are pool-freed with the descriptor pool */
	img->descriptor = VK_NULL_HANDLE;
	Com_Memset( img, 0, sizeof( *img ) );
}

qboolean vk_sparse_pool_init( vkSparsePool_t *pool, vkSparseImage_t *owner, int capacity )
{
	if ( !pool || !owner || !owner->image ) {
		return qfalse;
	}
	if ( capacity < 1 ) {
		capacity = 1;
	}
	if ( capacity > VK_SPARSE_MAX_PAGES ) {
		capacity = VK_SPARSE_MAX_PAGES;
	}
	Com_Memset( pool, 0, sizeof( *pool ) );
	pool->owner = owner;
	pool->capacity = capacity;
	return qtrue;
}

void vk_sparse_pool_shutdown( vkSparsePool_t *pool )
{
	int i;

	if ( !pool ) {
		return;
	}
	if ( pool->owner && pool->owner->image ) {
		for ( i = 0; i < pool->capacity; i++ ) {
			if ( pool->pages[i].bound ) {
				vk_sparse_unbind_page( pool, i );
			}
		}
	}
	for ( i = 0; i < pool->capacity; i++ ) {
		if ( pool->pages[i].memory ) {
			qvkFreeMemory( vk.device, pool->pages[i].memory, NULL );
			pool->pages[i].memory = VK_NULL_HANDLE;
		}
		pool->pages[i].allocated = qfalse;
		pool->pages[i].bound = qfalse;
	}
	if ( pool->fence ) {
		qvkDestroyFence( vk.device, pool->fence, NULL );
		pool->fence = VK_NULL_HANDLE;
	}
	pool->allocated = 0;
	pool->owner = NULL;
}

void vk_sparse_page_pixel_offset( const vkSparseImage_t *img, int pageX, int pageY, int *outX, int *outY )
{
	if ( outX ) {
		*outX = pageX * (int)( img ? img->granW : 0 );
	}
	if ( outY ) {
		*outY = pageY * (int)( img ? img->granH : 0 );
	}
}

static int vk_sparse_alloc_page_memory( vkSparsePool_t *pool, int slot )
{
	VkMemoryAllocateInfo alloc;
	vkSparsePage_t *page;

	if ( !pool || slot < 0 || slot >= pool->capacity || !pool->owner ) {
		return -1;
	}
	page = &pool->pages[slot];
	if ( page->allocated && page->memory ) {
		return slot;
	}

	Com_Memset( &alloc, 0, sizeof( alloc ) );
	alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc.allocationSize = pool->owner->pageBytes;
	alloc.memoryTypeIndex = vk_find_memory_type( vk.physical_device, pool->owner->memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

	if ( qvkAllocateMemory( vk.device, &alloc, NULL, &page->memory ) != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, "[VK][sparse] page memory alloc failed\n" );
		return -1;
	}
	page->allocated = qtrue;
	pool->allocated++;
	return slot;
}

int vk_sparse_bind_page( vkSparsePool_t *pool, int pageX, int pageY, int preferSlot )
{
	VkSparseImageMemoryBind bind;
	int slot;
	int i;

	if ( !pool || !pool->owner || !pool->owner->image ) {
		return -1;
	}
	if ( pageX < 0 || pageY < 0 ) {
		return -1;
	}
	if ( ( pageX + 1 ) * (int)pool->owner->granW > pool->owner->width ||
		( pageY + 1 ) * (int)pool->owner->granH > pool->owner->height ) {
		return -1;
	}

	/* Already bound at this virtual page? */
	for ( i = 0; i < pool->capacity; i++ ) {
		if ( pool->pages[i].bound && pool->pages[i].pageX == pageX && pool->pages[i].pageY == pageY ) {
			return i;
		}
	}

	slot = preferSlot;
	if ( slot < 0 || slot >= pool->capacity || pool->pages[slot].bound ) {
		slot = -1;
		for ( i = 0; i < pool->capacity; i++ ) {
			if ( !pool->pages[i].bound ) {
				slot = i;
				break;
			}
		}
	}
	if ( slot < 0 ) {
		return -1;
	}

	if ( pool->pages[slot].bound ) {
		vk_sparse_unbind_page( pool, slot );
	}
	if ( vk_sparse_alloc_page_memory( pool, slot ) < 0 ) {
		return -1;
	}

	Com_Memset( &bind, 0, sizeof( bind ) );
	bind.subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	bind.subresource.mipLevel = 0;
	bind.subresource.arrayLayer = 0;
	bind.offset.x = pageX * (int32_t)pool->owner->granW;
	bind.offset.y = pageY * (int32_t)pool->owner->granH;
	bind.offset.z = 0;
	bind.extent.width = pool->owner->granW;
	bind.extent.height = pool->owner->granH;
	bind.extent.depth = 1;
	bind.memory = pool->pages[slot].memory;
	bind.memoryOffset = 0;

	if ( !vk_sparse_submit_binds( pool, &bind, 1 ) ) {
		return -1;
	}

	pool->pages[slot].bound = qtrue;
	pool->pages[slot].pageX = pageX;
	pool->pages[slot].pageY = pageY;
	return slot;
}

qboolean vk_sparse_unbind_page( vkSparsePool_t *pool, int slot )
{
	VkSparseImageMemoryBind bind;

	if ( !pool || slot < 0 || slot >= pool->capacity || !pool->pages[slot].bound || !pool->owner ) {
		return qfalse;
	}

	Com_Memset( &bind, 0, sizeof( bind ) );
	bind.subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	bind.offset.x = pool->pages[slot].pageX * (int32_t)pool->owner->granW;
	bind.offset.y = pool->pages[slot].pageY * (int32_t)pool->owner->granH;
	bind.extent.width = pool->owner->granW;
	bind.extent.height = pool->owner->granH;
	bind.extent.depth = 1;
	bind.memory = VK_NULL_HANDLE;
	bind.memoryOffset = 0;

	if ( !vk_sparse_submit_binds( pool, &bind, 1 ) ) {
		return qfalse;
	}
	pool->pages[slot].bound = qfalse;
	pool->pages[slot].pageX = -1;
	pool->pages[slot].pageY = -1;
	return qtrue;
}
