/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

GRTX — Vulkan KHR ray tracing over 3D Gaussian AABB proxies (streamlined BLAS).
Requires USE_VULKAN_RTX at build time and r_grtx > 0 (latched) at runtime.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_grtx.h"
#include "vk_cmd.h"
#include "vk_staging.h"
#include "vk_util.h"
#include "vk_view_state.h"
#include "vk_image_layout.h"

#ifdef USE_VULKAN_RTX

#include "vk_grtx_spirv.inc"

#define GRTX_TRIS_PER_BOX       12u
#define GRTX_INDICES_PER_BOX    36u
#define GRTX_VERTS_PER_BOX      8u
#define GRTX_MAX_GAUSSIANS      4096

typedef struct {
	float position[3];
	float opacity;
	float scale[3];
	float sigmaScale;
	float rotation[4];
	float color[3];
	float pad;
} grtxGaussian_t;

typedef struct {
	float invViewProj[16];
	float viewOrigin[4];
	float zNearFar[4];
	float outputSize[4];
	float traceParams[4];
} VkGrtxFrameUBO_t;

static const float s_box_verts[8][3] = {
	{ 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 },
	{ 0, 0, 1 }, { 1, 0, 1 }, { 0, 1, 1 }, { 1, 1, 1 }
};

static const uint16_t s_box_indices[36] = {
	0, 2, 1, 1, 2, 3, 4, 5, 6, 5, 7, 6, 0, 1, 4, 1, 5, 4,
	2, 6, 3, 3, 6, 7, 0, 4, 2, 2, 4, 6, 1, 3, 5, 3, 7, 5
};

static cvar_t *r_grtx;
static cvar_t *r_grtxDemo;
static cvar_t *r_grtxMaxGaussians;
static cvar_t *r_grtxComposite;
static cvar_t *r_grtxSamples;
static cvar_t *r_grtxSigmaScale;
static cvar_t *r_grtx_debug;

static struct {
	qboolean		ready;
	uint32_t		width;
	uint32_t		height;
	uint32_t		handle_size;
	uint32_t		shader_group_base_alignment;
	uint32_t		gaussian_count;
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
	VkBuffer		gaussian_buffer;
	VkDeviceMemory		gaussian_memory;
	VkImage			rt_image;
	VkDeviceMemory		rt_image_memory;
	VkImageView		rt_image_view;
	VkBuffer		grtx_ubo;
	VkDeviceMemory		grtx_ubo_memory;
	void			*grtx_ubo_ptr;
	qboolean		scene_valid;
	qboolean		rt_image_traced;
	char			map_name[MAX_QPATH];
} grtx;

static VkShaderModule vk_grtx_shader_module( const uint8_t *code, uint32_t codeSize, const char *name )
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

