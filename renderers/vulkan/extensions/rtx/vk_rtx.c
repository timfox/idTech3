/*
===========================================================================
Vulkan KHR ray tracing: world BLAS (all brush submodels: BSP faces + trisoups) + primary rays.

Build with USE_VULKAN_RTX, set r_rtx > 0 before vid_restart. r_rtxDemo 1 runs
per-frame trace using scene depth + invViewProj (see rgen). r_rtxWorldPrimCap
caps BLAS triangles (default 262144). See docs/RENDERERS_FUTURE.md.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_rtx.h"
#include "vk_rtx_entities.h"
#include "vk_rtx_world.h"
#include "vk_util.h"
#include "vk_view_state.h"
#include "vk_image_layout.h"
#include "vk_cmd.h"
#include "vk_fsa.h"

#ifdef USE_VULKAN_RTX

#include "vk_rtx_demo_spirv.inc"

typedef struct {
	float invViewProj[16];
	float viewOrigin[4];
	float zNearFar[4];
	/* xy = RT output resolution; z = r_rtx mode; w = r_rtxComposite blend */
	float outputSize[4];
	/* x = r_rtxSamples; yzw reserved */
	float traceParams[4];
} VkRtxFrameUBO_t;

static void vk_rtx_get_trace_extent( uint32_t *w, uint32_t *h )
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

static struct {
	qboolean		ready;
	uint32_t		width;
	uint32_t		height;
	uint32_t		handle_size;
	uint32_t		shader_group_base_alignment;
	uint32_t		scratch_alignment; /* minAccelerationStructureScratchOffsetAlignment */
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
	VkBuffer		albedo_buffer;
	VkDeviceMemory		albedo_memory;
	VkBuffer		normal_buffer;
	VkDeviceMemory		normal_memory;
	uint32_t		world_vertex_count;
	uint32_t		world_albedo_count;
	uint32_t		world_normal_count;
	VkImage			rt_image;
	VkDeviceMemory		rt_image_memory;
	VkImageView		rt_image_view;
	VkBuffer		rtx_ubo;
	VkDeviceMemory		rtx_ubo_memory;
	void			*rtx_ubo_ptr;
	qboolean		world_blas_valid;
	qboolean		blas_geo_is_world;
	qboolean		rt_image_traced;
	uint32_t		world_primitive_count;
	uint32_t		entity_primitive_count;
	uint32_t		entity_packed_count;
	uint32_t		entity_vertex_count;
	uint32_t		entity_mesh_count;
	uint32_t		entity_mesh_md3;
	uint32_t		entity_mesh_iqm;
	uint32_t		entity_mesh_gltf;
	uint32_t		entity_mesh_mdr;
	uint32_t		entity_mesh_cpu_skinned;
	uint32_t		entity_proxy_count;
	uint32_t		entity_proxy_non_mesh;
	uint32_t		entity_proxy_skinned;
	uint32_t		entity_proxy_md3_fail;
	uint32_t		entity_proxy_iqm_fail;
	uint32_t		entity_proxy_gltf_fail;
	uint32_t		entity_proxy_mdr_fail;
	char			tlas_build_mode[16];
	char			tlas_rebuild_reason[48];
	VkBuffer		entity_vertex_buffer;
	VkDeviceMemory	entity_vertex_memory;
	VkBuffer		entity_index_buffer;
	VkDeviceMemory	entity_index_memory;
	VkBuffer		entity_albedo_buffer;
	VkDeviceMemory	entity_albedo_memory;
	VkBuffer		entity_normal_buffer;
	VkDeviceMemory	entity_normal_memory;
	uint32_t		entity_albedo_count;
	uint32_t		entity_normal_count;
	VkAccelerationStructureKHR entity_blas;
	VkBuffer		entity_blas_buffer;
	VkDeviceMemory	entity_blas_memory;
	uint32_t		entity_blas_built_prims;
	uint32_t		entity_vb_capacity;
	uint32_t		entity_ib_capacity;
	VkDeviceSize	entity_ab_capacity;
	qboolean		entity_blas_valid;
	char			entity_blas_mode[16];
	char			entity_blas_reason[48];
	char			world_name[MAX_QPATH];
	uint32_t		tlas_instance_count;
	qboolean		tlas_valid;
	qboolean		cmd_registered;
	/* Deferred AS destroy: mid-frame wait_idle+destroy while Hybrid1 records TraceRays
	 * raced on NVIDIA. Retire old TLAS/entity BLAS and free them at frame_begin. */
	VkAccelerationStructureKHR retired_tlas;
	VkBuffer		retired_tlas_buffer;
	VkDeviceMemory	retired_tlas_memory;
	VkAccelerationStructureKHR retired_entity_blas;
	VkBuffer		retired_entity_blas_buffer;
	VkDeviceMemory	retired_entity_blas_memory;
	int				entity_tlas_frame;
} rtx;

static void vk_rtx_destroy_entity_blas( void );
static void vk_rtx_destroy_entity_as_only( void );
static void vk_rtx_flush_retired_as( void );
static void vk_rtx_retire_entity_blas( void );
static void vk_rtx_retire_tlas( void );

static const char *vk_rtx_state_string( void )
{
	if ( !vk.rtxAvailable ) {
		return "idle: latch r_rtx/r_hybrid1/r_raygun before vid_restart (or GPU lacks KHR RT)";
	}
	if ( ( !r_rtx || r_rtx->integer <= 0 ) && ( !r_hybrid1 || r_hybrid1->integer <= 0 )
		&& ( !r_raygun || r_raygun->integer <= 0 ) ) {
		return "idle: enable r_rtx, r_hybrid1, or r_raygun before vid_restart";
	}
	if ( ( !r_rtxDemo || !r_rtxDemo->integer ) && ( !r_hybrid1 || r_hybrid1->integer <= 0 )
		&& ( !r_raygun || r_raygun->integer <= 0 ) ) {
		return "idle: no RTX consumer requested";
	}
	if ( !rtx.ready ) {
		return "blocked: RTX pipeline is not ready";
	}
	if ( rtx.tlas == VK_NULL_HANDLE || !rtx.tlas_valid ) {
		return "waiting: TLAS not built yet";
	}
	return "ready";
}

static void RTX_Status_f( void )
{
	const char *wn = ( tr.world && tr.world->name[0] ) ? tr.world->name : "(none)";

	ri.Printf( PRINT_ALL, "[VK][RTX] state=%s sceneReady=%d worldBLAS=%d tlasValid=%d rtOutput=%d\n",
		vk_rtx_state_string(), vk_rtx_scene_ready() ? 1 : 0,
		rtx.world_blas_valid ? 1 : 0, rtx.tlas_valid ? 1 : 0,
		( rtx.rt_image != VK_NULL_HANDLE && rtx.rt_image_view != VK_NULL_HANDLE ) ? 1 : 0 );
	ri.Printf( PRINT_ALL, "[VK][RTX] ready=%d rtxAvailable=%d demo=%d hybrid=%d raygun=%d\n",
		rtx.ready ? 1 : 0, vk.rtxAvailable ? 1 : 0,
		( r_rtxDemo && r_rtxDemo->integer ) ? 1 : 0,
		( r_hybrid1 && r_hybrid1->integer ) ? 1 : 0,
		( r_raygun && r_raygun->integer ) ? 1 : 0 );
	ri.Printf( PRINT_ALL, "[VK][RTX] world=%s blas_tris=%u world_verts=%u albedo_prims=%u normal_prims=%u entity_ents=%u entity_tris=%u entity_verts=%u entity_albedo_prims=%u entity_normal_prims=%u mesh=%u (md3=%u iqm=%u gltf=%u mdr=%u cpuskin=%u) proxy=%u (nonmesh=%u skinfail=%u md3fail=%u iqmfail=%u gltffail=%u mdrfail=%u) proxy_rate=%u%% entity_blas=%s/%s tlas_instances=%u tlas_mode=%s reason=%s\n",
		wn, rtx.world_primitive_count, rtx.world_vertex_count, rtx.world_albedo_count, rtx.world_normal_count,
		rtx.entity_packed_count, rtx.entity_primitive_count,
		rtx.entity_vertex_count, rtx.entity_albedo_count, rtx.entity_normal_count, rtx.entity_mesh_count,
		rtx.entity_mesh_md3, rtx.entity_mesh_iqm, rtx.entity_mesh_gltf, rtx.entity_mesh_mdr,
		rtx.entity_mesh_cpu_skinned,
		rtx.entity_proxy_count,
		rtx.entity_proxy_non_mesh, rtx.entity_proxy_skinned,
		rtx.entity_proxy_md3_fail, rtx.entity_proxy_iqm_fail, rtx.entity_proxy_gltf_fail, rtx.entity_proxy_mdr_fail,
		( rtx.entity_packed_count > 0u )
			? (unsigned)( ( rtx.entity_proxy_count * 100u ) / rtx.entity_packed_count ) : 0u,
		rtx.entity_blas_mode[0] ? rtx.entity_blas_mode : "n/a",
		rtx.entity_blas_reason[0] ? rtx.entity_blas_reason : "n/a",
		rtx.tlas_instance_count,
		rtx.tlas_build_mode[0] ? rtx.tlas_build_mode : "n/a",
		rtx.tlas_rebuild_reason[0] ? rtx.tlas_rebuild_reason : "n/a" );
	ri.Printf( PRINT_ALL, "[VK][RTX] entity_albedo=materials:%d uv_thumb:%d (r_rtxEntityMaterials / r_rtxEntityUvSample)\n",
		( r_rtxEntityMaterials && r_rtxEntityMaterials->integer ) ? 1 : 0,
		( r_rtxEntityUvSample && r_rtxEntityUvSample->integer
			&& r_rtxEntityMaterials && r_rtxEntityMaterials->integer ) ? 1 : 0 );
	ri.Printf( PRINT_ALL, "[VK][RTX] world_albedo=materials:%d uv_thumb:%d mode:%d (r_rtxWorldMaterials / UvSample / AlbedoMode 0=replace 1=modulate)\n",
		( r_rtxWorldMaterials && r_rtxWorldMaterials->integer ) ? 1 : 0,
		( r_rtxWorldUvSample && r_rtxWorldUvSample->integer
			&& r_rtxWorldMaterials && r_rtxWorldMaterials->integer ) ? 1 : 0,
		( r_rtxWorldAlbedoMode ) ? r_rtxWorldAlbedoMode->integer : 0 );
	ri.Printf( PRINT_ALL, "[VK][RTX] note: Hybrid1 is the production RT lighting path; r_rtx demo overlay is diagnostic unless modes gain real rays\n" );
	ri.Printf( PRINT_ALL, "[VK][RTX] trace_extent=%ux%u r_rtx=%d composite=%.2f samples=%d\n",
		rtx.width, rtx.height,
		( r_rtx && r_rtx->integer > 0 ) ? r_rtx->integer : 0,
		r_rtxComposite ? r_rtxComposite->value : 0.0f,
		( r_rtxSamples && r_rtxSamples->integer > 0 ) ? r_rtxSamples->integer : 1 );
	{
		uint32_t liveCount = 0u;
		uint32_t faceN = 0u, triN = 0u, gridN = 0u, otherN = 0u, nullN = 0u;
		int bmCount = 0;
		int bi, i;

		if ( tr.world && tr.world->bmodels && tr.world->surfaces ) {
			bmCount = tr.world->numBModels > 0 ? tr.world->numBModels : 1;
			for ( bi = 0; bi < bmCount; bi++ ) {
				const bmodel_t *bm = &tr.world->bmodels[bi];
				if ( bm->numSurfaces <= 0 || bm->firstSurface == NULL ) {
					continue;
				}
				for ( i = 0; i < bm->numSurfaces; i++ ) {
					const msurface_t *sf = bm->firstSurface + i;
					const surfaceType_t *st;
					if ( !sf->data ) {
						nullN++;
						continue;
					}
					st = sf->data;
					if ( *st == SF_FACE ) {
						faceN++;
					} else if ( *st == SF_TRIANGLES ) {
						triN++;
					} else if ( *st == SF_GRID ) {
						gridN++;
					} else {
						otherN++;
					}
				}
			}
			liveCount = vk_rtx_world_count_primitives( tr.world, 262144u );
		}
		ri.Printf( PRINT_ALL, "[VK][RTX] live_pack_diag: count=%u bmodels=%d face=%u trisoup=%u grid=%u other=%u nullData=%u geo_is_world=%d cached_name=%s\n",
			liveCount, bmCount, faceN, triN, gridN, otherN, nullN,
			rtx.blas_geo_is_world ? 1 : 0,
			rtx.world_name[0] ? rtx.world_name : "(empty)" );
	}
}

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

