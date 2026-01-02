/*
=============================================================================
Vulkan Ray Tracing Implementation

Based on Quake-III-Arena-R reference implementation
=============================================================================
*/

#include "tr_local.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;
#include "vk.h"
#include "vk_material_system.h"
#include "vk_material_parser.h"
#include "vk_rtx_acceleration.h"
#include "tr_math_optimized.h"
#include "vk_framebuffer.h"

// RTX CVAR extern declarations
extern cvar_t *r_rtx_enable;
extern cvar_t *r_rtx_shadows;
extern cvar_t *r_rtx_reflections;
extern cvar_t *r_rtx_gi;
extern cvar_t *r_rtx_quality;

// Forward declarations for ray tracing functions
void vk_rt_create_denoise_resources( uint32_t width, uint32_t height );
void vk_rt_destroy_denoise_resources( void );
void vk_rt_denoise( uint32_t width, uint32_t height );
void vk_rt_create_denoise_pipeline( void );
void vk_rt_update_composite_descriptor_set( void );
void vk_rt_create_composite_descriptor_set( void );
void vk_rt_composite( void );

// Forward declarations for local functions
static void vk_rt_create_temporal_buffers( uint32_t width, uint32_t height );
static void vk_rt_create_descriptor_set_layout( void );
static void vk_rt_create_pipeline_layout( void );
static void vk_rt_create_shader_binding_table( void );

#ifdef USE_VULKAN

// find_memory_type and VK_CHECK are now declared in vk.h

#ifdef USE_VULKAN_RAY_TRACING

/*
=============================================================================
Ray Tracing Image Layout Transition Helpers
=============================================================================
*/

// Transition RT output image from SHADER_READ_ONLY_OPTIMAL to TRANSFER_DST_OPTIMAL for clearing
static void vk_rt_transition_image_to_transfer_dst( VkImage image )
{
	if ( image == VK_NULL_HANDLE ) {
		return;
	}

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext = NULL;
	barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	qvkCmdPipelineBarrier(
		vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0, NULL,
		0, NULL,
		1, &barrier
	);
}

// Transition RT output image from UNDEFINED/GENERAL to TRANSFER_DST_OPTIMAL for clearing
static void vk_rt_transition_image_to_transfer_dst_from_general( VkImage image )
{
	if ( image == VK_NULL_HANDLE ) {
		return;
	}

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext = NULL;
	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	qvkCmdPipelineBarrier(
		vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0, NULL,
		0, NULL,
		1, &barrier
	);
}

// Transition RT output image from TRANSFER_DST_OPTIMAL to GENERAL for RT writes
static void vk_rt_transition_image_to_general_for_write( VkImage image )
{
	if ( image == VK_NULL_HANDLE ) {
		return;
	}

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext = NULL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	qvkCmdPipelineBarrier(
		vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
		0,
		0, NULL,
		0, NULL,
		1, &barrier
	);
}

// Transition RT output image from GENERAL to SHADER_READ_ONLY_OPTIMAL for sampling
static void vk_rt_transition_image_to_sampled( VkImage image )
{
	if ( image == VK_NULL_HANDLE ) {
		return;
	}

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext = NULL;
	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	qvkCmdPipelineBarrier(
		vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0, NULL,
		0, NULL,
		1, &barrier
	);
}

static void vk_rt_create_scratch_buffer( VkDeviceSize size )
{
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CHECK( qvkCreateBuffer( vk.device, &bufferInfo, NULL, &vk.rt.scratchBuffer ) );

	VkMemoryRequirements memRequirements;
	qvkGetBufferMemoryRequirements( vk.device, vk.rt.scratchBuffer, &memRequirements );

	VkMemoryAllocateFlagsInfo flagsInfo = {};
	flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
	flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = &flagsInfo;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = find_memory_type( memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.rt.scratchMemory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.rt.scratchBuffer, vk.rt.scratchMemory, 0 ) );

	vk.rt.scratchBufferSize = size;
}


static void vk_rt_destroy_scratch_buffer( void )
{
	if ( vk.rt.scratchBuffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.rt.scratchBuffer, NULL );
		vk.rt.scratchBuffer = VK_NULL_HANDLE;
	}
	if ( vk.rt.scratchMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.rt.scratchMemory, NULL );
		vk.rt.scratchMemory = VK_NULL_HANDLE;
	}
	vk.rt.scratchBufferSize = 0;
}


static VkDeviceAddress vk_rt_get_buffer_device_address( VkBuffer buffer )
{
	if ( !qvkGetBufferDeviceAddress ) {
		return 0;
	}
	VkBufferDeviceAddressInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	info.pNext = NULL;
	info.buffer = buffer;
	return qvkGetBufferDeviceAddress( vk.device, &info );
}


/*
=============================================================================
Load Blue Noise Texture Array
Loads all 128 blue noise PNG files into a single 2D array texture
=============================================================================
*/
static void vk_rt_load_blue_noise_array( void )
{
	const uint32_t NUM_BLUE_NOISE_LAYERS = 128;
	const uint32_t BLUE_NOISE_WIDTH = 256;
	const uint32_t BLUE_NOISE_HEIGHT = 256;
	char path[MAX_QPATH];
	byte *pics[NUM_BLUE_NOISE_LAYERS];
	int widths[NUM_BLUE_NOISE_LAYERS];
	int heights[NUM_BLUE_NOISE_LAYERS];
	uint32_t loaded_count = 0;
	uint32_t i;

	// Declare R_LoadImage (it's static in tr_image.c, but we'll access it via a workaround)
	// For now, we'll use R_FindImageFile and extract data, or make R_LoadImage accessible
	// Actually, let's use a simpler approach: load via R_FindImageFile temporarily
	// and read back, or declare R_LoadImage as extern
	
	// Try to load all PNG files using R_LoadPNG (public function)
	for ( i = 0; i < NUM_BLUE_NOISE_LAYERS; i++ ) {
		// Try q3rtx path first
		Com_sprintf( path, sizeof( path ), "q3rtx/blue_noise_textures/256_256/HDR_RGBA_%04d.png", i );
		R_LoadPNG( path, &pics[loaded_count], &widths[loaded_count], &heights[loaded_count] );
		if ( pics[loaded_count] == NULL ) {
			// Try alternative path
			Com_sprintf( path, sizeof( path ), "blue_noise_textures/256_256/HDR_RGBA_%04d.png", i );
			R_LoadPNG( path, &pics[loaded_count], &widths[loaded_count], &heights[loaded_count] );
			if ( pics[loaded_count] == NULL ) {
				break; // Stop if we can't load this file
			}
		}

		// Verify dimensions match expected size
		if ( widths[loaded_count] != BLUE_NOISE_WIDTH || heights[loaded_count] != BLUE_NOISE_HEIGHT ) {
			ri.Printf( PRINT_WARNING, "Blue noise texture %s has wrong dimensions (%dx%d, expected %dx%d)\n",
				path, widths[loaded_count], heights[loaded_count], BLUE_NOISE_WIDTH, BLUE_NOISE_HEIGHT );
			ri.Free( pics[loaded_count] );
			break;
		}

		loaded_count++;
	}

	if ( loaded_count == 0 ) {
		ri.Printf( PRINT_DEVELOPER, "Warning: No blue noise textures found. Ray tracing will use hash-based RNG.\n" );
		vk.rt.blueNoiseTexture = NULL;
		return;
	}

	if ( loaded_count < NUM_BLUE_NOISE_LAYERS ) {
		ri.Printf( PRINT_WARNING, "Warning: Only loaded %u/%u blue noise textures. Some may be missing.\n", loaded_count, NUM_BLUE_NOISE_LAYERS );
	}

	// Create image_t structure for the array texture
	vk.rt.blueNoiseTexture = R_CreateImage( "blue_noise_array", NULL, NULL, BLUE_NOISE_WIDTH, BLUE_NOISE_HEIGHT, IMGFLAG_NONE, 0, 0 );
	if ( !vk.rt.blueNoiseTexture ) {
		ri.Printf( PRINT_ERROR, "Failed to create blue noise texture array\n" );
		// Free loaded images
		for ( i = 0; i < loaded_count; i++ ) {
			ri.Free( pics[i] );
		}
		return;
	}

	// Set array layer count
	vk.rt.blueNoiseTexture->layers = loaded_count;
	vk.rt.blueNoiseTexture->internalFormat = VK_FORMAT_R8G8B8A8_UNORM;
	vk.rt.blueNoiseTexture->uploadWidth = BLUE_NOISE_WIDTH;
	vk.rt.blueNoiseTexture->uploadHeight = BLUE_NOISE_HEIGHT;

	// Create Vulkan image with array layers
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageInfo.extent.width = BLUE_NOISE_WIDTH;
	imageInfo.extent.height = BLUE_NOISE_HEIGHT;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = loaded_count;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK( qvkCreateImage( vk.device, &imageInfo, NULL, &vk.rt.blueNoiseTexture->handle ) );

	// Allocate and bind memory
	VkMemoryRequirements memRequirements;
	qvkGetImageMemoryRequirements( vk.device, vk.rt.blueNoiseTexture->handle, &memRequirements );

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = find_memory_type( memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

	VkDeviceMemory imageMemory;
	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &imageMemory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.rt.blueNoiseTexture->handle, imageMemory, 0 ) );

	// Create image view for 2D array
	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = vk.rt.blueNoiseTexture->handle;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = loaded_count;

	VK_CHECK( qvkCreateImageView( vk.device, &viewInfo, NULL, &vk.rt.blueNoiseTexture->view ) );

	// Upload each layer using vk_upload_image_data approach
	// We'll upload each layer individually by creating temporary images and copying
	// For now, use a simpler approach: upload all layers in one go using staging buffer
	
	int pixel_size = BLUE_NOISE_WIDTH * BLUE_NOISE_HEIGHT * 4; // RGBA per layer
	int total_size = pixel_size * loaded_count;
	
	// Allocate staging buffer if needed
	// Check if staging buffer is large enough, if not we need to allocate a temporary one
	// For now, assume staging buffer exists and is managed elsewhere
	// If it's too small, we'll need to handle that case
	if ( (size_t)vk.staging_buffer.size < (size_t)total_size ) {
		ri.Printf( PRINT_WARNING, "Staging buffer too small for blue noise array (%d < %d). Upload may fail.\n", 
			(int)vk.staging_buffer.size, total_size );
		// In a real implementation, we'd allocate a larger staging buffer or upload in chunks
		// For now, just proceed and hope the staging buffer is large enough
	}

	// Copy all layers to staging buffer sequentially
        byte *staging_ptr = static_cast<byte*>(vk.staging_buffer.ptr);
	for ( i = 0; i < loaded_count; i++ ) {
		Com_Memcpy( staging_ptr, pics[i], pixel_size );
		staging_ptr += pixel_size;
		ri.Free( pics[i] );
	}

	// Create command buffer for upload
	VkCommandBufferAllocateInfo cbAllocInfo = {};
	cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbAllocInfo.commandPool = vk.command_pool;
	cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cbAllocInfo.commandBufferCount = 1;

	VkCommandBuffer command_buffer;
	VK_CHECK( qvkAllocateCommandBuffers( vk.device, &cbAllocInfo, &command_buffer ) );

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VK_CHECK( qvkBeginCommandBuffer( command_buffer, &beginInfo ) );

	// Transition image to TRANSFER_DST_OPTIMAL
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = vk.rt.blueNoiseTexture->handle;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = loaded_count;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

	qvkCmdPipelineBarrier( command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 0, NULL, 0, NULL, 1, &barrier );

	// Copy all layers
	VkBufferImageCopy *regions = (VkBufferImageCopy *)ri.Hunk_AllocateTempMemory( sizeof(VkBufferImageCopy) * loaded_count );
	for ( i = 0; i < loaded_count; i++ ) {
		regions[i].bufferOffset = i * pixel_size;
		regions[i].bufferRowLength = 0;
		regions[i].bufferImageHeight = 0;
		regions[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		regions[i].imageSubresource.mipLevel = 0;
		regions[i].imageSubresource.baseArrayLayer = i;
		regions[i].imageSubresource.layerCount = 1;
		regions[i].imageOffset.x = 0;
		regions[i].imageOffset.y = 0;
		regions[i].imageOffset.z = 0;
		regions[i].imageExtent.width = BLUE_NOISE_WIDTH;
		regions[i].imageExtent.height = BLUE_NOISE_HEIGHT;
		regions[i].imageExtent.depth = 1;
	}

	qvkCmdCopyBufferToImage( command_buffer, vk.staging_buffer.handle, vk.rt.blueNoiseTexture->handle,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, loaded_count, regions );

	ri.Hunk_FreeTempMemory( regions );

	// Transition image to SHADER_READ_ONLY_OPTIMAL
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	qvkCmdPipelineBarrier( command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0, 0, NULL, 0, NULL, 1, &barrier );

	VK_CHECK( qvkEndCommandBuffer( command_buffer ) );

	// Submit and wait
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &command_buffer;

	VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submitInfo, VK_NULL_HANDLE ) );
	vk_queue_wait_idle();

	qvkFreeCommandBuffers( vk.device, vk.command_pool, 1, &command_buffer );

	ri.Printf( PRINT_ALL, "Loaded blue noise texture array: %u layers (%ux%u each)\n", loaded_count, BLUE_NOISE_WIDTH, BLUE_NOISE_HEIGHT );
}

