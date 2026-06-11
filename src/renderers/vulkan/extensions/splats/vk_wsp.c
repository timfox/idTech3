/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

WebSplatter — WebGPU-aligned tile compute Gaussian splatting (Vulkan path).
See docs/WEB_SPLATTER.md.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_wsp.h"
#include "vk_util.h"
#include "vk_image_layout.h"
#include "vk_view_state.h"
#include "vk_staging.h"
#include "vk_cmd.h"

#define WSP_TILE_SIZE               16u
#define WSP_MAX_SPLATS_PER_TILE     16u
#define WSP_MAX_TILES_PER_SPLAT     24u
#define WSP_MAX_GAUSSIANS           1024u
#define WSP_SPLAT_STRIDE            48u

typedef struct {
	float position[3];
	float opacity;
	float scale[3];
	float sigmaScale;
	float rotation[4];
	float color[3];
	float pad;
} wspGaussian_t;

typedef struct {
	uint32_t tier;
	uint32_t maxGaussians;
	uint32_t maxFootprint;
	float scale;
	float strength;
} wspTierParams_t;

typedef struct {
	qboolean loaded;
	char mapName[MAX_QPATH];
	uint32_t gaussianCount;
	uint32_t targetWidth;
	uint32_t targetHeight;
	uint32_t tileCols;
	uint32_t tileRows;
} wspState_t;

static wspState_t wsp;

static cvar_t *r_wsp;
static cvar_t *r_wsp_strength;
static cvar_t *r_wsp_maxSplats;
static cvar_t *r_wsp_scale;
static cvar_t *r_wsp_focal;
static cvar_t *r_wsp_skipSky;
static cvar_t *r_wsp_depthTest;
static cvar_t *r_wsp_debug;

typedef struct {
	float viewProj[16];
	float viewOrigin[4];
	float viewport[4];
	int gaussianCount;
	int pad0;
	int pad1;
	int pad2;
} vk_wsp_prepare_push_t;

typedef struct {
	int tileCount;
	int pad0;
	int pad1;
	int pad2;
} vk_wsp_clear_push_t;

typedef struct {
	float viewport[4];
	int splatCount;
	int tileCols;
	int tileRows;
	int maxPerTile;
	int tileSize;
	int maxTilesPerSplat;
	float pad0;
} vk_wsp_bin_push_t;

typedef struct {
	float viewport[4];
	int tileCols;
	int tileRows;
	int maxPerTile;
	int tileSize;
	float strength;
	float pad0;
} vk_wsp_draw_push_t;

typedef struct {
	vec4_t extent;
	vec4_t params;
} vk_wsp_composite_push_t;

static VkSampler WSP_DepthSampler( void )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;
	return vk_find_sampler( &sd );
}

static VkSampler WSP_LinearSampler( void )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	return vk_find_sampler( &sd );
}

static wspTierParams_t WSP_TierParams( void )
{
	wspTierParams_t p;
	int tier;

	Com_Memset( &p, 0, sizeof( p ) );
	tier = r_wsp ? r_wsp->integer : 0;
	if ( tier < 1 ) {
		tier = 1;
	}
	if ( tier > 3 ) {
		tier = 3;
	}
	p.tier = (uint32_t)tier;
	p.strength = r_wsp_strength ? r_wsp_strength->value : 0.85f;

	if ( tier == 1 ) {
		p.maxGaussians = 64u;
		p.maxFootprint = 12u;
		p.scale = 0.25f;
	} else if ( tier == 2 ) {
		p.maxGaussians = 256u;
		p.maxFootprint = 24u;
		p.scale = 0.5f;
	} else {
		p.maxGaussians = WSP_MAX_GAUSSIANS;
		p.maxFootprint = 48u;
		p.scale = 1.0f;
	}

	if ( r_wsp_maxSplats && r_wsp_maxSplats->integer > 0 ) {
		p.maxGaussians = (uint32_t)r_wsp_maxSplats->integer;
		if ( p.maxGaussians > WSP_MAX_GAUSSIANS ) {
			p.maxGaussians = WSP_MAX_GAUSSIANS;
		}
	}
	if ( r_wsp_scale && r_wsp_scale->value > 0.0f ) {
		p.scale = r_wsp_scale->value;
	}
	return p;
}