/*
===============
vk_rtx_alloc_scratch

Allocate AS build scratch with device address aligned to
minAccelerationStructureScratchOffsetAlignment. Unaligned scratch is a common
NVIDIA DEVICE_LOST on non-trivial BLAS builds.
===============
*/
static void vk_rtx_alloc_scratch( VkDeviceSize scratchSize, VkDeviceAddress *outAddr )
{
	VkDeviceAddress raw;
	uint32_t align = rtx.scratch_alignment ? rtx.scratch_alignment : 256u;
	VkDeviceSize allocSize = scratchSize + (VkDeviceSize)align;

	vk_rtx_destroy_buffer( &rtx.scratch_buffer, &rtx.scratch_memory );
	vk_rtx_alloc_buffer( allocSize,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &rtx.scratch_buffer, &rtx.scratch_memory, &raw );
	*outAddr = ( raw + (VkDeviceAddress)( align - 1u ) ) & ~( (VkDeviceAddress)( align - 1u ) );
}

static void vk_rtx_host_to_as_barrier( VkCommandBuffer cmd )
{
	VkMemoryBarrier mb;

	Com_Memset( &mb, 0, sizeof( mb ) );
	mb.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	mb.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
	mb.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	qvkCmdPipelineBarrier( cmd,
		VK_PIPELINE_STAGE_HOST_BIT,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		0, 1, &mb, 0, NULL, 0, NULL );
}

/* One-time AS builds must use a dedicated CB (vk_begin/end_command_buffer).
 * Reusing vk.tess[0] mid-frame resets/submits the active recorder and faults
 * the driver on the subsequent TraceRays (NVIDIA SIGSEGV / DEVICE_LOST). */

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
	rtx.rt_image_traced = qfalse;
}

