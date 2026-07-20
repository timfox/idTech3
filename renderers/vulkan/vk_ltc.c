/*
===========================================================================
Cinematic Engine Platform 1.0 — LTC LUT upload + descriptor bind.
===========================================================================
*/

#include "tr_local.h"
#include "vk_ltc.h"
#include "ltc_tables.h"
#include "vk_image_layout.h"
#include "vk_cmd.h"
#include "vk_util.h"

static vkLtcState_t s_ltc;
static qboolean s_cmds;

static qboolean vk_ltc_upload_one( const uint16_t *texels, VkImage *outImage, VkImageView *outView,
	VkDeviceMemory *outMemory, const char *debugName )
{
	VkImageCreateInfo image_info;
	VkMemoryRequirements mem_req;
	VkMemoryAllocateInfo alloc_info;
	VkImageViewCreateInfo view_info;
	VkBufferCreateInfo buffer_info;
	VkBuffer staging_buffer = VK_NULL_HANDLE;
	VkDeviceMemory staging_memory = VK_NULL_HANDLE;
	VkBufferImageCopy copy_region;
	VkCommandBuffer command_buffer;
	VkDeviceSize bytes;
	void *mapped = NULL;
	Vk_Sampler_Def sd;

	bytes = (VkDeviceSize)LTC_LUT_TEXELS * (VkDeviceSize)LTC_LUT_COMPONENTS * sizeof( uint16_t );

	Com_Memset( &image_info, 0, sizeof( image_info ) );
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	image_info.extent.width = LTC_LUT_SIZE;
	image_info.extent.height = LTC_LUT_SIZE;
	image_info.extent.depth = 1;
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if ( qvkCreateImage( vk.device, &image_info, NULL, outImage ) != VK_SUCCESS ) {
		return qfalse;
	}

	qvkGetImageMemoryRequirements( vk.device, *outImage, &mem_req );
	Com_Memset( &alloc_info, 0, sizeof( alloc_info ) );
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	if ( qvkAllocateMemory( vk.device, &alloc_info, NULL, outMemory ) != VK_SUCCESS ) {
		qvkDestroyImage( vk.device, *outImage, NULL );
		*outImage = VK_NULL_HANDLE;
		return qfalse;
	}
	VK_CHECK( qvkBindImageMemory( vk.device, *outImage, *outMemory, 0 ) );

	Com_Memset( &view_info, 0, sizeof( view_info ) );
	view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_info.image = *outImage;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_info.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_info.subresourceRange.levelCount = 1;
	view_info.subresourceRange.layerCount = 1;
	if ( qvkCreateImageView( vk.device, &view_info, NULL, outView ) != VK_SUCCESS ) {
		qvkFreeMemory( vk.device, *outMemory, NULL );
		qvkDestroyImage( vk.device, *outImage, NULL );
		*outMemory = VK_NULL_HANDLE;
		*outImage = VK_NULL_HANDLE;
		return qfalse;
	}

	Com_Memset( &buffer_info, 0, sizeof( buffer_info ) );
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.size = bytes;
	buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &buffer_info, NULL, &staging_buffer ) );
	qvkGetBufferMemoryRequirements( vk.device, staging_buffer, &mem_req );
	Com_Memset( &alloc_info, 0, sizeof( alloc_info ) );
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &staging_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, staging_buffer, staging_memory, 0 ) );
	VK_CHECK( qvkMapMemory( vk.device, staging_memory, 0, bytes, 0, &mapped ) );
	Com_Memcpy( mapped, texels, (size_t)bytes );
	qvkUnmapMemory( vk.device, staging_memory );

	Com_Memset( &copy_region, 0, sizeof( copy_region ) );
	copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy_region.imageSubresource.layerCount = 1;
	copy_region.imageExtent.width = LTC_LUT_SIZE;
	copy_region.imageExtent.height = LTC_LUT_SIZE;
	copy_region.imageExtent.depth = 1;

	command_buffer = vk_begin_command_buffer();
	record_image_layout_transition( command_buffer, *outImage, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );
	qvkCmdCopyBufferToImage( command_buffer, staging_buffer, *outImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region );
	record_image_layout_transition( command_buffer, *outImage, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	vk_end_command_buffer( command_buffer, __func__ );

	qvkDestroyBuffer( vk.device, staging_buffer, NULL );
	qvkFreeMemory( vk.device, staging_memory, NULL );

	SET_OBJECT_NAME( *outImage, debugName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( *outView, debugName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );

	(void)sd;
	return qtrue;
}

void vk_ltc_init( void )
{
	Vk_Sampler_Def sd;

	if ( s_ltc.uploaded ) {
		return;
	}
	if ( vk.device == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &s_ltc, 0, sizeof( s_ltc ) );

	if ( !vk_ltc_upload_one( s_ltcMatCanonical, &s_ltc.matImage, &s_ltc.matView, &s_ltc.matMemory, "ltc_mat" ) ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[VK][LTC] mat LUT upload failed\n" S_COLOR_WHITE );
		vk_ltc_shutdown();
		return;
	}
	if ( !vk_ltc_upload_one( s_ltcAmpCanonical, &s_ltc.ampImage, &s_ltc.ampView, &s_ltc.ampMemory, "ltc_amp" ) ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[VK][LTC] amp LUT upload failed\n" S_COLOR_WHITE );
		vk_ltc_shutdown();
		return;
	}

	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;
	s_ltc.sampler = vk_find_sampler( &sd );
	s_ltc.uploaded = qtrue;

	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "ltc_status", vk_ltc_status_f );
		s_cmds = qtrue;
	}

	ri.Printf( PRINT_ALL, "[VK][LTC] uploaded mat+amp LUTs (%dx%d R16G16B16A16) for area lights\n",
		LTC_LUT_SIZE, LTC_LUT_SIZE );
}