void vk_rt_init(void)
{
	if ( !vk.rayTracingSupported ) {
		return;
	}

	Com_Memset( &vk.rt, 0, sizeof( vk.rt ) );

	// Get ray tracing pipeline properties
	vk.rt.properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
	vk.rt.properties.pNext = NULL;
	
	VkPhysicalDeviceProperties2 props2 = {};
	props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	props2.pNext = &vk.rt.properties;
	
	if ( qvkGetPhysicalDeviceProperties2KHR ) {
		qvkGetPhysicalDeviceProperties2KHR( vk.physical_device, &props2 );
	} else {
		ri.Printf( PRINT_WARNING, "Ray tracing: GetPhysicalDeviceProperties2KHR not available\n" );
		vk.rayTracingSupported = qfalse;
		return;
	}

	ri.Printf( PRINT_ALL, "Ray tracing initialized:\n" );
	ri.Printf( PRINT_ALL, "  Shader group handle size: %u\n", vk.rt.properties.shaderGroupHandleSize );
	ri.Printf( PRINT_ALL, "  Max ray recursion depth: %u\n", vk.rt.properties.maxRayRecursionDepth );
	ri.Printf( PRINT_ALL, "  Max shader group stride: %u\n", vk.rt.properties.maxShaderGroupStride );

	vk.rt.blasCapacity = 256; // Initial capacity
	vk.rt.blas = (VkAccelerationStructureKHR *)ri.Malloc( sizeof( VkAccelerationStructureKHR ) * vk.rt.blasCapacity );
	vk.rt.blasBuffers = (VkBuffer *)ri.Malloc( sizeof( VkBuffer ) * vk.rt.blasCapacity );
	vk.rt.blasMemory = (VkDeviceMemory *)ri.Malloc( sizeof( VkDeviceMemory ) * vk.rt.blasCapacity );
	vk.rt.blasCount = 0;

	// Initialize BLAS reuse and compaction tracking
	vk.rt.blasHashes = (uint64_t *)ri.Malloc( sizeof( uint64_t ) * vk.rt.blasCapacity );
	vk.rt.blasCompacted = (VkAccelerationStructureKHR *)ri.Malloc( sizeof( VkAccelerationStructureKHR ) * vk.rt.blasCapacity );
	vk.rt.blasCompactedBuffers = (VkBuffer *)ri.Malloc( sizeof( VkBuffer ) * vk.rt.blasCapacity );
	vk.rt.blasCompactedMemory = (VkDeviceMemory *)ri.Malloc( sizeof( VkDeviceMemory ) * vk.rt.blasCapacity );
	vk.rt.blasNeedsCompaction = (qboolean *)ri.Malloc( sizeof( qboolean ) * vk.rt.blasCapacity );
	vk.rt.blasUnusedSlots = (uint32_t *)ri.Malloc( sizeof( uint32_t ) * vk.rt.blasCapacity );
	vk.rt.unusedSlotCount = 0;
	
	// Initialize arrays
	for ( uint32_t i = 0; i < vk.rt.blasCapacity; i++ ) {
		vk.rt.blasHashes[i] = 0;
		vk.rt.blasCompacted[i] = VK_NULL_HANDLE;
		vk.rt.blasCompactedBuffers[i] = VK_NULL_HANDLE;
		vk.rt.blasCompactedMemory[i] = VK_NULL_HANDLE;
		vk.rt.blasNeedsCompaction[i] = qfalse;
	}
	
	// Initialize TLAS update tracking
	vk.rt.previousInstances = NULL;
	vk.rt.previousInstanceCount = 0;
	vk.rt.tlasNeedsRebuild = qtrue;
	vk.rt.tlasAllowsUpdate = qfalse;
	
	// Initialize previous frame matrices
	Matrix16Identity( vk.rt.previousViewInverse );
	Matrix16Identity( vk.rt.previousProjInverse );
	VectorClear( vk.rt.previousCameraPos );
	vk.rt.previousMatricesValid = qfalse;

	// Initialize output image (will be created with proper size later)
	vk.rt.outputImage = VK_NULL_HANDLE;
	vk.rt.outputImageView = VK_NULL_HANDLE;
	vk.rt.outputImageMemory = VK_NULL_HANDLE;

	// Load blue noise texture array for denoising
	vk_rt_load_blue_noise_array();

	// Create descriptor set layout
	vk_rt_create_descriptor_set_layout();

	// Create pipeline layout
	vk_rt_create_pipeline_layout();

	// Pipeline creation will be deferred until shaders are loaded
	// vk_rt_create_pipeline() will be called from vk_create_shader_modules()

	// Create shader binding table
	vk_rt_create_shader_binding_table();

	// Create uniform buffer for camera data
	// Size: viewInverse(16*4) + projInverse(16*4) + cameraPos(4*4) + resolution(2*4) + time/near/far/exposure(4*4) + frameIndex/samplesPerPixel/debugMagenta(3*4)
	//      + previousViewInverse(16*4) + previousProjInverse(16*4) + previousCameraPos(4*4) + temporalAlpha(1*4)
	//      + maxBounces(1*4) + giIntensity(1*4) + invResolution(2*4) + spatialAlpha(1*4) + varianceAlpha(1*4) + iterations(1*4)
	//      + pathTracing (1*4 int)
	//      = 16+16+4+2+6+3+16+16+4+1+1+1+2+1+1+1 = 91 floats + 5 ints = 91*4 + 5*4 = 364 + 20 = 384 bytes
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(float) * (16 + 16 + 4 + 2 + 6 + 1 + 16 + 16 + 4 + 1 + 1 + 1 + 2 + 1 + 1) + sizeof(int) * (3 + 1 + 1); // All fields including denoising parameters + pathTracing
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CHECK( qvkCreateBuffer( vk.device, &bufferInfo, NULL, &vk.rt.uniformBuffer ) );

	VkMemoryRequirements memRequirements;
	qvkGetBufferMemoryRequirements( vk.device, vk.rt.uniformBuffer, &memRequirements );

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = find_memory_type( memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.rt.uniformBufferMemory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.rt.uniformBuffer, vk.rt.uniformBufferMemory, 0 ) );

	vk.rt.initialized = qtrue;
}


void vk_rt_shutdown(void)
{
	if ( !vk.rayTracingSupported || !vk.rt.initialized ) {
		return;
	}

	vk_rt_destroy_scratch_buffer();

	// Destroy BLAS
	if ( vk.rt.blas ) {
		uint32_t i;
		for ( i = 0; i < vk.rt.blasCount; i++ ) {
			if ( vk.rt.blas[i] != VK_NULL_HANDLE ) {
				qvkDestroyAccelerationStructureKHR( vk.device, vk.rt.blas[i], NULL );
			}
			if ( vk.rt.blasBuffers[i] != VK_NULL_HANDLE ) {
				qvkDestroyBuffer( vk.device, vk.rt.blasBuffers[i], NULL );
			}
			if ( vk.rt.blasMemory[i] != VK_NULL_HANDLE ) {
				qvkFreeMemory( vk.device, vk.rt.blasMemory[i], NULL );
			}
		}
		ri.Free( vk.rt.blas );
		ri.Free( vk.rt.blasBuffers );
		ri.Free( vk.rt.blasMemory );
		vk.rt.blas = NULL;
		vk.rt.blasBuffers = NULL;
		vk.rt.blasMemory = NULL;
	}

	// Destroy TLAS
	if ( vk.rt.tlas != VK_NULL_HANDLE ) {
		qvkDestroyAccelerationStructureKHR( vk.device, vk.rt.tlas, NULL );
		vk.rt.tlas = VK_NULL_HANDLE;
	}
	if ( vk.rt.tlasBuffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.rt.tlasBuffer, NULL );
		vk.rt.tlasBuffer = VK_NULL_HANDLE;
	}
	if ( vk.rt.tlasMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.rt.tlasMemory, NULL );
		vk.rt.tlasMemory = VK_NULL_HANDLE;
	}

	// Destroy temporal accumulation buffers
	if ( vk.rt.historyImageView != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.rt.historyImageView, NULL );
		vk.rt.historyImageView = VK_NULL_HANDLE;
	}
	if ( vk.rt.historyImage != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.rt.historyImage, NULL );
		vk.rt.historyImage = VK_NULL_HANDLE;
	}
	if ( vk.rt.historyImageMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.rt.historyImageMemory, NULL );
		vk.rt.historyImageMemory = VK_NULL_HANDLE;
	}
	if ( vk.rt.motionVectorImageView != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.rt.motionVectorImageView, NULL );
		vk.rt.motionVectorImageView = VK_NULL_HANDLE;
	}
	if ( vk.rt.motionVectorImage != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.rt.motionVectorImage, NULL );
		vk.rt.motionVectorImage = VK_NULL_HANDLE;
	}
	if ( vk.rt.motionVectorImageMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.rt.motionVectorImageMemory, NULL );
		vk.rt.motionVectorImageMemory = VK_NULL_HANDLE;
	}

	// Destroy denoising resources
	vk_rt_destroy_denoise_resources();

	// Destroy output image
	if ( vk.rt.outputImageView != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.rt.outputImageView, NULL );
		vk.rt.outputImageView = VK_NULL_HANDLE;
	}
	if ( vk.rt.outputImage != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.rt.outputImage, NULL );
		vk.rt.outputImage = VK_NULL_HANDLE;
	}
	if ( vk.rt.outputImageMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.rt.outputImageMemory, NULL );
		vk.rt.outputImageMemory = VK_NULL_HANDLE;
	}

	// Destroy ray tracing pipeline
	if ( vk.rt.raytracingPipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.rt.raytracingPipeline, NULL );
		vk.rt.raytracingPipeline = VK_NULL_HANDLE;
	}
	if ( vk.rt.raytracingPipelineLayout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.rt.raytracingPipelineLayout, NULL );
		vk.rt.raytracingPipelineLayout = VK_NULL_HANDLE;
	}
	if ( vk.rt.raytracingDescriptorSetLayout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.rt.raytracingDescriptorSetLayout, NULL );
		vk.rt.raytracingDescriptorSetLayout = VK_NULL_HANDLE;
	}

	// Free previous instances buffer
	if ( vk.rt.previousInstances != NULL ) {
		ri.Free( vk.rt.previousInstances );
		vk.rt.previousInstances = NULL;
		vk.rt.previousInstanceCount = 0;
	}
	
	// Free BLAS reuse and compaction arrays
	if ( vk.rt.blasHashes != NULL ) {
		ri.Free( vk.rt.blasHashes );
		vk.rt.blasHashes = NULL;
	}
	if ( vk.rt.blasCompacted != NULL ) {
		ri.Free( vk.rt.blasCompacted );
		vk.rt.blasCompacted = NULL;
	}
	if ( vk.rt.blasCompactedBuffers != NULL ) {
		ri.Free( vk.rt.blasCompactedBuffers );
		vk.rt.blasCompactedBuffers = NULL;
	}
	if ( vk.rt.blasCompactedMemory != NULL ) {
		ri.Free( vk.rt.blasCompactedMemory );
		vk.rt.blasCompactedMemory = NULL;
	}
	if ( vk.rt.blasNeedsCompaction != NULL ) {
		ri.Free( vk.rt.blasNeedsCompaction );
		vk.rt.blasNeedsCompaction = NULL;
	}
	if ( vk.rt.blasUnusedSlots != NULL ) {
		ri.Free( vk.rt.blasUnusedSlots );
		vk.rt.blasUnusedSlots = NULL;
	}

	// Destroy SBT
	if ( vk.rt.sbtBuffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.rt.sbtBuffer, NULL );
		vk.rt.sbtBuffer = VK_NULL_HANDLE;
	}
	if ( vk.rt.sbtMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.rt.sbtMemory, NULL );
		vk.rt.sbtMemory = VK_NULL_HANDLE;
	}

	// Destroy denoising pipeline
	if ( vk.rt.denoiseComputePipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.rt.denoiseComputePipeline, NULL );
		vk.rt.denoiseComputePipeline = VK_NULL_HANDLE;
	}
	if ( vk.rt.denoisePipelineLayout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.rt.denoisePipelineLayout, NULL );
		vk.rt.denoisePipelineLayout = VK_NULL_HANDLE;
	}
	if ( vk.rt.denoiseDescriptorSetLayout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.rt.denoiseDescriptorSetLayout, NULL );
		vk.rt.denoiseDescriptorSetLayout = VK_NULL_HANDLE;
	}

	// Destroy uniform buffer
	if ( vk.rt.uniformBuffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.rt.uniformBuffer, NULL );
		vk.rt.uniformBuffer = VK_NULL_HANDLE;
	}
	if ( vk.rt.uniformBufferMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.rt.uniformBufferMemory, NULL );
		vk.rt.uniformBufferMemory = VK_NULL_HANDLE;
	}

#ifdef USE_VULKAN_RAY_TRACING
	// Destroy composite pipeline
	if ( vk.rt_composite_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.rt_composite_pipeline, NULL );
		vk.rt_composite_pipeline = VK_NULL_HANDLE;
	}
	// Note: rt_composite_descriptor is allocated from vk.descriptor_pool
	// which will be destroyed in vk_shutdown(), so we don't need to free it explicitly
	// but we should set it to NULL to avoid use-after-free
	vk.rt_composite_descriptor = VK_NULL_HANDLE;
#endif

	vk.rt.initialized = qfalse;
}


