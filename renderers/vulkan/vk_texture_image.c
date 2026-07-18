/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Texture VkImage / VkImageView creation, CPU→GPU uploads via staging, and
per-texture combined image+sampler descriptor updates (split from vk.c).
===========================================================================
*/

#include "tr_local.h"
#include "vk_texture_image.h"
#include "vk_image_layout.h"
#include "vk_cmd.h"
#include "vk_staging.h"
#include "vk_util.h"

void vk_create_image( image_t *image, int width, int height, int mip_levels ) {

	VkFormat			format = (VkFormat)image->internalFormat;
	VkImageCreateFlags	image_flags = 0;
	VkImageViewType		view_type = (VkImageViewType)VK_IMAGE_VIEW_TYPE_2D;

	if ( image->handle ) {
		qvkDestroyImage( vk.device, image->handle, NULL );
		image->handle = VK_NULL_HANDLE;
	}

	if ( image->view ) {
		qvkDestroyImageView( vk.device, image->view, NULL );
		image->view = VK_NULL_HANDLE;
	}

	if ( image->flags & IMGFLAG_CUBEMAP ) {
		image_flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		view_type = VK_IMAGE_VIEW_TYPE_CUBE;
	}

	// create image
	{
		VkImageCreateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = image_flags;
		desc.imageType = VK_IMAGE_TYPE_2D;
		desc.format = format;
		desc.extent.width = width;
		desc.extent.height = height;
		desc.extent.depth = 1;
		desc.mipLevels = mip_levels;
		desc.arrayLayers = image->layers;
		desc.samples = VK_SAMPLE_COUNT_1_BIT;
		desc.tiling = VK_IMAGE_TILING_OPTIMAL;
		desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		desc.queueFamilyIndexCount = 0;
		desc.pQueueFamilyIndices = NULL;
		desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK( qvkCreateImage( vk.device, &desc, NULL, &image->handle ) );

		vk_allocate_and_bind_image_memory( image->handle );
	}

	// create image view
	{
		VkImageViewCreateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.image = image->handle;
		desc.viewType = (VkImageViewType)view_type;
		desc.format = format;
		desc.components = textureMapTypes[image->type].swizzle;
		desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		desc.subresourceRange.baseMipLevel = 0;
		desc.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		desc.subresourceRange.baseArrayLayer = 0;
		desc.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

		VK_CHECK( qvkCreateImageView( vk.device, &desc, NULL, &image->view ) );
	}

	// create associated descriptor set
	if ( image->descriptor == VK_NULL_HANDLE ) {
		VkDescriptorSetAllocateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		desc.pNext = NULL;
		desc.descriptorPool = vk.descriptor_pool;
		desc.descriptorSetCount = 1;
		desc.pSetLayouts = &vk.set_layout_sampler;

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &desc, &image->descriptor ) );
	}

	vk_update_descriptor_set( image, mip_levels > 1 ? qtrue : qfalse );

	SET_OBJECT_NAME( image->handle, image->imgName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( image->view, image->imgName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	SET_OBJECT_NAME( image->descriptor, image->imgName, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
}


static byte *resample_image_data( const int target_format, byte *data, const int data_size, int *bytes_per_pixel )
{
	byte* buffer;
	uint16_t* p;
	int i, n;

	switch ( target_format ) {
	case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
		buffer = (byte*)ri.Hunk_AllocateTempMemory( data_size / 2 );
		p = (uint16_t*)buffer;
		for ( i = 0; i < data_size; i += 4, p++ ) {
			byte r = data[i + 0];
			byte g = data[i + 1];
			byte b = data[i + 2];
			byte a = data[i + 3];
			*p = (uint32_t)((a / 255.0) * 15.0 + 0.5) |
				((uint32_t)((r / 255.0) * 15.0 + 0.5) << 4) |
				((uint32_t)((g / 255.0) * 15.0 + 0.5) << 8) |
				((uint32_t)((b / 255.0) * 15.0 + 0.5) << 12);
		}
		*bytes_per_pixel = 2;
		return buffer; // must be freed after upload!

	case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
		buffer = (byte*)ri.Hunk_AllocateTempMemory( data_size / 2 );
		p = (uint16_t*)buffer;
		for ( i = 0; i < data_size; i += 4, p++ ) {
			byte r = data[i + 0];
			byte g = data[i + 1];
			byte b = data[i + 2];
			*p = (uint32_t)((b / 255.0) * 31.0 + 0.5) |
				((uint32_t)((g / 255.0) * 31.0 + 0.5) << 5) |
				((uint32_t)((r / 255.0) * 31.0 + 0.5) << 10) |
				(1 << 15);
		}
		*bytes_per_pixel = 2;
		return buffer; // must be freed after upload!

	case VK_FORMAT_B8G8R8A8_UNORM:
		buffer = (byte*)ri.Hunk_AllocateTempMemory( data_size );
		for ( i = 0; i < data_size; i += 4 ) {
			buffer[i + 0] = data[i + 2];
			buffer[i + 1] = data[i + 1];
			buffer[i + 2] = data[i + 0];
			buffer[i + 3] = data[i + 3];
		}
		*bytes_per_pixel = 4;
		return buffer;

	case VK_FORMAT_R8G8B8_UNORM: {
		buffer = (byte*)ri.Hunk_AllocateTempMemory( (data_size * 3) / 4 );
		for ( i = 0, n = 0; i < data_size; i += 4, n += 3 ) {
			buffer[n + 0] = data[i + 0];
			buffer[n + 1] = data[i + 1];
			buffer[n + 2] = data[i + 2];
		}
		*bytes_per_pixel = 3;
		return buffer;
	}

	default:
		*bytes_per_pixel = 4;
		return data;
	}
}


void vk_upload_image_data( image_t *image, int x, int y, int width, int height, int mipmaps, byte *pixels, int size, qboolean update ) {

	VkCommandBuffer   command_buffer;
	VkBufferImageCopy regions[16];
	VkBufferImageCopy region;
	byte *buf;
	int n;

	int num_regions = 0;
	int buffer_size = 0;

	buf = resample_image_data( image->internalFormat, pixels, size, &n /*bpp*/ );

	while (qtrue) {
		Com_Memset(&region, 0, sizeof(region));
		region.bufferOffset = buffer_size;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = num_regions;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset.x = x;
		region.imageOffset.y = y;
		region.imageOffset.z = 0;
		region.imageExtent.width = width;
		region.imageExtent.height = height;
		region.imageExtent.depth = 1;

		regions[num_regions] = region;
		num_regions++;

		buffer_size += width * height * n;

		if ( num_regions >= mipmaps || (width == 1 && height == 1) || (size_t) num_regions >= ARRAY_LEN( regions ) )
			break;

		x >>= 1;
		y >>= 1;

		width >>= 1;
		if (width < 1) width = 1;

		height >>= 1;
		if (height < 1) height = 1;
	}

	if ( vk.staging_buffer.size < (VkDeviceSize) buffer_size ) {
		vk_alloc_staging_buffer( buffer_size );
	}

	Com_Memcpy( vk.staging_buffer.ptr, buf, buffer_size );

	command_buffer = vk_begin_command_buffer();
	// record_buffer_memory_barrier( command_buffer, vk_world.staging_buffer, VK_WHOLE_SIZE, 0, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT );
	if ( update ) {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );
	} else {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, 0 );
	}
	qvkCmdCopyBufferToImage( command_buffer, vk.staging_buffer.handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions );
	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	vk_end_command_buffer( command_buffer, __func__ );

	if ( buf != pixels ) {
		ri.Hunk_FreeTempMemory( buf );
	}
}

