/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

CuRast — Vulkan compute software rasterization scaffold.
Schütz, Lipp, Kristmann & Wimmer, arXiv:2604.21749.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_curast.h"
#include "vk_util.h"
#include "vk_image_layout.h"
#include "vk_cmd.h"
#include "vk_view_state.h"

#define CURAST_MAX_TRIS         65536u
#define CURAST_TRI_STRIDE       48u
#define CURAST_MAX_SMALL_PX     128
#define CURAST_TRI_BITS         16
#define CURAST_DEFAULT_W        640u
#define CURAST_DEFAULT_H        480u

typedef struct {
	vec4_t v0;
	vec4_t v1;
	vec4_t v2;
} curastTriangle_t;

typedef struct {
	uint32_t width;
	uint32_t height;
	uint32_t tri_count;
	uint32_t frames;
	uint32_t stage1_tris;
	uint32_t stage2_tris;
	uint32_t stage3_tris;
	uint32_t near_plane_tris;
	qboolean ready;
} curastState_t;

static curastState_t curast;

static cvar_t *r_curast;
static cvar_t *r_curast_tris;
static cvar_t *r_curast_width;
static cvar_t *r_curast_height;
static cvar_t *r_curast_debug;

typedef struct {
	VkDescriptorSetLayout layout;
	VkPipelineLayout pipeline_layout;
	VkPipeline pipeline;
	VkDescriptorPool pool;
	VkDescriptorSet descriptor;
	qboolean ready;
} curastPipeline_t;

static curastPipeline_t curast_clear;
static curastPipeline_t curast_stage1;
static curastPipeline_t curast_resolve;

static void CuRast_DestroyGpu( void );
static void CuRast_EnsurePipelines( void );
static qboolean CuRast_EnsureBuffers( void );
static qboolean CuRast_SeedMesh( void );
static void CuRast_AnalyzeStageRouting( void );

static qboolean CuRast_ProjectVertex( const vec4_t in, vec2_t out )
{
	float f;
	float aspect;
	float clipX;
	float clipY;
	float clipW;
	float ndcX;
	float ndcY;

	if ( in[2] <= 0.01f ) {
		return qfalse;
	}

	f = 1.0f / tanf( 45.0f * 0.5f * (float)M_PI / 180.0f );
	aspect = (float)curast.width / (float)curast.height;
	clipX = ( f / aspect ) * in[0];
	clipY = f * in[1];
	clipW = in[2];
	if ( fabsf( clipW ) < 1e-6f ) {
		return qfalse;
	}

	ndcX = clipX / clipW;
	ndcY = clipY / clipW;

	out[0] = ( ndcX * 0.5f + 0.5f ) * (float)curast.width;
	out[1] = ( -ndcY * 0.5f + 0.5f ) * (float)curast.height;
	return qtrue;
}

static void CuRast_ClassifyTriangle( const curastTriangle_t *tri )
{
	vec2_t p0, p1, p2;
	float minX, minY, maxX, maxY;
	float bboxArea;

	if ( !tri ) {
		return;
	}

	if ( !CuRast_ProjectVertex( tri->v0, p0 ) ||
	     !CuRast_ProjectVertex( tri->v1, p1 ) ||
	     !CuRast_ProjectVertex( tri->v2, p2 ) ) {
		curast.near_plane_tris++;
		return;
	}

	minX = p0[0];
	maxX = p0[0];
	minY = p0[1];
	maxY = p0[1];

	if ( p1[0] < minX ) minX = p1[0];
	if ( p1[0] > maxX ) maxX = p1[0];
	if ( p1[1] < minY ) minY = p1[1];
	if ( p1[1] > maxY ) maxY = p1[1];
	if ( p2[0] < minX ) minX = p2[0];
	if ( p2[0] > maxX ) maxX = p2[0];
	if ( p2[1] < minY ) minY = p2[1];
	if ( p2[1] > maxY ) maxY = p2[1];

	bboxArea = ( maxX - minX ) * ( maxY - minY );
	if ( bboxArea < (float)CURAST_MAX_SMALL_PX ) {
		curast.stage1_tris++;
	} else if ( bboxArea < 4096.0f ) {
		curast.stage2_tris++;
	} else {
		curast.stage3_tris++;
	}
}