static void WSP_ClearGpu( void )
{
	if ( vk.wsp.clear_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.wsp.clear_pipeline, NULL );
		vk.wsp.clear_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.wsp.prepare_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.wsp.prepare_pipeline, NULL );
		vk.wsp.prepare_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.wsp.bin_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.wsp.bin_pipeline, NULL );
		vk.wsp.bin_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.wsp.draw_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.wsp.draw_pipeline, NULL );
		vk.wsp.draw_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.wsp.composite_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.wsp.composite_pipeline, NULL );
		vk.wsp.composite_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.wsp.clear_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.wsp.clear_pipeline_layout, NULL );
		vk.wsp.clear_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.wsp.prepare_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.wsp.prepare_pipeline_layout, NULL );
		vk.wsp.prepare_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.wsp.bin_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.wsp.bin_pipeline_layout, NULL );
		vk.wsp.bin_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.wsp.draw_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.wsp.draw_pipeline_layout, NULL );
		vk.wsp.draw_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.wsp.composite_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.wsp.composite_pipeline_layout, NULL );
		vk.wsp.composite_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.wsp.clear_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.wsp.clear_layout, NULL );
		vk.wsp.clear_layout = VK_NULL_HANDLE;
	}
	if ( vk.wsp.prepare_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.wsp.prepare_layout, NULL );
		vk.wsp.prepare_layout = VK_NULL_HANDLE;
	}
	if ( vk.wsp.bin_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.wsp.bin_layout, NULL );
		vk.wsp.bin_layout = VK_NULL_HANDLE;
	}
	if ( vk.wsp.draw_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.wsp.draw_layout, NULL );
		vk.wsp.draw_layout = VK_NULL_HANDLE;
	}
	if ( vk.wsp.composite_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.wsp.composite_layout, NULL );
		vk.wsp.composite_layout = VK_NULL_HANDLE;
	}
	if ( vk.wsp.clear_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.wsp.clear_pool, NULL );
		vk.wsp.clear_pool = VK_NULL_HANDLE;
	}
	if ( vk.wsp.prepare_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.wsp.prepare_pool, NULL );
		vk.wsp.prepare_pool = VK_NULL_HANDLE;
	}
	if ( vk.wsp.bin_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.wsp.bin_pool, NULL );
		vk.wsp.bin_pool = VK_NULL_HANDLE;
	}
	if ( vk.wsp.draw_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.wsp.draw_pool, NULL );
		vk.wsp.draw_pool = VK_NULL_HANDLE;
	}
	if ( vk.wsp.composite_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.wsp.composite_pool, NULL );
		vk.wsp.composite_pool = VK_NULL_HANDLE;
	}
	if ( vk.wsp.gaussian_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.wsp.gaussian_buffer, NULL );
		vk.wsp.gaussian_buffer = VK_NULL_HANDLE;
	}
	if ( vk.wsp.gaussian_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.wsp.gaussian_memory, NULL );
		vk.wsp.gaussian_memory = VK_NULL_HANDLE;
	}
	if ( vk.wsp.splat_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.wsp.splat_buffer, NULL );
		vk.wsp.splat_buffer = VK_NULL_HANDLE;
	}
	if ( vk.wsp.splat_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.wsp.splat_memory, NULL );
		vk.wsp.splat_memory = VK_NULL_HANDLE;
	}
	if ( vk.wsp.tile_count_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.wsp.tile_count_buffer, NULL );
		vk.wsp.tile_count_buffer = VK_NULL_HANDLE;
	}
	if ( vk.wsp.tile_count_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.wsp.tile_count_memory, NULL );
		vk.wsp.tile_count_memory = VK_NULL_HANDLE;
	}
	if ( vk.wsp.tile_index_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.wsp.tile_index_buffer, NULL );
		vk.wsp.tile_index_buffer = VK_NULL_HANDLE;
	}
	if ( vk.wsp.tile_index_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.wsp.tile_index_memory, NULL );
		vk.wsp.tile_index_memory = VK_NULL_HANDLE;
	}
	if ( vk.wsp.accum_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.wsp.accum_view, NULL );
		vk.wsp.accum_view = VK_NULL_HANDLE;
	}
	if ( vk.wsp.accum_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.wsp.accum_image, NULL );
		vk.wsp.accum_image = VK_NULL_HANDLE;
	}
	if ( vk.wsp.accum_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.wsp.accum_memory, NULL );
		vk.wsp.accum_memory = VK_NULL_HANDLE;
	}
	vk.wsp.clear_ready = qfalse;
	vk.wsp.prepare_ready = qfalse;
	vk.wsp.bin_ready = qfalse;
	vk.wsp.draw_ready = qfalse;
	vk.wsp.composite_ready = qfalse;
	vk.wsp.gaussian_count = 0u;
	vk.wsp.tile_cols = 0u;
	vk.wsp.tile_rows = 0u;
	vk.wsp.tile_count = 0u;
	vk.wspAllocated = qfalse;
}

static void WSP_FillProceduralGaussians( wspGaussian_t *out, uint32_t count, float sigmaScale )
{
	uint32_t i;
	float origin[3];

	if ( tr.world ) {
		origin[0] = tr.world->lightGridOrigin[0] + tr.world->lightGridSize[0] * tr.world->lightGridBounds[0] * 0.5f;
		origin[1] = tr.world->lightGridOrigin[1] + tr.world->lightGridSize[1] * tr.world->lightGridBounds[1] * 0.5f;
		origin[2] = tr.world->lightGridOrigin[2] + 96.0f;
	} else {
		VectorCopy( backEnd.viewParms.or.origin, origin );
	}

	for ( i = 0; i < count; i++ ) {
		float u = (float)( i % 12 ) / 12.0f;
		float v = (float)( ( i / 12 ) % 12 ) / 12.0f;
		float w = (float)( i / 144 ) / 8.0f;
		wspGaussian_t *g = &out[i];

		g->position[0] = origin[0] + ( u - 0.5f ) * 384.0f;
		g->position[1] = origin[1] + ( v - 0.5f ) * 384.0f;
		g->position[2] = origin[2] + w * 128.0f + 32.0f;
		g->scale[0] = 4.0f + (float)( i % 4 );
		g->scale[1] = 3.0f + (float)( ( i + 1 ) % 4 );
		g->scale[2] = 5.0f + (float)( ( i + 2 ) % 4 );
		g->sigmaScale = sigmaScale;
		g->rotation[0] = 0.0f;
		g->rotation[1] = 0.0f;
		g->rotation[2] = 0.0f;
		g->rotation[3] = 1.0f;
		g->opacity = 0.55f + 0.4f * ( (float)( i % 5 ) / 5.0f );
		g->color[0] = 0.5f + 0.45f * sinf( (float)i * 0.37f );
		g->color[1] = 0.45f + 0.45f * cosf( (float)i * 0.29f );
		g->color[2] = 0.6f + 0.35f * sinf( (float)i * 0.23f );
		g->pad = 0.0f;
	}
}