static void vk_grtx_alloc_buffer( VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memProps,
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

static void vk_grtx_destroy_buffer( VkBuffer *buf, VkDeviceMemory *mem )
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

static void vk_grtx_destroy_as( VkAccelerationStructureKHR *as )
{
	if ( *as != VK_NULL_HANDLE ) {
		qvkDestroyAccelerationStructureKHR( vk.device, *as, NULL );
		*as = VK_NULL_HANDLE;
	}
}

static void vk_grtx_fill_procedural_gaussians( grtxGaussian_t *out, uint32_t count, float sigmaScale )
{
	uint32_t i;
	float origin[3];

	if ( tr.world ) {
		origin[0] = tr.world->lightGridOrigin[0] + tr.world->lightGridSize[0] * tr.world->lightGridBounds[0] * 0.5f;
		origin[1] = tr.world->lightGridOrigin[1] + tr.world->lightGridSize[1] * tr.world->lightGridBounds[1] * 0.5f;
		origin[2] = tr.world->lightGridOrigin[2] + 128.0f;
	} else {
		VectorCopy( backEnd.viewParms.or.origin, origin );
	}

	for ( i = 0; i < count; i++ ) {
		float u = (float)( i % 16 ) / 16.0f;
		float v = (float)( ( i / 16 ) % 16 ) / 16.0f;
		float w = (float)( i / 256 ) / 16.0f;
		grtxGaussian_t *g = &out[i];

		g->position[0] = origin[0] + ( u - 0.5f ) * 512.0f;
		g->position[1] = origin[1] + ( v - 0.5f ) * 512.0f;
		g->position[2] = origin[2] + w * 192.0f;
		g->scale[0] = 8.0f + (float)( i % 5 );
		g->scale[1] = 6.0f + (float)( ( i + 2 ) % 5 );
		g->scale[2] = 10.0f + (float)( ( i + 4 ) % 5 );
		g->sigmaScale = sigmaScale;
		g->rotation[0] = 0.0f;
		g->rotation[1] = 0.0f;
		g->rotation[2] = 0.0f;
		g->rotation[3] = 1.0f;
		g->opacity = 0.65f + 0.35f * ( (float)( i % 7 ) / 7.0f );
		g->color[0] = 0.45f + 0.5f * sinf( (float)i * 0.31f );
		g->color[1] = 0.35f + 0.5f * cosf( (float)i * 0.27f );
		g->color[2] = 0.55f + 0.4f * sinf( (float)i * 0.19f );
		g->pad = 0.0f;
	}
}

static uint32_t vk_grtx_pack_boxes( const grtxGaussian_t *gaussians, uint32_t count,
	float *positions, uint32_t *indices )
{
	uint32_t g, v, t;
	float sigma;

	for ( g = 0; g < count; g++ ) {
		const grtxGaussian_t *gp = &gaussians[g];
		uint32_t vertBase = g * GRTX_VERTS_PER_BOX;
		float hx = gp->scale[0] * gp->sigmaScale;
		float hy = gp->scale[1] * gp->sigmaScale;
		float hz = gp->scale[2] * gp->sigmaScale;

		for ( v = 0; v < GRTX_VERTS_PER_BOX; v++ ) {
			float *dst = positions + ( vertBase + v ) * 3u;
			dst[0] = gp->position[0] + ( s_box_verts[v][0] - 0.5f ) * hx * 2.0f;
			dst[1] = gp->position[1] + ( s_box_verts[v][1] - 0.5f ) * hy * 2.0f;
			dst[2] = gp->position[2] + ( s_box_verts[v][2] - 0.5f ) * hz * 2.0f;
		}
		for ( t = 0; t < GRTX_INDICES_PER_BOX; t++ ) {
			indices[g * GRTX_INDICES_PER_BOX + t] = vertBase + (uint32_t)s_box_indices[t];
		}
	}
	(void)sigma;
	return count;
}

static void vk_grtx_rebuild_scene( void )
{
	VkAccelerationStructureCreateInfoKHR asci;
	VkAccelerationStructureBuildGeometryInfoKHR buildInfoBLAS;
	VkAccelerationStructureGeometryKHR geometryBLAS;
	VkAccelerationStructureGeometryTrianglesDataKHR triangles;
	VkAccelerationStructureBuildRangeInfoKHR rangeBLAS;
	const VkAccelerationStructureBuildRangeInfoKHR *pRangeBLAS;
	VkAccelerationStructureBuildSizesInfoKHR sizeInfoBLAS;
	VkAccelerationStructureBuildGeometryInfoKHR buildInfoTLAS;
	VkAccelerationStructureGeometryKHR geometryTLAS;
	VkAccelerationStructureGeometryInstancesDataKHR instGeom;
	VkAccelerationStructureBuildRangeInfoKHR rangeTLAS;
	const VkAccelerationStructureBuildRangeInfoKHR *pRangeTLAS;
	VkAccelerationStructureBuildSizesInfoKHR sizeInfoTLAS;
	VkAccelerationStructureDeviceAddressInfoKHR addrInfo;
	VkAccelerationStructureInstanceKHR instance;
	VkDeviceAddress vbAddr, ibAddr, scratchAddr, blasDeviceAddress, instAddr;
	VkDeviceSize scratchSize;
	uint32_t maxGaussians;
	uint32_t count;
	grtxGaussian_t *gaussians;
	float *posHost;
	uint32_t *idxHost;
	VkCommandBuffer buildCmd;
	VkWriteDescriptorSetAccelerationStructureKHR asWrite;
	VkWriteDescriptorSet writeAS;
	VkDescriptorBufferInfo gaussInfo;
	VkWriteDescriptorSet writeGauss;

	if ( !grtx.descriptor_set ) {
		return;
	}

	maxGaussians = ( r_grtxMaxGaussians && r_grtxMaxGaussians->integer > 0 ) ?
		(uint32_t)r_grtxMaxGaussians->integer : 256u;
	if ( maxGaussians > GRTX_MAX_GAUSSIANS ) {
		maxGaussians = GRTX_MAX_GAUSSIANS;
	}
	count = maxGaussians;

	gaussians = (grtxGaussian_t *)ri.Malloc( (size_t)count * sizeof( grtxGaussian_t ) );
	vk_grtx_fill_procedural_gaussians( gaussians, count,
		r_grtxSigmaScale ? r_grtxSigmaScale->value : 3.0f );

	posHost = (float *)ri.Malloc( (size_t)count * GRTX_VERTS_PER_BOX * 3u * sizeof( float ) );
	idxHost = (uint32_t *)ri.Malloc( (size_t)count * GRTX_INDICES_PER_BOX * sizeof( uint32_t ) );
	vk_grtx_pack_boxes( gaussians, count, posHost, idxHost );
	grtx.gaussian_count = count;

	vk_grtx_destroy_as( &grtx.tlas );
	vk_grtx_destroy_as( &grtx.blas );
	vk_grtx_destroy_buffer( &grtx.tlas_buffer, &grtx.tlas_memory );
	vk_grtx_destroy_buffer( &grtx.instance_buffer, &grtx.instance_memory );
	vk_grtx_destroy_buffer( &grtx.blas_buffer, &grtx.blas_memory );
	vk_grtx_destroy_buffer( &grtx.vertex_buffer, &grtx.vertex_memory );
	vk_grtx_destroy_buffer( &grtx.index_buffer, &grtx.index_memory );
	vk_grtx_destroy_buffer( &grtx.scratch_buffer, &grtx.scratch_memory );

	vk_grtx_alloc_buffer( (VkDeviceSize)count * GRTX_VERTS_PER_BOX * 3u * sizeof( float ),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &grtx.vertex_buffer, &grtx.vertex_memory, &vbAddr );
	vk_grtx_alloc_buffer( (VkDeviceSize)count * GRTX_INDICES_PER_BOX * sizeof( uint32_t ),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &grtx.index_buffer, &grtx.index_memory, &ibAddr );

	vk_grtx_alloc_buffer( (VkDeviceSize)count * sizeof( grtxGaussian_t ),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &grtx.gaussian_buffer, &grtx.gaussian_memory, NULL );

	if ( vk.staging_buffer.size < (VkDeviceSize)count * GRTX_VERTS_PER_BOX * 3u * sizeof( float ) ) {
		vk_alloc_staging_buffer( (VkDeviceSize)count * GRTX_VERTS_PER_BOX * 3u * sizeof( float ) );
	}
	if ( vk.staging_buffer.ptr ) {
		Com_Memcpy( vk.staging_buffer.ptr, posHost, (size_t)count * GRTX_VERTS_PER_BOX * 3u * sizeof( float ) );
	}
	buildCmd = vk_begin_command_buffer();
	{
		VkBufferMemoryBarrier stagingBarrier;
		Com_Memset( &stagingBarrier, 0, sizeof( stagingBarrier ) );
		stagingBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		stagingBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
		stagingBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		stagingBarrier.buffer = vk.staging_buffer.handle;
		stagingBarrier.size = VK_WHOLE_SIZE;
		qvkCmdPipelineBarrier( buildCmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, NULL, 1, &stagingBarrier, 0, NULL );
	}
	{
		VkBufferCopy copy;
		Com_Memset( &copy, 0, sizeof( copy ) );
		copy.size = (VkDeviceSize)count * GRTX_VERTS_PER_BOX * 3u * sizeof( float );
		qvkCmdCopyBuffer( buildCmd, vk.staging_buffer.handle, grtx.vertex_buffer, 1, &copy );
	}
	if ( vk.staging_buffer.ptr ) {
		Com_Memcpy( vk.staging_buffer.ptr, idxHost, (size_t)count * GRTX_INDICES_PER_BOX * sizeof( uint32_t ) );
	}
	{
		VkBufferCopy copy;
		Com_Memset( &copy, 0, sizeof( copy ) );
		copy.size = (VkDeviceSize)count * GRTX_INDICES_PER_BOX * sizeof( uint32_t );
		qvkCmdCopyBuffer( buildCmd, vk.staging_buffer.handle, grtx.index_buffer, 1, &copy );
	}
	if ( vk.staging_buffer.ptr ) {
		Com_Memcpy( vk.staging_buffer.ptr, gaussians, (size_t)count * sizeof( grtxGaussian_t ) );
	}
	{
		VkBufferCopy copy;
		Com_Memset( &copy, 0, sizeof( copy ) );
		copy.size = (VkDeviceSize)count * sizeof( grtxGaussian_t );
		qvkCmdCopyBuffer( buildCmd, vk.staging_buffer.handle, grtx.gaussian_buffer, 1, &copy );
	}
	vk_end_command_buffer( buildCmd, __func__ );

	ri.Free( posHost );
	ri.Free( idxHost );
	ri.Free( gaussians );

	Com_Memset( &triangles, 0, sizeof( triangles ) );
	triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	triangles.vertexData.deviceAddress = vbAddr;
	triangles.vertexStride = sizeof( float ) * 3u;
	triangles.maxVertex = count * GRTX_VERTS_PER_BOX - 1u;
	triangles.indexType = VK_INDEX_TYPE_UINT32;
	triangles.indexData.deviceAddress = ibAddr;
	triangles.transformData.deviceAddress = 0;

	Com_Memset( &geometryBLAS, 0, sizeof( geometryBLAS ) );
	geometryBLAS.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometryBLAS.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometryBLAS.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	geometryBLAS.geometry.triangles = triangles;

	Com_Memset( &buildInfoBLAS, 0, sizeof( buildInfoBLAS ) );
	buildInfoBLAS.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfoBLAS.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	buildInfoBLAS.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfoBLAS.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfoBLAS.dstAccelerationStructure = VK_NULL_HANDLE;
	buildInfoBLAS.geometryCount = 1;
	buildInfoBLAS.pGeometries = &geometryBLAS;

	rangeBLAS.primitiveCount = count * GRTX_TRIS_PER_BOX;
	rangeBLAS.primitiveOffset = 0;
	rangeBLAS.firstVertex = 0;
	rangeBLAS.transformOffset = 0;
	pRangeBLAS = &rangeBLAS;

	Com_Memset( &sizeInfoBLAS, 0, sizeof( sizeInfoBLAS ) );
	sizeInfoBLAS.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	qvkGetAccelerationStructureBuildSizesKHR( vk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&buildInfoBLAS, &rangeBLAS.primitiveCount, &sizeInfoBLAS );

	vk_grtx_alloc_buffer( sizeInfoBLAS.accelerationStructureSize,
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &grtx.blas_buffer, &grtx.blas_memory, NULL );

	Com_Memset( &asci, 0, sizeof( asci ) );
	asci.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	asci.buffer = grtx.blas_buffer;
	asci.size = sizeInfoBLAS.accelerationStructureSize;
	asci.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	VK_CHECK( qvkCreateAccelerationStructureKHR( vk.device, &asci, NULL, &grtx.blas ) );

	buildInfoBLAS.dstAccelerationStructure = grtx.blas;
	scratchSize = sizeInfoBLAS.buildScratchSize;
	vk_grtx_alloc_buffer( scratchSize,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &grtx.scratch_buffer, &grtx.scratch_memory, &scratchAddr );
	buildInfoBLAS.scratchData.deviceAddress = scratchAddr;

	Com_Memset( &addrInfo, 0, sizeof( addrInfo ) );
	addrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	addrInfo.accelerationStructure = grtx.blas;
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

	vk_grtx_alloc_buffer( sizeof( instance ),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &grtx.instance_buffer, &grtx.instance_memory, &instAddr );

	Com_Memset( &instGeom, 0, sizeof( instGeom ) );
	instGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	instGeom.data.deviceAddress = instAddr;

	Com_Memset( &geometryTLAS, 0, sizeof( geometryTLAS ) );
	geometryTLAS.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometryTLAS.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geometryTLAS.geometry.instances = instGeom;

	Com_Memset( &buildInfoTLAS, 0, sizeof( buildInfoTLAS ) );
	buildInfoTLAS.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfoTLAS.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	buildInfoTLAS.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfoTLAS.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfoTLAS.geometryCount = 1;
	buildInfoTLAS.pGeometries = &geometryTLAS;

	rangeTLAS.primitiveCount = 1;
	rangeTLAS.primitiveOffset = 0;
	rangeTLAS.firstVertex = 0;
	rangeTLAS.transformOffset = 0;
	pRangeTLAS = &rangeTLAS;

	Com_Memset( &sizeInfoTLAS, 0, sizeof( sizeInfoTLAS ) );
	sizeInfoTLAS.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	qvkGetAccelerationStructureBuildSizesKHR( vk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&buildInfoTLAS, &rangeTLAS.primitiveCount, &sizeInfoTLAS );

	vk_grtx_alloc_buffer( sizeInfoTLAS.accelerationStructureSize,
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &grtx.tlas_buffer, &grtx.tlas_memory, NULL );

	asci.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	asci.buffer = grtx.tlas_buffer;
	asci.size = sizeInfoTLAS.accelerationStructureSize;
	VK_CHECK( qvkCreateAccelerationStructureKHR( vk.device, &asci, NULL, &grtx.tlas ) );

	buildInfoTLAS.dstAccelerationStructure = grtx.tlas;
	if ( sizeInfoTLAS.buildScratchSize > scratchSize ) {
		vk_grtx_destroy_buffer( &grtx.scratch_buffer, &grtx.scratch_memory );
		vk_grtx_alloc_buffer( sizeInfoTLAS.buildScratchSize,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &grtx.scratch_buffer, &grtx.scratch_memory, &scratchAddr );
	}
	buildInfoTLAS.scratchData.deviceAddress = scratchAddr;

	buildCmd = vk_begin_command_buffer();

	if ( vk.staging_buffer.ptr ) {
		Com_Memcpy( vk.staging_buffer.ptr, &instance, sizeof( instance ) );
	}
	{
		VkBufferCopy copy;
		Com_Memset( &copy, 0, sizeof( copy ) );
		copy.size = sizeof( instance );
		qvkCmdCopyBuffer( buildCmd, vk.staging_buffer.handle, grtx.instance_buffer, 1, &copy );
	}

	qvkCmdBuildAccelerationStructuresKHR( buildCmd, 1, &buildInfoBLAS, &pRangeBLAS );
	qvkCmdBuildAccelerationStructuresKHR( buildCmd, 1, &buildInfoTLAS, &pRangeTLAS );
	vk_end_command_buffer( buildCmd, "vk_grtx_rebuild_scene" );

	Com_Memset( &asWrite, 0, sizeof( asWrite ) );
	asWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	asWrite.accelerationStructureCount = 1;
	asWrite.pAccelerationStructures = &grtx.tlas;

	Com_Memset( &writeAS, 0, sizeof( writeAS ) );
	writeAS.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeAS.dstSet = grtx.descriptor_set;
	writeAS.dstBinding = 0;
	writeAS.descriptorCount = 1;
	writeAS.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	writeAS.pNext = &asWrite;

	Com_Memset( &gaussInfo, 0, sizeof( gaussInfo ) );
	gaussInfo.buffer = grtx.gaussian_buffer;
	gaussInfo.offset = 0;
	gaussInfo.range = (VkDeviceSize)count * sizeof( grtxGaussian_t );

	Com_Memset( &writeGauss, 0, sizeof( writeGauss ) );
	writeGauss.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeGauss.dstSet = grtx.descriptor_set;
	writeGauss.dstBinding = 5;
	writeGauss.descriptorCount = 1;
	writeGauss.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writeGauss.pBufferInfo = &gaussInfo;

	qvkUpdateDescriptorSets( vk.device, 1, &writeAS, 0, NULL );
	qvkUpdateDescriptorSets( vk.device, 1, &writeGauss, 0, NULL );

	grtx.scene_valid = qtrue;
}

static void vk_grtx_get_trace_extent( uint32_t *w, uint32_t *h )
{
	*w = vk_get_render_target_width();
	*h = vk_get_render_target_height();
	if ( *w == 0u ) {
		*w = 1u;
	}
	if ( *h == 0u ) {
		*h = 1u;
	}
}

static void vk_grtx_create_rt_output( uint32_t w, uint32_t h, VkDescriptorSet descriptor_set )
{
	VkImageCreateInfo ici;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo ai;
	VkImageViewCreateInfo vci;
	VkDescriptorImageInfo imgInfo;
	VkWriteDescriptorSet write;

	if ( grtx.rt_image != VK_NULL_HANDLE && grtx.width == w && grtx.height == h ) {
		return;
	}

	if ( grtx.rt_image_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, grtx.rt_image_view, NULL );
		grtx.rt_image_view = VK_NULL_HANDLE;
	}
	if ( grtx.rt_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, grtx.rt_image, NULL );
		grtx.rt_image = VK_NULL_HANDLE;
	}
	if ( grtx.rt_image_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, grtx.rt_image_memory, NULL );
		grtx.rt_image_memory = VK_NULL_HANDLE;
	}

	Com_Memset( &ici, 0, sizeof( ici ) );
	ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ici.imageType = VK_IMAGE_TYPE_2D;
	ici.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	ici.extent.width = w;
	ici.extent.height = h;
	ici.extent.depth = 1;
	ici.mipLevels = 1;
	ici.arrayLayers = 1;
	ici.samples = VK_SAMPLE_COUNT_1_BIT;
	ici.tiling = VK_IMAGE_TILING_OPTIMAL;
	ici.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK( qvkCreateImage( vk.device, &ici, NULL, &grtx.rt_image ) );

	qvkGetImageMemoryRequirements( vk.device, grtx.rt_image, &req );
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, &grtx.rt_image_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, grtx.rt_image, grtx.rt_image_memory, 0 ) );

	Com_Memset( &vci, 0, sizeof( vci ) );
	vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	vci.image = grtx.rt_image;
	vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
	vci.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vci.subresourceRange.levelCount = 1;
	vci.subresourceRange.layerCount = 1;
	VK_CHECK( qvkCreateImageView( vk.device, &vci, NULL, &grtx.rt_image_view ) );

	Com_Memset( &imgInfo, 0, sizeof( imgInfo ) );
	imgInfo.imageView = grtx.rt_image_view;
	imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	Com_Memset( &write, 0, sizeof( write ) );
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = descriptor_set;
	write.dstBinding = 1;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	write.pImageInfo = &imgInfo;
	qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );

	grtx.width = w;
	grtx.height = h;
	grtx.rt_image_traced = qfalse;
}