static void vk_rtx_update_color_descriptor( void )
{
	VkDescriptorImageInfo colorInfo;
	VkWriteDescriptorSet write;
	Vk_Sampler_Def sd;

	if ( rtx.descriptor_set == VK_NULL_HANDLE || vk.color_image_view == VK_NULL_HANDLE ) {
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
	write.dstSet = rtx.descriptor_set;
	write.dstBinding = 4;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &colorInfo;
	qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
}

static void vk_rtx_rebuild_world_blas( void )
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
	uint32_t maxInstTLAS;
	uint32_t maxPrimBLAS;
	uint32_t capPrim;
	uint32_t worldPrim;
	uint32_t packedPrim;
	qboolean useWorld;
	float triVerts[9];
	uint16_t triIdx[3];
	VkCommandBuffer buildCmd;
	uint8_t *vertMap;
	uint8_t *idxMap;
	uint8_t *instMap;
	VkWriteDescriptorSetAccelerationStructureKHR asWrite;
	VkWriteDescriptorSet writeAS;
	const char *wn;

	if ( !rtx.descriptor_set ) {
		return;
	}

	/* World BLAS tracks tr.world geometry, not the current view. Ignoring
	 * RDF_NOWORLDMODEL here — UI frames must not pin an empty BLAS forever. */
	wn = ( tr.world && tr.world->name[0] ) ? tr.world->name : "";
	capPrim = ( r_rtxWorldPrimCap && r_rtxWorldPrimCap->integer > 0 ) ? (uint32_t)r_rtxWorldPrimCap->integer : 262144u;
	/*
	 * Early-out only when BLAS already matches this world successfully, or when
	 * there is no world (UI fallback triangle). A named-world fallback must retry —
	 * the first post-map-load frame often has zero countable tris, which used to
	 * stick forever (world_name set, blas_geo_is_world=0, never rebuilt).
	 */
	if ( rtx.world_blas_valid && !strcmp( rtx.world_name, wn ) ) {
		if ( wn[0] == '\0' || rtx.blas_geo_is_world ) {
			return;
		}
		if ( vk_rtx_world_count_primitives( tr.world, capPrim ) == 0u ) {
			return;
		}
	}

	worldPrim = ( wn[0] != '\0' ) ? vk_rtx_world_count_primitives( tr.world, capPrim ) : 0u;
	useWorld = ( worldPrim > 0u ) ? qtrue : qfalse;

	/* In-flight frame CBs may still reference the current TLAS/BLAS. Wait before
	 * destroy+rebuild or TraceRays on the next submit can DEVICE_LOST. */
	vk_queue_wait_idle();
	vk_rtx_flush_retired_as();

	vk_rtx_destroy_as( &rtx.tlas );
	vk_rtx_destroy_as( &rtx.blas );
	vk_rtx_destroy_as( &rtx.entity_blas );
	rtx.tlas_valid = qfalse;
	rtx.tlas_instance_count = 0u;
	vk_rtx_destroy_buffer( &rtx.tlas_buffer, &rtx.tlas_memory );
	vk_rtx_destroy_buffer( &rtx.instance_buffer, &rtx.instance_memory );
	vk_rtx_destroy_buffer( &rtx.blas_buffer, &rtx.blas_memory );
	vk_rtx_destroy_buffer( &rtx.entity_blas_buffer, &rtx.entity_blas_memory );
	vk_rtx_destroy_buffer( &rtx.vertex_buffer, &rtx.vertex_memory );
	vk_rtx_destroy_buffer( &rtx.index_buffer, &rtx.index_memory );
	vk_rtx_destroy_buffer( &rtx.albedo_buffer, &rtx.albedo_memory );
	vk_rtx_destroy_buffer( &rtx.normal_buffer, &rtx.normal_memory );
	vk_rtx_destroy_buffer( &rtx.entity_vertex_buffer, &rtx.entity_vertex_memory );
	vk_rtx_destroy_buffer( &rtx.entity_index_buffer, &rtx.entity_index_memory );
	vk_rtx_destroy_buffer( &rtx.scratch_buffer, &rtx.scratch_memory );
	rtx.entity_primitive_count = 0u;
	rtx.entity_packed_count = 0u;
	rtx.entity_vertex_count = 0u;
	rtx.entity_mesh_count = 0u;
	rtx.entity_mesh_md3 = 0u;
	rtx.entity_mesh_iqm = 0u;
	rtx.entity_mesh_gltf = 0u;
	rtx.entity_mesh_mdr = 0u;
	rtx.entity_mesh_cpu_skinned = 0u;
	rtx.entity_proxy_count = 0u;
	rtx.entity_proxy_non_mesh = 0u;
	rtx.entity_proxy_skinned = 0u;
	rtx.entity_proxy_md3_fail = 0u;
	rtx.entity_proxy_iqm_fail = 0u;
	rtx.entity_proxy_gltf_fail = 0u;
	rtx.entity_proxy_mdr_fail = 0u;
	rtx.world_vertex_count = 0u;
	rtx.world_albedo_count = 0u;
	rtx.world_normal_count = 0u;

	maxPrimBLAS = 1u;
	if ( useWorld ) {
		VkDeviceSize vbSize = (VkDeviceSize)worldPrim * 9u * sizeof( float );
		VkDeviceSize ibSize = (VkDeviceSize)worldPrim * 3u * sizeof( uint32_t );
		VkDeviceSize abSize = (VkDeviceSize)worldPrim * 3u * sizeof( float );
		float *posHost;
		uint32_t *idxHost;
		float *albedoHost;
		float *normalHost;
		uint32_t packedVerts = 0u;

		vk_rtx_alloc_buffer( vbSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&rtx.vertex_buffer, &rtx.vertex_memory, &vbAddr );
		vk_rtx_alloc_buffer( ibSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&rtx.index_buffer, &rtx.index_memory, &ibAddr );
		vk_rtx_alloc_buffer( abSize,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&rtx.albedo_buffer, &rtx.albedo_memory, NULL );
		vk_rtx_alloc_buffer( abSize,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&rtx.normal_buffer, &rtx.normal_memory, NULL );
		VK_CHECK( qvkMapMemory( vk.device, rtx.vertex_memory, 0, vbSize, 0, (void **)&posHost ) );
		VK_CHECK( qvkMapMemory( vk.device, rtx.index_memory, 0, ibSize, 0, (void **)&idxHost ) );
		VK_CHECK( qvkMapMemory( vk.device, rtx.albedo_memory, 0, abSize, 0, (void **)&albedoHost ) );
		VK_CHECK( qvkMapMemory( vk.device, rtx.normal_memory, 0, abSize, 0, (void **)&normalHost ) );
		{
			uint32_t ai;
			for ( ai = 0u; ai < worldPrim; ai++ ) {
				albedoHost[ai * 3u + 0u] = 0.72f;
				albedoHost[ai * 3u + 1u] = 0.70f;
				albedoHost[ai * 3u + 2u] = 0.66f;
				normalHost[ai * 3u + 0u] = 0.0f;
				normalHost[ai * 3u + 1u] = 0.0f;
				normalHost[ai * 3u + 2u] = 1.0f;
			}
		}
		packedPrim = vk_rtx_world_pack( tr.world, worldPrim, posHost, idxHost, albedoHost, normalHost, &packedVerts );
		qvkUnmapMemory( vk.device, rtx.vertex_memory );
		qvkUnmapMemory( vk.device, rtx.index_memory );
		qvkUnmapMemory( vk.device, rtx.albedo_memory );
		qvkUnmapMemory( vk.device, rtx.normal_memory );
		if ( packedPrim == 0u ) {
			useWorld = qfalse;
			vk_rtx_destroy_buffer( &rtx.vertex_buffer, &rtx.vertex_memory );
			vk_rtx_destroy_buffer( &rtx.index_buffer, &rtx.index_memory );
			vk_rtx_destroy_buffer( &rtx.albedo_buffer, &rtx.albedo_memory );
			vk_rtx_destroy_buffer( &rtx.normal_buffer, &rtx.normal_memory );
		} else {
			maxPrimBLAS = packedPrim;
			rtx.world_primitive_count = packedPrim;
			rtx.world_vertex_count = packedVerts;
			rtx.world_albedo_count = packedPrim;
			rtx.world_normal_count = packedPrim;
		}
	}

	if ( !useWorld ) {
		float fallbackAlbedo[3] = { 0.72f, 0.70f, 0.66f };
		float fallbackNormal[3] = { 0.0f, 0.0f, 1.0f };
		float *albedoHost;
		float *normalHost;

		triVerts[0] = -1.0f; triVerts[1] = -1.0f; triVerts[2] = 0.0f;
		triVerts[3] =  1.0f; triVerts[4] = -1.0f; triVerts[5] = 0.0f;
		triVerts[6] =  0.0f; triVerts[7] =  1.0f; triVerts[8] = 0.0f;
		triIdx[0] = 0; triIdx[1] = 1; triIdx[2] = 2;
		vk_rtx_alloc_buffer( sizeof( triVerts ),
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&rtx.vertex_buffer, &rtx.vertex_memory, &vbAddr );
		VK_CHECK( qvkMapMemory( vk.device, rtx.vertex_memory, 0, sizeof( triVerts ), 0, (void **)&vertMap ) );
		Com_Memcpy( vertMap, triVerts, sizeof( triVerts ) );
		qvkUnmapMemory( vk.device, rtx.vertex_memory );
		vk_rtx_alloc_buffer( sizeof( triIdx ),
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&rtx.index_buffer, &rtx.index_memory, &ibAddr );
		VK_CHECK( qvkMapMemory( vk.device, rtx.index_memory, 0, sizeof( triIdx ), 0, (void **)&idxMap ) );
		Com_Memcpy( idxMap, triIdx, sizeof( triIdx ) );
		qvkUnmapMemory( vk.device, rtx.index_memory );
		vk_rtx_alloc_buffer( sizeof( fallbackAlbedo ),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&rtx.albedo_buffer, &rtx.albedo_memory, NULL );
		VK_CHECK( qvkMapMemory( vk.device, rtx.albedo_memory, 0, sizeof( fallbackAlbedo ), 0, (void **)&albedoHost ) );
		Com_Memcpy( albedoHost, fallbackAlbedo, sizeof( fallbackAlbedo ) );
		qvkUnmapMemory( vk.device, rtx.albedo_memory );
		vk_rtx_alloc_buffer( sizeof( fallbackNormal ),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&rtx.normal_buffer, &rtx.normal_memory, NULL );
		VK_CHECK( qvkMapMemory( vk.device, rtx.normal_memory, 0, sizeof( fallbackNormal ), 0, (void **)&normalHost ) );
		Com_Memcpy( normalHost, fallbackNormal, sizeof( fallbackNormal ) );
		qvkUnmapMemory( vk.device, rtx.normal_memory );
		maxPrimBLAS = 1u;
		rtx.world_primitive_count = 0u;
		rtx.world_vertex_count = 3u;
		rtx.world_albedo_count = 0u;
		rtx.world_normal_count = 0u;
	}

	Com_Memset( &triangles, 0, sizeof( triangles ) );
	triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	triangles.vertexData.deviceAddress = vbAddr;
	triangles.vertexStride = sizeof( float ) * 3u;
	if ( useWorld && rtx.world_vertex_count > 0u ) {
		triangles.maxVertex = rtx.world_vertex_count - 1u;
	} else {
		triangles.maxVertex = ( maxPrimBLAS > 0u ) ? ( maxPrimBLAS * 3u - 1u ) : 0u;
	}
	triangles.indexType = useWorld ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
	triangles.indexData.deviceAddress = ibAddr;

	Com_Memset( &geometryBLAS, 0, sizeof( geometryBLAS ) );
	geometryBLAS.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometryBLAS.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometryBLAS.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	geometryBLAS.geometry.triangles = triangles;

	Com_Memset( &buildInfoBLAS, 0, sizeof( buildInfoBLAS ) );
	buildInfoBLAS.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfoBLAS.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	buildInfoBLAS.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfoBLAS.geometryCount = 1;
	buildInfoBLAS.pGeometries = &geometryBLAS;

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
	buildInfoTLAS.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
		| VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
	buildInfoTLAS.geometryCount = 1;
	buildInfoTLAS.pGeometries = &geometryTLAS;

	maxInstTLAS = 1u;
	Com_Memset( &sizeInfoTLAS, 0, sizeof( sizeInfoTLAS ) );
	sizeInfoTLAS.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	qvkGetAccelerationStructureBuildSizesKHR( vk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&buildInfoTLAS, &maxInstTLAS, &sizeInfoTLAS );

	scratchSize = sizeInfoBLAS.buildScratchSize;
	if ( sizeInfoTLAS.buildScratchSize > scratchSize ) {
		scratchSize = sizeInfoTLAS.buildScratchSize;
	}
	vk_rtx_alloc_scratch( scratchSize, &scratchAddr );

	Com_Memset( &rangeBLAS, 0, sizeof( rangeBLAS ) );
	rangeBLAS.primitiveCount = maxPrimBLAS;
	pRangeBLAS = &rangeBLAS;
	buildInfoBLAS.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfoBLAS.dstAccelerationStructure = rtx.blas;
	buildInfoBLAS.scratchData.deviceAddress = scratchAddr;

	buildCmd = vk_begin_command_buffer();
	vk_rtx_host_to_as_barrier( buildCmd );
	qvkCmdBuildAccelerationStructuresKHR( buildCmd, 1, &buildInfoBLAS, &pRangeBLAS );
	vk_end_command_buffer( buildCmd, "rtx world blas" );

	/* Entity TLAS path owns TLAS when r_rtxEntities 1 (per-frame instance refresh). */
	if ( !r_rtxEntities || !r_rtxEntities->integer ) {
		vk_rtx_destroy_as( &rtx.tlas );
		vk_rtx_destroy_buffer( &rtx.tlas_buffer, &rtx.tlas_memory );
		/* Keep instance_buffer — geometryTLAS still references its device address for this build. */
		rtx.tlas_valid = qfalse;
		rtx.tlas_instance_count = 0u;

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
		rangeTLAS.primitiveCount = 1u;
		pRangeTLAS = &rangeTLAS;
		buildInfoTLAS.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		buildInfoTLAS.dstAccelerationStructure = rtx.tlas;
		buildInfoTLAS.scratchData.deviceAddress = scratchAddr;

		buildCmd = vk_begin_command_buffer();
		vk_rtx_host_to_as_barrier( buildCmd );
		qvkCmdBuildAccelerationStructuresKHR( buildCmd, 1, &buildInfoTLAS, &pRangeTLAS );
		vk_end_command_buffer( buildCmd, "rtx world tlas" );

		Com_Memset( &asWrite, 0, sizeof( asWrite ) );
		asWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
		asWrite.accelerationStructureCount = 1;
		asWrite.pAccelerationStructures = &rtx.tlas;

		Com_Memset( &writeAS, 0, sizeof( writeAS ) );
		writeAS.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeAS.dstSet = rtx.descriptor_set;
		writeAS.dstBinding = 0;
		writeAS.descriptorCount = 1;
		writeAS.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		writeAS.pNext = &asWrite;
		qvkUpdateDescriptorSets( vk.device, 1, &writeAS, 0, NULL );

		rtx.tlas_valid = qtrue;
		rtx.tlas_instance_count = 1u;
	}

	Q_strncpyz( rtx.world_name, wn, sizeof( rtx.world_name ) );
	rtx.blas_geo_is_world = ( wn[0] != '\0' && useWorld ) ? qtrue : qfalse;
	rtx.world_blas_valid = qtrue;

	if ( useWorld && wn[0] != '\0' ) {
		int subm = ( tr.world->numBModels > 0 ) ? tr.world->numBModels : 1;
		ri.Printf( PRINT_DEVELOPER, "[VK][RTX] BLAS rebuilt: %u tris from %s (%d brush submodels, faces+trisoups+grids, cap %u)\n",
			maxPrimBLAS, wn, subm, capPrim );
	} else if ( wn[0] != '\0' ) {
		ri.Printf( PRINT_DEVELOPER, "[VK][RTX] BLAS fallback triangle (no packable world tris for %s)\n", wn );
	} else {
		ri.Printf( PRINT_DEVELOPER, "[VK][RTX] BLAS fallback triangle (no world)\n" );
	}
}