static void CuRast_AnalyzeStageRouting( void )
{
	curastTriangle_t *host;
	uint32_t i;
	void *mapped;
	VkDeviceSize size;

	curast.stage1_tris = 0;
	curast.stage2_tris = 0;
	curast.stage3_tris = 0;
	curast.near_plane_tris = 0;

	if ( vk.curast.tri_memory == VK_NULL_HANDLE || curast.tri_count == 0 ) {
		return;
	}

	size = (VkDeviceSize)curast.tri_count * sizeof( curastTriangle_t );
	if ( qvkMapMemory( vk.device, vk.curast.tri_memory, 0, size, 0, &mapped ) != VK_SUCCESS ) {
		return;
	}

	host = (curastTriangle_t *)mapped;
	for ( i = 0; i < curast.tri_count; i++ ) {
		CuRast_ClassifyTriangle( &host[i] );
	}

	qvkUnmapMemory( vk.device, vk.curast.tri_memory );
}

static void CuRast_DestroyPipeline( curastPipeline_t *p )
{
	if ( !p ) {
		return;
	}
	if ( p->pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, p->pipeline, NULL );
	}
	if ( p->pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, p->pipeline_layout, NULL );
	}
	if ( p->layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, p->layout, NULL );
	}
	if ( p->pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, p->pool, NULL );
	}
	Com_Memset( p, 0, sizeof( *p ) );
}

static qboolean CuRast_CreateComputePipeline( curastPipeline_t *p, VkShaderModule module,
	const VkDescriptorSetLayoutBinding *bindings, uint32_t bindingCount, uint32_t pushSize )
{
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;
	VkDescriptorPoolCreateInfo pci;
	VkDescriptorSetAllocateInfo ai;
	uint32_t bufCount = 0;
	uint32_t imgCount = 0;
	uint32_t poolCount = 0;
	uint32_t i;
	VkDescriptorPoolSize poolSizes[2];

	if ( !p || module == VK_NULL_HANDLE || p->ready ) {
		return qfalse;
	}

	Com_Memset( &layout_ci, 0, sizeof( layout_ci ) );
	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.bindingCount = bindingCount;
	layout_ci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &p->layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = pushSize;

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &p->layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &p->pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = module;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = p->pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &p->pipeline ) );

	for ( i = 0; i < bindingCount; i++ ) {
		if ( bindings[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ) {
			bufCount += bindings[i].descriptorCount;
		} else if ( bindings[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ) {
			imgCount += bindings[i].descriptorCount;
		}
	}
	if ( bufCount > 0 ) {
		poolSizes[poolCount].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		poolSizes[poolCount].descriptorCount = bufCount;
		poolCount++;
	}
	if ( imgCount > 0 ) {
		poolSizes[poolCount].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		poolSizes[poolCount].descriptorCount = imgCount;
		poolCount++;
	}
	if ( poolCount == 0 ) {
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		poolSizes[0].descriptorCount = 1;
		poolCount = 1;
	}

	Com_Memset( &pci, 0, sizeof( pci ) );
	pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pci.maxSets = 1;
	pci.poolSizeCount = poolCount;
	pci.pPoolSizes = poolSizes;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &pci, NULL, &p->pool ) );

	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	ai.descriptorPool = p->pool;
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &p->layout;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &ai, &p->descriptor ) );

	p->ready = qtrue;
	return qtrue;
}

static void CuRast_EnsurePipelines( void )
{
	VkDescriptorSetLayoutBinding bindings[3];

	if ( curast_clear.ready && curast_stage1.ready && curast_resolve.ready ) {
		return;
	}

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	(void)CuRast_CreateComputePipeline( &curast_clear, vk.modules.curast_clear_cs,
		bindings, 1, 16 );

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	(void)CuRast_CreateComputePipeline( &curast_stage1, vk.modules.curast_stage1_cs,
		bindings, 2, 48 );

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	(void)CuRast_CreateComputePipeline( &curast_resolve, vk.modules.curast_resolve_cs,
		bindings, 3, 32 );
}