static void vk_grtx_update_color_descriptor( void )
{
	VkDescriptorImageInfo colorInfo;
	VkWriteDescriptorSet write;
	Vk_Sampler_Def sd;

	if ( grtx.descriptor_set == VK_NULL_HANDLE || vk.color_image_view == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.max_lod_1_0 = qtrue;
	sd.noAnisotropy = qtrue;

	Com_Memset( &colorInfo, 0, sizeof( colorInfo ) );
	colorInfo.sampler = vk_find_sampler( &sd );
	colorInfo.imageView = vk.color_image_view;
	colorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	Com_Memset( &write, 0, sizeof( write ) );
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = grtx.descriptor_set;
	write.dstBinding = 4;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &colorInfo;
	qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
}

static void GRTX_Cmd_Status( void )
{
	ri.Printf( PRINT_ALL,
		"[GRTX] active=%d ready=%d scene=%d gaussians=%u map='%s' extent=%ux%u\n",
		vk_grtx_active() ? 1 : 0,
		grtx.ready ? 1 : 0,
		grtx.scene_valid ? 1 : 0,
		grtx.gaussian_count,
		grtx.map_name[0] ? grtx.map_name : "(none)",
		grtx.width, grtx.height );
}

void R_GRTX_Init( void )
{
	r_grtx = ri.Cvar_Get( "r_grtx", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_grtxDemo = ri.Cvar_Get( "r_grtxDemo", "1", CVAR_ARCHIVE_ND );
	r_grtxMaxGaussians = ri.Cvar_Get( "r_grtxMaxGaussians", "256", CVAR_ARCHIVE_ND );
	r_grtxComposite = ri.Cvar_Get( "r_grtxComposite", "0.35", CVAR_ARCHIVE_ND );
	r_grtxSamples = ri.Cvar_Get( "r_grtxSamples", "1", CVAR_ARCHIVE_ND );
	r_grtxSigmaScale = ri.Cvar_Get( "r_grtxSigmaScale", "3", CVAR_ARCHIVE_ND );
	r_grtx_debug = ri.Cvar_Get( "r_grtx_debug", "0", CVAR_ARCHIVE_ND );

	ri.Cvar_CheckRange( r_grtx, "0", "3", CV_INTEGER );
	ri.Cvar_CheckRange( r_grtxDemo, "0", "1", CV_INTEGER );
	ri.Cvar_CheckRange( r_grtxMaxGaussians, "1", "4096", CV_INTEGER );
	ri.Cvar_CheckRange( r_grtxComposite, "0", "1", CV_FLOAT );
	ri.Cvar_CheckRange( r_grtxSamples, "1", "8", CV_INTEGER );
	ri.Cvar_CheckRange( r_grtxSigmaScale, "0.5", "16", CV_FLOAT );
	ri.Cvar_SetDescription( r_grtx,
		"Gaussian ray tracing (GRTX): 0=off, 1–3=visualization modes over 3D Gaussian AABB proxies. "
		"Requires USE_VULKAN_RTX build and vid_restart after change." );
	ri.Cvar_SetDescription( r_grtxDemo,
		"When r_grtx>0: 1=trace procedural Gaussian BLAS each frame; 0=RT extensions only." );
	ri.Cvar_SetDescription( r_grtxMaxGaussians,
		"Max procedural Gaussians in the demo BLAS (capped at 4096)." );
	ri.Cvar_SetDescription( r_grtxComposite,
		"Blend weight for raster HDR color into the GRTX trace result (0=RT only)." );
	ri.Cvar_SetDescription( r_grtxSamples,
		"Primary rays per pixel in grtx_trace.rgen (1–8)." );
	ri.Cvar_SetDescription( r_grtxSigmaScale,
		"Scale factor for AABB proxy size from Gaussian scale." );

	ri.Cmd_AddCommand( "grtx_status", GRTX_Cmd_Status );

	if ( r_grtx->integer > 0 ) {
		ri.Printf( PRINT_ALL,
			"[GRTX] Gaussian ray tracing enabled (experimental). See docs/GAUSSIAN_RAY_TRACING_GRTX.md\n" );
	}
}

void R_GRTX_Shutdown( void )
{
	ri.Cmd_RemoveCommand( "grtx_status" );
}

void vk_grtx_shutdown( void )
{
	if ( grtx.rt_image_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, grtx.rt_image_view, NULL );
	}
	if ( grtx.rt_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, grtx.rt_image, NULL );
	}
	if ( grtx.rt_image_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, grtx.rt_image_memory, NULL );
	}
	if ( grtx.pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, grtx.pipeline, NULL );
	}
	if ( grtx.pl != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, grtx.pl, NULL );
	}
	if ( grtx.dsl != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, grtx.dsl, NULL );
	}
	if ( grtx.pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, grtx.pool, NULL );
	}
	if ( grtx.rgen != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, grtx.rgen, NULL );
	}
	if ( grtx.rmiss != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, grtx.rmiss, NULL );
	}
	if ( grtx.rchit != VK_NULL_HANDLE ) {
		qvkDestroyShaderModule( vk.device, grtx.rchit, NULL );
	}
	vk_grtx_destroy_as( &grtx.tlas );
	vk_grtx_destroy_as( &grtx.blas );
	vk_grtx_destroy_buffer( &grtx.tlas_buffer, &grtx.tlas_memory );
	vk_grtx_destroy_buffer( &grtx.instance_buffer, &grtx.instance_memory );
	vk_grtx_destroy_buffer( &grtx.blas_buffer, &grtx.blas_memory );
	vk_grtx_destroy_buffer( &grtx.vertex_buffer, &grtx.vertex_memory );
	vk_grtx_destroy_buffer( &grtx.index_buffer, &grtx.index_memory );
	vk_grtx_destroy_buffer( &grtx.gaussian_buffer, &grtx.gaussian_memory );
	vk_grtx_destroy_buffer( &grtx.sbt_buffer, &grtx.sbt_memory );
	vk_grtx_destroy_buffer( &grtx.scratch_buffer, &grtx.scratch_memory );
	vk_grtx_destroy_buffer( &grtx.grtx_ubo, &grtx.grtx_ubo_memory );
	Com_Memset( &grtx, 0, sizeof( grtx ) );
}