void vk_upload_image_rgba32f( image_t *image, int width, int height, const float *rgba, int floatCount ) {
	VkCommandBuffer command_buffer;
	VkBufferImageCopy region;
	int byteSize;

	if ( !image || !rgba || width <= 0 || height <= 0 || floatCount < width * height * 4 ) {
		return;
	}

	byteSize = width * height * 4 * (int)sizeof( float );

	if ( vk.staging_buffer.size < (VkDeviceSize)byteSize ) {
		vk_alloc_staging_buffer( byteSize );
	}

	Com_Memcpy( vk.staging_buffer.ptr, rgba, byteSize );

	Com_Memset( &region, 0, sizeof( region ) );
	region.bufferOffset = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageExtent.width = (uint32_t)width;
	region.imageExtent.height = (uint32_t)height;
	region.imageExtent.depth = 1;

	command_buffer = vk_begin_command_buffer();
	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, 0 );
	qvkCmdCopyBufferToImage( command_buffer, vk.staging_buffer.handle, image->handle,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );
	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	vk_end_command_buffer( command_buffer, __func__ );
}

void vk_upload_cubemap_mip_data( image_t *image, int face_size, int miplevels, const byte *pixels, int size, int bytes_per_pixel, qboolean update ) {
	VkCommandBuffer command_buffer;
	VkBufferImageCopy regions[64];
	int num_regions = 0;
	int buffer_size = 0;
	int mip;
	int face;
	int width = face_size;
	int height = face_size;

	if ( !image || !pixels || !( image->flags & IMGFLAG_CUBEMAP ) || face_size <= 0 || miplevels <= 0 || bytes_per_pixel <= 0 ) {
		return;
	}

	for ( mip = 0; mip < miplevels && num_regions < (int)ARRAY_LEN( regions ); mip++ ) {
		const int face_bytes = width * height * bytes_per_pixel;

		for ( face = 0; face < 6 && num_regions < (int)ARRAY_LEN( regions ); face++ ) {
			VkBufferImageCopy region;

			Com_Memset( &region, 0, sizeof( region ) );
			region.bufferOffset = buffer_size;
			region.bufferRowLength = 0;
			region.bufferImageHeight = 0;
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.mipLevel = mip;
			region.imageSubresource.baseArrayLayer = face;
			region.imageSubresource.layerCount = 1;
			region.imageOffset.x = 0;
			region.imageOffset.y = 0;
			region.imageOffset.z = 0;
			region.imageExtent.width = width;
			region.imageExtent.height = height;
			region.imageExtent.depth = 1;
			regions[num_regions++] = region;

			buffer_size += face_bytes;
		}

		width >>= 1;
		height >>= 1;
		if ( width < 1 ) width = 1;
		if ( height < 1 ) height = 1;
	}

	if ( buffer_size > size ) {
		ri.Printf( PRINT_WARNING, "vk_upload_cubemap_mip_data: buffer underrun for %s (%d > %d)\n",
			image->imgName ? image->imgName : "<unnamed cubemap>", buffer_size, size );
		return;
	}

	if ( vk.staging_buffer.size < (VkDeviceSize)buffer_size ) {
		vk_alloc_staging_buffer( buffer_size );
	}

	Com_Memcpy( vk.staging_buffer.ptr, pixels, buffer_size );
	command_buffer = vk_begin_command_buffer();

	if ( update ) {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );
	} else {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, 0 );
	}

	qvkCmdCopyBufferToImage( command_buffer, vk.staging_buffer.handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions );

	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );

	vk_end_command_buffer( command_buffer, __func__ );
}