static qboolean CuRast_EnsureImages( uint32_t w, uint32_t h )
{
	if ( vk.curast.vis_image != VK_NULL_HANDLE &&
		curast.width == w && curast.height == h ) {
		return qtrue;
	}

	if ( vk.curast.vis_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.curast.vis_view, NULL );
		qvkDestroyImage( vk.device, vk.curast.vis_image, NULL );
		qvkFreeMemory( vk.device, vk.curast.vis_memory, NULL );
	}
	if ( vk.curast.color_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.curast.color_view, NULL );
		qvkDestroyImage( vk.device, vk.curast.color_image, NULL );
		qvkFreeMemory( vk.device, vk.curast.color_memory, NULL );
	}
	vk.curast.vis_view = VK_NULL_HANDLE;
	vk.curast.vis_image = VK_NULL_HANDLE;
	vk.curast.vis_memory = VK_NULL_HANDLE;
	vk.curast.color_view = VK_NULL_HANDLE;
	vk.curast.color_image = VK_NULL_HANDLE;
	vk.curast.color_memory = VK_NULL_HANDLE;

	{
		VkImageCreateInfo image_desc;
		VkImageViewCreateInfo view_desc;
		VkMemoryRequirements mem_req;
		VkMemoryAllocateInfo alloc_info;

		Com_Memset( &image_desc, 0, sizeof( image_desc ) );
		image_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_desc.imageType = VK_IMAGE_TYPE_2D;
		image_desc.format = VK_FORMAT_R32_UINT;
		image_desc.extent.width = w;
		image_desc.extent.height = h;
		image_desc.extent.depth = 1;
		image_desc.mipLevels = 1;
		image_desc.arrayLayers = 1;
		image_desc.samples = VK_SAMPLE_COUNT_1_BIT;
		image_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
		image_desc.usage = VK_IMAGE_USAGE_STORAGE_BIT;
		image_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VK_CHECK( qvkCreateImage( vk.device, &image_desc, NULL, &vk.curast.vis_image ) );
		qvkGetImageMemoryRequirements( vk.device, vk.curast.vis_image, &mem_req );
		Com_Memset( &alloc_info, 0, sizeof( alloc_info ) );
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = mem_req.size;
		alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.curast.vis_memory ) );
		VK_CHECK( qvkBindImageMemory( vk.device, vk.curast.vis_image, vk.curast.vis_memory, 0 ) );
		Com_Memset( &view_desc, 0, sizeof( view_desc ) );
		view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_desc.image = vk.curast.vis_image;
		view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_desc.format = VK_FORMAT_R32_UINT;
		view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view_desc.subresourceRange.levelCount = 1;
		view_desc.subresourceRange.layerCount = 1;
		VK_CHECK( qvkCreateImageView( vk.device, &view_desc, NULL, &vk.curast.vis_view ) );

		image_desc.format = VK_FORMAT_R8G8B8A8_UNORM;
		VK_CHECK( qvkCreateImage( vk.device, &image_desc, NULL, &vk.curast.color_image ) );
		qvkGetImageMemoryRequirements( vk.device, vk.curast.color_image, &mem_req );
		alloc_info.allocationSize = mem_req.size;
		VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.curast.color_memory ) );
		VK_CHECK( qvkBindImageMemory( vk.device, vk.curast.color_image, vk.curast.color_memory, 0 ) );
		view_desc.image = vk.curast.color_image;
		view_desc.format = VK_FORMAT_R8G8B8A8_UNORM;
		VK_CHECK( qvkCreateImageView( vk.device, &view_desc, NULL, &vk.curast.color_view ) );
	}

	curast.width = w;
	curast.height = h;
	return qtrue;
}