static qboolean WSP_UploadGaussians( uint32_t count, const wspGaussian_t *src )
{
	VkBufferCreateInfo bi;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo ai;
	VkDeviceSize size;
	void *mapped;

	if ( vk.wsp.gaussian_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.wsp.gaussian_buffer, NULL );
		qvkFreeMemory( vk.device, vk.wsp.gaussian_memory, NULL );
		vk.wsp.gaussian_buffer = VK_NULL_HANDLE;
		vk.wsp.gaussian_memory = VK_NULL_HANDLE;
	}

	size = (VkDeviceSize)count * sizeof( wspGaussian_t );
	Com_Memset( &bi, 0, sizeof( bi ) );
	bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bi.size = size;
	bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &bi, NULL, &vk.wsp.gaussian_buffer ) );
	qvkGetBufferMemoryRequirements( vk.device, vk.wsp.gaussian_buffer, &req );
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, &vk.wsp.gaussian_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.wsp.gaussian_buffer, vk.wsp.gaussian_memory, 0 ) );
	VK_CHECK( qvkMapMemory( vk.device, vk.wsp.gaussian_memory, 0, size, 0, &mapped ) );
	Com_Memcpy( mapped, src, (size_t)size );
	qvkUnmapMemory( vk.device, vk.wsp.gaussian_memory );

	if ( vk.wsp.splat_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.wsp.splat_buffer, NULL );
		qvkFreeMemory( vk.device, vk.wsp.splat_memory, NULL );
		vk.wsp.splat_buffer = VK_NULL_HANDLE;
		vk.wsp.splat_memory = VK_NULL_HANDLE;
	}

	size = (VkDeviceSize)count * WSP_SPLAT_STRIDE;
	Com_Memset( &bi, 0, sizeof( bi ) );
	bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bi.size = size;
	bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &bi, NULL, &vk.wsp.splat_buffer ) );
	qvkGetBufferMemoryRequirements( vk.device, vk.wsp.splat_buffer, &req );
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, &vk.wsp.splat_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.wsp.splat_buffer, vk.wsp.splat_memory, 0 ) );

	vk.wsp.gaussian_count = count;
	return qtrue;
}

static qboolean WSP_EnsureAccumTarget( uint32_t width, uint32_t height )
{
	if ( vk.wsp.accum_image != VK_NULL_HANDLE &&
		wsp.targetWidth == width && wsp.targetHeight == height ) {
		return qtrue;
	}

	if ( vk.wsp.accum_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.wsp.accum_view, NULL );
		qvkDestroyImage( vk.device, vk.wsp.accum_image, NULL );
		qvkFreeMemory( vk.device, vk.wsp.accum_memory, NULL );
		vk.wsp.accum_view = VK_NULL_HANDLE;
		vk.wsp.accum_image = VK_NULL_HANDLE;
		vk.wsp.accum_memory = VK_NULL_HANDLE;
	}

	{
		VkImageCreateInfo image_desc;
		VkImageViewCreateInfo view_desc;
		VkMemoryRequirements mem_req;
		VkMemoryAllocateInfo alloc_info;

		Com_Memset( &image_desc, 0, sizeof( image_desc ) );
		image_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_desc.imageType = VK_IMAGE_TYPE_2D;
		image_desc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		image_desc.extent.width = width;
		image_desc.extent.height = height;
		image_desc.extent.depth = 1;
		image_desc.mipLevels = 1;
		image_desc.arrayLayers = 1;
		image_desc.samples = VK_SAMPLE_COUNT_1_BIT;
		image_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
		image_desc.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		image_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		if ( qvkCreateImage( vk.device, &image_desc, NULL, &vk.wsp.accum_image ) != VK_SUCCESS ) {
			return qfalse;
		}
		qvkGetImageMemoryRequirements( vk.device, vk.wsp.accum_image, &mem_req );
		Com_Memset( &alloc_info, 0, sizeof( alloc_info ) );
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = mem_req.size;
		alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		if ( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.wsp.accum_memory ) != VK_SUCCESS ||
			qvkBindImageMemory( vk.device, vk.wsp.accum_image, vk.wsp.accum_memory, 0 ) != VK_SUCCESS ) {
			qvkDestroyImage( vk.device, vk.wsp.accum_image, NULL );
			vk.wsp.accum_image = VK_NULL_HANDLE;
			return qfalse;
		}
		Com_Memset( &view_desc, 0, sizeof( view_desc ) );
		view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_desc.image = vk.wsp.accum_image;
		view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_desc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view_desc.subresourceRange.levelCount = 1;
		view_desc.subresourceRange.layerCount = 1;
		if ( qvkCreateImageView( vk.device, &view_desc, NULL, &vk.wsp.accum_view ) != VK_SUCCESS ) {
			return qfalse;
		}
	}

	wsp.targetWidth = width;
	wsp.targetHeight = height;
	return qtrue;
}