void vk_grtx_init( void )
{
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps;
	VkPhysicalDeviceProperties2 props2;
	VkDescriptorSetLayoutBinding bindings[6];
	VkDescriptorSetLayoutCreateInfo dslci;
	VkDescriptorPoolSize poolSizes[5];
	VkDescriptorPoolCreateInfo pci;
	VkPipelineLayoutCreateInfo plci;
	VkDescriptorSetAllocateInfo allocInfo;
	VkPipelineShaderStageCreateInfo stages[3];
	VkRayTracingShaderGroupCreateInfoKHR groups[3];
	VkRayTracingPipelineCreateInfoKHR rtpci;
	VkWriteDescriptorSet writes[2];
	VkDescriptorBufferInfo uboInfo;
	VkDescriptorImageInfo depthInfo;
	VkMemoryRequirements uboReq;
	VkMemoryAllocateInfo uboAi;
	VkBufferCreateInfo uboBi;
	Vk_Sampler_Def sd;
	uint32_t uboMemType;
	VkDeviceSize uboAllocSize;
	VkDeviceSize sbtSize;
	uint8_t *sbtHost;
	size_t hbufSize;
	int gi;
	VkResult pipeRes;
	uint32_t w, h;

	vk_grtx_shutdown();

	if ( !vk.rtxAvailable || !r_grtx || r_grtx->integer <= 0 ) {
		return;
	}
	if ( !r_grtxDemo || !r_grtxDemo->integer ) {
		ri.Printf( PRINT_DEVELOPER, "[GRTX] r_grtxDemo 0: extensions only, no Gaussian trace pipeline\n" );
		return;
	}

	vk_grtx_get_trace_extent( &w, &h );

	Com_Memset( &rtProps, 0, sizeof( rtProps ) );
	rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
	Com_Memset( &props2, 0, sizeof( props2 ) );
	props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	props2.pNext = &rtProps;
	if ( qvkGetPhysicalDeviceProperties2 ) {
		qvkGetPhysicalDeviceProperties2( vk.physical_device, &props2 );
	}
	grtx.handle_size = rtProps.shaderGroupHandleSize ? rtProps.shaderGroupHandleSize : 32u;
	grtx.shader_group_base_alignment = rtProps.shaderGroupBaseAlignment ? rtProps.shaderGroupBaseAlignment : 64u;

	grtx.rgen = vk_grtx_shader_module( vk_grtx_trace_rgen_spv, VK_GRTX_TRACE_RGEN_SPV_SIZE, "grtx_trace.rgen" );
	grtx.rmiss = vk_grtx_shader_module( vk_grtx_miss_rmiss_spv, VK_GRTX_MISS_RMISS_SPV_SIZE, "grtx_miss.rmiss" );
	grtx.rchit = vk_grtx_shader_module( vk_grtx_gaussian_rchit_spv, VK_GRTX_GAUSSIAN_RCHIT_SPV_SIZE, "grtx_gaussian.rchit" );

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	bindings[3].binding = 3;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[3].descriptorCount = 1;
	bindings[3].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	bindings[4].binding = 4;
	bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[4].descriptorCount = 1;
	bindings[4].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	bindings[5].binding = 5;
	bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[5].descriptorCount = 1;
	bindings[5].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

	Com_Memset( &dslci, 0, sizeof( dslci ) );
	dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dslci.bindingCount = 6;
	dslci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &dslci, NULL, &grtx.dsl ) );

	poolSizes[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	poolSizes[0].descriptorCount = 1;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSizes[1].descriptorCount = 1;
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[2].descriptorCount = 1;
	poolSizes[3].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[3].descriptorCount = 2;
	poolSizes[4].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[4].descriptorCount = 1;
	Com_Memset( &pci, 0, sizeof( pci ) );
	pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pci.maxSets = 1;
	pci.poolSizeCount = 5;
	pci.pPoolSizes = poolSizes;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &pci, NULL, &grtx.pool ) );

	Com_Memset( &plci, 0, sizeof( plci ) );
	plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &grtx.dsl;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &grtx.pl ) );

	Com_Memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = grtx.pool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &grtx.dsl;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &grtx.descriptor_set ) );

	vk_grtx_create_rt_output( w, h, grtx.descriptor_set );

	uboAllocSize = (VkDeviceSize)PAD( (uint32_t)sizeof( VkGrtxFrameUBO_t ), (uint32_t)vk.uniform_alignment );
	Com_Memset( &uboBi, 0, sizeof( uboBi ) );
	uboBi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	uboBi.size = uboAllocSize;
	uboBi.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &uboBi, NULL, &grtx.grtx_ubo ) );
	qvkGetBufferMemoryRequirements( vk.device, grtx.grtx_ubo, &uboReq );
	uboMemType = vk_find_memory_type( vk.physical_device, uboReq.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	Com_Memset( &uboAi, 0, sizeof( uboAi ) );
	uboAi.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	uboAi.allocationSize = uboReq.size;
	uboAi.memoryTypeIndex = uboMemType;
	VK_CHECK( qvkAllocateMemory( vk.device, &uboAi, NULL, &grtx.grtx_ubo_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, grtx.grtx_ubo, grtx.grtx_ubo_memory, 0 ) );
	VK_CHECK( qvkMapMemory( vk.device, grtx.grtx_ubo_memory, 0, uboAllocSize, 0, &grtx.grtx_ubo_ptr ) );

	Com_Memset( &uboInfo, 0, sizeof( uboInfo ) );
	uboInfo.buffer = grtx.grtx_ubo;
	uboInfo.offset = 0;
	uboInfo.range = uboAllocSize;

	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
	sd.max_lod_1_0 = qtrue;
	sd.noAnisotropy = qtrue;
	Com_Memset( &depthInfo, 0, sizeof( depthInfo ) );
	depthInfo.sampler = vk_find_sampler( &sd );
	depthInfo.imageView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = grtx.descriptor_set;
	writes[0].dstBinding = 2;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writes[0].pBufferInfo = &uboInfo;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = grtx.descriptor_set;
	writes[1].dstBinding = 3;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].pImageInfo = &depthInfo;
	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );
	vk_grtx_update_color_descriptor();

	Com_Memset( stages, 0, sizeof( stages ) );
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	stages[0].module = grtx.rgen;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	stages[1].module = grtx.rmiss;
	stages[1].pName = "main";
	stages[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[2].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	stages[2].module = grtx.rchit;
	stages[2].pName = "main";

	Com_Memset( groups, 0, sizeof( groups ) );
	groups[0].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	groups[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	groups[0].generalShader = 0;
	groups[1].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	groups[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	groups[1].generalShader = 1;
	groups[2].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	groups[2].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	groups[2].closestHitShader = 2;

	Com_Memset( &rtpci, 0, sizeof( rtpci ) );
	rtpci.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	rtpci.stageCount = 3;
	rtpci.pStages = stages;
	rtpci.groupCount = 3;
	rtpci.pGroups = groups;
	rtpci.maxPipelineRayRecursionDepth = 1;
	rtpci.layout = grtx.pl;
	pipeRes = qvkCreateRayTracingPipelinesKHR( vk.device, VK_NULL_HANDLE, vk.pipelineCache, 1, &rtpci, NULL, &grtx.pipeline );
	if ( pipeRes != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, "[GRTX] vkCreateRayTracingPipelinesKHR failed (%s)\n", vk_result_string( pipeRes ) );
		vk_grtx_shutdown();
		return;
	}

	hbufSize = (size_t)grtx.shader_group_base_alignment * 3u;
	sbtSize = (VkDeviceSize)hbufSize;
	vk_grtx_alloc_buffer( sbtSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&grtx.sbt_buffer, &grtx.sbt_memory, NULL );
	VK_CHECK( qvkMapMemory( vk.device, grtx.sbt_memory, 0, sbtSize, 0, (void **)&sbtHost ) );
	Com_Memset( sbtHost, 0, hbufSize );
	{
		uint8_t packedHandles[96];
		VK_CHECK( qvkGetRayTracingShaderGroupHandlesKHR( vk.device, grtx.pipeline, 0, 3, grtx.handle_size * 3u, packedHandles ) );
		for ( gi = 0; gi < 3; gi++ ) {
			Com_Memcpy( sbtHost + (size_t)grtx.shader_group_base_alignment * (size_t)gi,
				packedHandles + (size_t)grtx.handle_size * (size_t)gi, grtx.handle_size );
		}
	}
	qvkUnmapMemory( vk.device, grtx.sbt_memory );

	grtx.ready = qtrue;
	ri.Printf( PRINT_ALL,
		"[GRTX] Gaussian ray tracing ready (r_grtx=%d, maxGaussians=%d). Build with -DUSE_VULKAN_RTX=ON\n",
		r_grtx->integer, r_grtxMaxGaussians ? r_grtxMaxGaussians->integer : 256 );
}

void vk_grtx_frame_begin( void )
{
	uint32_t w, h;

	if ( !grtx.ready || !r_grtxDemo || !r_grtxDemo->integer ) {
		return;
	}
	vk_grtx_get_trace_extent( &w, &h );
	if ( w == grtx.width && h == grtx.height ) {
		return;
	}
	vk_grtx_create_rt_output( w, h, grtx.descriptor_set );
	vk_grtx_update_color_descriptor();
}

void vk_grtx_on_map_load( const char *mapBaseName )
{
	if ( !r_grtx || !r_grtx->integer || !vk.rtxAvailable ) {
		return;
	}
	if ( mapBaseName && mapBaseName[0] ) {
		Q_strncpyz( grtx.map_name, mapBaseName, sizeof( grtx.map_name ) );
	}
	grtx.scene_valid = qfalse;
	if ( grtx.ready ) {
		vk_grtx_rebuild_scene();
		ri.Printf( PRINT_ALL, "[GRTX] Rebuilt Gaussian BLAS for '%s' (%u primitives)\n",
			grtx.map_name, grtx.gaussian_count * GRTX_TRIS_PER_BOX );
	}
}

qboolean vk_grtx_active( void )
{
	return ( grtx.ready && r_grtx && r_grtx->integer > 0 && r_grtxDemo && r_grtxDemo->integer &&
		grtx.scene_valid && vk.fboActive ) ? qtrue : qfalse;
}

void vk_grtx_record_pass( VkCommandBuffer cmd )
{
	VkGrtxFrameUBO_t frameUbo;
	VkBufferDeviceAddressInfo addr;
	VkDeviceAddress sbtBase;
	VkStridedDeviceAddressRegionKHR raygenRegion, missRegion, hitRegion, callableRegion;
	VkImageMemoryBarrier barriers[2];
	VkImageBlit blit;
	VkImageLayout colorRestoreLayout;
	VkImageAspectFlags depthAspect;
	uint32_t preBarrierCount;

	if ( !vk_grtx_active() || !cmd ) {
		return;
	}

	if ( !grtx.scene_valid ) {
		vk_grtx_rebuild_scene();
	}

	if ( grtx.grtx_ubo_ptr ) {
		float viewProj[16];
		const float *view = backEnd.viewParms.world.modelViewMatrix;
		const float *projection = backEnd.useFirstPersonProjection ?
			backEnd.firstPersonProjectionMatrix : backEnd.viewParms.projectionMatrix;
		float proj_vk[16];
		float zNear, zFar;

		vk_get_projection_matrix_vk( projection, proj_vk );
		myGlMultMatrix( view, proj_vk, viewProj );
		if ( !vk_mat4_inverse( viewProj, frameUbo.invViewProj ) ) {
			Com_Memcpy( frameUbo.invViewProj, viewProj, sizeof( frameUbo.invViewProj ) );
		}
		frameUbo.viewOrigin[0] = backEnd.viewParms.or.origin[0];
		frameUbo.viewOrigin[1] = backEnd.viewParms.or.origin[1];
		frameUbo.viewOrigin[2] = backEnd.viewParms.or.origin[2];
		zNear = ( r_znear && r_znear->value > 0.0f ) ? r_znear->value : 8.0f;
		zFar = backEnd.viewParms.zFar;
		if ( zFar <= zNear ) {
			zFar = zNear + 100.0f;
		}
		frameUbo.zNearFar[0] = zNear;
		frameUbo.zNearFar[1] = zFar;
		frameUbo.outputSize[0] = (float)grtx.width;
		frameUbo.outputSize[1] = (float)grtx.height;
		frameUbo.outputSize[2] = (float)( ( r_grtx && r_grtx->integer > 0 ) ? r_grtx->integer : 1 );
		frameUbo.outputSize[3] = r_grtxComposite ? r_grtxComposite->value : 0.0f;
		frameUbo.traceParams[0] = ( r_grtxSamples && r_grtxSamples->integer > 0 ) ? (float)r_grtxSamples->integer : 1.0f;
		Com_Memcpy( grtx.grtx_ubo_ptr, &frameUbo, sizeof( frameUbo ) );
	}

	depthAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 ) {
		depthAspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	if ( vk.depth_image != VK_NULL_HANDLE ) {
		record_depth_image_layout_transition( cmd, depthAspect,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, 0, 0 );
	}

	colorRestoreLayout = ( vk.renderPassIndex == RENDER_PASS_POST_BLOOM ) ?
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	Com_Memset( &addr, 0, sizeof( addr ) );
	addr.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addr.buffer = grtx.sbt_buffer;
	sbtBase = qvkGetBufferDeviceAddress( vk.device, &addr );
	raygenRegion.deviceAddress = sbtBase;
	raygenRegion.stride = grtx.shader_group_base_alignment;
	raygenRegion.size = grtx.shader_group_base_alignment;
	missRegion.deviceAddress = sbtBase + grtx.shader_group_base_alignment;
	missRegion.stride = grtx.shader_group_base_alignment;
	missRegion.size = grtx.shader_group_base_alignment;
	hitRegion.deviceAddress = sbtBase + 2u * grtx.shader_group_base_alignment;
	hitRegion.stride = grtx.shader_group_base_alignment;
	hitRegion.size = grtx.shader_group_base_alignment;

	Com_Memset( barriers, 0, sizeof( barriers ) );
	barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[0].oldLayout = grtx.rt_image_traced ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barriers[0].image = grtx.rt_image;
	barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barriers[0].subresourceRange.levelCount = 1;
	barriers[0].subresourceRange.layerCount = 1;
	barriers[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

	barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[1].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barriers[1].image = vk.color_image;
	barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barriers[1].subresourceRange.levelCount = 1;
	barriers[1].subresourceRange.layerCount = 1;
	preBarrierCount = 2u;
	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
		0, 0, NULL, 0, NULL, preBarrierCount, barriers );

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, grtx.pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, grtx.pl, 0, 1, &grtx.descriptor_set, 0, NULL );
	qvkCmdTraceRaysKHR( cmd, &raygenRegion, &missRegion, &hitRegion, &callableRegion,
		grtx.width, grtx.height, 1 );

	grtx.rt_image_traced = qtrue;

	Com_Memset( &barriers[0], 0, sizeof( barriers[0] ) );
	barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barriers[0].image = grtx.rt_image;
	barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barriers[0].subresourceRange.levelCount = 1;
	barriers[0].subresourceRange.layerCount = 1;
	barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

	barriers[1].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barriers[1].srcAccessMask = 0;
	barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 0, NULL, 0, NULL, 2, barriers );

	Com_Memset( &blit, 0, sizeof( blit ) );
	blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit.srcSubresource.layerCount = 1;
	blit.dstSubresource = blit.srcSubresource;
	blit.srcOffsets[0].x = 0;
	blit.srcOffsets[0].y = 0;
	blit.srcOffsets[0].z = 0;
	blit.srcOffsets[1].x = (int32_t)grtx.width;
	blit.srcOffsets[1].y = (int32_t)grtx.height;
	blit.srcOffsets[1].z = 1;
	blit.dstOffsets[0].x = 0;
	blit.dstOffsets[0].y = 0;
	blit.dstOffsets[0].z = 0;
	blit.dstOffsets[1].x = (int32_t)vk_get_render_target_width();
	blit.dstOffsets[1].y = (int32_t)vk_get_render_target_height();
	blit.dstOffsets[1].z = 1;
	qvkCmdBlitImage( cmd, grtx.rt_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		vk.color_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR );

	barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barriers[1].newLayout = colorRestoreLayout;
	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0, 0, NULL, 0, NULL, 2, barriers );

	if ( r_grtx_debug && r_grtx_debug->integer ) {
		ri.Printf( PRINT_DEVELOPER, "[GRTX] traced %ux%u gaussians=%u\n",
			grtx.width, grtx.height, grtx.gaussian_count );
	}
}

#else /* !USE_VULKAN_RTX */

void R_GRTX_Init( void )
{
	cvar_t *cv;

	cv = ri.Cvar_Get( "r_grtx", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_SetDescription( cv, "Gaussian ray tracing requires USE_VULKAN_RTX build." );
}

void R_GRTX_Shutdown( void ) {}

void vk_grtx_init( void ) {}
void vk_grtx_shutdown( void ) {}
void vk_grtx_frame_begin( void ) {}
void vk_grtx_on_map_load( const char *mapBaseName ) { (void)mapBaseName; }
void vk_grtx_record_pass( VkCommandBuffer cmd ) { (void)cmd; }
qboolean vk_grtx_active( void ) { return qfalse; }

#endif /* USE_VULKAN_RTX */