/*
 * Upload pre-compressed BC7 block data. pixels layout: mip0, mip1, ... (no resampling).
 */
void vk_upload_compressed_image_data( image_t *image, int width, int height, int miplevels, byte *pixels, int size, qboolean update ) {
	VkCommandBuffer   command_buffer;
	VkBufferImageCopy regions[16];
	VkBufferImageCopy region;
	int num_regions = 0;
	int buffer_offset = 0;
	int w, h;
	int n;

	for ( n = 0; n < miplevels && (size_t)n < ARRAY_LEN( regions ); n++ ) {
		w = width >> n;
		h = height >> n;
		if ( w < 1 ) w = 1;
		if ( h < 1 ) h = 1;

		Com_Memset( &region, 0, sizeof( region ) );
		region.bufferOffset = buffer_offset;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = n;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset.x = 0;
		region.imageOffset.y = 0;
		region.imageOffset.z = 0;
		region.imageExtent.width = w;
		region.imageExtent.height = h;
		region.imageExtent.depth = 1;

		regions[num_regions] = region;
		num_regions++;

		/* BC7: 4x4 blocks, 16 bytes per block */
		buffer_offset += ( ( w + 3 ) / 4 ) * ( ( h + 3 ) / 4 ) * 16;

		if ( w == 1 && h == 1 )
			break;
	}

	if ( vk.staging_buffer.size < (VkDeviceSize)size ) {
		vk_alloc_staging_buffer( size );
	}
	Com_Memcpy( vk.staging_buffer.ptr, pixels, size );
	command_buffer = vk_begin_command_buffer();

	if ( update ) {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );
	} else {
		record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, 0 );
	}
	qvkCmdCopyBufferToImage( command_buffer, vk.staging_buffer.handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions );
	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );

	vk_end_command_buffer( command_buffer, __func__ );
}