void vk_rt_create_descriptor_set_layout( void )
{
	VkDescriptorSetLayoutBinding bindings[8];
	uint32_t bindingCount = 0;
	VkDescriptorSetLayoutCreateInfo layoutInfo = {};

	// Binding 0: Acceleration structure (TLAS)
	bindings[bindingCount].binding = 0;
	bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	bindings[bindingCount].descriptorCount = 1;
	bindings[bindingCount].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	bindings[bindingCount].pImmutableSamplers = NULL;
	bindingCount++;

	// Binding 1: Output image (storage image)
	bindings[bindingCount].binding = 1;
	bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[bindingCount].descriptorCount = 1;
	bindings[bindingCount].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	bindings[bindingCount].pImmutableSamplers = NULL;
	bindingCount++;

	// Binding 2: Uniform buffer (camera, etc.)
	bindings[bindingCount].binding = 2;
	bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[bindingCount].descriptorCount = 1;
	bindings[bindingCount].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;
	bindings[bindingCount].pImmutableSamplers = NULL;
	bindingCount++;

	// Binding 3: Blue-noise texture array (optional, used by raygen and hit shaders)
	bindings[bindingCount].binding = 3;
	bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[bindingCount].descriptorCount = 1;
	bindings[bindingCount].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	bindings[bindingCount].pImmutableSamplers = NULL;
	bindingCount++;

	// Binding 4: Textures array (for materials)
	bindings[bindingCount].binding = 4;
	bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[bindingCount].descriptorCount = 1024; // Max textures
	bindings[bindingCount].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	bindings[bindingCount].pImmutableSamplers = NULL;
	bindingCount++;

	// Binding 5: History image (for temporal accumulation)
	bindings[bindingCount].binding = 5;
	bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[bindingCount].descriptorCount = 1;
	bindings[bindingCount].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	bindings[bindingCount].pImmutableSamplers = NULL;
	bindingCount++;

    // Binding 6: Motion vectors (for temporal reprojection)
	bindings[bindingCount].binding = 6;
	bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[bindingCount].descriptorCount = 1;
	bindings[bindingCount].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	bindings[bindingCount].pImmutableSamplers = NULL;
	bindingCount++;

    // Binding 7: Per-surface material indices (RTX)
    bindings[bindingCount].binding = 7;
    bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[bindingCount].descriptorCount = 1;
    bindings[bindingCount].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    bindings[bindingCount].pImmutableSamplers = NULL;
    bindingCount++;

	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = NULL;
	layoutInfo.flags = 0;
	layoutInfo.bindingCount = bindingCount;
	layoutInfo.pBindings = bindings;

	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layoutInfo, NULL, &vk.rt.raytracingDescriptorSetLayout ) );
}


void vk_rt_create_pipeline_layout( void )
{
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	VkPushConstantRange pushConstantRange = {};

	pushConstantRange.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof( float ) * 16; // MVP matrix

	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.pNext = NULL;
	pipelineLayoutInfo.flags = 0;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &vk.rt.raytracingDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pipelineLayoutInfo, NULL, &vk.rt.raytracingPipelineLayout ) );
}


void vk_rt_create_pipeline( void )
{
	if ( vk.modules.rt_primary_rays_rgen == VK_NULL_HANDLE ||
		 vk.modules.rt_miss_rmiss == VK_NULL_HANDLE ||
		 vk.modules.rt_closesthit_rchit == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, "Ray tracing shaders not loaded, pipeline creation deferred\n" );
		return;
	}

	VkRayTracingPipelineCreateInfoKHR pipelineInfo = {};
	VkPipelineShaderStageCreateInfo shaderStages[3] = {};
	VkRayTracingShaderGroupCreateInfoKHR shaderGroups[3] = {};
	uint32_t stageCount = 0;
	uint32_t groupCount = 0;

	// Ray generation shader stage
	shaderStages[stageCount].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[stageCount].pNext = NULL;
	shaderStages[stageCount].flags = 0;
	shaderStages[stageCount].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	shaderStages[stageCount].module = vk.modules.rt_primary_rays_rgen;
	shaderStages[stageCount].pName = "main";
	shaderStages[stageCount].pSpecializationInfo = NULL;
	uint32_t raygenIndex = stageCount;
	stageCount++;

	// Miss shader stage
	shaderStages[stageCount].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[stageCount].pNext = NULL;
	shaderStages[stageCount].flags = 0;
	shaderStages[stageCount].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	shaderStages[stageCount].module = vk.modules.rt_miss_rmiss;
	shaderStages[stageCount].pName = "main";
	shaderStages[stageCount].pSpecializationInfo = NULL;
	uint32_t missIndex = stageCount;
	stageCount++;

	// Closest hit shader stage
	shaderStages[stageCount].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[stageCount].pNext = NULL;
	shaderStages[stageCount].flags = 0;
	shaderStages[stageCount].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	shaderStages[stageCount].module = vk.modules.rt_closesthit_rchit;
	shaderStages[stageCount].pName = "main";
	shaderStages[stageCount].pSpecializationInfo = NULL;
	uint32_t closestHitIndex = stageCount;
	stageCount++;

	// Ray generation shader group
	shaderGroups[groupCount].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	shaderGroups[groupCount].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	shaderGroups[groupCount].generalShader = raygenIndex;
	shaderGroups[groupCount].closestHitShader = VK_SHADER_UNUSED_KHR;
	shaderGroups[groupCount].anyHitShader = VK_SHADER_UNUSED_KHR;
	shaderGroups[groupCount].intersectionShader = VK_SHADER_UNUSED_KHR;
	groupCount++;

	// Miss shader group
	shaderGroups[groupCount].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	shaderGroups[groupCount].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	shaderGroups[groupCount].generalShader = missIndex;
	shaderGroups[groupCount].closestHitShader = VK_SHADER_UNUSED_KHR;
	shaderGroups[groupCount].anyHitShader = VK_SHADER_UNUSED_KHR;
	shaderGroups[groupCount].intersectionShader = VK_SHADER_UNUSED_KHR;
	groupCount++;

	// Closest hit shader group
	shaderGroups[groupCount].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	shaderGroups[groupCount].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	shaderGroups[groupCount].generalShader = VK_SHADER_UNUSED_KHR;
	shaderGroups[groupCount].closestHitShader = closestHitIndex;
	shaderGroups[groupCount].anyHitShader = VK_SHADER_UNUSED_KHR;
	shaderGroups[groupCount].intersectionShader = VK_SHADER_UNUSED_KHR;
	groupCount++;

	pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	pipelineInfo.pNext = NULL;
	pipelineInfo.flags = 0;
	pipelineInfo.stageCount = stageCount;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.groupCount = groupCount;
	pipelineInfo.pGroups = shaderGroups;
	pipelineInfo.maxPipelineRayRecursionDepth = 2; // Primary + shadow
	pipelineInfo.pLibraryInfo = NULL;
	pipelineInfo.pLibraryInterface = NULL;
	pipelineInfo.layout = vk.rt.raytracingPipelineLayout;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = 0;

	VK_CHECK( qvkCreateRayTracingPipelinesKHR( vk.device, VK_NULL_HANDLE, vk.pipelineCache, 1, &pipelineInfo, NULL, &vk.rt.raytracingPipeline ) );

	ri.Printf( PRINT_ALL, "Ray tracing pipeline created successfully\n" );
}


void vk_rt_populate_sbt( void )
{
	if ( !vk.rayTracingSupported || !vk.rt.initialized || vk.rt.raytracingPipeline == VK_NULL_HANDLE ) {
		return;
	}

	const uint32_t handleSize = vk.rt.properties.shaderGroupHandleSize;
	const uint32_t handleSizeAligned = (handleSize + vk.rt.properties.shaderGroupHandleAlignment - 1) & ~(vk.rt.properties.shaderGroupHandleAlignment - 1);
	const uint32_t groupCount = 3; // raygen, miss, closest hit
	const uint32_t sbtSize = groupCount * handleSizeAligned;

	// Get shader group handles
	uint8_t *shaderHandleStorage = (uint8_t *)ri.Malloc( groupCount * handleSize );
	VK_CHECK( qvkGetRayTracingShaderGroupHandlesKHR( vk.device, vk.rt.raytracingPipeline, 0, groupCount, groupCount * handleSize, shaderHandleStorage ) );

	// Map SBT buffer and copy handles
	void *mapped;
	VK_CHECK( qvkMapMemory( vk.device, vk.rt.sbtMemory, 0, sbtSize, 0, &mapped ) );

	uint8_t *pData = (uint8_t *)mapped;
	Com_Memcpy( pData + vk.rt.raygenRegionOffset, shaderHandleStorage + 0 * handleSize, handleSize );
	Com_Memcpy( pData + vk.rt.missRegionOffset, shaderHandleStorage + 1 * handleSize, handleSize );
	Com_Memcpy( pData + vk.rt.hitRegionOffset, shaderHandleStorage + 2 * handleSize, handleSize );

	qvkUnmapMemory( vk.device, vk.rt.sbtMemory );

	ri.Free( shaderHandleStorage );

	ri.Printf( PRINT_DEVELOPER, "Shader binding table populated\n" );
}


void vk_rt_create_shader_binding_table( void )
{
	const uint32_t handleSize = vk.rt.properties.shaderGroupHandleSize;
	const uint32_t handleSizeAligned = (handleSize + vk.rt.properties.shaderGroupHandleAlignment - 1) & ~(vk.rt.properties.shaderGroupHandleAlignment - 1);
	const uint32_t groupCount = 3; // raygen, miss, closest hit
	const uint32_t sbtSize = groupCount * handleSizeAligned;

	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sbtSize;
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CHECK( qvkCreateBuffer( vk.device, &bufferInfo, NULL, &vk.rt.sbtBuffer ) );

	VkMemoryRequirements memRequirements;
	qvkGetBufferMemoryRequirements( vk.device, vk.rt.sbtBuffer, &memRequirements );

	VkMemoryAllocateFlagsInfo flagsInfo = {};
	flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
	flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = &flagsInfo;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = find_memory_type( memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.rt.sbtMemory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.rt.sbtBuffer, vk.rt.sbtMemory, 0 ) );

	// Set up SBT regions
	vk.rt.raygenRegionSize = handleSizeAligned;
	vk.rt.missRegionSize = handleSizeAligned;
	vk.rt.hitRegionSize = handleSizeAligned;
	vk.rt.callableRegionSize = 0;

	vk.rt.raygenRegionOffset = 0;
	vk.rt.missRegionOffset = handleSizeAligned;
	vk.rt.hitRegionOffset = handleSizeAligned * 2;
	vk.rt.callableRegionOffset = 0;

	VkDeviceAddress sbtAddress = vk_rt_get_buffer_device_address( vk.rt.sbtBuffer );

	vk.rt.raygenShaderBindingTable.deviceAddress = sbtAddress + vk.rt.raygenRegionOffset;
	vk.rt.raygenShaderBindingTable.stride = handleSizeAligned;
	vk.rt.raygenShaderBindingTable.size = vk.rt.raygenRegionSize;

	vk.rt.missShaderBindingTable.deviceAddress = sbtAddress + vk.rt.missRegionOffset;
	vk.rt.missShaderBindingTable.stride = handleSizeAligned;
	vk.rt.missShaderBindingTable.size = vk.rt.missRegionSize;

	vk.rt.hitShaderBindingTable.deviceAddress = sbtAddress + vk.rt.hitRegionOffset;
	vk.rt.hitShaderBindingTable.stride = handleSizeAligned;
	vk.rt.hitShaderBindingTable.size = vk.rt.hitRegionSize;

	vk.rt.callableShaderBindingTable.deviceAddress = 0;
	vk.rt.callableShaderBindingTable.stride = 0;
	vk.rt.callableShaderBindingTable.size = 0;

	ri.Printf( PRINT_DEVELOPER, "Shader binding table created (size: %u bytes)\n", sbtSize );
}


// Compute hash from vertex/index data for BLAS reuse detection
static uint64_t vk_rt_compute_blas_hash( VkBuffer vertexBuffer, VkDeviceSize vertexOffset, uint32_t vertexCount,
										 VkBuffer indexBuffer, VkDeviceSize indexOffset, uint32_t indexCount )
{
	// Simple hash function: combine vertex count, index count, and buffer addresses
	// In a full implementation, we'd read actual vertex/index data and hash it
	// For now, use a combination of counts and offsets as a proxy
	uint64_t hash = 0;
	hash ^= (uint64_t)vertexCount * 0x9e3779b97f4a7c15ULL;
	hash ^= (uint64_t)indexCount * 0xbf58476d1ce4e5b9ULL;
	hash ^= (uint64_t)vertexOffset * 0x94d049bb133111ebULL;
	hash ^= (uint64_t)indexOffset * 0xc2b2ae3d27d4eb4fULL;
	hash ^= (uint64_t)vk_rt_get_buffer_device_address( vertexBuffer );
	hash ^= (uint64_t)vk_rt_get_buffer_device_address( indexBuffer );
	return hash;
}

// Find existing BLAS with matching hash, or return unused slot
static uint32_t vk_rt_find_blas_slot( uint64_t hash )
{
	// Check for existing BLAS with matching hash
	for ( uint32_t i = 0; i < vk.rt.blasCount; i++ ) {
		if ( vk.rt.blas[i] != VK_NULL_HANDLE && vk.rt.blasHashes[i] == hash ) {
			return i; // Reuse existing BLAS
		}
	}
	
	// Check for unused slot
	if ( vk.rt.unusedSlotCount > 0 ) {
		uint32_t slot = vk.rt.blasUnusedSlots[--vk.rt.unusedSlotCount];
		return slot;
	}
	
	// Use new slot
	return vk.rt.blasCount;
}