static void vk_rtx_destroy_entity_as_only( void )
{
	vk_rtx_destroy_as( &rtx.entity_blas );
	vk_rtx_destroy_buffer( &rtx.entity_blas_buffer, &rtx.entity_blas_memory );
	rtx.entity_blas_built_prims = 0u;
	rtx.entity_blas_valid = qfalse;
}

static void vk_rtx_flush_retired_as( void )
{
	vk_rtx_destroy_as( &rtx.retired_tlas );
	vk_rtx_destroy_buffer( &rtx.retired_tlas_buffer, &rtx.retired_tlas_memory );
	vk_rtx_destroy_as( &rtx.retired_entity_blas );
	vk_rtx_destroy_buffer( &rtx.retired_entity_blas_buffer, &rtx.retired_entity_blas_memory );
}

static void vk_rtx_retire_entity_blas( void )
{
	if ( rtx.entity_blas == VK_NULL_HANDLE && rtx.entity_blas_buffer == VK_NULL_HANDLE ) {
		rtx.entity_blas_built_prims = 0u;
		rtx.entity_blas_valid = qfalse;
		return;
	}
	/* Drop any prior retired (safe after frame_begin flush / queue idle). */
	vk_rtx_destroy_as( &rtx.retired_entity_blas );
	vk_rtx_destroy_buffer( &rtx.retired_entity_blas_buffer, &rtx.retired_entity_blas_memory );
	rtx.retired_entity_blas = rtx.entity_blas;
	rtx.retired_entity_blas_buffer = rtx.entity_blas_buffer;
	rtx.retired_entity_blas_memory = rtx.entity_blas_memory;
	rtx.entity_blas = VK_NULL_HANDLE;
	rtx.entity_blas_buffer = VK_NULL_HANDLE;
	rtx.entity_blas_memory = VK_NULL_HANDLE;
	rtx.entity_blas_built_prims = 0u;
	rtx.entity_blas_valid = qfalse;
}

static void vk_rtx_retire_tlas( void )
{
	if ( rtx.tlas == VK_NULL_HANDLE && rtx.tlas_buffer == VK_NULL_HANDLE ) {
		rtx.tlas_valid = qfalse;
		rtx.tlas_instance_count = 0u;
		return;
	}
	vk_rtx_destroy_as( &rtx.retired_tlas );
	vk_rtx_destroy_buffer( &rtx.retired_tlas_buffer, &rtx.retired_tlas_memory );
	rtx.retired_tlas = rtx.tlas;
	rtx.retired_tlas_buffer = rtx.tlas_buffer;
	rtx.retired_tlas_memory = rtx.tlas_memory;
	rtx.tlas = VK_NULL_HANDLE;
	rtx.tlas_buffer = VK_NULL_HANDLE;
	rtx.tlas_memory = VK_NULL_HANDLE;
	rtx.tlas_valid = qfalse;
	rtx.tlas_instance_count = 0u;
}

static void vk_rtx_destroy_entity_blas( void )
{
	vk_rtx_destroy_entity_as_only();
	vk_rtx_destroy_buffer( &rtx.entity_vertex_buffer, &rtx.entity_vertex_memory );
	vk_rtx_destroy_buffer( &rtx.entity_index_buffer, &rtx.entity_index_memory );
	vk_rtx_destroy_buffer( &rtx.entity_albedo_buffer, &rtx.entity_albedo_memory );
	vk_rtx_destroy_buffer( &rtx.entity_normal_buffer, &rtx.entity_normal_memory );
	rtx.entity_vb_capacity = 0u;
	rtx.entity_ib_capacity = 0u;
	rtx.entity_ab_capacity = 0;
	rtx.entity_primitive_count = 0u;
	rtx.entity_packed_count = 0u;
	rtx.entity_vertex_count = 0u;
	rtx.entity_mesh_count = 0u;
	rtx.entity_mesh_md3 = 0u;
	rtx.entity_mesh_iqm = 0u;
	rtx.entity_mesh_gltf = 0u;
	rtx.entity_mesh_mdr = 0u;
	rtx.entity_mesh_cpu_skinned = 0u;
	rtx.entity_proxy_count = 0u;
	rtx.entity_proxy_non_mesh = 0u;
	rtx.entity_proxy_skinned = 0u;
	rtx.entity_proxy_md3_fail = 0u;
	rtx.entity_proxy_iqm_fail = 0u;
	rtx.entity_proxy_gltf_fail = 0u;
	rtx.entity_proxy_mdr_fail = 0u;
	rtx.entity_albedo_count = 0u;
	rtx.entity_normal_count = 0u;
	rtx.entity_blas_mode[0] = '\0';
	rtx.entity_blas_reason[0] = '\0';
}