void vk_ltc_shutdown( void )
{
	if ( s_cmds ) {
		ri.Cmd_RemoveCommand( "ltc_status" );
		s_cmds = qfalse;
	}
	if ( s_ltc.matView ) {
		qvkDestroyImageView( vk.device, s_ltc.matView, NULL );
	}
	if ( s_ltc.ampView ) {
		qvkDestroyImageView( vk.device, s_ltc.ampView, NULL );
	}
	if ( s_ltc.matImage ) {
		qvkDestroyImage( vk.device, s_ltc.matImage, NULL );
	}
	if ( s_ltc.ampImage ) {
		qvkDestroyImage( vk.device, s_ltc.ampImage, NULL );
	}
	if ( s_ltc.matMemory ) {
		qvkFreeMemory( vk.device, s_ltc.matMemory, NULL );
	}
	if ( s_ltc.ampMemory ) {
		qvkFreeMemory( vk.device, s_ltc.ampMemory, NULL );
	}
	Com_Memset( &s_ltc, 0, sizeof( s_ltc ) );
}

qboolean vk_ltc_uploaded( void )
{
	return s_ltc.uploaded;
}

const vkLtcState_t *vk_ltc_state( void )
{
	return &s_ltc;
}

static void vk_ltc_write_pair( VkDescriptorSet set, uint32_t bindingMat, uint32_t bindingAmp )
{
	VkDescriptorImageInfo infos[2];
	VkWriteDescriptorSet writes[2];

	if ( set == VK_NULL_HANDLE || !s_ltc.uploaded || s_ltc.matView == VK_NULL_HANDLE ||
		s_ltc.ampView == VK_NULL_HANDLE || s_ltc.sampler == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( infos, 0, sizeof( infos ) );
	infos[0].sampler = s_ltc.sampler;
	infos[0].imageView = s_ltc.matView;
	infos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	infos[1].sampler = s_ltc.sampler;
	infos[1].imageView = s_ltc.ampView;
	infos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = set;
	writes[0].dstBinding = bindingMat;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &infos[0];
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = set;
	writes[1].dstBinding = bindingAmp;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].pImageInfo = &infos[1];
	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );
}

void vk_ltc_update_forward_plus_descriptors( VkDescriptorSet set )
{
	vk_ltc_write_pair( set, 6u, 7u );
}

void vk_ltc_update_deferred_lighting_descriptors( VkDescriptorSet set )
{
	vk_ltc_write_pair( set, 8u, 9u );
}

void vk_ltc_status_f( void )
{
	ri.Printf( PRINT_ALL, "=== LTC Area Lights (Cinematic Platform 1.0) ===\n" );
	ri.Printf( PRINT_ALL, "uploaded       : %s\n", s_ltc.uploaded ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "format         : R16G16B16A16_SFLOAT %dx%d mat+amp\n", LTC_LUT_SIZE, LTC_LUT_SIZE );
	ri.Printf( PRINT_ALL, "bindings       : Forward+ set18 binding 6/7; deferred lighting 8/9\n" );
	ri.Printf( PRINT_ALL, "pack type      : lc.w >= 1.5 = rect area (halfU/halfV in lpack/ltail)\n" );
}