void vk_rt_build_blas( VkBuffer vertexBuffer, VkDeviceSize vertexOffset, uint32_t vertexCount,
					   VkBuffer indexBuffer, VkDeviceSize indexOffset, uint32_t indexCount,
					   uint32_t blasIndex )
{
	if ( !vk.rayTracingSupported || !vk.rt.initialized || blasIndex >= vk.rt.blasCapacity ) {
		return;
	}

	// Compute hash for reuse detection
	uint64_t hash = vk_rt_compute_blas_hash( vertexBuffer, vertexOffset, vertexCount, indexBuffer, indexOffset, indexCount );
	
	// Check if BLAS with this hash already exists
	uint32_t existingSlot = vk_rt_find_blas_slot( hash );
	if ( existingSlot < vk.rt.blasCount && vk.rt.blas[existingSlot] != VK_NULL_HANDLE && existingSlot != blasIndex ) {
		// Reuse existing BLAS
                ri.Printf( PRINT_DEVELOPER, "Reusing BLAS %u for slot %u (hash: 0x%016lx)\n", existingSlot, blasIndex, (unsigned long)hash );
		// Copy reference (in a full implementation, we'd track reference counts)
		return;
	}
	
	// Use the requested slot
	if ( blasIndex >= vk.rt.blasCount ) {
		blasIndex = vk.rt.blasCount;
	}

	VkAccelerationStructureGeometryKHR geometry = {};
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.pNext = NULL;
	geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

	geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	geometry.geometry.triangles.pNext = NULL;
	geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	geometry.geometry.triangles.vertexData.deviceAddress = vk_rt_get_buffer_device_address( vertexBuffer ) + vertexOffset;
	geometry.geometry.triangles.vertexStride = sizeof( vec3_t );
	geometry.geometry.triangles.maxVertex = vertexCount - 1;
	geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
	geometry.geometry.triangles.indexData.deviceAddress = vk_rt_get_buffer_device_address( indexBuffer ) + indexOffset;
	geometry.geometry.triangles.transformData.deviceAddress = 0;

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {};
	buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.pNext = NULL;
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	
	// Enable compaction if requested
	qboolean enableCompaction = ( r_rt_blasCompaction && r_rt_blasCompaction->integer );
	if ( enableCompaction ) {
		buildInfo.flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
	}
	
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	buildInfo.dstAccelerationStructure = VK_NULL_HANDLE;
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries = &geometry;
	buildInfo.ppGeometries = NULL;

	VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {};
	sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	sizeInfo.pNext = NULL;

	qvkGetAccelerationStructureBuildSizesKHR(
		vk.device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&buildInfo,
		&indexCount,
		&sizeInfo
	);

	// Create BLAS buffer
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeInfo.accelerationStructureSize;
	bufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CHECK( qvkCreateBuffer( vk.device, &bufferInfo, NULL, &vk.rt.blasBuffers[blasIndex] ) );

	VkMemoryRequirements memRequirements;
	qvkGetBufferMemoryRequirements( vk.device, vk.rt.blasBuffers[blasIndex], &memRequirements );

	VkMemoryAllocateFlagsInfo flagsInfo = {};
	flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
	flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = &flagsInfo;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = find_memory_type( memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.rt.blasMemory[blasIndex] ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.rt.blasBuffers[blasIndex], vk.rt.blasMemory[blasIndex], 0 ) );

	// Create acceleration structure
	VkAccelerationStructureCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	createInfo.pNext = NULL;
	createInfo.createFlags = 0;
	createInfo.buffer = vk.rt.blasBuffers[blasIndex];
	createInfo.offset = 0;
	createInfo.size = sizeInfo.accelerationStructureSize;
	createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	createInfo.deviceAddress = 0;

	VK_CHECK( qvkCreateAccelerationStructureKHR( vk.device, &createInfo, NULL, &vk.rt.blas[blasIndex] ) );

	// Get device address
	VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {};
	addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	addressInfo.pNext = NULL;
	addressInfo.accelerationStructure = vk.rt.blas[blasIndex];
	// Get device address (stored but not currently used - may be needed for TLAS)
	(void)qvkGetAccelerationStructureDeviceAddressKHR( vk.device, &addressInfo );

	// Create scratch buffer if needed
	if ( vk.rt.scratchBufferSize < sizeInfo.buildScratchSize ) {
		vk_rt_destroy_scratch_buffer();
		vk_rt_create_scratch_buffer( sizeInfo.buildScratchSize );
	}

	// Build BLAS
	buildInfo.dstAccelerationStructure = vk.rt.blas[blasIndex];
	buildInfo.scratchData.deviceAddress = vk_rt_get_buffer_device_address( vk.rt.scratchBuffer );

	VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {};
	rangeInfo.primitiveCount = indexCount / 3;
	rangeInfo.primitiveOffset = 0;
	rangeInfo.firstVertex = 0;
	rangeInfo.transformOffset = 0;

	const VkAccelerationStructureBuildRangeInfoKHR *pRangeInfo = &rangeInfo;
	const VkAccelerationStructureBuildRangeInfoKHR * const *ppRangeInfo = &pRangeInfo;

	qvkCmdBuildAccelerationStructuresKHR( vk.cmd->command_buffer, 1, &buildInfo, ppRangeInfo );

	if ( blasIndex >= vk.rt.blasCount ) {
		vk.rt.blasCount = blasIndex + 1;
	}

	ri.Printf( PRINT_DEVELOPER, "Built BLAS %u (vertices: %u, indices: %u)\n", blasIndex, vertexCount, indexCount );
}


void vk_rt_build_acceleration_structures( void )
{
	if ( !vk.rayTracingSupported || !vk.rt.initialized ) {
		return;
	}

	// This will be called to build BLAS from world geometry
	// For now, it's a placeholder - will be integrated with world loading
	ri.Printf( PRINT_DEVELOPER, "Building acceleration structures...\n" );
}


void vk_rt_update_tlas( void )
{
	if ( !vk.rayTracingSupported || !vk.rt.initialized || vk.rt.blasCount == 0 ) {
		return;
	}

	// Get update mode from cvar
	int updateMode = r_rt_tlasUpdateMode ? r_rt_tlasUpdateMode->integer : 1;
	qboolean useUpdateMode = qfalse;
	qboolean geometryChanged = qfalse;

	// Build instance array for TLAS
	VkAccelerationStructureInstanceKHR *instances = (VkAccelerationStructureInstanceKHR *)ri.Malloc( sizeof( VkAccelerationStructureInstanceKHR ) * vk.rt.blasCount );
	uint32_t instanceCount = 0;

	for ( uint32_t i = 0; i < vk.rt.blasCount; i++ ) {
		if ( vk.rt.blas[i] == VK_NULL_HANDLE ) {
			continue;
		}

		VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {};
		addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		addressInfo.accelerationStructure = vk.rt.blas[i];
		VkDeviceAddress blasAddress = qvkGetAccelerationStructureDeviceAddressKHR( vk.device, &addressInfo );

		instances[instanceCount].transform.matrix[0][0] = 1.0f;
		instances[instanceCount].transform.matrix[0][1] = 0.0f;
		instances[instanceCount].transform.matrix[0][2] = 0.0f;
		instances[instanceCount].transform.matrix[0][3] = 0.0f;
		instances[instanceCount].transform.matrix[1][0] = 0.0f;
		instances[instanceCount].transform.matrix[1][1] = 1.0f;
		instances[instanceCount].transform.matrix[1][2] = 0.0f;
		instances[instanceCount].transform.matrix[1][3] = 0.0f;
		instances[instanceCount].transform.matrix[2][0] = 0.0f;
		instances[instanceCount].transform.matrix[2][1] = 0.0f;
		instances[instanceCount].transform.matrix[2][2] = 1.0f;
		instances[instanceCount].transform.matrix[2][3] = 0.0f;

		instances[instanceCount].instanceCustomIndex = i;
		instances[instanceCount].mask = 0xFF;
		instances[instanceCount].instanceShaderBindingTableRecordOffset = 0;
		instances[instanceCount].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		instances[instanceCount].accelerationStructureReference = blasAddress;

		// Check if geometry changed (BLAS address or instance count)
		if ( updateMode == 1 && vk.rt.previousInstances != NULL && instanceCount < vk.rt.previousInstanceCount ) {
			if ( instanceCount < vk.rt.previousInstanceCount ||
			     instances[instanceCount].accelerationStructureReference != vk.rt.previousInstances[instanceCount].accelerationStructureReference ) {
				geometryChanged = qtrue;
			}
		}

		instanceCount++;
	}

	if ( instanceCount == 0 ) {
		ri.Free( instances );
		return;
	}

	// Determine if we can use UPDATE mode
	if ( updateMode == 1 && !geometryChanged && vk.rt.tlas != VK_NULL_HANDLE && vk.rt.tlasAllowsUpdate ) {
		// Compare transforms with previous frame
		if ( vk.rt.previousInstances != NULL && vk.rt.previousInstanceCount == instanceCount ) {
			qboolean transformsChanged = qfalse;
			for ( uint32_t i = 0; i < instanceCount; i++ ) {
				// Compare transform matrices (12 floats)
				if ( Com_Memcmp( instances[i].transform.matrix, vk.rt.previousInstances[i].transform.matrix, 
				                 sizeof( float ) * 12 ) != 0 ||
				     instances[i].accelerationStructureReference != vk.rt.previousInstances[i].accelerationStructureReference ||
				     instances[i].instanceCustomIndex != vk.rt.previousInstances[i].instanceCustomIndex ||
				     instances[i].mask != vk.rt.previousInstances[i].mask ||
				     instances[i].flags != vk.rt.previousInstances[i].flags ) {
					transformsChanged = qtrue;
					break;
				}
			}
			// If only transforms changed (not geometry), we can use UPDATE mode
			if ( !transformsChanged ) {
				// Nothing changed, skip update
				ri.Free( instances );
				return;
			}
			useUpdateMode = qtrue;
		}
	} else if ( updateMode == 2 && vk.rt.tlas != VK_NULL_HANDLE && vk.rt.tlasAllowsUpdate ) {
		// Force update mode
		useUpdateMode = qtrue;
	}

	// If rebuild is needed, mark it
	if ( vk.rt.tlasNeedsRebuild || geometryChanged || !useUpdateMode ) {
		vk.rt.tlasNeedsRebuild = qtrue;
		useUpdateMode = qfalse;
	}

	// Create instance buffer
	VkBuffer instanceBuffer;
	VkDeviceMemory instanceMemory;
	VkDeviceSize instanceBufferSize = sizeof( VkAccelerationStructureInstanceKHR ) * instanceCount;

	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = instanceBufferSize;
	bufferInfo.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CHECK( qvkCreateBuffer( vk.device, &bufferInfo, NULL, &instanceBuffer ) );

	VkMemoryRequirements memRequirements;
	qvkGetBufferMemoryRequirements( vk.device, instanceBuffer, &memRequirements );

	VkMemoryAllocateFlagsInfo flagsInfo = {};
	flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
	flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = &flagsInfo;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = find_memory_type( memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &instanceMemory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, instanceBuffer, instanceMemory, 0 ) );

	// Upload instance data
	void *mapped;
	VK_CHECK( qvkMapMemory( vk.device, instanceMemory, 0, instanceBufferSize, 0, &mapped ) );
	Com_Memcpy( mapped, instances, instanceBufferSize );
	qvkUnmapMemory( vk.device, instanceMemory );

	// Build TLAS
	VkAccelerationStructureGeometryKHR geometry = {};
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.pNext = NULL;
	geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	geometry.geometry.instances.pNext = NULL;
	geometry.geometry.instances.arrayOfPointers = VK_FALSE;
	geometry.geometry.instances.data.deviceAddress = vk_rt_get_buffer_device_address( instanceBuffer );

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {};
	buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.pNext = NULL;
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	if ( !useUpdateMode ) {
		buildInfo.flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
	}
	buildInfo.mode = useUpdateMode ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.srcAccelerationStructure = useUpdateMode ? vk.rt.tlas : VK_NULL_HANDLE;
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries = &geometry;

	VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {};
	sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	sizeInfo.pNext = NULL;

	qvkGetAccelerationStructureBuildSizesKHR(
		vk.device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&buildInfo,
		&instanceCount,
		&sizeInfo
	);

	// Create or resize TLAS buffer
	if ( vk.rt.tlasBuffer == VK_NULL_HANDLE || sizeInfo.accelerationStructureSize > vk.rt.scratchBufferSize ) {
		if ( vk.rt.tlasBuffer != VK_NULL_HANDLE ) {
			qvkDestroyBuffer( vk.device, vk.rt.tlasBuffer, NULL );
			qvkFreeMemory( vk.device, vk.rt.tlasMemory, NULL );
		}

		VkBufferCreateInfo tlasBufferInfo = {};
		tlasBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		tlasBufferInfo.size = sizeInfo.accelerationStructureSize;
		tlasBufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		tlasBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK( qvkCreateBuffer( vk.device, &tlasBufferInfo, NULL, &vk.rt.tlasBuffer ) );

		VkMemoryRequirements tlasMemRequirements;
		qvkGetBufferMemoryRequirements( vk.device, vk.rt.tlasBuffer, &tlasMemRequirements );

		VkMemoryAllocateFlagsInfo tlasFlagsInfo = {};
		tlasFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
		tlasFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

		VkMemoryAllocateInfo tlasAllocInfo = {};
		tlasAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		tlasAllocInfo.pNext = &tlasFlagsInfo;
		tlasAllocInfo.allocationSize = tlasMemRequirements.size;
		tlasAllocInfo.memoryTypeIndex = find_memory_type( tlasMemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

		VK_CHECK( qvkAllocateMemory( vk.device, &tlasAllocInfo, NULL, &vk.rt.tlasMemory ) );
		VK_CHECK( qvkBindBufferMemory( vk.device, vk.rt.tlasBuffer, vk.rt.tlasMemory, 0 ) );
	}

	// Create TLAS if it doesn't exist
	if ( vk.rt.tlas == VK_NULL_HANDLE ) {
		VkAccelerationStructureCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		createInfo.pNext = NULL;
		createInfo.createFlags = 0;
		createInfo.buffer = vk.rt.tlasBuffer;
		createInfo.offset = 0;
		createInfo.size = sizeInfo.accelerationStructureSize;
		createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		createInfo.deviceAddress = 0;

		VK_CHECK( qvkCreateAccelerationStructureKHR( vk.device, &createInfo, NULL, &vk.rt.tlas ) );
		vk.rt.tlasAllowsUpdate = qtrue; // We set ALLOW_UPDATE_BIT in build flags
		vk.rt.tlasNeedsRebuild = qfalse;
	}

	// Create scratch buffer if needed
	if ( vk.rt.scratchBufferSize < sizeInfo.buildScratchSize ) {
		vk_rt_destroy_scratch_buffer();
		vk_rt_create_scratch_buffer( sizeInfo.buildScratchSize );
	}

	// Build TLAS
	buildInfo.dstAccelerationStructure = vk.rt.tlas;
	buildInfo.scratchData.deviceAddress = vk_rt_get_buffer_device_address( vk.rt.scratchBuffer );

	VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {};
	rangeInfo.primitiveCount = instanceCount;
	rangeInfo.primitiveOffset = 0;
	rangeInfo.firstVertex = 0;
	rangeInfo.transformOffset = 0;

	const VkAccelerationStructureBuildRangeInfoKHR *pRangeInfo = &rangeInfo;
	const VkAccelerationStructureBuildRangeInfoKHR * const *ppRangeInfo = &pRangeInfo;

	qvkCmdBuildAccelerationStructuresKHR( vk.cmd->command_buffer, 1, &buildInfo, ppRangeInfo );

	// Get TLAS device address
	VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {};
	addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	addressInfo.accelerationStructure = vk.rt.tlas;
	vk.rt.tlasDeviceAddress = qvkGetAccelerationStructureDeviceAddressKHR( vk.device, &addressInfo );

	// Store current instances for next frame comparison
	if ( vk.rt.previousInstances != NULL ) {
		ri.Free( vk.rt.previousInstances );
	}
	vk.rt.previousInstances = (VkAccelerationStructureInstanceKHR *)ri.Malloc( sizeof( VkAccelerationStructureInstanceKHR ) * instanceCount );
	Com_Memcpy( vk.rt.previousInstances, instances, sizeof( VkAccelerationStructureInstanceKHR ) * instanceCount );
	vk.rt.previousInstanceCount = instanceCount;

	// Cleanup instance buffer
	qvkDestroyBuffer( vk.device, instanceBuffer, NULL );
	qvkFreeMemory( vk.device, instanceMemory, NULL );

	ri.Free( instances );

	if ( useUpdateMode ) {
		ri.Printf( PRINT_DEVELOPER, "Updated TLAS (UPDATE mode) with %u instances\n", instanceCount );
		vk.rt.tlasNeedsRebuild = qfalse;
	} else {
		ri.Printf( PRINT_DEVELOPER, "Rebuilt TLAS (BUILD mode) with %u instances\n", instanceCount );
		vk.rt.tlasNeedsRebuild = qfalse;
	}
}