static void vk_rtx_rebuild_entity_tlas( void )
{
	VkAccelerationStructureInstanceKHR instances[2];
	VkAccelerationStructureDeviceAddressInfoKHR addrInfo;
	VkDeviceAddress worldBlasAddr = 0;
	VkDeviceAddress entityBlasAddr = 0;
	VkDeviceAddress instAddr = 0;
	VkDeviceAddress scratchAddr = 0;
	VkAccelerationStructureBuildGeometryInfoKHR buildInfoTLAS;
	VkAccelerationStructureGeometryKHR geometryTLAS;
	VkAccelerationStructureGeometryInstancesDataKHR instGeom;
	VkAccelerationStructureBuildRangeInfoKHR rangeTLAS;
	const VkAccelerationStructureBuildRangeInfoKHR *pRangeTLAS;
	VkAccelerationStructureBuildSizesInfoKHR sizeInfoTLAS;
	VkAccelerationStructureCreateInfoKHR asci;
	VkAccelerationStructureBuildGeometryInfoKHR buildInfoBLAS;
	VkAccelerationStructureGeometryKHR geometryBLAS;
	VkAccelerationStructureGeometryTrianglesDataKHR triangles;
	VkAccelerationStructureBuildRangeInfoKHR rangeBLAS;
	const VkAccelerationStructureBuildRangeInfoKHR *pRangeBLAS;
	VkAccelerationStructureBuildSizesInfoKHR sizeInfoBLAS;
	VkCommandBuffer buildCmd;
	VkWriteDescriptorSetAccelerationStructureKHR asWrite;
	VkWriteDescriptorSet writeAS;
	uint32_t maxInstTLAS;
	uint32_t maxPrimEntity;
	uint32_t capEnt;
	uint32_t packedEnt;
	uint8_t *instMap;
	VkDeviceSize scratchSize = 0;
	VkDeviceSize instBufSize = 0;
	VkDeviceAddress vbAddr = 0;
	VkDeviceAddress ibAddr = 0;
	qboolean tlasUpdate;
	qboolean tlasCanUpdate;
	qboolean entityBlasUpdate;
	qboolean entityBlasCanUpdate;

	if ( !rtx.ready || !rtx.descriptor_set || !rtx.blas || !rtx.world_blas_valid ) {
		return;
	}

	if ( !r_rtxEntities || !r_rtxEntities->integer ) {
		return;
	}

	/* Hybrid1 + demo both call prepare/rebuild; only refresh once per view frame. */
	if ( rtx.entity_tlas_frame == tr.frameCount ) {
		return;
	}
	rtx.entity_tlas_frame = tr.frameCount;

	/*
	 * Do not vk_queue_wait_idle here. Previous frame TraceRays already completed
	 * (frame fence waited before recording). Destroy of prior AS is deferred via
	 * retire → flush here (safe) / at world rebuild / shutdown.
	 */
	vk_rtx_flush_retired_as();
	Com_Memset( &addrInfo, 0, sizeof( addrInfo ) );
	addrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	addrInfo.accelerationStructure = rtx.blas;
	worldBlasAddr = qvkGetAccelerationStructureDeviceAddressKHR( vk.device, &addrInfo );

	capEnt = ( r_rtxEntityCap && r_rtxEntityCap->integer > 0 ) ? (uint32_t)r_rtxEntityCap->integer : 128u;
	if ( capEnt > 1024u ) {
		capEnt = 1024u;
	}

	maxPrimEntity = 0u;
	packedEnt = 0u;
	entityBlasUpdate = qfalse;
	entityBlasCanUpdate = qfalse;
	if ( backEnd.refdef.num_entities > 0 ) {
		uint32_t triCap = ( r_rtxEntityTriCap && r_rtxEntityTriCap->integer > 0 )
			? (uint32_t)r_rtxEntityTriCap->integer : 65536u;
		uint32_t maxVerts;
		uint32_t maxIndices;
		VkDeviceSize vbSize;
		VkDeviceSize ibSize;
		VkDeviceSize abSize;
		vkRtxEntityPackStats_t packStats;
		VkBufferDeviceAddressInfo bda;

		if ( triCap < 12u ) {
			triCap = 12u;
		}
		if ( triCap > 1048576u ) {
			triCap = 1048576u;
		}
		/* Worst case: one unique vert per triangle corner, plus AABB cubes (8 verts / 12 tris). */
		maxVerts = triCap * 3u;
		if ( maxVerts < capEnt * 8u ) {
			maxVerts = capEnt * 8u;
		}
		maxIndices = triCap * 3u;
		if ( maxIndices < capEnt * 36u ) {
			maxIndices = capEnt * 36u;
		}

		vbSize = (VkDeviceSize)maxVerts * 3u * sizeof( float );
		ibSize = (VkDeviceSize)maxIndices * sizeof( uint32_t );
		abSize = (VkDeviceSize)( maxIndices / 3u ) * 3u * sizeof( float );

		if ( rtx.entity_vb_capacity < maxVerts || rtx.entity_ib_capacity < maxIndices
			|| rtx.entity_ab_capacity < abSize
			|| rtx.entity_vertex_buffer == VK_NULL_HANDLE
			|| rtx.entity_index_buffer == VK_NULL_HANDLE ) {
			vk_rtx_destroy_entity_blas();
			vk_rtx_alloc_buffer( vbSize,
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&rtx.entity_vertex_buffer, &rtx.entity_vertex_memory, &vbAddr );
			vk_rtx_alloc_buffer( ibSize,
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&rtx.entity_index_buffer, &rtx.entity_index_memory, &ibAddr );
			vk_rtx_alloc_buffer( abSize,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&rtx.entity_albedo_buffer, &rtx.entity_albedo_memory, NULL );
			vk_rtx_alloc_buffer( abSize,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&rtx.entity_normal_buffer, &rtx.entity_normal_memory, NULL );
			rtx.entity_vb_capacity = maxVerts;
			rtx.entity_ib_capacity = maxIndices;
			rtx.entity_ab_capacity = abSize;
		} else {
			Com_Memset( &bda, 0, sizeof( bda ) );
			bda.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
			bda.buffer = rtx.entity_vertex_buffer;
			vbAddr = qvkGetBufferDeviceAddress( vk.device, &bda );
			bda.buffer = rtx.entity_index_buffer;
			ibAddr = qvkGetBufferDeviceAddress( vk.device, &bda );
		}
		{
			float *posHost;
			uint32_t *idxHost;
			float *albedoHost = NULL;
			float *normalHost = NULL;

			VK_CHECK( qvkMapMemory( vk.device, rtx.entity_vertex_memory, 0, vbSize, 0, (void **)&posHost ) );
			VK_CHECK( qvkMapMemory( vk.device, rtx.entity_index_memory, 0, ibSize, 0, (void **)&idxHost ) );
			if ( rtx.entity_albedo_memory != VK_NULL_HANDLE ) {
				VK_CHECK( qvkMapMemory( vk.device, rtx.entity_albedo_memory, 0, abSize, 0, (void **)&albedoHost ) );
			}
			if ( rtx.entity_normal_memory != VK_NULL_HANDLE ) {
				VK_CHECK( qvkMapMemory( vk.device, rtx.entity_normal_memory, 0, abSize, 0, (void **)&normalHost ) );
			}
			Com_Memset( &packStats, 0, sizeof( packStats ) );
			packedEnt = vk_rtx_entities_pack( &backEnd.refdef, &backEnd.viewParms, capEnt,
				posHost, maxVerts, idxHost, maxIndices, albedoHost, normalHost, &packStats );
			qvkUnmapMemory( vk.device, rtx.entity_vertex_memory );
			qvkUnmapMemory( vk.device, rtx.entity_index_memory );
			if ( albedoHost ) {
				qvkUnmapMemory( vk.device, rtx.entity_albedo_memory );
			}
			if ( normalHost ) {
				qvkUnmapMemory( vk.device, rtx.entity_normal_memory );
			}
		}
		if ( packedEnt == 0u || packStats.primitiveCount == 0u ) {
			vk_rtx_retire_entity_blas();
			rtx.entity_packed_count = 0u;
			rtx.entity_primitive_count = 0u;
			rtx.entity_vertex_count = 0u;
			rtx.entity_albedo_count = 0u;
			rtx.entity_normal_count = 0u;
			Q_strncpyz( rtx.entity_blas_mode, "NONE", sizeof( rtx.entity_blas_mode ) );
			Q_strncpyz( rtx.entity_blas_reason, "no_packed_prims", sizeof( rtx.entity_blas_reason ) );
		} else {
			maxPrimEntity = packStats.primitiveCount;
			rtx.entity_packed_count = packedEnt;
			rtx.entity_primitive_count = maxPrimEntity;
			rtx.entity_vertex_count = packStats.vertexCount;
			rtx.entity_mesh_count = packStats.meshEntityCount;
			rtx.entity_mesh_md3 = packStats.meshMd3Count;
			rtx.entity_mesh_iqm = packStats.meshIqmCount;
			rtx.entity_mesh_gltf = packStats.meshGltfCount;
			rtx.entity_mesh_mdr = packStats.meshMdrCount;
			rtx.entity_mesh_cpu_skinned = packStats.meshCpuSkinnedCount;
			rtx.entity_proxy_count = packStats.proxyEntityCount;
			rtx.entity_proxy_non_mesh = packStats.proxyNonMeshCount;
			rtx.entity_proxy_skinned = packStats.proxySkinnedCount;
			rtx.entity_proxy_md3_fail = packStats.proxyMd3FailCount;
			rtx.entity_proxy_iqm_fail = packStats.proxyIqmFailCount;
			rtx.entity_proxy_gltf_fail = packStats.proxyGltfFailCount;
			rtx.entity_proxy_mdr_fail = packStats.proxyMdrFailCount;
			rtx.entity_albedo_count = maxPrimEntity;
			rtx.entity_normal_count = maxPrimEntity;

			Com_Memset( &triangles, 0, sizeof( triangles ) );
			triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
			triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
			triangles.vertexData.deviceAddress = vbAddr;
			triangles.vertexStride = sizeof( float ) * 3u;
			triangles.maxVertex = ( packStats.vertexCount > 0u ) ? ( packStats.vertexCount - 1u ) : 0u;
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
			/* Skinned/animated verts: prefer fast build + allow REFIT/UPDATE across frames. */
			buildInfoBLAS.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR
				| VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
			buildInfoBLAS.geometryCount = 1;
			buildInfoBLAS.pGeometries = &geometryBLAS;

			Com_Memset( &sizeInfoBLAS, 0, sizeof( sizeInfoBLAS ) );
			sizeInfoBLAS.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
			qvkGetAccelerationStructureBuildSizesKHR( vk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
				&buildInfoBLAS, &maxPrimEntity, &sizeInfoBLAS );

			entityBlasCanUpdate = rtx.entity_blas_valid && rtx.entity_blas != VK_NULL_HANDLE
				&& rtx.entity_blas_built_prims == maxPrimEntity;
			entityBlasUpdate = entityBlasCanUpdate && r_rtxEntityBlasUpdate && r_rtxEntityBlasUpdate->integer;

			if ( entityBlasUpdate ) {
				Q_strncpyz( rtx.entity_blas_mode, "UPDATE", sizeof( rtx.entity_blas_mode ) );
				Q_strncpyz( rtx.entity_blas_reason, "stable_prim_count", sizeof( rtx.entity_blas_reason ) );
			} else {
				Q_strncpyz( rtx.entity_blas_mode, "REBUILD", sizeof( rtx.entity_blas_mode ) );
				if ( !rtx.entity_blas_valid || rtx.entity_blas == VK_NULL_HANDLE ) {
					Q_strncpyz( rtx.entity_blas_reason, "no_prior_blas", sizeof( rtx.entity_blas_reason ) );
				} else if ( rtx.entity_blas_built_prims != maxPrimEntity ) {
					Q_strncpyz( rtx.entity_blas_reason, "prim_count_changed", sizeof( rtx.entity_blas_reason ) );
				} else if ( !r_rtxEntityBlasUpdate || !r_rtxEntityBlasUpdate->integer ) {
					Q_strncpyz( rtx.entity_blas_reason, "r_rtxEntityBlasUpdate=0", sizeof( rtx.entity_blas_reason ) );
				} else {
					Q_strncpyz( rtx.entity_blas_reason, "unknown", sizeof( rtx.entity_blas_reason ) );
				}
				vk_rtx_retire_entity_blas();
				vk_rtx_alloc_buffer( sizeInfoBLAS.accelerationStructureSize,
					VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &rtx.entity_blas_buffer, &rtx.entity_blas_memory, NULL );

				Com_Memset( &asci, 0, sizeof( asci ) );
				asci.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
				asci.buffer = rtx.entity_blas_buffer;
				asci.size = sizeInfoBLAS.accelerationStructureSize;
				asci.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
				VK_CHECK( qvkCreateAccelerationStructureKHR( vk.device, &asci, NULL, &rtx.entity_blas ) );
			}

			addrInfo.accelerationStructure = rtx.entity_blas;
			entityBlasAddr = qvkGetAccelerationStructureDeviceAddressKHR( vk.device, &addrInfo );

			scratchSize = entityBlasUpdate ? sizeInfoBLAS.updateScratchSize : sizeInfoBLAS.buildScratchSize;
			if ( scratchSize == 0u ) {
				scratchSize = sizeInfoBLAS.buildScratchSize;
			}
			vk_rtx_alloc_scratch( scratchSize, &scratchAddr );

			Com_Memset( &rangeBLAS, 0, sizeof( rangeBLAS ) );
			rangeBLAS.primitiveCount = maxPrimEntity;
			pRangeBLAS = &rangeBLAS;
			buildInfoBLAS.mode = entityBlasUpdate
				? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR
				: VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
			buildInfoBLAS.srcAccelerationStructure = entityBlasUpdate ? rtx.entity_blas : VK_NULL_HANDLE;
			buildInfoBLAS.dstAccelerationStructure = rtx.entity_blas;
			buildInfoBLAS.scratchData.deviceAddress = scratchAddr;

			buildCmd = vk_begin_command_buffer();
			vk_rtx_host_to_as_barrier( buildCmd );
			qvkCmdBuildAccelerationStructuresKHR( buildCmd, 1, &buildInfoBLAS, &pRangeBLAS );
			vk_end_command_buffer( buildCmd, "rtx entity blas" );

			rtx.entity_blas_built_prims = maxPrimEntity;
			rtx.entity_blas_valid = qtrue;
			if ( entityBlasUpdate ) {
				static qboolean loggedUpdate;
				if ( !loggedUpdate ) {
					ri.Printf( PRINT_ALL, "[VK][RTX] entity BLAS UPDATE path active (r_rtxEntityBlasUpdate 1, stable prim count)\n" );
					loggedUpdate = qtrue;
				}
			}
		}
	} else {
		vk_rtx_retire_entity_blas();
		rtx.entity_packed_count = 0u;
		rtx.entity_primitive_count = 0u;
		rtx.entity_vertex_count = 0u;
		rtx.entity_albedo_count = 0u;
		rtx.entity_normal_count = 0u;
		Q_strncpyz( rtx.entity_blas_mode, "NONE", sizeof( rtx.entity_blas_mode ) );
		Q_strncpyz( rtx.entity_blas_reason, "no_entities", sizeof( rtx.entity_blas_reason ) );
	}

	maxInstTLAS = ( packedEnt > 0u && rtx.entity_blas != VK_NULL_HANDLE ) ? 2u : 1u;

	Com_Memset( instances, 0, sizeof( instances ) );
	instances[0].transform.matrix[0][0] = 1.0f;
	instances[0].transform.matrix[1][1] = 1.0f;
	instances[0].transform.matrix[2][2] = 1.0f;
	instances[0].instanceCustomIndex = 0;
	instances[0].mask = 0xFF;
	instances[0].instanceShaderBindingTableRecordOffset = 0;
	instances[0].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
	instances[0].accelerationStructureReference = worldBlasAddr;

	if ( maxInstTLAS > 1u ) {
		instances[1] = instances[0];
		instances[1].instanceCustomIndex = 1;
		instances[1].accelerationStructureReference = entityBlasAddr;
	}

	instBufSize = sizeof( instances[0] ) * maxInstTLAS;
	if ( rtx.instance_buffer == VK_NULL_HANDLE || rtx.tlas_instance_count != maxInstTLAS ) {
		vk_rtx_destroy_buffer( &rtx.instance_buffer, &rtx.instance_memory );
		vk_rtx_alloc_buffer( instBufSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&rtx.instance_buffer, &rtx.instance_memory, &instAddr );
	} else {
		VkBufferDeviceAddressInfo bda;

		Com_Memset( &bda, 0, sizeof( bda ) );
		bda.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		bda.buffer = rtx.instance_buffer;
		instAddr = qvkGetBufferDeviceAddress( vk.device, &bda );
	}
	VK_CHECK( qvkMapMemory( vk.device, rtx.instance_memory, 0, instBufSize, 0, (void **)&instMap ) );
	Com_Memcpy( instMap, instances, instBufSize );
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
	buildInfoTLAS.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
		| VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
	buildInfoTLAS.geometryCount = 1;
	buildInfoTLAS.pGeometries = &geometryTLAS;

	Com_Memset( &sizeInfoTLAS, 0, sizeof( sizeInfoTLAS ) );
	sizeInfoTLAS.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	qvkGetAccelerationStructureBuildSizesKHR( vk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&buildInfoTLAS, &maxInstTLAS, &sizeInfoTLAS );

	tlasCanUpdate = rtx.tlas_valid && rtx.tlas != VK_NULL_HANDLE
		&& rtx.tlas_instance_count == maxInstTLAS;
	tlasUpdate = tlasCanUpdate && r_rtxTlasUpdate && r_rtxTlasUpdate->integer;
	if ( tlasUpdate ) {
		Q_strncpyz( rtx.tlas_build_mode, "UPDATE", sizeof( rtx.tlas_build_mode ) );
		Q_strncpyz( rtx.tlas_rebuild_reason, "stable_instance_count", sizeof( rtx.tlas_rebuild_reason ) );
	} else {
		Q_strncpyz( rtx.tlas_build_mode, "REBUILD", sizeof( rtx.tlas_build_mode ) );
		if ( !rtx.tlas_valid || rtx.tlas == VK_NULL_HANDLE ) {
			Q_strncpyz( rtx.tlas_rebuild_reason, "no_prior_tlas", sizeof( rtx.tlas_rebuild_reason ) );
		} else if ( rtx.tlas_instance_count != maxInstTLAS ) {
			Q_strncpyz( rtx.tlas_rebuild_reason, "instance_count_changed", sizeof( rtx.tlas_rebuild_reason ) );
		} else if ( !r_rtxTlasUpdate || !r_rtxTlasUpdate->integer ) {
			Q_strncpyz( rtx.tlas_rebuild_reason, "r_rtxTlasUpdate=0", sizeof( rtx.tlas_rebuild_reason ) );
		} else {
			Q_strncpyz( rtx.tlas_rebuild_reason, "unknown", sizeof( rtx.tlas_rebuild_reason ) );
		}
	}

	if ( sizeInfoTLAS.buildScratchSize > scratchSize ) {
		scratchSize = sizeInfoTLAS.buildScratchSize;
	}
	if ( scratchSize > 0u ) {
		vk_rtx_alloc_scratch( scratchSize, &scratchAddr );
	}

	if ( !tlasUpdate ) {
		vk_rtx_retire_tlas();

		vk_rtx_alloc_buffer( sizeInfoTLAS.accelerationStructureSize,
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &rtx.tlas_buffer, &rtx.tlas_memory, NULL );

		Com_Memset( &asci, 0, sizeof( asci ) );
		asci.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		asci.buffer = rtx.tlas_buffer;
		asci.size = sizeInfoTLAS.accelerationStructureSize;
		asci.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		VK_CHECK( qvkCreateAccelerationStructureKHR( vk.device, &asci, NULL, &rtx.tlas ) );
	}

	Com_Memset( &rangeTLAS, 0, sizeof( rangeTLAS ) );
	rangeTLAS.primitiveCount = maxInstTLAS;
	pRangeTLAS = &rangeTLAS;
	buildInfoTLAS.mode = tlasUpdate
		? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR
		: VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfoTLAS.srcAccelerationStructure = tlasUpdate ? rtx.tlas : VK_NULL_HANDLE;
	buildInfoTLAS.dstAccelerationStructure = rtx.tlas;
	buildInfoTLAS.scratchData.deviceAddress = scratchAddr;

	buildCmd = vk_begin_command_buffer();
	vk_rtx_host_to_as_barrier( buildCmd );
	qvkCmdBuildAccelerationStructuresKHR( buildCmd, 1, &buildInfoTLAS, &pRangeTLAS );
	vk_end_command_buffer( buildCmd, "rtx entity tlas" );

	rtx.tlas_valid = qtrue;
	rtx.tlas_instance_count = maxInstTLAS;

	if ( tlasUpdate ) {
		static qboolean tlas_update_logged;
		if ( !tlas_update_logged ) {
			ri.Printf( PRINT_DEVELOPER, "[VK][RTX] TLAS UPDATE path active (r_rtxTlasUpdate 1, stable instance count)\n" );
			tlas_update_logged = qtrue;
		}
	} else {
		ri.Printf( PRINT_DEVELOPER, "[VK][RTX] TLAS REBUILD (%s)\n", rtx.tlas_rebuild_reason );
	}

	Com_Memset( &asWrite, 0, sizeof( asWrite ) );
	asWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	asWrite.accelerationStructureCount = 1;
	asWrite.pAccelerationStructures = &rtx.tlas;

	Com_Memset( &writeAS, 0, sizeof( writeAS ) );
	writeAS.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeAS.dstSet = rtx.descriptor_set;
	writeAS.dstBinding = 0;
	writeAS.descriptorCount = 1;
	writeAS.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	writeAS.pNext = &asWrite;
	qvkUpdateDescriptorSets( vk.device, 1, &writeAS, 0, NULL );

	if ( packedEnt > 0u ) {
		static qboolean entity_tlas_logged;
		if ( !entity_tlas_logged ) {
			ri.Printf( PRINT_ALL, "[VK][RTX] r_rtxEntities=1: entity mesh/AABB BLAS + TLAS (cap %u ents, mesh=%u proxy=%u tris=%u)\n",
				capEnt, rtx.entity_mesh_count, rtx.entity_proxy_count, rtx.entity_primitive_count );
			entity_tlas_logged = qtrue;
		}
	}
}

