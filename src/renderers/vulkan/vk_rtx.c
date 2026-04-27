/*
===========================================================================
Vulkan KHR ray tracing: minimal demo (BLAS triangle + TLAS + trace + blit).

Build with USE_VULKAN_RTX, set r_rtx > 0 before vid_restart. Optional r_rtxDemo
(default 1) gates the per-frame trace; set 0 to keep extensions without demo.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_rtx.h"

#ifdef USE_VULKAN_RTX

#include "vk_rtx_demo_spirv.inc"

static struct {
	qboolean		ready;
	uint32_t		width;
	uint32_t		height;
	uint32_t		handle_size;
	uint32_t		shader_group_base_alignment;
	VkShaderModule		rgen;
	VkShaderModule		rmiss;
	VkShaderModule		rchit;
	VkDescriptorPool	pool;
	VkDescriptorSetLayout	dsl;
	VkPipelineLayout	pl;
	VkPipeline		pipeline;
	VkDescriptorSet		descriptor_set;
	VkBuffer		sbt_buffer;
	VkDeviceMemory		sbt_memory;
	VkBuffer		scratch_buffer;
	VkDeviceMemory		scratch_memory;
	VkBuffer		blas_buffer;
	VkDeviceMemory		blas_memory;
	VkAccelerationStructureKHR blas;
	VkBuffer		instance_buffer;
	VkDeviceMemory		instance_memory;
	VkBuffer		tlas_buffer;
	VkDeviceMemory		tlas_memory;
	VkAccelerationStructureKHR tlas;
	VkBuffer		vertex_buffer;
	VkDeviceMemory		vertex_memory;
	VkBuffer		index_buffer;
	VkDeviceMemory		index_memory;
	VkImage			rt_image;
	VkDeviceMemory		rt_image_memory;
	VkImageView		rt_image_view;
} rtx;

static VkShaderModule vk_rtx_shader_module( const uint8_t *code, uint32_t codeSize, const char *name )
{
	VkShaderModuleCreateInfo ci;
	VkShaderModule mod;

	Com_Memset( &ci, 0, sizeof( ci ) );
	ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.codeSize = (size_t)codeSize;
	ci.pCode = (const uint32_t *)(uintptr_t)code;
	VK_CHECK( qvkCreateShaderModule( vk.device, &ci, NULL, &mod ) );
	SET_OBJECT_NAME( mod, name, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	return mod;
}

static void vk_rtx_alloc_buffer( VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memProps,
	VkBuffer *outBuf, VkDeviceMemory *outMem, VkDeviceAddress *outAddr )
{
	VkBufferCreateInfo bi;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo ai;
	VkMemoryAllocateFlagsInfo flagsInfo;
	uint32_t memType;

	Com_Memset( &bi, 0, sizeof( bi ) );
	bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bi.size = size;
	bi.usage = usage;
	bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &bi, NULL, outBuf ) );

	qvkGetBufferMemoryRequirements( vk.device, *outBuf, &req );
	memType = vk_find_memory_type( vk.physical_device, req.memoryTypeBits, memProps );

	Com_Memset( &flagsInfo, 0, sizeof( flagsInfo ) );
	flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
	if ( usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT ) {
		flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
	}

	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.pNext = ( usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT ) ? &flagsInfo : NULL;
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = memType;
	VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, outMem ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, *outBuf, *outMem, 0 ) );

	if ( outAddr ) {
		VkBufferDeviceAddressInfo addrInfo;
		Com_Memset( &addrInfo, 0, sizeof( addrInfo ) );
		addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		addrInfo.buffer = *outBuf;
		*outAddr = qvkGetBufferDeviceAddress( vk.device, &addrInfo );
	}
}

static void vk_rtx_destroy_buffer( VkBuffer *buf, VkDeviceMemory *mem )
{
	if ( *buf != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, *buf, NULL );
		*buf = VK_NULL_HANDLE;
	}
	if ( *mem != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, *mem, NULL );
		*mem = VK_NULL_HANDLE;
	}
}

static void vk_rtx_destroy_as( VkAccelerationStructureKHR *as )
{
	if ( *as != VK_NULL_HANDLE ) {
		qvkDestroyAccelerationStructureKHR( vk.device, *as, NULL );
		*as = VK_NULL_HANDLE;
	}
}

static void vk_rtx_submit_oneshot_build( VkCommandBuffer cmd )
{
	VkSubmitInfo si;

	Com_Memset( &si, 0, sizeof( si ) );
	si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &cmd;
	VK_CHECK( qvkQueueSubmit( vk.queue, 1, &si, VK_NULL_HANDLE ) );
	VK_CHECK( qvkQueueWaitIdle( vk.queue ) );
	VK_CHECK( qvkResetCommandBuffer( cmd, 0 ) );
}

static void vk_rtx_destroy_rt_output( void )
{
	if ( rtx.rt_image_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, rtx.rt_image_view, NULL );
		rtx.rt_image_view = VK_NULL_HANDLE;
	}
	if ( rtx.rt_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, rtx.rt_image, NULL );
		rtx.rt_image = VK_NULL_HANDLE;
	}
	if ( rtx.rt_image_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, rtx.rt_image_memory, NULL );
		rtx.rt_image_memory = VK_NULL_HANDLE;
	}
}

static void vk_rtx_create_rt_output( uint32_t w, uint32_t h, VkDescriptorSet descriptor_set )
{
	VkImageCreateInfo ici;
	VkMemoryRequirements imgReq;
	VkMemoryAllocateInfo ai;
	VkImageViewCreateInfo ivci;
	VkDescriptorImageInfo imgInfo;
	VkWriteDescriptorSet write;

	vk_rtx_destroy_rt_output();

	Com_Memset( &ici, 0, sizeof( ici ) );
	ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ici.imageType = VK_IMAGE_TYPE_2D;
	ici.format = vk.color_format;
	ici.extent.width = w;
	ici.extent.height = h;
	ici.extent.depth = 1;
	ici.mipLevels = 1;
	ici.arrayLayers = 1;
	ici.samples = VK_SAMPLE_COUNT_1_BIT;
	ici.tiling = VK_IMAGE_TILING_OPTIMAL;
	ici.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK( qvkCreateImage( vk.device, &ici, NULL, &rtx.rt_image ) );
	qvkGetImageMemoryRequirements( vk.device, rtx.rt_image, &imgReq );
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = imgReq.size;
	ai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, imgReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, &rtx.rt_image_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, rtx.rt_image, rtx.rt_image_memory, 0 ) );

	Com_Memset( &ivci, 0, sizeof( ivci ) );
	ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	ivci.image = rtx.rt_image;
	ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
	ivci.format = vk.color_format;
	ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	ivci.subresourceRange.levelCount = 1;
	ivci.subresourceRange.layerCount = 1;
	VK_CHECK( qvkCreateImageView( vk.device, &ivci, NULL, &rtx.rt_image_view ) );

	Com_Memset( &imgInfo, 0, sizeof( imgInfo ) );
	imgInfo.imageView = rtx.rt_image_view;
	imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	Com_Memset( &write, 0, sizeof( write ) );
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = descriptor_set;
	write.dstBinding = 1;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	write.pImageInfo = &imgInfo;
	qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );

	rtx.width = w;
	rtx.height = h;
}

void vk_rtx_shutdown( void )
{
	if ( !rtx.ready ) {
		return;
	}

	if ( rtx.pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, rtx.pipeline, NULL );
		rtx.pipeline = VK_NULL_HANDLE;
	}
	if ( rtx.pl != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, rtx.pl, NULL );
		rtx.pl = VK_NULL_HANDLE;
	}
	if ( rtx.dsl != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, rtx.dsl, NULL );
		rtx.dsl = VK_NULL_HANDLE;
	}
	if ( rtx.pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, rtx.pool, NULL );
		rtx.pool = VK_NULL_HANDLE;
	}
	if ( rtx.rgen != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, rtx.rgen, NULL );
		rtx.rgen = VK_NULL_HANDLE;
	}
	if ( rtx.rmiss != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, rtx.rmiss, NULL );
		rtx.rmiss = VK_NULL_HANDLE;
	}
	if ( rtx.rchit != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, rtx.rchit, NULL );
		rtx.rchit = VK_NULL_HANDLE;
	}

	vk_rtx_destroy_as( &rtx.tlas );
	vk_rtx_destroy_as( &rtx.blas );
	vk_rtx_destroy_buffer( &rtx.tlas_buffer, &rtx.tlas_memory );
	vk_rtx_destroy_buffer( &rtx.instance_buffer, &rtx.instance_memory );
	vk_rtx_destroy_buffer( &rtx.blas_buffer, &rtx.blas_memory );
	vk_rtx_destroy_buffer( &rtx.vertex_buffer, &rtx.vertex_memory );
	vk_rtx_destroy_buffer( &rtx.index_buffer, &rtx.index_memory );
	vk_rtx_destroy_buffer( &rtx.scratch_buffer, &rtx.scratch_memory );
	vk_rtx_destroy_buffer( &rtx.sbt_buffer, &rtx.sbt_memory );

	vk_rtx_destroy_rt_output();

	Com_Memset( &rtx, 0, sizeof( rtx ) );
}

void vk_rtx_init( void )
{
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps;
	VkPhysicalDeviceProperties2 props2;
	VkDescriptorSetLayoutBinding bindings[2];
	VkDescriptorSetLayoutCreateInfo dslci;
	VkDescriptorPoolSize poolSizes[2];
	VkDescriptorPoolCreateInfo pci;
	VkPipelineLayoutCreateInfo plci;
	VkDescriptorSetAllocateInfo allocInfo;
	VkWriteDescriptorSetAccelerationStructureKHR asWrite;
	VkAccelerationStructureCreateInfoKHR asci;
	VkAccelerationStructureBuildGeometryInfoKHR buildInfoBLAS;
	VkAccelerationStructureGeometryKHR geometryBLAS;
	VkAccelerationStructureGeometryTrianglesDataKHR triangles;
	VkAccelerationStructureBuildRangeInfoKHR rangeBLAS;
	const VkAccelerationStructureBuildRangeInfoKHR *pRangeBLAS;
	VkAccelerationStructureBuildSizesInfoKHR sizeInfoBLAS;
	VkDeviceAddress vbAddr, ibAddr, scratchAddr, blasDeviceAddress, instAddr;
	VkDeviceSize scratchSize;
	uint32_t maxPrimBLAS;
	VkAccelerationStructureBuildGeometryInfoKHR buildInfoTLAS;
	VkAccelerationStructureGeometryKHR geometryTLAS;
	VkAccelerationStructureGeometryInstancesDataKHR instGeom;
	VkAccelerationStructureBuildRangeInfoKHR rangeTLAS;
	const VkAccelerationStructureBuildRangeInfoKHR *pRangeTLAS;
	VkAccelerationStructureBuildSizesInfoKHR sizeInfoTLAS;
	uint32_t maxInstTLAS;
	VkAccelerationStructureDeviceAddressInfoKHR addrInfo;
	VkAccelerationStructureInstanceKHR instance;
	VkDeviceSize sbtSize;
	uint32_t w, h;
	uint8_t *sbtHost;
	uint8_t *vertMap;
	uint8_t *idxMap;
	uint8_t *instMap;
	VkPipelineShaderStageCreateInfo stages[3];
	VkRayTracingShaderGroupCreateInfoKHR groups[3];
	VkRayTracingPipelineCreateInfoKHR rtpci;
	VkWriteDescriptorSet writes[1];
	VkResult pipeRes;
	VkCommandBuffer buildCmd;
	VkCommandBufferBeginInfo beginInfo;
	float vertices[9];
	uint16_t indices[3];
	size_t hbufSize;
	int gi;

	vk_rtx_shutdown();

	if ( !vk.rtxAvailable || !r_rtx || r_rtx->integer <= 0 ) {
		return;
	}

	if ( !r_rtxDemo || !r_rtxDemo->integer ) {
		ri.Printf( PRINT_DEVELOPER, "[VK][RTX] r_rtxDemo 0: skipping demo pipeline init\n" );
		return;
	}

	w = ( glConfig.vidWidth > 0 ) ? (uint32_t)glConfig.vidWidth : 1u;
	h = ( glConfig.vidHeight > 0 ) ? (uint32_t)glConfig.vidHeight : 1u;

	Com_Memset( &rtProps, 0, sizeof( rtProps ) );
	rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
	Com_Memset( &props2, 0, sizeof( props2 ) );
	props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	props2.pNext = &rtProps;
	if ( qvkGetPhysicalDeviceProperties2 ) {
		qvkGetPhysicalDeviceProperties2( vk.physical_device, &props2 );
	} else {
		rtProps.shaderGroupHandleSize = 32;
		rtProps.shaderGroupHandleAlignment = 32;
		rtProps.shaderGroupBaseAlignment = 64;
	}
	rtx.handle_size = rtProps.shaderGroupHandleSize;
	rtx.shader_group_base_alignment = rtProps.shaderGroupBaseAlignment;

	rtx.rgen = vk_rtx_shader_module( vk_rtx_demo_rgen_spv, VK_RTX_DEMO_RGEN_SPV_SIZE, "rtx_demo.rgen" );
	rtx.rmiss = vk_rtx_shader_module( vk_rtx_demo_rmiss_spv, VK_RTX_DEMO_RMISS_SPV_SIZE, "rtx_demo.rmiss" );
	rtx.rchit = vk_rtx_shader_module( vk_rtx_demo_rchit_spv, VK_RTX_DEMO_RCHIT_SPV_SIZE, "rtx_demo.rchit" );

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	Com_Memset( &dslci, 0, sizeof( dslci ) );
	dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dslci.bindingCount = 2;
	dslci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &dslci, NULL, &rtx.dsl ) );

	poolSizes[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	poolSizes[0].descriptorCount = 1;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSizes[1].descriptorCount = 1;
	Com_Memset( &pci, 0, sizeof( pci ) );
	pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pci.maxSets = 1;
	pci.poolSizeCount = 2;
	pci.pPoolSizes = poolSizes;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &pci, NULL, &rtx.pool ) );

	Com_Memset( &plci, 0, sizeof( plci ) );
	plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &rtx.dsl;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &rtx.pl ) );

	Com_Memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = rtx.pool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &rtx.dsl;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &rtx.descriptor_set ) );

	vk_rtx_create_rt_output( w, h, rtx.descriptor_set );

	vertices[0] = -1.0f; vertices[1] = -1.0f; vertices[2] = 0.0f;
	vertices[3] =  1.0f; vertices[4] = -1.0f; vertices[5] = 0.0f;
	vertices[6] =  0.0f; vertices[7] =  1.0f; vertices[8] = 0.0f;
	indices[0] = 0; indices[1] = 1; indices[2] = 2;

	vk_rtx_alloc_buffer( sizeof( vertices ),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&rtx.vertex_buffer, &rtx.vertex_memory, &vbAddr );
	VK_CHECK( qvkMapMemory( vk.device, rtx.vertex_memory, 0, sizeof( vertices ), 0, (void **)&vertMap ) );
	Com_Memcpy( vertMap, vertices, sizeof( vertices ) );
	qvkUnmapMemory( vk.device, rtx.vertex_memory );

	vk_rtx_alloc_buffer( sizeof( indices ),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&rtx.index_buffer, &rtx.index_memory, &ibAddr );
	VK_CHECK( qvkMapMemory( vk.device, rtx.index_memory, 0, sizeof( indices ), 0, (void **)&idxMap ) );
	Com_Memcpy( idxMap, indices, sizeof( indices ) );
	qvkUnmapMemory( vk.device, rtx.index_memory );

	Com_Memset( &triangles, 0, sizeof( triangles ) );
	triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	triangles.vertexData.deviceAddress = vbAddr;
	triangles.vertexStride = sizeof( float ) * 3;
	triangles.maxVertex = 2;
	triangles.indexType = VK_INDEX_TYPE_UINT16;
	triangles.indexData.deviceAddress = ibAddr;

	Com_Memset( &geometryBLAS, 0, sizeof( geometryBLAS ) );
	geometryBLAS.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometryBLAS.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometryBLAS.geometry.triangles = triangles;

	Com_Memset( &buildInfoBLAS, 0, sizeof( buildInfoBLAS ) );
	buildInfoBLAS.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfoBLAS.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	buildInfoBLAS.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfoBLAS.geometryCount = 1;
	buildInfoBLAS.pGeometries = &geometryBLAS;

	maxPrimBLAS = 1;
	Com_Memset( &sizeInfoBLAS, 0, sizeof( sizeInfoBLAS ) );
	sizeInfoBLAS.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	qvkGetAccelerationStructureBuildSizesKHR( vk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&buildInfoBLAS, &maxPrimBLAS, &sizeInfoBLAS );

	vk_rtx_alloc_buffer( sizeInfoBLAS.accelerationStructureSize,
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &rtx.blas_buffer, &rtx.blas_memory, NULL );

	Com_Memset( &asci, 0, sizeof( asci ) );
	asci.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	asci.buffer = rtx.blas_buffer;
	asci.offset = 0;
	asci.size = sizeInfoBLAS.accelerationStructureSize;
	asci.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	VK_CHECK( qvkCreateAccelerationStructureKHR( vk.device, &asci, NULL, &rtx.blas ) );

	Com_Memset( &addrInfo, 0, sizeof( addrInfo ) );
	addrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	addrInfo.accelerationStructure = rtx.blas;
	blasDeviceAddress = qvkGetAccelerationStructureDeviceAddressKHR( vk.device, &addrInfo );

	Com_Memset( &instance, 0, sizeof( instance ) );
	instance.transform.matrix[0][0] = 1.0f;
	instance.transform.matrix[1][1] = 1.0f;
	instance.transform.matrix[2][2] = 1.0f;
	instance.instanceCustomIndex = 0;
	instance.mask = 0xFF;
	instance.instanceShaderBindingTableRecordOffset = 0;
	instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
	instance.accelerationStructureReference = blasDeviceAddress;

	vk_rtx_alloc_buffer( sizeof( instance ),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&rtx.instance_buffer, &rtx.instance_memory, &instAddr );
	VK_CHECK( qvkMapMemory( vk.device, rtx.instance_memory, 0, sizeof( instance ), 0, (void **)&instMap ) );
	Com_Memcpy( instMap, &instance, sizeof( instance ) );
	qvkUnmapMemory( vk.device, rtx.instance_memory );

	Com_Memset( &instGeom, 0, sizeof( instGeom ) );
	instGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	instGeom.arrayOfPointers = VK_FALSE;
	instGeom.data.deviceAddress = instAddr;

	Com_Memset( &geometryTLAS, 0, sizeof( geometryTLAS ) );
	geometryTLAS.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometryTLAS.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geometryTLAS.geometry.instances = instGeom;

	Com_Memset( &buildInfoTLAS, 0, sizeof( buildInfoTLAS ) );
	buildInfoTLAS.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfoTLAS.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	buildInfoTLAS.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfoTLAS.geometryCount = 1;
	buildInfoTLAS.pGeometries = &geometryTLAS;

	maxInstTLAS = 1;
	Com_Memset( &sizeInfoTLAS, 0, sizeof( sizeInfoTLAS ) );
	sizeInfoTLAS.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	qvkGetAccelerationStructureBuildSizesKHR( vk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&buildInfoTLAS, &maxInstTLAS, &sizeInfoTLAS );

	scratchSize = sizeInfoBLAS.buildScratchSize;
	if ( sizeInfoTLAS.buildScratchSize > scratchSize ) {
		scratchSize = sizeInfoTLAS.buildScratchSize;
	}
	vk_rtx_alloc_buffer( scratchSize,
		VK_BUFFER_USAGE_STORAGE_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &rtx.scratch_buffer, &rtx.scratch_memory, &scratchAddr );

	Com_Memset( &rangeBLAS, 0, sizeof( rangeBLAS ) );
	rangeBLAS.primitiveCount = 1;
	pRangeBLAS = &rangeBLAS;
	buildInfoBLAS.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfoBLAS.dstAccelerationStructure = rtx.blas;
	buildInfoBLAS.scratchData.deviceAddress = scratchAddr;

	buildCmd = vk.tess[0].command_buffer;
	Com_Memset( &beginInfo, 0, sizeof( beginInfo ) );
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VK_CHECK( qvkBeginCommandBuffer( buildCmd, &beginInfo ) );
	qvkCmdBuildAccelerationStructuresKHR( buildCmd, 1, &buildInfoBLAS, &pRangeBLAS );
	VK_CHECK( qvkEndCommandBuffer( buildCmd ) );
	vk_rtx_submit_oneshot_build( buildCmd );

	vk_rtx_alloc_buffer( sizeInfoTLAS.accelerationStructureSize,
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &rtx.tlas_buffer, &rtx.tlas_memory, NULL );

	Com_Memset( &asci, 0, sizeof( asci ) );
	asci.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	asci.buffer = rtx.tlas_buffer;
	asci.offset = 0;
	asci.size = sizeInfoTLAS.accelerationStructureSize;
	asci.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	VK_CHECK( qvkCreateAccelerationStructureKHR( vk.device, &asci, NULL, &rtx.tlas ) );

	Com_Memset( &rangeTLAS, 0, sizeof( rangeTLAS ) );
	rangeTLAS.primitiveCount = 1;
	pRangeTLAS = &rangeTLAS;
	buildInfoTLAS.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfoTLAS.dstAccelerationStructure = rtx.tlas;
	buildInfoTLAS.scratchData.deviceAddress = scratchAddr;

	VK_CHECK( qvkBeginCommandBuffer( buildCmd, &beginInfo ) );
	qvkCmdBuildAccelerationStructuresKHR( buildCmd, 1, &buildInfoTLAS, &pRangeTLAS );
	VK_CHECK( qvkEndCommandBuffer( buildCmd ) );
	vk_rtx_submit_oneshot_build( buildCmd );

	Com_Memset( &asWrite, 0, sizeof( asWrite ) );
	asWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	asWrite.accelerationStructureCount = 1;
	asWrite.pAccelerationStructures = &rtx.tlas;

	Com_Memset( &writes[0], 0, sizeof( writes[0] ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = rtx.descriptor_set;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	writes[0].pNext = &asWrite;
	qvkUpdateDescriptorSets( vk.device, 1, &writes[0], 0, NULL );

	Com_Memset( stages, 0, sizeof( stages ) );
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	stages[0].module = rtx.rgen;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	stages[1].module = rtx.rmiss;
	stages[1].pName = "main";
	stages[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[2].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	stages[2].module = rtx.rchit;
	stages[2].pName = "main";

	Com_Memset( groups, 0, sizeof( groups ) );
	groups[0].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	groups[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	groups[0].generalShader = 0;
	groups[0].closestHitShader = VK_SHADER_UNUSED_KHR;
	groups[0].anyHitShader = VK_SHADER_UNUSED_KHR;
	groups[0].intersectionShader = VK_SHADER_UNUSED_KHR;

	groups[1].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	groups[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	groups[1].generalShader = 1;
	groups[1].closestHitShader = VK_SHADER_UNUSED_KHR;
	groups[1].anyHitShader = VK_SHADER_UNUSED_KHR;
	groups[1].intersectionShader = VK_SHADER_UNUSED_KHR;

	groups[2].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	groups[2].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	groups[2].generalShader = VK_SHADER_UNUSED_KHR;
	groups[2].closestHitShader = 2;
	groups[2].anyHitShader = VK_SHADER_UNUSED_KHR;
	groups[2].intersectionShader = VK_SHADER_UNUSED_KHR;

	Com_Memset( &rtpci, 0, sizeof( rtpci ) );
	rtpci.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	rtpci.stageCount = 3;
	rtpci.pStages = stages;
	rtpci.groupCount = 3;
	rtpci.pGroups = groups;
	rtpci.maxPipelineRayRecursionDepth = 1;
	rtpci.layout = rtx.pl;
	pipeRes = qvkCreateRayTracingPipelinesKHR( vk.device, VK_NULL_HANDLE, vk.pipelineCache, 1, &rtpci, NULL, &rtx.pipeline );
	if ( pipeRes != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, "[VK][RTX] vkCreateRayTracingPipelinesKHR failed (%s); demo disabled\n", vk_result_string( pipeRes ) );
		vk_rtx_shutdown();
		return;
	}
	SET_OBJECT_NAME( rtx.pipeline, "rtx_demo_pipeline", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	hbufSize = (size_t)rtx.shader_group_base_alignment * 3u;
	sbtSize = (VkDeviceSize)hbufSize;
	vk_rtx_alloc_buffer( sbtSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&rtx.sbt_buffer, &rtx.sbt_memory, NULL );
	VK_CHECK( qvkMapMemory( vk.device, rtx.sbt_memory, 0, sbtSize, 0, (void **)&sbtHost ) );
	Com_Memset( sbtHost, 0, hbufSize );
	{
		uint8_t packedHandles[96];
		if ( rtx.handle_size * 3u > sizeof( packedHandles ) ) {
			ri.Error( ERR_FATAL, "[VK][RTX] shader group handle size overflow" );
		}
		VK_CHECK( qvkGetRayTracingShaderGroupHandlesKHR( vk.device, rtx.pipeline, 0, 3, rtx.handle_size * 3u, packedHandles ) );
		for ( gi = 0; gi < 3; gi++ ) {
			Com_Memcpy( sbtHost + (size_t)rtx.shader_group_base_alignment * (size_t)gi,
				packedHandles + (size_t)rtx.handle_size * (size_t)gi,
				rtx.handle_size );
		}
	}
	qvkUnmapMemory( vk.device, rtx.sbt_memory );

	rtx.ready = qtrue;
	ri.Printf( PRINT_ALL, "[VK][RTX] Demo pipeline ready (r_rtx=%d); triangle trace composites into scene color\n", r_rtx->integer );
}

void vk_rtx_frame_begin( void )
{
	uint32_t w, h;

	if ( !rtx.ready || !r_rtxDemo || !r_rtxDemo->integer ) {
		return;
	}

	w = ( glConfig.vidWidth > 0 ) ? (uint32_t)glConfig.vidWidth : 1u;
	h = ( glConfig.vidHeight > 0 ) ? (uint32_t)glConfig.vidHeight : 1u;
	if ( w == rtx.width && h == rtx.height ) {
		return;
	}

	vk_rtx_create_rt_output( w, h, rtx.descriptor_set );
	ri.Printf( PRINT_ALL, "[VK][RTX] Resized RT output to %ux%u\n", w, h );
}

void vk_rtx_record_demo_pass( VkCommandBuffer cmd )
{
	VkBufferDeviceAddressInfo addr;
	VkDeviceAddress sbtBase;
	VkStridedDeviceAddressRegionKHR raygenRegion, missRegion, hitRegion, callableRegion;
	VkImageMemoryBarrier barriers[2];
	VkImageBlit blit;
	VkImageLayout colorOldLayout;
	VkImageLayout colorRestoreLayout;

	if ( !rtx.ready || !cmd || !r_rtxDemo || !r_rtxDemo->integer ) {
		return;
	}

	/* vk_end_render_pass_tracked runs *after* vkCmdEndRenderPass: FBO color is already in finalLayout
	 * (SHADER_READ_ONLY_OPTIMAL), not COLOR_ATTACHMENT_OPTIMAL. */
	colorOldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	if ( vk.renderPassIndex == RENDER_PASS_POST_BLOOM ) {
		colorRestoreLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	} else {
		/* Next pass (post_bloom) may expect COLOR_ATTACHMENT when RTX adjusted its load/initial layouts. */
		colorRestoreLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}

	Com_Memset( &addr, 0, sizeof( addr ) );
	addr.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addr.buffer = rtx.sbt_buffer;
	sbtBase = qvkGetBufferDeviceAddress( vk.device, &addr );
	Com_Memset( &raygenRegion, 0, sizeof( raygenRegion ) );
	Com_Memset( &missRegion, 0, sizeof( missRegion ) );
	Com_Memset( &hitRegion, 0, sizeof( hitRegion ) );
	Com_Memset( &callableRegion, 0, sizeof( callableRegion ) );
	raygenRegion.deviceAddress = sbtBase;
	raygenRegion.stride = rtx.shader_group_base_alignment;
	raygenRegion.size = rtx.shader_group_base_alignment;
	missRegion.deviceAddress = sbtBase + rtx.shader_group_base_alignment;
	missRegion.stride = rtx.shader_group_base_alignment;
	missRegion.size = rtx.shader_group_base_alignment;
	hitRegion.deviceAddress = sbtBase + 2u * rtx.shader_group_base_alignment;
	hitRegion.stride = rtx.shader_group_base_alignment;
	hitRegion.size = rtx.shader_group_base_alignment;

	Com_Memset( barriers, 0, sizeof( barriers ) );
	barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].image = rtx.rt_image;
	barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barriers[0].subresourceRange.levelCount = 1;
	barriers[0].subresourceRange.layerCount = 1;
	barriers[0].srcAccessMask = 0;
	barriers[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
		0, 0, NULL, 0, NULL, 1, barriers );

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtx.pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtx.pl, 0, 1, &rtx.descriptor_set, 0, NULL );
	qvkCmdTraceRaysKHR( cmd, &raygenRegion, &missRegion, &hitRegion, &callableRegion, rtx.width, rtx.height, 1 );

	barriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

	barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[1].image = vk.color_image;
	barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barriers[1].subresourceRange.levelCount = 1;
	barriers[1].subresourceRange.layerCount = 1;
	barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].oldLayout = colorOldLayout;
	barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 0, NULL, 0, NULL, 2, barriers );

	Com_Memset( &blit, 0, sizeof( blit ) );
	blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit.srcSubresource.mipLevel = 0;
	blit.srcSubresource.baseArrayLayer = 0;
	blit.srcSubresource.layerCount = 1;
	blit.dstSubresource = blit.srcSubresource;
	blit.srcOffsets[0].x = 0;
	blit.srcOffsets[0].y = 0;
	blit.srcOffsets[0].z = 0;
	blit.srcOffsets[1].x = (int32_t)rtx.width;
	blit.srcOffsets[1].y = (int32_t)rtx.height;
	blit.srcOffsets[1].z = 1;
	blit.dstOffsets[0] = blit.srcOffsets[0];
	blit.dstOffsets[1] = blit.srcOffsets[1];
	qvkCmdBlitImage( cmd, rtx.rt_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		vk.color_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR );

	barriers[0].image = rtx.rt_image;
	barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barriers[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

	barriers[1].image = vk.color_image;
	barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barriers[1].newLayout = colorRestoreLayout;
	barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	if ( colorRestoreLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
		barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	} else {
		barriers[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	}

	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
		( colorRestoreLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL )
			? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		0, 0, NULL, 0, NULL, 2, barriers );
}

#else /* !USE_VULKAN_RTX */

void vk_rtx_init( void ) {}
void vk_rtx_shutdown( void ) {}
void vk_rtx_frame_begin( void ) {}
void vk_rtx_record_demo_pass( VkCommandBuffer cmd ) { (void)cmd; }

#endif /* USE_VULKAN_RTX */