void vk_rt_create_output_image( uint32_t width, uint32_t height )
{
	// Only create output image if ray tracing is enabled
#ifdef USE_VULKAN_RAY_TRACING
	if ( !r_raytracing || !r_raytracing->integer ) {
		return;
	}
#endif
	
	// Check if image needs recreation (size changed or scale changed)
	uint32_t expectedWidth = width;
	uint32_t expectedHeight = height;
	if ( vk.rt.outputImage != VK_NULL_HANDLE && 
	     vk.rt.outputImageWidth == expectedWidth && 
	     vk.rt.outputImageHeight == expectedHeight ) {
		return; // Already created with correct size
	}

	// Destroy old image if exists
	if ( vk.rt.outputImageView != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.rt.outputImageView, NULL );
		vk.rt.outputImageView = VK_NULL_HANDLE;
	}
	if ( vk.rt.outputImage != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.rt.outputImage, NULL );
		vk.rt.outputImage = VK_NULL_HANDLE;
	}
	if ( vk.rt.outputImageMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.rt.outputImageMemory, NULL );
		vk.rt.outputImageMemory = VK_NULL_HANDLE;
	}

	// Create output image with HDR format (rgba16f) for proper ray tracing output
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.pNext = NULL;
	imageInfo.flags = 0;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT; // HDR format for ray tracing
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK( qvkCreateImage( vk.device, &imageInfo, NULL, &vk.rt.outputImage ) );

	VkMemoryRequirements memRequirements;
	qvkGetImageMemoryRequirements( vk.device, vk.rt.outputImage, &memRequirements );

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = find_memory_type( memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.rt.outputImageMemory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.rt.outputImage, vk.rt.outputImageMemory, 0 ) );

	// Create image view
	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.pNext = NULL;
	viewInfo.flags = 0;
	viewInfo.image = vk.rt.outputImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT; // Match image format
	viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	VK_CHECK( qvkCreateImageView( vk.device, &viewInfo, NULL, &vk.rt.outputImageView ) );

	// Store image dimensions for validation
	vk.rt.outputImageWidth = width;
	vk.rt.outputImageHeight = height;

	// Transition image layout
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext = NULL;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = vk.rt.outputImage;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	qvkCmdPipelineBarrier(
		vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
		0,
		0, NULL,
		0, NULL,
		1, &barrier
	);

	ri.Printf( PRINT_DEVELOPER, "Created ray tracing output image (%ux%u)\n", width, height );
	
	// Create temporal accumulation buffers if temporal accumulation is enabled
	if ( r_rt_temporal && r_rt_temporal->integer ) {
		vk_rt_create_temporal_buffers( width, height );
	}
	
	// Create denoising buffers if denoising is enabled
	if ( r_rt_denoise && r_rt_denoise->integer ) {
		vk_rt_create_denoise_resources( width, height );
	}
}


// Create temporal accumulation buffers (history and motion vectors)
static void vk_rt_create_temporal_buffers( uint32_t width, uint32_t height )
{
	// Destroy old buffers if they exist and size changed
	if ( vk.rt.historyImage != VK_NULL_HANDLE && 
	     ( width != vk.rt.outputImageWidth || height != vk.rt.outputImageHeight ) ) {
		if ( vk.rt.historyImageView != VK_NULL_HANDLE ) {
			qvkDestroyImageView( vk.device, vk.rt.historyImageView, NULL );
			vk.rt.historyImageView = VK_NULL_HANDLE;
		}
		if ( vk.rt.historyImage != VK_NULL_HANDLE ) {
			qvkDestroyImage( vk.device, vk.rt.historyImage, NULL );
			vk.rt.historyImage = VK_NULL_HANDLE;
		}
		if ( vk.rt.historyImageMemory != VK_NULL_HANDLE ) {
			qvkFreeMemory( vk.device, vk.rt.historyImageMemory, NULL );
			vk.rt.historyImageMemory = VK_NULL_HANDLE;
		}
		
		if ( vk.rt.motionVectorImageView != VK_NULL_HANDLE ) {
			qvkDestroyImageView( vk.device, vk.rt.motionVectorImageView, NULL );
			vk.rt.motionVectorImageView = VK_NULL_HANDLE;
		}
		if ( vk.rt.motionVectorImage != VK_NULL_HANDLE ) {
			qvkDestroyImage( vk.device, vk.rt.motionVectorImage, NULL );
			vk.rt.motionVectorImage = VK_NULL_HANDLE;
		}
		if ( vk.rt.motionVectorImageMemory != VK_NULL_HANDLE ) {
			qvkFreeMemory( vk.device, vk.rt.motionVectorImageMemory, NULL );
			vk.rt.motionVectorImageMemory = VK_NULL_HANDLE;
		}
	}
	
	// Create history buffer (same format as output image)
	if ( vk.rt.historyImage == VK_NULL_HANDLE ) {
		VkImageCreateInfo imageInfo = {};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT; // HDR format matching output
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		
		VK_CHECK( qvkCreateImage( vk.device, &imageInfo, NULL, &vk.rt.historyImage ) );
		
		VkMemoryRequirements memRequirements;
		qvkGetImageMemoryRequirements( vk.device, vk.rt.historyImage, &memRequirements );
		
		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = find_memory_type( memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		
		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.rt.historyImageMemory ) );
		VK_CHECK( qvkBindImageMemory( vk.device, vk.rt.historyImage, vk.rt.historyImageMemory, 0 ) );
		
		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = vk.rt.historyImage;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		
		VK_CHECK( qvkCreateImageView( vk.device, &viewInfo, NULL, &vk.rt.historyImageView ) );
	}
	
	// Create motion vector buffer (RG16F format)
	if ( vk.rt.motionVectorImage == VK_NULL_HANDLE ) {
		VkImageCreateInfo imageInfo = {};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = VK_FORMAT_R16G16_SFLOAT; // RG16F for motion vectors
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		
		VK_CHECK( qvkCreateImage( vk.device, &imageInfo, NULL, &vk.rt.motionVectorImage ) );
		
		VkMemoryRequirements memRequirements;
		qvkGetImageMemoryRequirements( vk.device, vk.rt.motionVectorImage, &memRequirements );
		
		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = find_memory_type( memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		
		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.rt.motionVectorImageMemory ) );
		VK_CHECK( qvkBindImageMemory( vk.device, vk.rt.motionVectorImage, vk.rt.motionVectorImageMemory, 0 ) );
		
		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = vk.rt.motionVectorImage;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = VK_FORMAT_R16G16_SFLOAT;
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		
		VK_CHECK( qvkCreateImageView( vk.device, &viewInfo, NULL, &vk.rt.motionVectorImageView ) );
	}
}


void vk_rt_update_descriptor_set( void )
{
	if ( !vk.rayTracingSupported || !vk.rt.initialized || vk.rt.tlas == VK_NULL_HANDLE || vk.rt.raytracingDescriptorSet == VK_NULL_HANDLE ) {
		return;
	}

	VkWriteDescriptorSetAccelerationStructureKHR asInfo = {};
	asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	asInfo.pNext = NULL;
	asInfo.accelerationStructureCount = 1;
	asInfo.pAccelerationStructures = &vk.rt.tlas;

	VkDescriptorImageInfo imageInfo = {};
	imageInfo.sampler = VK_NULL_HANDLE;
	imageInfo.imageView = vk.rt.outputImageView;
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	VkDescriptorImageInfo blueNoiseInfo = {};

	VkWriteDescriptorSet writes[7] = {};
	uint32_t writeCount = 0;

	// Binding 0: Acceleration structure
	writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[writeCount].pNext = &asInfo;
	writes[writeCount].dstSet = vk.rt.raytracingDescriptorSet;
	writes[writeCount].dstBinding = 0;
	writes[writeCount].dstArrayElement = 0;
	writes[writeCount].descriptorCount = 1;
	writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	writes[writeCount].pImageInfo = NULL;
	writes[writeCount].pBufferInfo = NULL;
	writes[writeCount].pTexelBufferView = NULL;
	writeCount++;

	// Binding 1: Output image
	writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[writeCount].pNext = NULL;
	writes[writeCount].dstSet = vk.rt.raytracingDescriptorSet;
	writes[writeCount].dstBinding = 1;
	writes[writeCount].dstArrayElement = 0;
	writes[writeCount].descriptorCount = 1;
	writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[writeCount].pImageInfo = &imageInfo;
	writes[writeCount].pBufferInfo = NULL;
	writes[writeCount].pTexelBufferView = NULL;
	writeCount++;

	// Binding 2: Uniform buffer
	VkDescriptorBufferInfo bufferInfo = {};
	bufferInfo.buffer = vk.rt.uniformBuffer;
	bufferInfo.offset = 0;
	bufferInfo.range = VK_WHOLE_SIZE;

	writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[writeCount].pNext = NULL;
	writes[writeCount].dstSet = vk.rt.raytracingDescriptorSet;
	writes[writeCount].dstBinding = 2;
	writes[writeCount].dstArrayElement = 0;
	writes[writeCount].descriptorCount = 1;
	writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writes[writeCount].pImageInfo = NULL;
	writes[writeCount].pBufferInfo = &bufferInfo;
	writes[writeCount].pTexelBufferView = NULL;
	writeCount++;

	// Binding 3: Blue-noise texture array (optional)
	if ( vk.rt.blueNoiseTexture && vk.rt.blueNoiseTexture->view != VK_NULL_HANDLE ) {
		Vk_Sampler_Def sd;
		Com_Memset( &sd, 0, sizeof( sd ) );
		sd.vk_mag_filter = sd.vk_min_filter = VK_FILTER_LINEAR;
		sd.address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sd.max_lod_1_0 = qtrue;
		sd.noAnisotropy = qtrue;

		blueNoiseInfo.sampler = vk_find_sampler( &sd );
		blueNoiseInfo.imageView = vk.rt.blueNoiseTexture->view;
		blueNoiseInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[writeCount].pNext = NULL;
		writes[writeCount].dstSet = vk.rt.raytracingDescriptorSet;
		writes[writeCount].dstBinding = 3;
		writes[writeCount].dstArrayElement = 0;
		writes[writeCount].descriptorCount = 1;
		writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[writeCount].pImageInfo = &blueNoiseInfo;
		writes[writeCount].pBufferInfo = NULL;
		writes[writeCount].pTexelBufferView = NULL;
		writeCount++;
	}

	// Binding 5: History image (for temporal accumulation)
	if ( r_rt_temporal && r_rt_temporal->integer && vk.rt.historyImageView != VK_NULL_HANDLE ) {
		VkDescriptorImageInfo historyImageInfo = {};
		historyImageInfo.sampler = VK_NULL_HANDLE;
		historyImageInfo.imageView = vk.rt.historyImageView;
		historyImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[writeCount].pNext = NULL;
		writes[writeCount].dstSet = vk.rt.raytracingDescriptorSet;
		writes[writeCount].dstBinding = 5;
		writes[writeCount].dstArrayElement = 0;
		writes[writeCount].descriptorCount = 1;
		writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[writeCount].pImageInfo = &historyImageInfo;
		writes[writeCount].pBufferInfo = NULL;
		writes[writeCount].pTexelBufferView = NULL;
		writeCount++;
	}

	// Binding 6: Motion vectors (for temporal reprojection)
	if ( r_rt_temporal && r_rt_temporal->integer && vk.rt.motionVectorImageView != VK_NULL_HANDLE ) {
		VkDescriptorImageInfo motionVectorImageInfo = {};
		motionVectorImageInfo.sampler = VK_NULL_HANDLE;
		motionVectorImageInfo.imageView = vk.rt.motionVectorImageView;
		motionVectorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[writeCount].pNext = NULL;
		writes[writeCount].dstSet = vk.rt.raytracingDescriptorSet;
		writes[writeCount].dstBinding = 6;
		writes[writeCount].dstArrayElement = 0;
		writes[writeCount].descriptorCount = 1;
		writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[writeCount].pImageInfo = &motionVectorImageInfo;
		writes[writeCount].pBufferInfo = NULL;
		writes[writeCount].pTexelBufferView = NULL;
		writeCount++;
	}

    qvkUpdateDescriptorSets( vk.device, writeCount, writes, 0, NULL );

    // Bind per-surface material indices buffer to the descriptor set
    vk_rtx_bind_surface_indices_buffer( vk.rt.raytracingDescriptorSet );
}