static qboolean CuRast_EnsureBuffers( void )
{
	VkDeviceSize triBytes;
	uint32_t count;
	uint32_t w;
	uint32_t h;

	count = r_curast_tris ? (uint32_t)r_curast_tris->integer : 8192u;
	if ( count < 64u ) {
		count = 64u;
	}
	if ( count > CURAST_MAX_TRIS ) {
		count = CURAST_MAX_TRIS;
	}

	w = r_curast_width ? (uint32_t)r_curast_width->integer : CURAST_DEFAULT_W;
	h = r_curast_height ? (uint32_t)r_curast_height->integer : CURAST_DEFAULT_H;
	if ( w < 64u ) {
		w = 64u;
	}
	if ( h < 64u ) {
		h = 64u;
	}
	if ( w > 4096u ) {
		w = 4096u;
	}
	if ( h > 4096u ) {
		h = 4096u;
	}

	if ( vk.curast.tri_buffer != VK_NULL_HANDLE && curast.tri_count == count &&
		curast.width == w && curast.height == h ) {
		return qtrue;
	}

	CuRast_DestroyGpu();

	triBytes = (VkDeviceSize)count * CURAST_TRI_STRIDE;
	{
		VkBufferCreateInfo bi;
		VkMemoryRequirements req;
		VkMemoryAllocateInfo ai;

		Com_Memset( &bi, 0, sizeof( bi ) );
		bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bi.size = triBytes;
		bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		VK_CHECK( qvkCreateBuffer( vk.device, &bi, NULL, &vk.curast.tri_buffer ) );
		qvkGetBufferMemoryRequirements( vk.device, vk.curast.tri_buffer, &req );
		Com_Memset( &ai, 0, sizeof( ai ) );
		ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		ai.allocationSize = req.size;
		ai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, &vk.curast.tri_memory ) );
		VK_CHECK( qvkBindBufferMemory( vk.device, vk.curast.tri_buffer, vk.curast.tri_memory, 0 ) );
	}

	if ( !CuRast_EnsureImages( w, h ) ) {
		return qfalse;
	}

	curast.tri_count = count;
	curast.ready = qtrue;
	return CuRast_SeedMesh();
}

static qboolean CuRast_SeedMesh( void )
{
	curastTriangle_t *host;
	uint32_t grid;
	uint32_t i;
	uint32_t triIdx;
	void *mapped;
	VkDeviceSize size;

	grid = (uint32_t)sqrtf( (float)curast.tri_count * 2.0f );
	if ( grid < 8u ) {
		grid = 8u;
	}

	size = (VkDeviceSize)curast.tri_count * sizeof( curastTriangle_t );
	host = (curastTriangle_t *)ri.Hunk_AllocateTempMemory( (int)size );
	if ( !host ) {
		return qfalse;
	}

	triIdx = 0;
	for ( i = 0; i + 1 < grid && triIdx + 1 < curast.tri_count; i++ ) {
		uint32_t j;

		for ( j = 0; j + 1 < grid && triIdx + 1 < curast.tri_count; j++ ) {
			float x0 = (float)i / (float)grid * 80.0f - 40.0f;
			float y0 = (float)j / (float)grid * 60.0f - 30.0f;
			float x1 = (float)( i + 1 ) / (float)grid * 80.0f - 40.0f;
			float y1 = (float)( j + 1 ) / (float)grid * 60.0f - 30.0f;
			float z = 100.0f + sinf( x0 * 0.05f ) * 4.0f;

			host[triIdx].v0[0] = x0; host[triIdx].v0[1] = y0; host[triIdx].v0[2] = z; host[triIdx].v0[3] = 1.0f;
			host[triIdx].v1[0] = x1; host[triIdx].v1[1] = y0; host[triIdx].v1[2] = z; host[triIdx].v1[3] = 1.0f;
			host[triIdx].v2[0] = x0; host[triIdx].v2[1] = y1; host[triIdx].v2[2] = z; host[triIdx].v2[3] = 1.0f;
			triIdx++;

			if ( triIdx >= curast.tri_count ) {
				break;
			}

			host[triIdx].v0[0] = x1; host[triIdx].v0[1] = y0; host[triIdx].v0[2] = z; host[triIdx].v0[3] = 1.0f;
			host[triIdx].v1[0] = x1; host[triIdx].v1[1] = y1; host[triIdx].v1[2] = z; host[triIdx].v1[3] = 1.0f;
			host[triIdx].v2[0] = x0; host[triIdx].v2[1] = y1; host[triIdx].v2[2] = z; host[triIdx].v2[3] = 1.0f;
			triIdx++;
		}
	}

	while ( triIdx < curast.tri_count ) {
		float t = (float)triIdx;
		host[triIdx].v0[0] = sinf( t * 0.1f ) * 10.0f;
		host[triIdx].v0[1] = cosf( t * 0.07f ) * 8.0f;
		host[triIdx].v0[2] = 100.0f;
		host[triIdx].v0[3] = 1.0f;
		host[triIdx].v1[0] = sinf( t * 0.1f + 0.2f ) * 10.0f;
		host[triIdx].v1[1] = cosf( t * 0.07f ) * 8.0f;
		host[triIdx].v1[2] = 100.0f;
		host[triIdx].v1[3] = 1.0f;
		host[triIdx].v2[0] = sinf( t * 0.1f ) * 10.0f;
		host[triIdx].v2[1] = cosf( t * 0.07f + 0.2f ) * 8.0f;
		host[triIdx].v2[2] = 100.0f;
		host[triIdx].v2[3] = 1.0f;
		triIdx++;
	}

	if ( qvkMapMemory( vk.device, vk.curast.tri_memory, 0, size, 0, &mapped ) != VK_SUCCESS ) {
		ri.Hunk_FreeTempMemory( host );
		return qfalse;
	}
	Com_Memcpy( mapped, host, (size_t)size );
	qvkUnmapMemory( vk.device, vk.curast.tri_memory );
	ri.Hunk_FreeTempMemory( host );
	CuRast_AnalyzeStageRouting();
	return qtrue;
}