void vk_update_descriptor_set( image_t *image, qboolean mipmap ) {
	Vk_Sampler_Def sampler_def;
	VkDescriptorImageInfo image_info;
	VkWriteDescriptorSet descriptor_write;

	Com_Memset( &sampler_def, 0, sizeof( sampler_def ) );

	sampler_def.address_mode = image->wrapClampMode;

	if ( mipmap ) {
		sampler_def.gl_mag_filter = gl_filter_max;
		sampler_def.gl_min_filter = gl_filter_min;
	} else {
		sampler_def.gl_mag_filter = GL_LINEAR;
		sampler_def.gl_min_filter = GL_LINEAR;
		// no anisotropy without mipmaps
		sampler_def.noAnisotropy = qtrue;
	}

	image_info.sampler = vk_find_sampler( &sampler_def );
	image_info.imageView = image->view;
	image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	image->vk_sampler = image_info.sampler;

	descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptor_write.dstSet = image->descriptor;
	descriptor_write.dstBinding = 0;
	descriptor_write.dstArrayElement = 0;
	descriptor_write.descriptorCount = 1;
	descriptor_write.pNext = NULL;
	descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptor_write.pImageInfo = &image_info;
	descriptor_write.pBufferInfo = NULL;
	descriptor_write.pTexelBufferView = NULL;

	qvkUpdateDescriptorSets( vk.device, 1, &descriptor_write, 0, NULL );
}


void vk_destroy_image_resources( VkImage *image, VkImageView *imageView )
{
	if ( vk.device == VK_NULL_HANDLE || qvkDestroyImage == NULL || qvkDestroyImageView == NULL ) {
		return;
	}
	if ( vk.device_lost ) {
		if ( image != NULL ) {
			*image = VK_NULL_HANDLE;
		}
		if ( imageView != NULL ) {
			*imageView = VK_NULL_HANDLE;
		}
		return;
	}
	if ( image != NULL ) {
		if ( *image != VK_NULL_HANDLE ) {
			qvkDestroyImage( vk.device, *image, NULL );
			*image = VK_NULL_HANDLE;
		}
	}
	if ( imageView != NULL ) {
		if ( *imageView != VK_NULL_HANDLE ) {
			qvkDestroyImageView( vk.device, *imageView, NULL );
			*imageView = VK_NULL_HANDLE;
		}
	}
}

static void thumb_create_readback_buffer( VkDeviceSize size, VkBuffer *buffer, VkDeviceMemory *memory, void **data )
{
	VkBufferCreateInfo buffer_desc;
	VkMemoryRequirements mem_reqs;
	VkMemoryAllocateInfo alloc_info;

	buffer_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_desc.pNext = NULL;
	buffer_desc.flags = 0;
	buffer_desc.size = size;
	buffer_desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	buffer_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buffer_desc.queueFamilyIndexCount = 0;
	buffer_desc.pQueueFamilyIndices = NULL;
	VK_CHECK( qvkCreateBuffer( vk.device, &buffer_desc, NULL, buffer ) );

	qvkGetBufferMemoryRequirements( vk.device, *buffer, &mem_reqs );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = mem_reqs.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_reqs.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, *buffer, *memory, 0 ) );
	VK_CHECK( qvkMapMemory( vk.device, *memory, 0, size, 0, data ) );
}

static void thumb_destroy_readback_buffer( VkBuffer buffer, VkDeviceMemory memory )
{
	qvkUnmapMemory( vk.device, memory );
	qvkDestroyBuffer( vk.device, buffer, NULL );
	qvkFreeMemory( vk.device, memory, NULL );
}

static void thumb_bind_temp_image_memory( VkImage image, VkDeviceMemory *memory )
{
	VkMemoryRequirements mem_reqs;
	VkMemoryAllocateInfo alloc_info;

	qvkGetImageMemoryRequirements( vk.device, image, &mem_reqs );
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = mem_reqs.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_reqs.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, image, *memory, 0 ) );
}