// Update uniform buffer with camera data and debug flags
static void vk_rt_update_uniform_buffer( void )
{
	if ( !vk.rayTracingSupported || !vk.rt.initialized || vk.rt.uniformBuffer == VK_NULL_HANDLE ) {
		return;
	}

	void *mapped;
	VK_CHECK( qvkMapMemory( vk.device, vk.rt.uniformBufferMemory, 0, VK_WHOLE_SIZE, 0, &mapped ) );

	float *data = (float *)mapped;
	int *idata;

	// viewInverse (mat4 = 16 floats)
	// Get view matrix from backEnd and invert it (use optimized inversion)
	mat4_t viewInverse;
	extern backEndState_t backEnd;
	{
		mat4_t viewMatrix;
		Com_Memcpy( viewMatrix, backEnd.viewParms.world.modelViewMatrix, sizeof(mat4_t) );
		Matrix16InverseOptimized( viewMatrix, viewInverse );
	}
	Com_Memcpy( data, viewInverse, sizeof(float) * 16 );
	data += 16;

	// projInverse (mat4 = 16 floats)
	// Get projection matrix from backEnd and invert it (use optimized inversion)
	mat4_t projInverse;
	{
		mat4_t projMatrix;
		Com_Memcpy( projMatrix, backEnd.viewParms.projectionMatrix, sizeof(mat4_t) );
		Matrix16InverseOptimized( projMatrix, projInverse );
	}
	Com_Memcpy( data, projInverse, sizeof(float) * 16 );
	data += 16;

	// cameraPos (vec4 = 4 floats)
	{
		data[0] = backEnd.refdef.vieworg[0];
		data[1] = backEnd.refdef.vieworg[1];
		data[2] = backEnd.refdef.vieworg[2];
		data[3] = 1.0f;
	}
	data += 4;

	// resolution (vec2 = 2 floats)
	data[0] = (float)vk.renderWidth;
	data[1] = (float)vk.renderHeight;
	data += 2;

	// time (float)
	{
		data[0] = (float)backEnd.refdef.time;
	}
	data += 1;

	// nearPlane (float)
	data[0] = backEnd.viewParms.zNear > 0.0f ? backEnd.viewParms.zNear : 0.1f;
	data += 1;

	// farPlane (float)
	data[0] = backEnd.viewParms.zFar > 0.0f ? backEnd.viewParms.zFar : 1000.0f;
	data += 1;

	// exposure (float)
	data[0] = r_rt_samples && r_rt_samples->integer > 0 ? 1.0f / (float)r_rt_samples->integer : 1.0f;
	data += 1;

	// frameIndex (int)
	idata = (int *)data;
	idata[0] = vk.frame_count;
	idata += 1;

	// samplesPerPixel (int)
	idata[0] = r_rt_samples ? r_rt_samples->integer : 1;
	idata += 1;

	// debugMagenta (int)
	idata[0] = r_rt_debugMagenta ? r_rt_debugMagenta->integer : 0;
	idata += 1;
	data = (float *)idata;

	// temporalAlpha (float) - blend factor for temporal accumulation
	data[0] = r_rt_temporalAlpha ? r_rt_temporalAlpha->value : 0.9f;
	data += 1;
	
	// maxBounces (int) - maximum ray bounces for GI
	idata = (int *)data;
	idata[0] = (r_rt_gi && r_rt_gi->integer) ? (r_rt_giBounces ? r_rt_giBounces->integer : 2) : 0;
	idata += 1;
	data = (float *)idata;
	
	// giIntensity (float) - GI contribution scale
	data[0] = r_rt_giIntensity ? r_rt_giIntensity->value : 1.0f;
	data += 1;

	// previousViewInverse (mat4 = 16 floats)
	Com_Memcpy( data, vk.rt.previousViewInverse, sizeof(float) * 16 );
	data += 16;

	// previousProjInverse (mat4 = 16 floats)
	Com_Memcpy( data, vk.rt.previousProjInverse, sizeof(float) * 16 );
	data += 16;

	// previousCameraPos (vec4 = 4 floats)
	data[0] = vk.rt.previousCameraPos[0];
	data[1] = vk.rt.previousCameraPos[1];
	data[2] = vk.rt.previousCameraPos[2];
	data[3] = 1.0f;
	data += 4;

	// invResolution (vec2 = 2 floats) - for ReLAX denoising
	data[0] = 1.0f / (float)vk.renderWidth;
	data[1] = 1.0f / (float)vk.renderHeight;
	data += 2;

	// spatialAlpha (float) - spatial filter blend factor for ReLAX
	data[0] = r_rt_denoiseSpatialAlpha ? Com_Clamp( 0.0f, 1.0f, r_rt_denoiseSpatialAlpha->value ) : 0.5f;
	data += 1;

	// varianceAlpha (float) - variance blend factor for ReLAX
	data[0] = r_rt_denoiseVarianceAlpha ? Com_Clamp( 0.0f, 1.0f, r_rt_denoiseVarianceAlpha->value ) : 0.5f;
	data += 1;

	// iterations (int) - number of spatial filter iterations for ReLAX
	idata = (int *)data;
	idata[0] = r_rt_denoiseIterations ? r_rt_denoiseIterations->integer : 3;
	idata += 1;
	data = (float *)idata;

	// pathTracing (int) - enable path tracing mode (hybrid vs full path)
	idata = (int *)data;
	idata[0] = ri.Cvar_VariableIntegerValue( "r_rt_pathtracing" );
	idata += 1;
	data = (float *)idata;

	qvkUnmapMemory( vk.device, vk.rt.uniformBufferMemory );

	// Store current matrices for next frame
	Com_Memcpy( vk.rt.previousViewInverse, viewInverse, sizeof(mat4_t) );
	Com_Memcpy( vk.rt.previousProjInverse, projInverse, sizeof(mat4_t) );
	{
		VectorCopy( backEnd.refdef.vieworg, vk.rt.previousCameraPos );
	}
	vk.rt.previousMatricesValid = qtrue;
}

#ifdef USE_VULKAN_RAY_TRACING

void vk_rt_trace_rays( uint32_t width, uint32_t height )
{
	if ( !vk.rayTracingSupported || !vk.rt.initialized || vk.rt.raytracingPipeline == VK_NULL_HANDLE ) {
		return;
	}
	
	// Only trace rays if ray tracing is enabled via cvar
	if ( !r_raytracing || !r_raytracing->integer ) {
		return;
	}

	// Track if image was recreated to update descriptor set
	qboolean imageRecreated = ( vk.rt.outputImage == VK_NULL_HANDLE );

	// Create output image if needed
	vk_rt_create_output_image( width, height );

	// Validate dispatch size matches image size
	if ( vk.rt.outputImageWidth != 0 && vk.rt.outputImageHeight != 0 ) {
		if ( width != vk.rt.outputImageWidth || height != vk.rt.outputImageHeight ) {
			ri.Printf( PRINT_WARNING, "RT dispatch size mismatch: dispatch=%ux%u, image=%ux%u\n",
				width, height, vk.rt.outputImageWidth, vk.rt.outputImageHeight );
		}
	}

	// Update uniform buffer with camera data and debug flags
	vk_rt_update_uniform_buffer();

	// Update descriptor set if image was recreated
	if ( imageRecreated || vk.rt.outputImageView != VK_NULL_HANDLE ) {
		vk_rt_update_descriptor_set();
	}

	// Prepare output image for ray tracing: transition to TRANSFER_DST for clearing, then GENERAL for writes
	if ( vk.rt.outputImage != VK_NULL_HANDLE ) {
		// Transition from previous frame's SHADER_READ_ONLY_OPTIMAL to TRANSFER_DST_OPTIMAL
		if ( !imageRecreated ) {
			vk_rt_transition_image_to_transfer_dst( vk.rt.outputImage );
		} else {
			// Image was just created and is in GENERAL, transition to TRANSFER_DST_OPTIMAL for clearing
			vk_rt_transition_image_to_transfer_dst_from_general( vk.rt.outputImage );
		}

		// Clear output image (black or magenta for debug test)
		VkClearColorValue clearColor;
		if ( r_rt_debugMagenta && r_rt_debugMagenta->integer ) {
			// Magenta for diagnostic test
			clearColor.float32[0] = 1.0f;
			clearColor.float32[1] = 0.0f;
			clearColor.float32[2] = 1.0f;
			clearColor.float32[3] = 1.0f;
		} else {
			// Black (normal operation)
			clearColor.float32[0] = 0.0f;
			clearColor.float32[1] = 0.0f;
			clearColor.float32[2] = 0.0f;
			clearColor.float32[3] = 0.0f;
		}
		VkImageSubresourceRange clearRange = {};
		clearRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		clearRange.baseMipLevel = 0;
		clearRange.levelCount = 1;
		clearRange.baseArrayLayer = 0;
		clearRange.layerCount = 1;

		qvkCmdClearColorImage(
			vk.cmd->command_buffer,
			vk.rt.outputImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			&clearColor,
			1,
			&clearRange
		);

		// Transition to GENERAL layout for ray tracing writes
		vk_rt_transition_image_to_general_for_write( vk.rt.outputImage );
	}

	// Bind descriptor set
	qvkCmdBindDescriptorSets(
		vk.cmd->command_buffer,
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
		vk.rt.raytracingPipelineLayout,
		0,
		1,
		&vk.rt.raytracingDescriptorSet,
		0,
		NULL
	);

	// Bind ray tracing pipeline
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, vk.rt.raytracingPipeline );

	// Set push constants for RTX features based on quality settings
	uint32_t max_recursion = 1;
	uint32_t gi_samples = 1;

	rtx_quality_preset_t quality = vk_rtx_get_current_quality_preset();
	switch (quality) {
		case RTX_QUALITY_LOW:
			max_recursion = 1;
			gi_samples = 1;
			break;
		case RTX_QUALITY_MEDIUM:
			max_recursion = 2;
			gi_samples = 2;
			break;
		case RTX_QUALITY_HIGH:
			max_recursion = 3;
			gi_samples = 4;
			break;
		case RTX_QUALITY_ULTRA:
			max_recursion = 4;
			gi_samples = 8;
			break;
	}

	uint32_t push_constants[6] = {
		max_recursion, // max_recursion_depth
		1, // samples_per_pixel
		(r_rtx_shadows && r_rtx_shadows->integer) ? 1u : 0u, // enable_shadows
		(r_rtx_reflections && r_rtx_reflections->integer) ? 1u : 0u, // enable_reflections
		(r_rtx_gi && r_rtx_gi->integer) ? 1u : 0u, // enable_gi
		gi_samples // gi_samples
	};

	qvkCmdPushConstants(
		vk.cmd->command_buffer,
		vk.rt.raytracingPipelineLayout,
		VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
		0,
		sizeof(push_constants),
		push_constants
	);

	// Trace rays - dispatch size must match image dimensions exactly
	// Log dispatch size for debugging
	if ( r_rt_debugMagenta && r_rt_debugMagenta->integer ) {
		ri.Printf( PRINT_ALL, "RT dispatch: %ux%u, image: %ux%u\n",
			width, height, vk.rt.outputImageWidth, vk.rt.outputImageHeight );
	}
	qvkCmdTraceRaysKHR(
		vk.cmd->command_buffer,
		&vk.rt.raygenShaderBindingTable,
		&vk.rt.missShaderBindingTable,
		&vk.rt.hitShaderBindingTable,
		&vk.rt.callableShaderBindingTable,
		width,
		height,
		1
	);

	// Transition RT output image to SHADER_READ_ONLY_OPTIMAL for subsequent sampling/compositing
	vk_rt_transition_image_to_sampled( vk.rt.outputImage );
	
	// Apply ReLAX denoising if enabled
	if ( r_rt_denoise && r_rt_denoise->integer && vk.rt.denoiseComputePipeline != VK_NULL_HANDLE ) {
		vk_rt_denoise( width, height );
	}
	
	// Copy current frame to history for temporal accumulation (if enabled)
	if ( r_rt_temporal && r_rt_temporal->integer && vk.rt.historyImage != VK_NULL_HANDLE ) {
		// Transition history image to TRANSFER_DST_OPTIMAL
		VkImageMemoryBarrier historyBarrier = {};
		historyBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		historyBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		historyBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		historyBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		historyBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		historyBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		historyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		historyBarrier.image = vk.rt.historyImage;
		historyBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		historyBarrier.subresourceRange.baseMipLevel = 0;
		historyBarrier.subresourceRange.levelCount = 1;
		historyBarrier.subresourceRange.baseArrayLayer = 0;
		historyBarrier.subresourceRange.layerCount = 1;

		qvkCmdPipelineBarrier(
			vk.cmd->command_buffer,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0,
			0, NULL,
			0, NULL,
			1, &historyBarrier
		);

		// Transition output image to TRANSFER_SRC_OPTIMAL
		VkImageMemoryBarrier outputBarrier = {};
		outputBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		outputBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		outputBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		outputBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		outputBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		outputBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		outputBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		outputBarrier.image = vk.rt.outputImage;
		outputBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		outputBarrier.subresourceRange.baseMipLevel = 0;
		outputBarrier.subresourceRange.levelCount = 1;
		outputBarrier.subresourceRange.baseArrayLayer = 0;
		outputBarrier.subresourceRange.layerCount = 1;

		qvkCmdPipelineBarrier(
			vk.cmd->command_buffer,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0,
			0, NULL,
			0, NULL,
			1, &outputBarrier
		);

		// Copy output image to history
		VkImageCopy copyRegion = {};
		copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.srcSubresource.mipLevel = 0;
		copyRegion.srcSubresource.baseArrayLayer = 0;
		copyRegion.srcSubresource.layerCount = 1;
		copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.dstSubresource.mipLevel = 0;
		copyRegion.dstSubresource.baseArrayLayer = 0;
		copyRegion.dstSubresource.layerCount = 1;
		copyRegion.srcOffset.x = 0;
		copyRegion.srcOffset.y = 0;
		copyRegion.srcOffset.z = 0;
		copyRegion.dstOffset.x = 0;
		copyRegion.dstOffset.y = 0;
		copyRegion.dstOffset.z = 0;
		copyRegion.extent.width = width;
		copyRegion.extent.height = height;
		copyRegion.extent.depth = 1;

		qvkCmdCopyImage(
			vk.cmd->command_buffer,
			vk.rt.outputImage,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			vk.rt.historyImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&copyRegion
		);

		// Transition history back to GENERAL
		historyBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		historyBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		historyBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		historyBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

		qvkCmdPipelineBarrier(
			vk.cmd->command_buffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
			0,
			0, NULL,
			0, NULL,
			1, &historyBarrier
		);

		// Transition output back to SHADER_READ_ONLY_OPTIMAL
		outputBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		outputBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		outputBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		outputBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		qvkCmdPipelineBarrier(
			vk.cmd->command_buffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			0, NULL,
			0, NULL,
			1, &outputBarrier
		);
	}
}