static qboolean WSP_EnsureTileBuffers( uint32_t splatW, uint32_t splatH )
{
	uint32_t tileCols;
	uint32_t tileRows;
	uint32_t tileCount;
	VkDeviceSize countSize;
	VkDeviceSize indexSize;
	VkBufferCreateInfo bi;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo ai;

	tileCols = ( splatW + WSP_TILE_SIZE - 1u ) / WSP_TILE_SIZE;
	tileRows = ( splatH + WSP_TILE_SIZE - 1u ) / WSP_TILE_SIZE;
	tileCount = tileCols * tileRows;

	if ( vk.wsp.tile_count_buffer != VK_NULL_HANDLE &&
		vk.wsp.tile_cols == tileCols && vk.wsp.tile_rows == tileRows ) {
		return qtrue;
	}

	if ( vk.wsp.tile_count_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.wsp.tile_count_buffer, NULL );
		qvkFreeMemory( vk.device, vk.wsp.tile_count_memory, NULL );
		vk.wsp.tile_count_buffer = VK_NULL_HANDLE;
		vk.wsp.tile_count_memory = VK_NULL_HANDLE;
	}
	if ( vk.wsp.tile_index_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.wsp.tile_index_buffer, NULL );
		qvkFreeMemory( vk.device, vk.wsp.tile_index_memory, NULL );
		vk.wsp.tile_index_buffer = VK_NULL_HANDLE;
		vk.wsp.tile_index_memory = VK_NULL_HANDLE;
	}

	countSize = (VkDeviceSize)tileCount * sizeof( uint32_t );
	indexSize = (VkDeviceSize)tileCount * WSP_MAX_SPLATS_PER_TILE * sizeof( uint32_t );

	Com_Memset( &bi, 0, sizeof( bi ) );
	bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bi.size = countSize;
	bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &bi, NULL, &vk.wsp.tile_count_buffer ) );
	qvkGetBufferMemoryRequirements( vk.device, vk.wsp.tile_count_buffer, &req );
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, &vk.wsp.tile_count_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.wsp.tile_count_buffer, vk.wsp.tile_count_memory, 0 ) );

	Com_Memset( &bi, 0, sizeof( bi ) );
	bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bi.size = indexSize;
	bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &bi, NULL, &vk.wsp.tile_index_buffer ) );
	qvkGetBufferMemoryRequirements( vk.device, vk.wsp.tile_index_buffer, &req );
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, &vk.wsp.tile_index_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.wsp.tile_index_buffer, vk.wsp.tile_index_memory, 0 ) );

	vk.wsp.tile_cols = tileCols;
	vk.wsp.tile_rows = tileRows;
	vk.wsp.tile_count = tileCount;
	wsp.tileCols = tileCols;
	wsp.tileRows = tileRows;
	return qtrue;
}

static void WSP_CreateClearPipeline( void )
{
	VkDescriptorSetLayoutBinding binding;
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;

	if ( vk.wsp.clear_ready || vk.modules.wsp_clear_tiles_cs == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &binding, 0, sizeof( binding ) );
	binding.binding = 0;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binding.descriptorCount = 1;
	binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	Com_Memset( &layout_ci, 0, sizeof( layout_ci ) );
	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.bindingCount = 1;
	layout_ci.pBindings = &binding;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.wsp.clear_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_wsp_clear_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.wsp.clear_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.wsp.clear_pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.wsp_clear_tiles_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.wsp.clear_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.wsp.clear_pipeline ) );

	{
		VkDescriptorPoolSize ps;
		VkDescriptorPoolCreateInfo pci;
		VkDescriptorSetAllocateInfo ai;

		Com_Memset( &ps, 0, sizeof( ps ) );
		ps.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		ps.descriptorCount = 1;
		Com_Memset( &pci, 0, sizeof( pci ) );
		pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pci.maxSets = 1;
		pci.poolSizeCount = 1;
		pci.pPoolSizes = &ps;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &pci, NULL, &vk.wsp.clear_pool ) );
		Com_Memset( &ai, 0, sizeof( ai ) );
		ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		ai.descriptorPool = vk.wsp.clear_pool;
		ai.descriptorSetCount = 1;
		ai.pSetLayouts = &vk.wsp.clear_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &ai, &vk.wsp.clear_descriptor ) );
	}

	vk.wsp.clear_ready = qtrue;
}

static void WSP_CreatePreparePipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[2];
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;

	if ( vk.wsp.prepare_ready || vk.modules.wsp_prepare_cs == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	Com_Memset( &layout_ci, 0, sizeof( layout_ci ) );
	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.bindingCount = 2;
	layout_ci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.wsp.prepare_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_wsp_prepare_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.wsp.prepare_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.wsp.prepare_pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.wsp_prepare_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.wsp.prepare_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.wsp.prepare_pipeline ) );

	{
		VkDescriptorPoolSize ps;
		VkDescriptorPoolCreateInfo pci;
		VkDescriptorSetAllocateInfo ai;

		Com_Memset( &ps, 0, sizeof( ps ) );
		ps.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		ps.descriptorCount = 2;
		Com_Memset( &pci, 0, sizeof( pci ) );
		pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pci.maxSets = 1;
		pci.poolSizeCount = 1;
		pci.pPoolSizes = &ps;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &pci, NULL, &vk.wsp.prepare_pool ) );
		Com_Memset( &ai, 0, sizeof( ai ) );
		ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		ai.descriptorPool = vk.wsp.prepare_pool;
		ai.descriptorSetCount = 1;
		ai.pSetLayouts = &vk.wsp.prepare_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &ai, &vk.wsp.prepare_descriptor ) );
	}

	vk.wsp.prepare_ready = qtrue;
}