void vk_rtx_shutdown( void )
{
	if ( rtx.cmd_registered ) {
		ri.Cmd_RemoveCommand( "rtx_status" );
		rtx.cmd_registered = qfalse;
	}

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

	if ( rtx.rtx_ubo != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, rtx.rtx_ubo, NULL );
		rtx.rtx_ubo = VK_NULL_HANDLE;
	}
	if ( rtx.rtx_ubo_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, rtx.rtx_ubo_memory, NULL );
		rtx.rtx_ubo_memory = VK_NULL_HANDLE;
	}
	rtx.rtx_ubo_ptr = NULL;

	vk_rtx_flush_retired_as();
	vk_rtx_destroy_as( &rtx.tlas );
	vk_rtx_destroy_as( &rtx.blas );
	vk_rtx_destroy_entity_blas();
	vk_rtx_destroy_buffer( &rtx.tlas_buffer, &rtx.tlas_memory );
	vk_rtx_destroy_buffer( &rtx.instance_buffer, &rtx.instance_memory );
	vk_rtx_destroy_buffer( &rtx.blas_buffer, &rtx.blas_memory );
	vk_rtx_destroy_buffer( &rtx.vertex_buffer, &rtx.vertex_memory );
	vk_rtx_destroy_buffer( &rtx.index_buffer, &rtx.index_memory );
	vk_rtx_destroy_buffer( &rtx.albedo_buffer, &rtx.albedo_memory );
	vk_rtx_destroy_buffer( &rtx.normal_buffer, &rtx.normal_memory );
	vk_rtx_destroy_buffer( &rtx.scratch_buffer, &rtx.scratch_memory );
	vk_rtx_destroy_buffer( &rtx.sbt_buffer, &rtx.sbt_memory );

	vk_rtx_destroy_rt_output();

	Com_Memset( &rtx, 0, sizeof( rtx ) );
	rtx.world_name[0] = '\0';
}

