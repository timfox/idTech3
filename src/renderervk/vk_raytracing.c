/*
=============================================================================
Vulkan Ray Tracing Implementation

Based on Quake-III-Arena-R reference implementation
=============================================================================
*/

#include "tr_local.h"
#include "vk.h"

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


void vk_rt_init( void )
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

	// Initialize output image (will be created with proper size later)
	vk.rt.outputImage = VK_NULL_HANDLE;
	vk.rt.outputImageView = VK_NULL_HANDLE;
	vk.rt.outputImageMemory = VK_NULL_HANDLE;

	// Load blue noise texture for denoising
	vk.rt.blueNoiseTexture = R_FindImageFile( "renderer/blue_noise_textures/LDR_RGBA_0.png", IMGFLAG_NONE, 0 );
	if ( !vk.rt.blueNoiseTexture ) {
		// Try alternative paths
		vk.rt.blueNoiseTexture = R_FindImageFile( "blue_noise_textures/LDR_RGBA_0.png", IMGFLAG_NONE, 0 );
		if ( !vk.rt.blueNoiseTexture ) {
			ri.Printf( PRINT_DEVELOPER, "Warning: Blue noise texture not found. Ray tracing denoising may be disabled.\n" );
		}
	}

	// Create descriptor set layout
	vk_rt_create_descriptor_set_layout();

	// Create pipeline layout
	vk_rt_create_pipeline_layout();

	// Pipeline creation will be deferred until shaders are loaded
	// vk_rt_create_pipeline() will be called from vk_create_shader_modules()

	// Create shader binding table
	vk_rt_create_shader_binding_table();

	// Create uniform buffer for camera data
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(float) * 16 * 2 + sizeof(float) * 4 + sizeof(float) * 2 + sizeof(float) * 5 + sizeof(int) * 3; // viewInverse, projInverse, cameraPos, resolution, time/near/far/exposure, frameIndex/samplesPerPixel/debugMagenta
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


void vk_rt_shutdown( void )
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

	// Destroy SBT
	if ( vk.rt.sbtBuffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.rt.sbtBuffer, NULL );
		vk.rt.sbtBuffer = VK_NULL_HANDLE;
	}
	if ( vk.rt.sbtMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.rt.sbtMemory, NULL );
		vk.rt.sbtMemory = VK_NULL_HANDLE;
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
	VkDescriptorSetLayoutBinding bindings[4];
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

	// Binding 3: Textures array (for materials)
	bindings[bindingCount].binding = 3;
	bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[bindingCount].descriptorCount = 1024; // Max textures
	bindings[bindingCount].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
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


void vk_rt_build_blas( VkBuffer vertexBuffer, VkDeviceSize vertexOffset, uint32_t vertexCount,
					   VkBuffer indexBuffer, VkDeviceSize indexOffset, uint32_t indexCount,
					   uint32_t blasIndex )
{
	if ( !vk.rayTracingSupported || !vk.rt.initialized || blasIndex >= vk.rt.blasCapacity ) {
		return;
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

		instanceCount++;
	}

	if ( instanceCount == 0 ) {
		ri.Free( instances );
		return;
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

	ri.Free( instances );

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
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
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

	// Cleanup instance buffer
	qvkDestroyBuffer( vk.device, instanceBuffer, NULL );
	qvkFreeMemory( vk.device, instanceMemory, NULL );

	ri.Printf( PRINT_DEVELOPER, "Updated TLAS with %u instances\n", instanceCount );
}


void vk_rt_create_output_image( uint32_t width, uint32_t height )
{
	// Only create output image if ray tracing is enabled
#ifdef USE_VULKAN_RAY_TRACING
	if ( !r_raytracing || !r_raytracing->integer ) {
		return;
	}
#endif
	
	if ( vk.rt.outputImage != VK_NULL_HANDLE && width == vk.renderWidth && height == vk.renderHeight ) {
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

	VkWriteDescriptorSet writes[2] = {};
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

	qvkUpdateDescriptorSets( vk.device, writeCount, writes, 0, NULL );
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
	// Get view matrix from backEnd and invert it
	mat4_t viewInverse;
	extern backEndState_t backEnd;
	if ( backEnd.viewParms.world.modelViewMatrix ) {
		mat4_t viewMatrix;
		Com_Memcpy( viewMatrix, backEnd.viewParms.world.modelViewMatrix, sizeof(mat4_t) );
		Matrix16Inverse( viewMatrix, viewInverse );
	} else {
		Matrix16Identity( viewInverse );
	}
	Com_Memcpy( data, viewInverse, sizeof(float) * 16 );
	data += 16;

	// projInverse (mat4 = 16 floats)
	// Get projection matrix from backEnd and invert it
	mat4_t projInverse;
	if ( backEnd.viewParms.projectionMatrix ) {
		mat4_t projMatrix;
		Com_Memcpy( projMatrix, backEnd.viewParms.projectionMatrix, sizeof(mat4_t) );
		Matrix16Inverse( projMatrix, projInverse );
	} else {
		Matrix16Identity( projInverse );
	}
	Com_Memcpy( data, projInverse, sizeof(float) * 16 );
	data += 16;

	// cameraPos (vec4 = 4 floats)
	if ( backEnd.refdef.vieworg ) {
		data[0] = backEnd.refdef.vieworg[0];
		data[1] = backEnd.refdef.vieworg[1];
		data[2] = backEnd.refdef.vieworg[2];
		data[3] = 1.0f;
	} else {
		data[0] = 0.0f;
		data[1] = 0.0f;
		data[2] = 0.0f;
		data[3] = 1.0f;
	}
	data += 4;

	// resolution (vec2 = 2 floats)
	data[0] = (float)vk.renderWidth;
	data[1] = (float)vk.renderHeight;
	data += 2;

	// time (float)
	if ( &backEnd.refdef ) {
		data[0] = (float)backEnd.refdef.time;
	} else {
		data[0] = 0.0f;
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

	qvkUnmapMemory( vk.device, vk.rt.uniformBufferMemory );
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
	sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
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