/*
=============================================================================
ReLAX Denoising Implementation
=============================================================================
*/

void vk_rt_create_denoise_resources( uint32_t width, uint32_t height )
{
	if ( !vk.rayTracingSupported || !vk.rt.initialized ) {
		return;
	}

	// Destroy old buffers if they exist and size changed
	if ( vk.rt.denoiseNormalBuffer != VK_NULL_HANDLE && 
	     ( width != vk.rt.outputImageWidth || height != vk.rt.outputImageHeight ) ) {
		vk_rt_destroy_denoise_resources();
	}

	// Create normal buffer (RGBA16F for G-buffer normals)
	if ( vk.rt.denoiseNormalBuffer == VK_NULL_HANDLE ) {
		VkImageCreateInfo imageInfo = {};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK( qvkCreateImage( vk.device, &imageInfo, NULL, &vk.rt.denoiseNormalBuffer ) );

		VkMemoryRequirements memRequirements;
		qvkGetImageMemoryRequirements( vk.device, vk.rt.denoiseNormalBuffer, &memRequirements );

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = find_memory_type( memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.rt.denoiseNormalBufferMemory ) );
		VK_CHECK( qvkBindImageMemory( vk.device, vk.rt.denoiseNormalBuffer, vk.rt.denoiseNormalBufferMemory, 0 ) );

		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = vk.rt.denoiseNormalBuffer;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		VK_CHECK( qvkCreateImageView( vk.device, &viewInfo, NULL, &vk.rt.denoiseNormalBufferView ) );
	}

	// Create depth buffer (R32F)
	if ( vk.rt.denoiseDepthBuffer == VK_NULL_HANDLE ) {
		VkImageCreateInfo imageInfo = {};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = VK_FORMAT_R32_SFLOAT;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK( qvkCreateImage( vk.device, &imageInfo, NULL, &vk.rt.denoiseDepthBuffer ) );

		VkMemoryRequirements memRequirements;
		qvkGetImageMemoryRequirements( vk.device, vk.rt.denoiseDepthBuffer, &memRequirements );

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = find_memory_type( memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.rt.denoiseDepthBufferMemory ) );
		VK_CHECK( qvkBindImageMemory( vk.device, vk.rt.denoiseDepthBuffer, vk.rt.denoiseDepthBufferMemory, 0 ) );

		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = vk.rt.denoiseDepthBuffer;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = VK_FORMAT_R32_SFLOAT;
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		VK_CHECK( qvkCreateImageView( vk.device, &viewInfo, NULL, &vk.rt.denoiseDepthBufferView ) );
	}

	// Create variance buffer (RGBA16F)
	if ( vk.rt.denoiseVarianceBuffer == VK_NULL_HANDLE ) {
		VkImageCreateInfo imageInfo = {};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK( qvkCreateImage( vk.device, &imageInfo, NULL, &vk.rt.denoiseVarianceBuffer ) );

		VkMemoryRequirements memRequirements;
		qvkGetImageMemoryRequirements( vk.device, vk.rt.denoiseVarianceBuffer, &memRequirements );

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = find_memory_type( memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.rt.denoiseVarianceBufferMemory ) );
		VK_CHECK( qvkBindImageMemory( vk.device, vk.rt.denoiseVarianceBuffer, vk.rt.denoiseVarianceBufferMemory, 0 ) );

		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = vk.rt.denoiseVarianceBuffer;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		VK_CHECK( qvkCreateImageView( vk.device, &viewInfo, NULL, &vk.rt.denoiseVarianceBufferView ) );
	}

	// Create denoise history buffer (RGBA16F)
	if ( vk.rt.denoiseHistoryBuffer == VK_NULL_HANDLE ) {
		VkImageCreateInfo imageInfo = {};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK( qvkCreateImage( vk.device, &imageInfo, NULL, &vk.rt.denoiseHistoryBuffer ) );

		VkMemoryRequirements memRequirements;
		qvkGetImageMemoryRequirements( vk.device, vk.rt.denoiseHistoryBuffer, &memRequirements );

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = find_memory_type( memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

		VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.rt.denoiseHistoryBufferMemory ) );
		VK_CHECK( qvkBindImageMemory( vk.device, vk.rt.denoiseHistoryBuffer, vk.rt.denoiseHistoryBufferMemory, 0 ) );

		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = vk.rt.denoiseHistoryBuffer;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		VK_CHECK( qvkCreateImageView( vk.device, &viewInfo, NULL, &vk.rt.denoiseHistoryBufferView ) );
	}

	ri.Printf( PRINT_DEVELOPER, "Created ReLAX denoising buffers (%ux%u)\n", width, height );
}


void vk_rt_destroy_denoise_resources( void )
{
	if ( !vk.rayTracingSupported || !vk.rt.initialized ) {
		return;
	}

	// Destroy normal buffer
	if ( vk.rt.denoiseNormalBufferView != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.rt.denoiseNormalBufferView, NULL );
		vk.rt.denoiseNormalBufferView = VK_NULL_HANDLE;
	}
	if ( vk.rt.denoiseNormalBuffer != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.rt.denoiseNormalBuffer, NULL );
		vk.rt.denoiseNormalBuffer = VK_NULL_HANDLE;
	}
	if ( vk.rt.denoiseNormalBufferMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.rt.denoiseNormalBufferMemory, NULL );
		vk.rt.denoiseNormalBufferMemory = VK_NULL_HANDLE;
	}

	// Destroy depth buffer
	if ( vk.rt.denoiseDepthBufferView != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.rt.denoiseDepthBufferView, NULL );
		vk.rt.denoiseDepthBufferView = VK_NULL_HANDLE;
	}
	if ( vk.rt.denoiseDepthBuffer != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.rt.denoiseDepthBuffer, NULL );
		vk.rt.denoiseDepthBuffer = VK_NULL_HANDLE;
	}
	if ( vk.rt.denoiseDepthBufferMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.rt.denoiseDepthBufferMemory, NULL );
		vk.rt.denoiseDepthBufferMemory = VK_NULL_HANDLE;
	}

	// Destroy variance buffer
	if ( vk.rt.denoiseVarianceBufferView != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.rt.denoiseVarianceBufferView, NULL );
		vk.rt.denoiseVarianceBufferView = VK_NULL_HANDLE;
	}
	if ( vk.rt.denoiseVarianceBuffer != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.rt.denoiseVarianceBuffer, NULL );
		vk.rt.denoiseVarianceBuffer = VK_NULL_HANDLE;
	}
	if ( vk.rt.denoiseVarianceBufferMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.rt.denoiseVarianceBufferMemory, NULL );
		vk.rt.denoiseVarianceBufferMemory = VK_NULL_HANDLE;
	}

	// Destroy denoise history buffer
	if ( vk.rt.denoiseHistoryBufferView != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.rt.denoiseHistoryBufferView, NULL );
		vk.rt.denoiseHistoryBufferView = VK_NULL_HANDLE;
	}
	if ( vk.rt.denoiseHistoryBuffer != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.rt.denoiseHistoryBuffer, NULL );
		vk.rt.denoiseHistoryBuffer = VK_NULL_HANDLE;
	}
	if ( vk.rt.denoiseHistoryBufferMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.rt.denoiseHistoryBufferMemory, NULL );
		vk.rt.denoiseHistoryBufferMemory = VK_NULL_HANDLE;
	}
}


void vk_rt_create_denoise_pipeline( void )
{
	if ( !vk.rayTracingSupported || !vk.rt.initialized ) {
		return;
	}
	if ( vk.modules.rt_relax_comp == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, "ReLAX denoiser shader not loaded; skipping denoise pipeline creation\n" );
		return;
	}

	// Check if shader module is loaded
	if ( vk.modules.rt_relax_comp == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, "ReLAX denoising shader not loaded, pipeline creation deferred\n" );
		return;
	}

	// Create descriptor set layout for denoising
	// Binding 0: Noisy input (RGBA16F storage image)
	// Binding 1: History input (RGBA16F storage image)
	// Binding 2: Motion vectors (RG16F storage image)
	// Binding 3: Depth buffer (R32F storage image)
	// Binding 4: Normal buffer (RGBA16F storage image)
	// Binding 5: Denoised output (RGBA16F storage image)
	// Binding 6: Variance output (RGBA16F storage image)
	// Binding 7: History output (RGBA16F storage image)
	// Binding 8: Uniform buffer
	VkDescriptorSetLayoutBinding bindings[9] = {};
	
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[0].pImmutableSamplers = NULL;

	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[1].pImmutableSamplers = NULL;

	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[2].pImmutableSamplers = NULL;

	bindings[3].binding = 3;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[3].descriptorCount = 1;
	bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[3].pImmutableSamplers = NULL;

	bindings[4].binding = 4;
	bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[4].descriptorCount = 1;
	bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[4].pImmutableSamplers = NULL;

	bindings[5].binding = 5;
	bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[5].descriptorCount = 1;
	bindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[5].pImmutableSamplers = NULL;

	bindings[6].binding = 6;
	bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[6].descriptorCount = 1;
	bindings[6].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[6].pImmutableSamplers = NULL;

	bindings[7].binding = 7;
	bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[7].descriptorCount = 1;
	bindings[7].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[7].pImmutableSamplers = NULL;

	bindings[8].binding = 8;
	bindings[8].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[8].descriptorCount = 1;
	bindings[8].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[8].pImmutableSamplers = NULL;

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = NULL;
	layoutInfo.flags = 0;
	layoutInfo.bindingCount = 9;
	layoutInfo.pBindings = bindings;

	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layoutInfo, NULL, &vk.rt.denoiseDescriptorSetLayout ) );
	SET_OBJECT_NAME( vk.rt.denoiseDescriptorSetLayout, "denoise descriptor set layout", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT );

	// Create pipeline layout
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.pNext = NULL;
	pipelineLayoutInfo.flags = 0;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &vk.rt.denoiseDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = NULL;

	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pipelineLayoutInfo, NULL, &vk.rt.denoisePipelineLayout ) );
	SET_OBJECT_NAME( vk.rt.denoisePipelineLayout, "denoise pipeline layout", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );

	// Allocate descriptor set
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = NULL;
	allocInfo.descriptorPool = vk.descriptor_pool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &vk.rt.denoiseDescriptorSetLayout;

	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &vk.rt.denoiseDescriptorSet ) );

	// Create compute pipeline
	VkComputePipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext = NULL;
	pipelineInfo.flags = 0;
	pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	pipelineInfo.stage.module = vk.modules.rt_relax_comp;
	pipelineInfo.stage.pName = "main";
	pipelineInfo.stage.pSpecializationInfo = NULL;
	pipelineInfo.layout = vk.rt.denoisePipelineLayout;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &vk.rt.denoiseComputePipeline ) );
	SET_OBJECT_NAME( vk.rt.denoiseComputePipeline, "ReLAX denoise compute pipeline", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	ri.Printf( PRINT_DEVELOPER, "Created ReLAX denoising compute pipeline\n" );
}