void vk_rtx_init( void )
{
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps;
	VkPhysicalDeviceAccelerationStructurePropertiesKHR asProps;
	VkPhysicalDeviceProperties2 props2;
	VkDescriptorSetLayoutBinding bindings[6];
	VkDescriptorSetLayoutCreateInfo dslci;
	VkDescriptorPoolSize poolSizes[4];
	VkDescriptorPoolCreateInfo pci;
	VkPipelineLayoutCreateInfo plci;
	VkDescriptorSetAllocateInfo allocInfo;
	VkPipelineShaderStageCreateInfo stages[3];
	VkRayTracingShaderGroupCreateInfoKHR groups[3];
	VkRayTracingPipelineCreateInfoKHR rtpci;
	VkWriteDescriptorSet writes[5];
	VkDescriptorBufferInfo uboInfo;
	VkDescriptorImageInfo depthInfo;
	VkMemoryRequirements uboReq;
	VkMemoryAllocateInfo uboAi;
	VkBufferCreateInfo uboBi;
	Vk_Sampler_Def sd;
	uint32_t uboMemType;
	VkDeviceSize uboAllocSize;
	VkDeviceSize sbtSize;
	uint32_t w, h;
	uint8_t *sbtHost;
	size_t hbufSize;
	int gi;
	int sampleCount;
	VkResult pipeRes;

	vk_rtx_shutdown();
	if ( !rtx.cmd_registered ) {
		ri.Cmd_AddCommand( "rtx_status", RTX_Status_f );
		rtx.cmd_registered = qtrue;
	}

	if ( !vk.rtxAvailable ) {
		ri.Printf( PRINT_DEVELOPER, "[VK][RTX] %s\n", vk_rtx_state_string() );
		return;
	}
	if ( ( !r_rtx || r_rtx->integer <= 0 ) && ( !r_hybrid1 || r_hybrid1->integer <= 0 )
		&& ( !r_raygun || r_raygun->integer <= 0 ) ) {
		ri.Printf( PRINT_DEVELOPER, "[VK][RTX] %s\n", vk_rtx_state_string() );
		return;
	}

	if ( ( !r_rtxDemo || !r_rtxDemo->integer ) && ( !r_hybrid1 || r_hybrid1->integer <= 0 )
		&& ( !r_raygun || r_raygun->integer <= 0 ) ) {
		ri.Printf( PRINT_DEVELOPER, "[VK][RTX] %s; skipping RT AS/pipeline init\n", vk_rtx_state_string() );
		return;
	}

	vk_rtx_get_trace_extent( &w, &h );

	Com_Memset( &rtProps, 0, sizeof( rtProps ) );
	rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
	Com_Memset( &asProps, 0, sizeof( asProps ) );
	asProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
	asProps.pNext = &rtProps;
	Com_Memset( &props2, 0, sizeof( props2 ) );
	props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	props2.pNext = &asProps;
	if ( qvkGetPhysicalDeviceProperties2 ) {
		qvkGetPhysicalDeviceProperties2( vk.physical_device, &props2 );
	} else {
		rtProps.shaderGroupHandleSize = 32;
		rtProps.shaderGroupHandleAlignment = 32;
		rtProps.shaderGroupBaseAlignment = 64;
		asProps.minAccelerationStructureScratchOffsetAlignment = 256;
	}
	rtx.handle_size = rtProps.shaderGroupHandleSize;
	rtx.shader_group_base_alignment = rtProps.shaderGroupBaseAlignment;
	rtx.scratch_alignment = asProps.minAccelerationStructureScratchOffsetAlignment
		? asProps.minAccelerationStructureScratchOffsetAlignment : 256u;

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
	bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[5].descriptorCount = 1;
	bindings[5].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	Com_Memset( &dslci, 0, sizeof( dslci ) );
	dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dslci.bindingCount = 6;
	dslci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &dslci, NULL, &rtx.dsl ) );

	poolSizes[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	poolSizes[0].descriptorCount = 1;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSizes[1].descriptorCount = 1;
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[2].descriptorCount = 1;
	poolSizes[3].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[3].descriptorCount = 3;
	Com_Memset( &pci, 0, sizeof( pci ) );
	pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pci.maxSets = 1;
	pci.poolSizeCount = 4;
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

	uboAllocSize = (VkDeviceSize)PAD( (uint32_t)sizeof( VkRtxFrameUBO_t ), (uint32_t)vk.uniform_alignment );
	Com_Memset( &uboBi, 0, sizeof( uboBi ) );
	uboBi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	uboBi.size = uboAllocSize;
	uboBi.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	uboBi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &uboBi, NULL, &rtx.rtx_ubo ) );
	qvkGetBufferMemoryRequirements( vk.device, rtx.rtx_ubo, &uboReq );
	uboMemType = vk_find_memory_type( vk.physical_device, uboReq.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	Com_Memset( &uboAi, 0, sizeof( uboAi ) );
	uboAi.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	uboAi.allocationSize = uboReq.size;
	uboAi.memoryTypeIndex = uboMemType;
	VK_CHECK( qvkAllocateMemory( vk.device, &uboAi, NULL, &rtx.rtx_ubo_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, rtx.rtx_ubo, rtx.rtx_ubo_memory, 0 ) );
	VK_CHECK( qvkMapMemory( vk.device, rtx.rtx_ubo_memory, 0, uboAllocSize, 0, &rtx.rtx_ubo_ptr ) );
	Com_Memset( rtx.rtx_ubo_ptr, 0, (size_t)uboAllocSize );

	Com_Memset( &uboInfo, 0, sizeof( uboInfo ) );
	uboInfo.buffer = rtx.rtx_ubo;
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
	writes[0].dstSet = rtx.descriptor_set;
	writes[0].dstBinding = 2;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writes[0].pBufferInfo = &uboInfo;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = rtx.descriptor_set;
	writes[1].dstBinding = 3;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].pImageInfo = &depthInfo;
	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );
	vk_rtx_update_color_descriptor();
	vk_fsa_write_rtx_importance_descriptor( rtx.descriptor_set );

	vk_rtx_rebuild_world_blas();

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
	sampleCount = ( r_rtxSamples && r_rtxSamples->integer > 0 ) ? r_rtxSamples->integer : 1;
	if ( sampleCount > 8 ) {
		sampleCount = 8;
	}
	ri.Printf( PRINT_ALL, "[VK][RTX] Ray pass ready (r_rtx=%d, samples=%d, blend=%.2f): world BLAS when map loaded, else fallback triangle; rtx_status for TLAS/BLAS state\n",
		r_rtx->integer, sampleCount, r_rtxComposite ? r_rtxComposite->value : 0.0f );
}