static void CuRast_DestroyGpu( void )
{
	if ( vk.curast.tri_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.curast.tri_buffer, NULL );
		qvkFreeMemory( vk.device, vk.curast.tri_memory, NULL );
	}
	if ( vk.curast.vis_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.curast.vis_view, NULL );
		qvkDestroyImage( vk.device, vk.curast.vis_image, NULL );
		qvkFreeMemory( vk.device, vk.curast.vis_memory, NULL );
	}
	if ( vk.curast.color_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.curast.color_view, NULL );
		qvkDestroyImage( vk.device, vk.curast.color_image, NULL );
		qvkFreeMemory( vk.device, vk.curast.color_memory, NULL );
	}
	Com_Memset( &vk.curast, 0, sizeof( vk.curast ) );
	curast.ready = qfalse;
}

static void CuRast_TransitionImages( VkCommandBuffer cmd )
{
	VkImageMemoryBarrier barriers[2];

	Com_Memset( barriers, 0, sizeof( barriers ) );
	barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[0].srcAccessMask = 0;
	barriers[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barriers[0].image = vk.curast.vis_image;
	barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barriers[0].subresourceRange.levelCount = 1;
	barriers[0].subresourceRange.layerCount = 1;

	barriers[1] = barriers[0];
	barriers[1].image = vk.curast.color_image;
	barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

	qvkCmdPipelineBarrier( cmd,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, NULL, 0, NULL, 2, barriers );
}

static qboolean CuRast_RunFrame( void )
{
	VkCommandBuffer cmd;
	VkDescriptorBufferInfo triInfo;
	VkDescriptorImageInfo visInfo;
	VkDescriptorImageInfo colorInfo;
	VkWriteDescriptorSet writes[3];
	struct {
		int viewport[4];
	} clearPush;
	struct {
		vec4_t viewport;
		vec4_t proj;
		int triCount;
		int maxSmallPx;
		int depthBits;
		int triBits;
	} stage1Push;
	struct {
		int viewport[4];
		int triBits;
		int triCount;
	} resolvePush;
	uint32_t groups;
	float f;
	float aspect;

	if ( !CuRast_EnsureBuffers() ) {
		return qfalse;
	}
	CuRast_EnsurePipelines();
	if ( !curast_clear.ready || !curast_stage1.ready || !curast_resolve.ready ) {
		return qfalse;
	}

	cmd = vk_begin_command_buffer();
	if ( cmd == VK_NULL_HANDLE ) {
		return qfalse;
	}

	CuRast_TransitionImages( cmd );

	triInfo.buffer = vk.curast.tri_buffer;
	triInfo.offset = 0;
	triInfo.range = VK_WHOLE_SIZE;

	visInfo.imageView = vk.curast.vis_view;
	visInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	colorInfo.imageView = vk.curast.color_view;
	colorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	/* Clear */
	clearPush.viewport[0] = (int)curast.width;
	clearPush.viewport[1] = (int)curast.height;
	clearPush.viewport[2] = 0;
	clearPush.viewport[3] = 0;

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = curast_clear.descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[0].pImageInfo = &visInfo;
	qvkUpdateDescriptorSets( vk.device, 1, writes, 0, NULL );

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, curast_clear.pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, curast_clear.pipeline_layout,
		0, 1, &curast_clear.descriptor, 0, NULL );
	qvkCmdPushConstants( cmd, curast_clear.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof( clearPush ), &clearPush );
	groups = ( curast.width + 15u ) / 16u;
	qvkCmdDispatch( cmd, groups, ( curast.height + 15u ) / 16u, 1 );

	/* Stage 1 */
	f = 1.0f / tanf( 45.0f * 0.5f * (float)M_PI / 180.0f );
	aspect = (float)curast.width / (float)curast.height;
	stage1Push.viewport[0] = (float)curast.width;
	stage1Push.viewport[1] = (float)curast.height;
	stage1Push.viewport[2] = 1.0f / (float)curast.width;
	stage1Push.viewport[3] = 1.0f / (float)curast.height;
	stage1Push.proj[0] = f / aspect;
	stage1Push.proj[1] = f;
	stage1Push.proj[2] = -1.0f;
	stage1Push.proj[3] = 0.0f;
	stage1Push.triCount = (int)curast.tri_count;
	stage1Push.maxSmallPx = CURAST_MAX_SMALL_PX;
	stage1Push.depthBits = 18;
	stage1Push.triBits = CURAST_TRI_BITS;

	writes[0].dstSet = curast_stage1.descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[0].pBufferInfo = &triInfo;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = curast_stage1.descriptor;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[1].pImageInfo = &visInfo;
	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, curast_stage1.pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, curast_stage1.pipeline_layout,
		0, 1, &curast_stage1.descriptor, 0, NULL );
	qvkCmdPushConstants( cmd, curast_stage1.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof( stage1Push ), &stage1Push );
	groups = ( curast.tri_count + 255u ) / 256u;
	qvkCmdDispatch( cmd, groups, 1, 1 );

	/* Resolve */
	resolvePush.viewport[0] = (int)curast.width;
	resolvePush.viewport[1] = (int)curast.height;
	resolvePush.triBits = CURAST_TRI_BITS;
	resolvePush.triCount = (int)curast.tri_count;

	writes[0].dstSet = curast_resolve.descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[0].pBufferInfo = &triInfo;
	writes[1].dstSet = curast_resolve.descriptor;
	writes[1].dstBinding = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[1].pImageInfo = &visInfo;
	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = curast_resolve.descriptor;
	writes[2].dstBinding = 2;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[2].pImageInfo = &colorInfo;
	qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, curast_resolve.pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, curast_resolve.pipeline_layout,
		0, 1, &curast_resolve.descriptor, 0, NULL );
	qvkCmdPushConstants( cmd, curast_resolve.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof( resolvePush ), &resolvePush );
	qvkCmdDispatch( cmd, ( curast.width + 15u ) / 16u, ( curast.height + 15u ) / 16u, 1 );

	vk_end_command_buffer( cmd, "CuRast_RenderFrame" );
	curast.frames++;
	return qtrue;
}