static void WSP_CreateBinPipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[3];
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;

	if ( vk.wsp.bin_ready || vk.modules.wsp_tile_bin_cs == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	Com_Memset( &layout_ci, 0, sizeof( layout_ci ) );
	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.bindingCount = 3;
	layout_ci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.wsp.bin_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_wsp_bin_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.wsp.bin_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.wsp.bin_pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.wsp_tile_bin_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.wsp.bin_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.wsp.bin_pipeline ) );

	{
		VkDescriptorPoolSize ps;
		VkDescriptorPoolCreateInfo pci;
		VkDescriptorSetAllocateInfo ai;

		Com_Memset( &ps, 0, sizeof( ps ) );
		ps.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		ps.descriptorCount = 3;
		Com_Memset( &pci, 0, sizeof( pci ) );
		pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pci.maxSets = 1;
		pci.poolSizeCount = 1;
		pci.pPoolSizes = &ps;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &pci, NULL, &vk.wsp.bin_pool ) );
		Com_Memset( &ai, 0, sizeof( ai ) );
		ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		ai.descriptorPool = vk.wsp.bin_pool;
		ai.descriptorSetCount = 1;
		ai.pSetLayouts = &vk.wsp.bin_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &ai, &vk.wsp.bin_descriptor ) );
	}

	vk.wsp.bin_ready = qtrue;
}

static void WSP_CreateDrawPipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[4];
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;

	if ( vk.wsp.draw_ready || vk.modules.wsp_tile_draw_cs == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[3].binding = 3;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[3].descriptorCount = 1;
	bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	Com_Memset( &layout_ci, 0, sizeof( layout_ci ) );
	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.bindingCount = 4;
	layout_ci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.wsp.draw_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_wsp_draw_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.wsp.draw_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.wsp.draw_pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.wsp_tile_draw_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.wsp.draw_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.wsp.draw_pipeline ) );

	{
		VkDescriptorPoolSize ps[2];
		VkDescriptorPoolCreateInfo pci;
		VkDescriptorSetAllocateInfo ai;

		Com_Memset( ps, 0, sizeof( ps ) );
		ps[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		ps[0].descriptorCount = 3;
		ps[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		ps[1].descriptorCount = 1;
		Com_Memset( &pci, 0, sizeof( pci ) );
		pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pci.maxSets = 1;
		pci.poolSizeCount = 2;
		pci.pPoolSizes = ps;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &pci, NULL, &vk.wsp.draw_pool ) );
		Com_Memset( &ai, 0, sizeof( ai ) );
		ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		ai.descriptorPool = vk.wsp.draw_pool;
		ai.descriptorSetCount = 1;
		ai.pSetLayouts = &vk.wsp.draw_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &ai, &vk.wsp.draw_descriptor ) );
	}

	vk.wsp.draw_ready = qtrue;
}

static void WSP_CreateCompositePipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[3];
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;

	if ( vk.wsp.composite_ready || vk.modules.wsp_composite_cs == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	Com_Memset( &layout_ci, 0, sizeof( layout_ci ) );
	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.bindingCount = 3;
	layout_ci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.wsp.composite_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_wsp_composite_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.wsp.composite_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.wsp.composite_pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.wsp_composite_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.wsp.composite_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.wsp.composite_pipeline ) );

	{
		VkDescriptorPoolSize ps[3];
		VkDescriptorPoolCreateInfo pci;
		VkDescriptorSetAllocateInfo ai;

		Com_Memset( ps, 0, sizeof( ps ) );
		ps[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		ps[0].descriptorCount = 1;
		ps[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		ps[1].descriptorCount = 1;
		ps[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		ps[2].descriptorCount = 1;
		Com_Memset( &pci, 0, sizeof( pci ) );
		pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pci.maxSets = 1;
		pci.poolSizeCount = 3;
		pci.pPoolSizes = ps;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &pci, NULL, &vk.wsp.composite_pool ) );
		Com_Memset( &ai, 0, sizeof( ai ) );
		ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		ai.descriptorPool = vk.wsp.composite_pool;
		ai.descriptorSetCount = 1;
		ai.pSetLayouts = &vk.wsp.composite_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &ai, &vk.wsp.composite_descriptor ) );
	}

	vk.wsp.composite_ready = qtrue;
}

static void WSP_Cmd_Status( void )
{
	wspTierParams_t tier = WSP_TierParams();
	ri.Printf( PRINT_ALL,
		"[WSP] active=%d loaded=%d gaussians=%u tier=%u scale=%.2f tiles=%ux%u count=%u accum=%ux%u\n",
		R_WSP_Active() ? 1 : 0,
		wsp.loaded ? 1 : 0,
		vk.wsp.gaussian_count,
		tier.tier,
		tier.scale,
		vk.wsp.tile_cols, vk.wsp.tile_rows, vk.wsp.tile_count,
		wsp.targetWidth, wsp.targetHeight );
}