void vk_rtx_frame_begin( void )
{
	uint32_t w, h;

	if ( !rtx.ready ) {
		return;
	}
	if ( ( !r_rtxDemo || !r_rtxDemo->integer ) && ( !r_hybrid1 || r_hybrid1->integer <= 0 )
		&& ( !r_raygun || r_raygun->integer <= 0 ) ) {
		return;
	}

	/* Rebuild world BLAS at frame start (queue idle, no open frame CB). Mid-pass
	 * destroy+rebuild of TLAS/BLAS while recording caused NVIDIA DEVICE_LOST. */
	vk_rtx_rebuild_world_blas();

	vk_rtx_get_trace_extent( &w, &h );
	if ( w == rtx.width && h == rtx.height ) {
		return;
	}

	vk_rtx_create_rt_output( w, h, rtx.descriptor_set );
	vk_rtx_update_color_descriptor();
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
	VkRtxFrameUBO_t frameUbo;
	float viewProj[16];
	float zNear, zFar;
	VkImageAspectFlags depthAspect;
	uint32_t preBarrierCount;

	if ( !rtx.ready || !cmd || !r_rtxDemo || !r_rtxDemo->integer ) {
		return;
	}

	/* World BLAS is rebuilt in vk_rtx_frame_begin; only refresh entity TLAS here. */
	vk_rtx_rebuild_entity_tlas();
	if ( rtx.tlas == VK_NULL_HANDLE ) {
		return;
	}

	if ( rtx.rtx_ubo_ptr ) {
		const float *view = backEnd.viewParms.world.modelViewMatrix;
		const float *projection = backEnd.useFirstPersonProjection
			? backEnd.firstPersonProjectionMatrix
			: backEnd.viewParms.projectionMatrix;
		float proj_vk[16];

		vk_get_projection_matrix_vk( projection, proj_vk );
		myGlMultMatrix( view, proj_vk, viewProj );
		if ( !vk_mat4_inverse( viewProj, frameUbo.invViewProj ) ) {
			Com_Memcpy( frameUbo.invViewProj, viewProj, sizeof( frameUbo.invViewProj ) );
		}
		frameUbo.viewOrigin[0] = backEnd.viewParms.or.origin[0];
		frameUbo.viewOrigin[1] = backEnd.viewParms.or.origin[1];
		frameUbo.viewOrigin[2] = backEnd.viewParms.or.origin[2];
		frameUbo.viewOrigin[3] = 0.0f;
		zNear = ( r_znear && r_znear->value > 0.0f ) ? r_znear->value : 8.0f;
		zFar = backEnd.viewParms.zFar;
		if ( zFar <= zNear ) {
			zFar = zNear + 100.0f;
		}
		frameUbo.zNearFar[0] = zNear;
		frameUbo.zNearFar[1] = zFar;
		frameUbo.zNearFar[2] = 0.0f;
		frameUbo.zNearFar[3] = 0.0f;
		frameUbo.outputSize[0] = (float)rtx.width;
		frameUbo.outputSize[1] = (float)rtx.height;
		{
			int rtxMode = ( r_rtx && r_rtx->integer > 0 ) ? r_rtx->integer : 0;
			if ( rtxMode < 0 ) {
				rtxMode = 0;
			}
			if ( rtxMode > 3 ) {
				rtxMode = 3;
			}
			frameUbo.outputSize[2] = (float)rtxMode;
		}
		frameUbo.outputSize[3] = r_rtxComposite ? r_rtxComposite->value : 0.0f;
		if ( frameUbo.outputSize[3] < 0.0f ) {
			frameUbo.outputSize[3] = 0.0f;
		}
		if ( frameUbo.outputSize[3] > 1.0f ) {
			frameUbo.outputSize[3] = 1.0f;
		}
		frameUbo.traceParams[0] = ( r_rtxSamples && r_rtxSamples->integer > 0 ) ? (float)r_rtxSamples->integer : 1.0f;
		if ( frameUbo.traceParams[0] > 8.0f ) {
			frameUbo.traceParams[0] = 8.0f;
		}
		vk_fsa_patch_rtx_trace_params( frameUbo.traceParams, (uint32_t)tr.frameCount );
		Com_Memcpy( rtx.rtx_ubo_ptr, &frameUbo, sizeof( frameUbo ) );
	}

	vk_fsa_write_rtx_importance_descriptor( rtx.descriptor_set );

	depthAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 ) {
		depthAspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	if ( vk.depth_image != VK_NULL_HANDLE && vk.renderPassIndex == RENDER_PASS_MAIN ) {
		record_depth_image_layout_transition( cmd, depthAspect,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, 0, 0 );
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
	barriers[0].oldLayout = rtx.rt_image_traced ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].image = rtx.rt_image;
	barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barriers[0].subresourceRange.levelCount = 1;
	barriers[0].subresourceRange.layerCount = 1;
	barriers[0].srcAccessMask = rtx.rt_image_traced ? VK_ACCESS_SHADER_WRITE_BIT : 0;
	barriers[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

	preBarrierCount = 1;
	if ( vk.color_image != VK_NULL_HANDLE ) {
		barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barriers[1].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barriers[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[1].image = vk.color_image;
		barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barriers[1].subresourceRange.levelCount = 1;
		barriers[1].subresourceRange.layerCount = 1;
		barriers[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		preBarrierCount = 2;
	}

	qvkCmdPipelineBarrier( cmd,
		( rtx.rt_image_traced ? VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT ) |
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
		0, 0, NULL, 0, NULL, preBarrierCount, barriers );

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

	rtx.rt_image_traced = qtrue;

	if ( vk.depth_image != VK_NULL_HANDLE && vk.renderPassIndex == RENDER_PASS_MAIN ) {
		record_depth_image_layout_transition( cmd, depthAspect,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, 0 );
	}
}

static void vk_rtx_write_tlas_descriptor( VkDescriptorSet set )
{
	VkWriteDescriptorSetAccelerationStructureKHR asWrite;
	VkWriteDescriptorSet writeAS;

	if ( set == VK_NULL_HANDLE || rtx.tlas == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &asWrite, 0, sizeof( asWrite ) );
	asWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	asWrite.accelerationStructureCount = 1;
	asWrite.pAccelerationStructures = &rtx.tlas;

	Com_Memset( &writeAS, 0, sizeof( writeAS ) );
	writeAS.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeAS.dstSet = set;
	writeAS.dstBinding = 0;
	writeAS.descriptorCount = 1;
	writeAS.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	writeAS.pNext = &asWrite;
	qvkUpdateDescriptorSets( vk.device, 1, &writeAS, 0, NULL );
}

qboolean vk_rtx_scene_ready( void )
{
	return rtx.ready && rtx.tlas != VK_NULL_HANDLE;
}

void vk_rtx_scene_prepare( void )
{
	if ( !rtx.ready ) {
		return;
	}
	/* World BLAS: vk_rtx_frame_begin. Entity TLAS may change every view. */
	vk_rtx_rebuild_entity_tlas();
}

void vk_rtx_scene_extent( uint32_t *w, uint32_t *h )
{
	vk_rtx_get_trace_extent( w, h );
}

void vk_rtx_bind_tlas_descriptor( VkDescriptorSet set )
{
	vk_rtx_write_tlas_descriptor( set );
}

void vk_rtx_bind_world_albedo_ssbo( VkDescriptorSet set, uint32_t binding )
{
	VkDescriptorBufferInfo info;
	VkWriteDescriptorSet write;
	VkDeviceSize rangeBytes;

	if ( set == VK_NULL_HANDLE || rtx.albedo_buffer == VK_NULL_HANDLE ) {
		return;
	}

	rangeBytes = (VkDeviceSize)rtx.world_albedo_count * 3u * sizeof( float );
	if ( rangeBytes < 3u * sizeof( float ) ) {
		rangeBytes = 3u * sizeof( float );
	}

	Com_Memset( &info, 0, sizeof( info ) );
	info.buffer = rtx.albedo_buffer;
	info.offset = 0;
	info.range = rangeBytes;

	Com_Memset( &write, 0, sizeof( write ) );
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = set;
	write.dstBinding = binding;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	write.pBufferInfo = &info;
	qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
}

uint32_t vk_rtx_world_albedo_count( void )
{
	return rtx.world_albedo_count;
}

void vk_rtx_bind_world_normal_ssbo( VkDescriptorSet set, uint32_t binding )
{
	VkDescriptorBufferInfo info;
	VkWriteDescriptorSet write;
	VkDeviceSize rangeBytes;

	if ( set == VK_NULL_HANDLE || rtx.normal_buffer == VK_NULL_HANDLE ) {
		return;
	}

	rangeBytes = (VkDeviceSize)rtx.world_normal_count * 3u * sizeof( float );
	if ( rangeBytes < 3u * sizeof( float ) ) {
		rangeBytes = 3u * sizeof( float );
	}

	Com_Memset( &info, 0, sizeof( info ) );
	info.buffer = rtx.normal_buffer;
	info.offset = 0;
	info.range = rangeBytes;

	Com_Memset( &write, 0, sizeof( write ) );
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = set;
	write.dstBinding = binding;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	write.pBufferInfo = &info;
	qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
}

uint32_t vk_rtx_world_normal_count( void )
{
	return rtx.world_normal_count;
}

void vk_rtx_bind_entity_albedo_ssbo( VkDescriptorSet set, uint32_t binding )
{
	VkDescriptorBufferInfo info;
	VkWriteDescriptorSet write;
	VkDeviceSize rangeBytes;

	if ( set == VK_NULL_HANDLE || rtx.entity_albedo_buffer == VK_NULL_HANDLE ) {
		return;
	}

	rangeBytes = (VkDeviceSize)rtx.entity_albedo_count * 3u * sizeof( float );
	if ( rangeBytes < 3u * sizeof( float ) ) {
		rangeBytes = 3u * sizeof( float );
	}

	Com_Memset( &info, 0, sizeof( info ) );
	info.buffer = rtx.entity_albedo_buffer;
	info.offset = 0;
	info.range = rangeBytes;

	Com_Memset( &write, 0, sizeof( write ) );
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = set;
	write.dstBinding = binding;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	write.pBufferInfo = &info;
	qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
}

uint32_t vk_rtx_entity_albedo_count( void )
{
	return rtx.entity_albedo_count;
}

void vk_rtx_bind_entity_normal_ssbo( VkDescriptorSet set, uint32_t binding )
{
	VkDescriptorBufferInfo info;
	VkWriteDescriptorSet write;
	VkDeviceSize rangeBytes;

	if ( set == VK_NULL_HANDLE || rtx.entity_normal_buffer == VK_NULL_HANDLE ) {
		return;
	}

	rangeBytes = (VkDeviceSize)rtx.entity_normal_count * 3u * sizeof( float );
	if ( rangeBytes < 3u * sizeof( float ) ) {
		rangeBytes = 3u * sizeof( float );
	}

	Com_Memset( &info, 0, sizeof( info ) );
	info.buffer = rtx.entity_normal_buffer;
	info.offset = 0;
	info.range = rangeBytes;

	Com_Memset( &write, 0, sizeof( write ) );
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = set;
	write.dstBinding = binding;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	write.pBufferInfo = &info;
	qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
}

uint32_t vk_rtx_entity_normal_count( void )
{
	return rtx.entity_normal_count;
}

#else /* !USE_VULKAN_RTX */

void vk_rtx_init( void )
{
	static qboolean s_logged;

	if ( !s_logged ) {
		ri.Printf( PRINT_ALL, "[VK][RTX] stub (build with -DUSE_VULKAN_RTX=ON)\n" );
		s_logged = qtrue;
	}
}
void vk_rtx_shutdown( void ) {}
void vk_rtx_frame_begin( void ) {}
void vk_rtx_record_demo_pass( VkCommandBuffer cmd ) { (void)cmd; }
qboolean vk_rtx_scene_ready( void ) { return qfalse; }
void vk_rtx_scene_prepare( void ) {}
void vk_rtx_scene_extent( uint32_t *w, uint32_t *h ) { if ( w ) { *w = 1u; } if ( h ) { *h = 1u; } }
void vk_rtx_bind_tlas_descriptor( VkDescriptorSet set ) { (void)set; }
void vk_rtx_bind_world_albedo_ssbo( VkDescriptorSet set, uint32_t binding ) { (void)set; (void)binding; }
uint32_t vk_rtx_world_albedo_count( void ) { return 0u; }
void vk_rtx_bind_world_normal_ssbo( VkDescriptorSet set, uint32_t binding ) { (void)set; (void)binding; }
uint32_t vk_rtx_world_normal_count( void ) { return 0u; }
void vk_rtx_bind_entity_albedo_ssbo( VkDescriptorSet set, uint32_t binding ) { (void)set; (void)binding; }
uint32_t vk_rtx_entity_albedo_count( void ) { return 0u; }
void vk_rtx_bind_entity_normal_ssbo( VkDescriptorSet set, uint32_t binding ) { (void)set; (void)binding; }
uint32_t vk_rtx_entity_normal_count( void ) { return 0u; }

#endif /* USE_VULKAN_RTX */