static void CuRast_Cmd_Status( void )
{
	ri.Printf( PRINT_ALL,
		"[CuRast] r_curast=%d tris=%u %ux%u frames=%u ready=%d\n",
		r_curast ? r_curast->integer : 0,
		curast.tri_count,
		curast.width, curast.height,
		curast.frames,
		curast.ready ? 1 : 0 );
	ri.Printf( PRINT_ALL,
		"[CuRast] Stage1 small-tri threshold=%d px (paper 128); stages 2/3 not wired in v1\n",
		CURAST_MAX_SMALL_PX );
	ri.Printf( PRINT_ALL,
		"[CuRast] Routing estimate: stage1=%u stage2=%u stage3=%u nearPlane=%u\n",
		curast.stage1_tris, curast.stage2_tris, curast.stage3_tris, curast.near_plane_tris );
}

static void CuRast_Cmd_Partition( void )
{
	uint32_t total;

	CuRast_AnalyzeStageRouting();
	total = curast.stage1_tris + curast.stage2_tris + curast.stage3_tris + curast.near_plane_tris;

	ri.Printf( PRINT_ALL,
		"[CuRast] Partitioned %u triangles with paper thresholds (<128 px, <4096 px, large)\n",
		total );
	ri.Printf( PRINT_ALL,
		"[CuRast] stage1=%u  stage2=%u  stage3=%u  nearPlane=%u\n",
		curast.stage1_tris, curast.stage2_tris, curast.stage3_tris, curast.near_plane_tris );
}