qboolean vk_build_image_thumb_from_gpu( image_t *image )
{
	VkCommandBuffer command_buffer;
	VkImage thumb_image;
	VkDeviceMemory thumb_memory;
	VkBuffer staging_buffer;
	VkDeviceMemory staging_memory;
	void *mapped;
	VkImageBlit blit;
	VkBufferImageCopy copy_region;
	VkImageCreateInfo image_desc;
	int w, h;
	const uint32_t thumbSize = TR_IMAGE_THUMB_SIZE;

	if ( !image || image->hasThumb || vk.device_lost ) {
		return qfalse;
	}
	if ( image->handle == VK_NULL_HANDLE || ( image->flags & IMGFLAG_CUBEMAP ) ) {
		return qfalse;
	}

	w = image->uploadWidth > 0 ? image->uploadWidth : image->width;
	h = image->uploadHeight > 0 ? image->uploadHeight : image->height;
	if ( w < 1 || h < 1 ) {
		return qfalse;
	}

	Com_Memset( &image_desc, 0, sizeof( image_desc ) );
	image_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_desc.imageType = VK_IMAGE_TYPE_2D;
	image_desc.format = VK_FORMAT_R8G8B8A8_UNORM;
	image_desc.extent.width = thumbSize;
	image_desc.extent.height = thumbSize;
	image_desc.extent.depth = 1;
	image_desc.mipLevels = 1;
	image_desc.arrayLayers = 1;
	image_desc.samples = VK_SAMPLE_COUNT_1_BIT;
	image_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	image_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK( qvkCreateImage( vk.device, &image_desc, NULL, &thumb_image ) );
	thumb_bind_temp_image_memory( thumb_image, &thumb_memory );

	thumb_create_readback_buffer( (VkDeviceSize)thumbSize * thumbSize * 4,
		&staging_buffer, &staging_memory, &mapped );

	command_buffer = vk_begin_command_buffer();

	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0 );
	record_image_layout_transition( command_buffer, thumb_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

	Com_Memset( &blit, 0, sizeof( blit ) );
	blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit.srcSubresource.mipLevel = 0;
	blit.srcSubresource.baseArrayLayer = 0;
	blit.srcSubresource.layerCount = 1;
	blit.srcOffsets[0].x = 0;
	blit.srcOffsets[0].y = 0;
	blit.srcOffsets[0].z = 0;
	blit.srcOffsets[1].x = w;
	blit.srcOffsets[1].y = h;
	blit.srcOffsets[1].z = 1;
	blit.dstSubresource = blit.srcSubresource;
	blit.dstOffsets[0].x = 0;
	blit.dstOffsets[0].y = 0;
	blit.dstOffsets[0].z = 0;
	blit.dstOffsets[1].x = (int32_t)thumbSize;
	blit.dstOffsets[1].y = (int32_t)thumbSize;
	blit.dstOffsets[1].z = 1;

	qvkCmdBlitImage( command_buffer,
		image->handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		thumb_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &blit, VK_FILTER_LINEAR );

	record_image_layout_transition( command_buffer, thumb_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0 );

	Com_Memset( &copy_region, 0, sizeof( copy_region ) );
	copy_region.bufferOffset = 0;
	copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy_region.imageSubresource.mipLevel = 0;
	copy_region.imageSubresource.baseArrayLayer = 0;
	copy_region.imageSubresource.layerCount = 1;
	copy_region.imageExtent.width = thumbSize;
	copy_region.imageExtent.height = thumbSize;
	copy_region.imageExtent.depth = 1;

	qvkCmdCopyImageToBuffer( command_buffer, thumb_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		staging_buffer, 1, &copy_region );

	record_image_layout_transition( command_buffer, image->handle, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );

	vk_end_command_buffer( command_buffer, "image thumb" );

	Com_Memcpy( image->thumbRGBA, mapped, thumbSize * thumbSize * 4 );
	image->hasThumb = qtrue;

	qvkDestroyImage( vk.device, thumb_image, NULL );
	qvkFreeMemory( vk.device, thumb_memory, NULL );
	thumb_destroy_readback_buffer( staging_buffer, staging_memory );

	return qtrue;
}