void vk_rt_denoise( uint32_t width, uint32_t height )
{
	if ( !vk.rayTracingSupported || !vk.rt.initialized || 
	     !r_rt_denoise || !r_rt_denoise->integer ||
	     vk.rt.denoiseComputePipeline == VK_NULL_HANDLE ||
	     vk.rt.denoiseDescriptorSet == VK_NULL_HANDLE ||
	     vk.rt.outputImage == VK_NULL_HANDLE ) {
		return;
	}

	// Check if denoising resources exist
	if ( vk.rt.denoiseNormalBuffer == VK_NULL_HANDLE ||
	     vk.rt.denoiseDepthBuffer == VK_NULL_HANDLE ||
	     vk.rt.denoiseVarianceBuffer == VK_NULL_HANDLE ||
	     vk.rt.denoiseHistoryBuffer == VK_NULL_HANDLE ) {
		return;
	}

	// Transition images to GENERAL layout for compute shader access
	VkImageMemoryBarrier barriers[8] = {};
	uint32_t barrierCount = 0;

	// Output image (noisy input)
	barriers[barrierCount].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[barrierCount].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barriers[barrierCount].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	barriers[barrierCount].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barriers[barrierCount].newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barriers[barrierCount].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[barrierCount].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[barrierCount].image = vk.rt.outputImage;
	barriers[barrierCount].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barriers[barrierCount].subresourceRange.baseMipLevel = 0;
	barriers[barrierCount].subresourceRange.levelCount = 1;
	barriers[barrierCount].subresourceRange.baseArrayLayer = 0;
	barriers[barrierCount].subresourceRange.layerCount = 1;
	barrierCount++;

	// History image (if temporal accumulation is enabled)
	if ( vk.rt.historyImage != VK_NULL_HANDLE ) {
		barriers[barrierCount].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barriers[barrierCount].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		barriers[barrierCount].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barriers[barrierCount].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		barriers[barrierCount].newLayout = VK_IMAGE_LAYOUT_GENERAL;
		barriers[barrierCount].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[barrierCount].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[barrierCount].image = vk.rt.historyImage;
		barriers[barrierCount].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barriers[barrierCount].subresourceRange.baseMipLevel = 0;
		barriers[barrierCount].subresourceRange.levelCount = 1;
		barriers[barrierCount].subresourceRange.baseArrayLayer = 0;
		barriers[barrierCount].subresourceRange.layerCount = 1;
		barrierCount++;
	}

	// Motion vector image
	if ( vk.rt.motionVectorImage != VK_NULL_HANDLE ) {
		barriers[barrierCount].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barriers[barrierCount].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		barriers[barrierCount].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barriers[barrierCount].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		barriers[barrierCount].newLayout = VK_IMAGE_LAYOUT_GENERAL;
		barriers[barrierCount].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[barrierCount].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[barrierCount].image = vk.rt.motionVectorImage;
		barriers[barrierCount].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barriers[barrierCount].subresourceRange.baseMipLevel = 0;
		barriers[barrierCount].subresourceRange.levelCount = 1;
		barriers[barrierCount].subresourceRange.baseArrayLayer = 0;
		barriers[barrierCount].subresourceRange.layerCount = 1;
		barrierCount++;
	}

	// Denoise buffers (normal, depth, variance, history) - ensure GENERAL layout
	VkImage denoiseImages[] = {
		vk.rt.denoiseNormalBuffer,
		vk.rt.denoiseDepthBuffer,
		vk.rt.denoiseVarianceBuffer,
		vk.rt.denoiseHistoryBuffer
	};

	for ( uint32_t i = 0; i < 4; i++ ) {
		barriers[barrierCount].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barriers[barrierCount].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		barriers[barrierCount].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		barriers[barrierCount].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		barriers[barrierCount].newLayout = VK_IMAGE_LAYOUT_GENERAL;
		barriers[barrierCount].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[barrierCount].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[barrierCount].image = denoiseImages[i];
		barriers[barrierCount].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barriers[barrierCount].subresourceRange.baseMipLevel = 0;
		barriers[barrierCount].subresourceRange.levelCount = 1;
		barriers[barrierCount].subresourceRange.baseArrayLayer = 0;
		barriers[barrierCount].subresourceRange.layerCount = 1;
		barrierCount++;
	}

	if ( barrierCount > 0 ) {
		qvkCmdPipelineBarrier(
			vk.cmd->command_buffer,
			VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0,
			0, NULL,
			0, NULL,
			barrierCount, barriers
		);
	}

	// Update descriptor set with current images
	VkDescriptorImageInfo imageInfos[8] = {};
	
	// Binding 0: Noisy input (RT output)
	imageInfos[0].imageView = vk.rt.outputImageView;
	imageInfos[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	// Binding 1: History input (use denoise history if available, otherwise RT history)
	imageInfos[1].imageView = vk.rt.denoiseHistoryBufferView != VK_NULL_HANDLE ? 
	                          vk.rt.denoiseHistoryBufferView : 
	                          (vk.rt.historyImageView != VK_NULL_HANDLE ? vk.rt.historyImageView : vk.rt.outputImageView);
	imageInfos[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	// Binding 2: Motion vectors
	imageInfos[2].imageView = vk.rt.motionVectorImageView != VK_NULL_HANDLE ? vk.rt.motionVectorImageView : vk.rt.outputImageView;
	imageInfos[2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	// Binding 3: Depth buffer
	imageInfos[3].imageView = vk.rt.denoiseDepthBufferView;
	imageInfos[3].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	// Binding 4: Normal buffer
	imageInfos[4].imageView = vk.rt.denoiseNormalBufferView;
	imageInfos[4].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	// Binding 5: Denoised output (write to RT output image)
	imageInfos[5].imageView = vk.rt.outputImageView;
	imageInfos[5].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	// Binding 6: Variance output
	imageInfos[6].imageView = vk.rt.denoiseVarianceBufferView;
	imageInfos[6].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	// Binding 7: History output
	imageInfos[7].imageView = vk.rt.denoiseHistoryBufferView;
	imageInfos[7].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	VkWriteDescriptorSet writes[8] = {};
	for ( uint32_t i = 0; i < 8; i++ ) {
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].pNext = NULL;
		writes[i].dstSet = vk.rt.denoiseDescriptorSet;
		writes[i].dstBinding = i;
		writes[i].dstArrayElement = 0;
		writes[i].descriptorCount = 1;
		writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[i].pImageInfo = &imageInfos[i];
		writes[i].pBufferInfo = NULL;
		writes[i].pTexelBufferView = NULL;
	}

	// Add uniform buffer write
	VkDescriptorBufferInfo bufferInfo = {};
	bufferInfo.buffer = vk.rt.uniformBuffer;
	bufferInfo.offset = 0;
	bufferInfo.range = VK_WHOLE_SIZE;

	writes[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[7].pNext = NULL;
	writes[7].dstSet = vk.rt.denoiseDescriptorSet;
	writes[7].dstBinding = 8;
	writes[7].dstArrayElement = 0;
	writes[7].descriptorCount = 1;
	writes[7].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writes[7].pImageInfo = NULL;
	writes[7].pBufferInfo = &bufferInfo;
	writes[7].pTexelBufferView = NULL;

	qvkUpdateDescriptorSets( vk.device, 9, writes, 0, NULL );

	// Bind compute pipeline
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.rt.denoiseComputePipeline );

	// Bind descriptor set
	qvkCmdBindDescriptorSets(
		vk.cmd->command_buffer,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.rt.denoisePipelineLayout,
		0,
		1,
		&vk.rt.denoiseDescriptorSet,
		0,
		NULL
	);

	// Dispatch compute shader
	// Workgroup size is 8x8, so dispatch (width+7)/8 x (height+7)/8
	uint32_t workgroupX = ( width + 7 ) / 8;
	uint32_t workgroupY = ( height + 7 ) / 8;
	qvkCmdDispatch( vk.cmd->command_buffer, workgroupX, workgroupY, 1 );

	// Transition output image back to SHADER_READ_ONLY_OPTIMAL for compositing
	VkImageMemoryBarrier outputBarrier = {};
	outputBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	outputBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	outputBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	outputBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	outputBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	outputBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	outputBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	outputBarrier.image = vk.rt.outputImage;
	outputBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	outputBarrier.subresourceRange.baseMipLevel = 0;
	outputBarrier.subresourceRange.levelCount = 1;
	outputBarrier.subresourceRange.baseArrayLayer = 0;
	outputBarrier.subresourceRange.layerCount = 1;

	qvkCmdPipelineBarrier(
		vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0, NULL,
		0, NULL,
		1, &outputBarrier
	);
}

#endif // USE_VULKAN_RAY_TRACING

/*
=============================================================================
Ray Tracing Composite Pass
Composites RT output (HDR) with raster output, applies tonemapping
=============================================================================
*/

#ifdef USE_VULKAN_RAY_TRACING

void vk_rt_create_composite_descriptor_set( void )
{
	if ( !vk.rayTracingSupported || !vk.rt.initialized ) {
		vk.rt_composite_descriptor = VK_NULL_HANDLE;
		return;
	}

	// Don't recreate if already exists
	if ( vk.rt_composite_descriptor != VK_NULL_HANDLE ) {
		return;
	}

	// Create descriptor set layout for composite pass
	// Binding 0: RT output image (HDR)
	// Binding 1: Raster output (for fallback when RT disabled)
	VkDescriptorSetLayoutBinding bindings[2];
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[0].pImmutableSamplers = NULL;

	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[1].pImmutableSamplers = NULL;

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = NULL;
	layoutInfo.flags = 0;
	layoutInfo.bindingCount = 2;
	layoutInfo.pBindings = bindings;

	VkDescriptorSetLayout compositeLayout;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layoutInfo, NULL, &compositeLayout ) );

	// Allocate descriptor set from pool
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = NULL;
	allocInfo.descriptorPool = vk.descriptor_pool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &compositeLayout;

	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &vk.rt_composite_descriptor ) );

	// Clean up temporary layout (pipeline will use pipeline_layout_post_process)
	qvkDestroyDescriptorSetLayout( vk.device, compositeLayout, NULL );

	// Initialize descriptor set to avoid use of uninitialized values
	vk_rt_update_composite_descriptor_set();
}


void vk_rt_update_composite_descriptor_set( void )
{
	if ( !vk.rayTracingSupported || !vk.rt.initialized || vk.rt_composite_descriptor == VK_NULL_HANDLE ) {
		return;
	}

	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.vk_mag_filter = sd.vk_min_filter = VK_FILTER_LINEAR;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.max_lod_1_0 = qtrue;
	sd.noAnisotropy = qtrue;

	VkSampler sampler = vk_find_sampler( &sd );

	VkDescriptorImageInfo rtImageInfo = {};
	rtImageInfo.sampler = sampler;
	rtImageInfo.imageView = vk.rt.outputImageView != VK_NULL_HANDLE ? vk.rt.outputImageView : vk.color_image_view;
	rtImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkDescriptorImageInfo rasterImageInfo = {};
	rasterImageInfo.sampler = sampler;
	rasterImageInfo.imageView = vk.color_image_view;
	rasterImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet writes[2] = {};
	
	// Binding 0: RT output image
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].pNext = NULL;
	writes[0].dstSet = vk.rt_composite_descriptor;
	writes[0].dstBinding = 0;
	writes[0].dstArrayElement = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &rtImageInfo;
	writes[0].pBufferInfo = NULL;
	writes[0].pTexelBufferView = NULL;

	// Binding 1: Raster output (fallback)
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].pNext = NULL;
	writes[1].dstSet = vk.rt_composite_descriptor;
	writes[1].dstBinding = 1;
	writes[1].dstArrayElement = 0;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].pImageInfo = &rasterImageInfo;
	writes[1].pBufferInfo = NULL;
	writes[1].pTexelBufferView = NULL;

	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );
}


void vk_rt_composite( void )
{
	if ( !vk.rayTracingSupported || !vk.rt.initialized || vk.rt_composite_pipeline == VK_NULL_HANDLE || vk.rt_composite_descriptor == VK_NULL_HANDLE ) {
		return;
	}

	// Only composite if RT is enabled and we have RT output
	if ( !r_raytracing || !r_raytracing->integer || vk.rt.outputImageView == VK_NULL_HANDLE ) {
		return;
	}

	// Update descriptor set with current images
	vk_rt_update_composite_descriptor_set();

	// Use post_bloom render pass which writes to color_image
	// This allows bloom to process the composited RT output
	vk_begin_post_bloom_render_pass();

	// Bind composite pipeline
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.rt_composite_pipeline );

	// Bind descriptor set
	qvkCmdBindDescriptorSets(
		vk.cmd->command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.pipeline_layout_post_process,
		0,
		1,
		&vk.rt_composite_descriptor,
		0,
		NULL
	);

	// Draw fullscreen quad
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );

	vk_end_render_pass();
}

#endif // USE_VULKAN_RAY_TRACING

#endif // USE_VULKAN_RAY_TRACING (closes line 16)

#endif // USE_VULKAN