static void CuRast_Cmd_Render( void )
{
	int n;
	int i;

	if ( !R_CuRast_Active() ) {
		ri.Printf( PRINT_WARNING, "[CuRast] Enable r_curast 1 + vid_restart\n" );
		return;
	}

	n = ( ri.Cmd_Argc() >= 2 ) ? atoi( ri.Cmd_Argv( 1 ) ) : 1;
	if ( n < 1 ) {
		n = 1;
	}
	if ( n > 64 ) {
		n = 64;
	}

	for ( i = 0; i < n; i++ ) {
		if ( !CuRast_RunFrame() ) {
			ri.Printf( PRINT_WARNING, "[CuRast] render failed at frame %d/%d\n", i + 1, n );
			return;
		}
	}
	ri.Printf( PRINT_ALL, "[CuRast] rendered %d frame(s), total=%u\n", n, curast.frames );
}

static void CuRast_Cmd_Reset( void )
{
	curast.frames = 0;
	(void)CuRast_SeedMesh();
	ri.Printf( PRINT_ALL, "[CuRast] reset mesh and frame counter\n" );
}

void R_CuRast_Init( void )
{
	r_curast = ri.Cvar_Get( "r_curast", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_curast_tris = ri.Cvar_Get( "r_curast_tris", "8192", CVAR_ARCHIVE_ND );
	r_curast_width = ri.Cvar_Get( "r_curast_width", "640", CVAR_ARCHIVE_ND );
	r_curast_height = ri.Cvar_Get( "r_curast_height", "480", CVAR_ARCHIVE_ND );
	r_curast_debug = ri.Cvar_Get( "r_curast_debug", "0", CVAR_ARCHIVE_ND );

	ri.Cvar_CheckRange( r_curast, "0", "1", CV_INTEGER );
	ri.Cvar_CheckRange( r_curast_tris, "64", "65536", CV_INTEGER );
	ri.Cvar_CheckRange( r_curast_width, "64", "4096", CV_INTEGER );
	ri.Cvar_CheckRange( r_curast_height, "64", "4096", CV_INTEGER );

	ri.Cvar_SetDescription( r_curast,
		"CuRast software rasterization scaffold (Schütz et al., arXiv:2604.21749)." );

	ri.Cmd_AddCommand( "curast_status", CuRast_Cmd_Status );
	ri.Cmd_AddCommand( "curast_render", CuRast_Cmd_Render );
	ri.Cmd_AddCommand( "curast_partition", CuRast_Cmd_Partition );
	ri.Cmd_AddCommand( "curast_reset", CuRast_Cmd_Reset );

	if ( r_curast->integer ) {
		ri.Printf( PRINT_ALL, "[CuRast] Enabled — curast_render, see docs/CURAST.md\n" );
	}
}

void R_CuRast_Shutdown( void )
{
	ri.Cmd_RemoveCommand( "curast_status" );
	ri.Cmd_RemoveCommand( "curast_render" );
	ri.Cmd_RemoveCommand( "curast_partition" );
	ri.Cmd_RemoveCommand( "curast_reset" );
	CuRast_DestroyPipeline( &curast_clear );
	CuRast_DestroyPipeline( &curast_stage1 );
	CuRast_DestroyPipeline( &curast_resolve );
	CuRast_DestroyGpu();
}

qboolean R_CuRast_Active( void )
{
	return ( r_curast && r_curast->integer && vk.device != VK_NULL_HANDLE ) ? qtrue : qfalse;
}

qboolean R_CuRast_RenderFrame( void )
{
	if ( !R_CuRast_Active() ) {
		return qfalse;
	}
	return CuRast_RunFrame();
}