void R_WSP_Init( void )
{
	r_wsp = ri.Cvar_Get( "r_wsp", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_wsp_strength = ri.Cvar_Get( "r_wsp_strength", "0.85", CVAR_ARCHIVE_ND );
	r_wsp_maxSplats = ri.Cvar_Get( "r_wsp_maxSplats", "0", CVAR_ARCHIVE_ND );
	r_wsp_scale = ri.Cvar_Get( "r_wsp_scale", "0", CVAR_ARCHIVE_ND );
	r_wsp_focal = ri.Cvar_Get( "r_wsp_focal", "512", CVAR_ARCHIVE_ND );
	r_wsp_skipSky = ri.Cvar_Get( "r_wsp_skipSky", "1", CVAR_ARCHIVE_ND );
	r_wsp_depthTest = ri.Cvar_Get( "r_wsp_depthTest", "1", CVAR_ARCHIVE_ND );
	r_wsp_debug = ri.Cvar_Get( "r_wsp_debug", "0", CVAR_ARCHIVE_ND );

	ri.Cvar_CheckRange( r_wsp, "0", "3", CV_INTEGER );
	ri.Cvar_CheckRange( r_wsp_maxSplats, "0", "1024", CV_INTEGER );
	ri.Cvar_CheckRange( r_wsp_scale, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_wsp,
		"WebSplatter tile Gaussian splatting: 0=off, 1=mobile tier, 2=balanced, 3=high (latched; vid_restart)." );
	ri.Cvar_SetDescription( r_wsp_strength,
		"Tile splat composite strength (alpha scale)." );
	ri.Cvar_SetDescription( r_wsp_maxSplats,
		"Override max splats (0=tier default: 64/256/1024)." );
	ri.Cvar_SetDescription( r_wsp_scale,
		"Override splat buffer resolution scale (0=tier: 0.25/0.5/1)." );
	ri.Cvar_SetDescription( r_wsp_focal,
		"Screen-radius focal scale for projected splat size." );

	ri.Cmd_AddCommand( "wsp_status", WSP_Cmd_Status );

	if ( r_wsp->integer > 0 ) {
		ri.Printf( PRINT_ALL,
			"[WSP] WebSplatter enabled (tier %d). See docs/WEB_SPLATTER.md\n",
			r_wsp->integer );
		ri.Printf( PRINT_ALL,
			"[WSP] WebGPU portable limits: tile=%u maxPerTile=%u maxTilesPerSplat=%u wg<=256\n",
			WSP_TILE_SIZE, WSP_MAX_SPLATS_PER_TILE, WSP_MAX_TILES_PER_SPLAT );
	}
}

void R_WSP_Shutdown( void )
{
	ri.Cmd_RemoveCommand( "wsp_status" );
	WSP_ClearGpu();
	Com_Memset( &wsp, 0, sizeof( wsp ) );
}

void R_WSP_OnMapLoad( const char *mapBaseName )
{
	wspGaussian_t *gaussians;
	wspTierParams_t tier;
	uint32_t count;
	float sigma;

	WSP_ClearGpu();
	Com_Memset( &wsp, 0, sizeof( wsp ) );

	if ( !r_wsp || r_wsp->integer <= 0 ) {
		return;
	}
	if ( !mapBaseName || !mapBaseName[0] ) {
		return;
	}

	Q_strncpyz( wsp.mapName, mapBaseName, sizeof( wsp.mapName ) );
	tier = WSP_TierParams();
	count = tier.maxGaussians;
	sigma = 2.5f;

	gaussians = (wspGaussian_t *)ri.Malloc( (size_t)count * sizeof( wspGaussian_t ) );
	WSP_FillProceduralGaussians( gaussians, count, sigma );
	if ( WSP_UploadGaussians( count, gaussians ) ) {
		wsp.gaussianCount = count;
		wsp.loaded = qtrue;
		ri.Printf( PRINT_ALL, "[WSP] Loaded %u procedural Gaussians for '%s' (tier %u)\n",
			count, wsp.mapName, tier.tier );
	}
	ri.Free( gaussians );

	WSP_CreateClearPipeline();
	WSP_CreatePreparePipeline();
	WSP_CreateBinPipeline();
	WSP_CreateDrawPipeline();
	WSP_CreateCompositePipeline();
	vk.wspAllocated = qtrue;
}

qboolean R_WSP_Active( void )
{
	return ( r_wsp && r_wsp->integer > 0 && wsp.loaded && vk.wsp.gaussian_count > 0 &&
		vk.fboActive && vk.color_image != VK_NULL_HANDLE &&
		vk.wsp.clear_ready && vk.wsp.prepare_ready && vk.wsp.bin_ready &&
		vk.wsp.draw_ready && vk.wsp.composite_ready ) ? qtrue : qfalse;
}

void vk_wsp_apply_after_geometry( void )
{
	VkCommandBuffer cmd;
	wspTierParams_t tier;
	uint32_t fullW, fullH, splatW, splatH;
	float viewProj[16];
	const float *view;
	const float *projection;
	float proj_vk[16];
	vk_wsp_clear_push_t clearPush;
	vk_wsp_prepare_push_t prepPush;
	vk_wsp_bin_push_t binPush;
	vk_wsp_draw_push_t drawPush;
	vk_wsp_composite_push_t compPush;
	VkDescriptorBufferInfo gaussInfo, splatInfo, tileCountInfo, tileIndexInfo;
	VkDescriptorImageInfo accumInfo, depthInfo, colorInfo;
	VkWriteDescriptorSet writes[6];
	VkImageMemoryBarrier barriers[2];
	VkImageAspectFlags depthAspect;
	uint32_t clearGroups, prepGroups, binGroups;

	if ( !R_WSP_Active() || !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}

	cmd = vk.cmd->command_buffer;
	tier = WSP_TierParams();
	fullW = vk_get_render_target_width();
	fullH = vk_get_render_target_height();
	splatW = (uint32_t)( (float)fullW * tier.scale );
	splatH = (uint32_t)( (float)fullH * tier.scale );
	if ( splatW < 64u ) {
		splatW = 64u;
	}
	if ( splatH < 64u ) {
		splatH = 64u;
	}

	if ( !WSP_EnsureAccumTarget( splatW, splatH ) ) {
		return;
	}
	if ( !WSP_EnsureTileBuffers( splatW, splatH ) ) {
		return;
	}

	view = backEnd.viewParms.world.modelViewMatrix;
	projection = backEnd.useFirstPersonProjection ?
		backEnd.firstPersonProjectionMatrix : backEnd.viewParms.projectionMatrix;
	vk_get_projection_matrix_vk( projection, proj_vk );
	myGlMultMatrix( view, proj_vk, viewProj );

	tileCountInfo.buffer = vk.wsp.tile_count_buffer;
	tileCountInfo.offset = 0;
	tileCountInfo.range = VK_WHOLE_SIZE;
	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = vk.wsp.clear_descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[0].pBufferInfo = &tileCountInfo;
	qvkUpdateDescriptorSets( vk.device, 1, writes, 0, NULL );

	Com_Memset( &clearPush, 0, sizeof( clearPush ) );
	clearPush.tileCount = (int)vk.wsp.tile_count;
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vk.wsp.clear_pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vk.wsp.clear_pipeline_layout,
		0, 1, &vk.wsp.clear_descriptor, 0, NULL );
	qvkCmdPushConstants( cmd, vk.wsp.clear_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof( clearPush ), &clearPush );
	clearGroups = ( vk.wsp.tile_count + 63u ) / 64u;
	qvkCmdDispatch( cmd, clearGroups, 1, 1 );

	Com_Memset( &prepPush, 0, sizeof( prepPush ) );
	Com_Memcpy( prepPush.viewProj, viewProj, sizeof( prepPush.viewProj ) );
	prepPush.viewOrigin[0] = backEnd.viewParms.or.origin[0];
	prepPush.viewOrigin[1] = backEnd.viewParms.or.origin[1];
	prepPush.viewOrigin[2] = backEnd.viewParms.or.origin[2];
	prepPush.viewOrigin[3] = 1.0f;
	prepPush.viewport[0] = (float)splatW;
	prepPush.viewport[1] = (float)splatH;
	prepPush.viewport[2] = r_wsp_focal ? r_wsp_focal->value : 512.0f;
	prepPush.viewport[3] = (float)tier.maxFootprint;
	prepPush.gaussianCount = (int)vk.wsp.gaussian_count;

	gaussInfo.buffer = vk.wsp.gaussian_buffer;
	gaussInfo.offset = 0;
	gaussInfo.range = VK_WHOLE_SIZE;
	splatInfo.buffer = vk.wsp.splat_buffer;
	splatInfo.offset = 0;
	splatInfo.range = VK_WHOLE_SIZE;
	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = vk.wsp.prepare_descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[0].pBufferInfo = &gaussInfo;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = vk.wsp.prepare_descriptor;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[1].pBufferInfo = &splatInfo;
	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vk.wsp.prepare_pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vk.wsp.prepare_pipeline_layout,
		0, 1, &vk.wsp.prepare_descriptor, 0, NULL );
	qvkCmdPushConstants( cmd, vk.wsp.prepare_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof( prepPush ), &prepPush );
	prepGroups = ( vk.wsp.gaussian_count + 63u ) / 64u;
	qvkCmdDispatch( cmd, prepGroups, 1, 1 );

	Com_Memset( &binPush, 0, sizeof( binPush ) );
	binPush.viewport[0] = (float)splatW;
	binPush.viewport[1] = (float)splatH;
	binPush.splatCount = (int)vk.wsp.gaussian_count;
	binPush.tileCols = (int)vk.wsp.tile_cols;
	binPush.tileRows = (int)vk.wsp.tile_rows;
	binPush.maxPerTile = (int)WSP_MAX_SPLATS_PER_TILE;
	binPush.tileSize = (int)WSP_TILE_SIZE;
	binPush.maxTilesPerSplat = (int)WSP_MAX_TILES_PER_SPLAT;

	tileIndexInfo.buffer = vk.wsp.tile_index_buffer;
	tileIndexInfo.offset = 0;
	tileIndexInfo.range = VK_WHOLE_SIZE;
	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = vk.wsp.bin_descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[0].pBufferInfo = &splatInfo;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = vk.wsp.bin_descriptor;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[1].pBufferInfo = &tileCountInfo;
	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = vk.wsp.bin_descriptor;
	writes[2].dstBinding = 2;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[2].pBufferInfo = &tileIndexInfo;
	qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vk.wsp.bin_pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vk.wsp.bin_pipeline_layout,
		0, 1, &vk.wsp.bin_descriptor, 0, NULL );
	qvkCmdPushConstants( cmd, vk.wsp.bin_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof( binPush ), &binPush );
	binGroups = ( vk.wsp.gaussian_count + 63u ) / 64u;
	qvkCmdDispatch( cmd, binGroups, 1, 1 );

	Com_Memset( barriers, 0, sizeof( barriers ) );
	barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barriers[0].image = vk.wsp.accum_image;
	barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barriers[0].subresourceRange.levelCount = 1;
	barriers[0].subresourceRange.layerCount = 1;
	barriers[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, NULL, 0, NULL, 1, barriers );

	Com_Memset( &accumInfo, 0, sizeof( accumInfo ) );
	accumInfo.imageView = vk.wsp.accum_view;
	accumInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = vk.wsp.draw_descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[0].pBufferInfo = &splatInfo;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = vk.wsp.draw_descriptor;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[1].pBufferInfo = &tileCountInfo;
	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = vk.wsp.draw_descriptor;
	writes[2].dstBinding = 2;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[2].pBufferInfo = &tileIndexInfo;
	writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[3].dstSet = vk.wsp.draw_descriptor;
	writes[3].dstBinding = 3;
	writes[3].descriptorCount = 1;
	writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[3].pImageInfo = &accumInfo;
	qvkUpdateDescriptorSets( vk.device, 4, writes, 0, NULL );

	Com_Memset( &drawPush, 0, sizeof( drawPush ) );
	drawPush.viewport[0] = (float)splatW;
	drawPush.viewport[1] = (float)splatH;
	drawPush.tileCols = (int)vk.wsp.tile_cols;
	drawPush.tileRows = (int)vk.wsp.tile_rows;
	drawPush.maxPerTile = (int)WSP_MAX_SPLATS_PER_TILE;
	drawPush.tileSize = (int)WSP_TILE_SIZE;
	drawPush.strength = tier.strength;

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vk.wsp.draw_pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vk.wsp.draw_pipeline_layout,
		0, 1, &vk.wsp.draw_descriptor, 0, NULL );
	qvkCmdPushConstants( cmd, vk.wsp.draw_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof( drawPush ), &drawPush );
	qvkCmdDispatch( cmd, vk.wsp.tile_cols, vk.wsp.tile_rows, 1 );

	barriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, NULL, 0, NULL, 1, barriers );

	depthAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 ) {
		depthAspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	record_depth_image_layout_transition( cmd, depthAspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, 0, 0 );
	record_image_layout_transition( cmd, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0 );

	Com_Memset( &depthInfo, 0, sizeof( depthInfo ) );
	depthInfo.sampler = WSP_DepthSampler();
	depthInfo.imageView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	Com_Memset( &accumInfo, 0, sizeof( accumInfo ) );
	accumInfo.sampler = WSP_LinearSampler();
	accumInfo.imageView = vk.wsp.accum_view;
	accumInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	Com_Memset( &colorInfo, 0, sizeof( colorInfo ) );
	colorInfo.imageView = vk.color_image_view;
	colorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = vk.wsp.composite_descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].descriptorCount = 1;
	writes[0].pImageInfo = &depthInfo;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = vk.wsp.composite_descriptor;
	writes[1].dstBinding = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].descriptorCount = 1;
	writes[1].pImageInfo = &accumInfo;
	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = vk.wsp.composite_descriptor;
	writes[2].dstBinding = 2;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[2].descriptorCount = 1;
	writes[2].pImageInfo = &colorInfo;
	qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );

	Com_Memset( &compPush, 0, sizeof( compPush ) );
	compPush.extent[0] = (float)fullW;
	compPush.extent[1] = (float)fullH;
	compPush.params[0] = tier.strength;
	compPush.params[1] = ( r_wsp_depthTest && r_wsp_depthTest->integer ) ? 1.0f : 0.0f;
	compPush.params[2] = ( r_wsp_skipSky && r_wsp_skipSky->integer ) ? 1.0f : 0.0f;
	compPush.params[3] = 0.002f;

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vk.wsp.composite_pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vk.wsp.composite_pipeline_layout,
		0, 1, &vk.wsp.composite_descriptor, 0, NULL );
	qvkCmdPushConstants( cmd, vk.wsp.composite_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof( compPush ), &compPush );
	qvkCmdDispatch( cmd, ( fullW + 7u ) / 8u, ( fullH + 7u ) / 8u, 1 );

	record_image_layout_transition( cmd, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, 0 );

	if ( r_wsp_debug && r_wsp_debug->integer ) {
		ri.Printf( PRINT_DEVELOPER,
			"[WSP] tiles %ux%u (%u) splat %ux%u -> composite %ux%u count=%u tier=%u\n",
			vk.wsp.tile_cols, vk.wsp.tile_rows, vk.wsp.tile_count,
			splatW, splatH, fullW, fullH, vk.wsp.gaussian_count, tier.tier );
	}
}
