/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Variable-Rate Compute Shading (VRCS) — SRI, 16x16 pack, deferred lighting wrap, deblock.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_vrcs.h"
#include "vk_deferred_gbuffer.h"
#include "vk_visibility_buffer.h"
#include "vk_image_layout.h"
#include "vk_util.h"
#include "vk_view_state.h"
#include "vk_cmd.h"

#define VRCS_TILE           16u
#define VRCS_MAX_PACK       224u
#define VRCS_WAVE           32u

typedef struct {
	VkBuffer buffer;
	VkDeviceMemory memory;
	VkDeviceSize size;
} vrcs_buffer_t;

typedef struct {
	VkImage image;
	VkImageView view;
	VkDeviceMemory memory;
	uint32_t width, height;
} vrcs_image_t;

typedef struct {
	uint32_t extent[2];
	uint32_t sriExtent[2];
	float edgeThresh;
	float flatThresh;
	uint32_t extraHalf;
	uint32_t quality;
} vrcs_sri_push_t;

typedef struct {
	uint32_t extent[2];
	uint32_t tileGrid[2];
	uint32_t rotatePrimary;
	uint32_t reserved;
} vrcs_pack_push_t;

typedef struct {
	uint32_t extent[2];
	uint32_t debugMode;
	uint32_t reserved;
} vrcs_deblock_push_t;

/* Must match deferred_lighting_vrcs.comp push (prefix + VRCS tail). */
typedef struct {
	float viewMatrix[16];
	float projInfo[4];
	uint32_t extent[2];
	uint32_t tileGrid[2];
	float strength;
	uint32_t maxPerTile;
	uint32_t numLights;
	uint32_t additive;
	uint32_t specular;
	float aoStrength;
	float specularStrength;
	uint32_t normalsAreWorld;
	uint32_t useMaterialClass;
	uint32_t vrcsTileGrid[2];
	uint32_t debugMode;
	uint32_t reserved;
	uint32_t zSlices;
	uint32_t zSliceMode;
	float zNear;
	float zFar;
} vrcs_light_push_t;

typedef struct {
	qboolean ready;
	qboolean logged;
	uint32_t width, height;
	uint32_t sriW, sriH;
	uint32_t tilesX, tilesY;
	uint32_t frame;

	vrcs_image_t sri;
	vrcs_buffer_t tileHeaders;
	vrcs_buffer_t tileCoords;

	VkDescriptorSetLayout sri_layout;
	VkPipelineLayout sri_pl;
	VkPipeline sri_pipe;
	VkDescriptorPool sri_pool;
	VkDescriptorSet sri_set;

	VkDescriptorSetLayout pack_layout;
	VkPipelineLayout pack_pl;
	VkPipeline pack_pipe;
	VkDescriptorPool pack_pool;
	VkDescriptorSet pack_set;

	VkDescriptorSetLayout deblock_layout;
	VkPipelineLayout deblock_pl;
	VkPipeline deblock_pipe;
	VkDescriptorPool deblock_pool;
	VkDescriptorSet deblock_set;

	VkDescriptorSetLayout light_layout;
	VkPipelineLayout light_pl;
	VkPipeline light_pipe;
	VkDescriptorPool light_pool;
	VkDescriptorSet light_set;
} vrcs_state_t;

static vrcs_state_t vrcs;

cvar_t *r_vrcs;
cvar_t *r_vrcs_quality;
cvar_t *r_vrcs_extraHalf;
cvar_t *r_vrcs_deblock;
cvar_t *r_vrcs_debug;
cvar_t *r_vrcs_edge;
cvar_t *r_vrcs_flat;

extern cvar_t *r_deferredLighting;
extern cvar_t *r_deferredLightingStrength;
extern cvar_t *r_deferredSpecular;
extern cvar_t *r_deferredSpecularStrength;
extern cvar_t *r_deferredAOCoupling;
extern cvar_t *r_deferredMaterialClassify;
extern cvar_t *r_deferredUnlitBase;
extern cvar_t *r_taa;

static void VRCS_DestroyBuffer( vrcs_buffer_t *b )
{
	if ( b->buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, b->buffer, NULL );
		b->buffer = VK_NULL_HANDLE;
	}
	if ( b->memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, b->memory, NULL );
		b->memory = VK_NULL_HANDLE;
	}
	b->size = 0;
}

static void VRCS_DestroyImage( vrcs_image_t *img )
{
	if ( img->view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, img->view, NULL );
		img->view = VK_NULL_HANDLE;
	}
	if ( img->image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, img->image, NULL );
		img->image = VK_NULL_HANDLE;
	}
	if ( img->memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, img->memory, NULL );
		img->memory = VK_NULL_HANDLE;
	}
	img->width = img->height = 0;
}

static qboolean VRCS_CreateBuffer( VkDeviceSize size, VkBufferUsageFlags usage, vrcs_buffer_t *out )
{
	VkBufferCreateInfo bi;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo ai;

	VRCS_DestroyBuffer( out );
	Com_Memset( &bi, 0, sizeof( bi ) );
	bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bi.size = size;
	bi.usage = usage;
	VK_CHECK( qvkCreateBuffer( vk.device, &bi, NULL, &out->buffer ) );
	qvkGetBufferMemoryRequirements( vk.device, out->buffer, &req );
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, &out->memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, out->buffer, out->memory, 0 ) );
	out->size = size;
	return qtrue;
}

static qboolean VRCS_CreateSriImage( uint32_t w, uint32_t h )
{
	VkImageCreateInfo ii;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo ai;
	VkImageViewCreateInfo vi;

	VRCS_DestroyImage( &vrcs.sri );
	if ( w < 1 || h < 1 ) {
		return qfalse;
	}
	Com_Memset( &ii, 0, sizeof( ii ) );
	ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ii.imageType = VK_IMAGE_TYPE_2D;
	ii.format = VK_FORMAT_R8_UINT;
	ii.extent.width = w;
	ii.extent.height = h;
	ii.extent.depth = 1;
	ii.mipLevels = 1;
	ii.arrayLayers = 1;
	ii.samples = VK_SAMPLE_COUNT_1_BIT;
	ii.tiling = VK_IMAGE_TILING_OPTIMAL;
	ii.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK( qvkCreateImage( vk.device, &ii, NULL, &vrcs.sri.image ) );
	qvkGetImageMemoryRequirements( vk.device, vrcs.sri.image, &req );
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, &vrcs.sri.memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vrcs.sri.image, vrcs.sri.memory, 0 ) );
	Com_Memset( &vi, 0, sizeof( vi ) );
	vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	vi.image = vrcs.sri.image;
	vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
	vi.format = VK_FORMAT_R8_UINT;
	vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vi.subresourceRange.levelCount = 1;
	vi.subresourceRange.layerCount = 1;
	VK_CHECK( qvkCreateImageView( vk.device, &vi, NULL, &vrcs.sri.view ) );
	vrcs.sri.width = w;
	vrcs.sri.height = h;
	return qtrue;
}

static void VRCS_DestroyPipelines( void )
{
	if ( vrcs.sri_pipe ) { qvkDestroyPipeline( vk.device, vrcs.sri_pipe, NULL ); vrcs.sri_pipe = VK_NULL_HANDLE; }
	if ( vrcs.pack_pipe ) { qvkDestroyPipeline( vk.device, vrcs.pack_pipe, NULL ); vrcs.pack_pipe = VK_NULL_HANDLE; }
	if ( vrcs.deblock_pipe ) { qvkDestroyPipeline( vk.device, vrcs.deblock_pipe, NULL ); vrcs.deblock_pipe = VK_NULL_HANDLE; }
	if ( vrcs.light_pipe ) { qvkDestroyPipeline( vk.device, vrcs.light_pipe, NULL ); vrcs.light_pipe = VK_NULL_HANDLE; }

	if ( vrcs.sri_pl ) { qvkDestroyPipelineLayout( vk.device, vrcs.sri_pl, NULL ); vrcs.sri_pl = VK_NULL_HANDLE; }
	if ( vrcs.pack_pl ) { qvkDestroyPipelineLayout( vk.device, vrcs.pack_pl, NULL ); vrcs.pack_pl = VK_NULL_HANDLE; }
	if ( vrcs.deblock_pl ) { qvkDestroyPipelineLayout( vk.device, vrcs.deblock_pl, NULL ); vrcs.deblock_pl = VK_NULL_HANDLE; }
	if ( vrcs.light_pl ) { qvkDestroyPipelineLayout( vk.device, vrcs.light_pl, NULL ); vrcs.light_pl = VK_NULL_HANDLE; }

	if ( vrcs.sri_layout ) { qvkDestroyDescriptorSetLayout( vk.device, vrcs.sri_layout, NULL ); vrcs.sri_layout = VK_NULL_HANDLE; }
	if ( vrcs.pack_layout ) { qvkDestroyDescriptorSetLayout( vk.device, vrcs.pack_layout, NULL ); vrcs.pack_layout = VK_NULL_HANDLE; }
	if ( vrcs.deblock_layout ) { qvkDestroyDescriptorSetLayout( vk.device, vrcs.deblock_layout, NULL ); vrcs.deblock_layout = VK_NULL_HANDLE; }
	if ( vrcs.light_layout ) { qvkDestroyDescriptorSetLayout( vk.device, vrcs.light_layout, NULL ); vrcs.light_layout = VK_NULL_HANDLE; }

	if ( vrcs.sri_pool ) { qvkDestroyDescriptorPool( vk.device, vrcs.sri_pool, NULL ); vrcs.sri_pool = VK_NULL_HANDLE; }
	if ( vrcs.pack_pool ) { qvkDestroyDescriptorPool( vk.device, vrcs.pack_pool, NULL ); vrcs.pack_pool = VK_NULL_HANDLE; }
	if ( vrcs.deblock_pool ) { qvkDestroyDescriptorPool( vk.device, vrcs.deblock_pool, NULL ); vrcs.deblock_pool = VK_NULL_HANDLE; }
	if ( vrcs.light_pool ) { qvkDestroyDescriptorPool( vk.device, vrcs.light_pool, NULL ); vrcs.light_pool = VK_NULL_HANDLE; }

	vrcs.sri_set = vrcs.pack_set = vrcs.deblock_set = vrcs.light_set = VK_NULL_HANDLE;
}

static qboolean VRCS_CreateComputePipe( VkShaderModule mod, VkDescriptorSetLayoutBinding *binds, uint32_t bindCount,
	uint32_t pushSize, VkDescriptorSetLayout *outLayout, VkPipelineLayout *outPl, VkPipeline *outPipe,
	VkDescriptorPool *outPool, VkDescriptorSet *outSet, const char *name )
{
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;
	VkDescriptorPoolSize pool_sizes[4];
	VkDescriptorPoolCreateInfo pool_ci;
	VkDescriptorSetAllocateInfo alloc;
	uint32_t i, nStorage = 0, nSampled = 0, nStorageImg = 0;

	if ( mod == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, "[VRCS] missing shader module for %s\n", name );
		return qfalse;
	}

	Com_Memset( &layout_ci, 0, sizeof( layout_ci ) );
	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.bindingCount = bindCount;
	layout_ci.pBindings = binds;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, outLayout ) );

	Com_Memset( &push_range, 0, sizeof( push_range ) );
	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.size = pushSize;

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = outLayout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, outPl ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = mod;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = *outPl;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, outPipe ) );
	SET_OBJECT_NAME( *outPipe, name, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	for ( i = 0; i < bindCount; i++ ) {
		if ( binds[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ) {
			nStorage++;
		} else if ( binds[i].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ) {
			nSampled++;
		} else if ( binds[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ) {
			nStorageImg++;
		}
	}

	i = 0;
	if ( nStorage ) {
		pool_sizes[i].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		pool_sizes[i].descriptorCount = nStorage;
		i++;
	}
	if ( nSampled ) {
		pool_sizes[i].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool_sizes[i].descriptorCount = nSampled;
		i++;
	}
	if ( nStorageImg ) {
		pool_sizes[i].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		pool_sizes[i].descriptorCount = nStorageImg;
		i++;
	}

	Com_Memset( &pool_ci, 0, sizeof( pool_ci ) );
	pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_ci.maxSets = 1;
	pool_ci.poolSizeCount = i;
	pool_ci.pPoolSizes = pool_sizes;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, outPool ) );

	Com_Memset( &alloc, 0, sizeof( alloc ) );
	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.descriptorPool = *outPool;
	alloc.descriptorSetCount = 1;
	alloc.pSetLayouts = outLayout;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, outSet ) );
	return qtrue;
}

static qboolean VRCS_CreatePipelines( void )
{
	VkDescriptorSetLayoutBinding binds[10];

	VRCS_DestroyPipelines();

	/* SRI: history sampled + sri storage */
	Com_Memset( binds, 0, sizeof( binds ) );
	binds[0].binding = 0;
	binds[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[0].descriptorCount = 1;
	binds[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[1].binding = 1;
	binds[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	binds[1].descriptorCount = 1;
	binds[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	if ( !VRCS_CreateComputePipe( vk.modules.vrcs_sri_cs, binds, 2, sizeof( vrcs_sri_push_t ),
		&vrcs.sri_layout, &vrcs.sri_pl, &vrcs.sri_pipe, &vrcs.sri_pool, &vrcs.sri_set, "vrcs sri" ) ) {
		return qfalse;
	}

	/* Pack: sri sampled, depth sampled, headers+coords storage */
	Com_Memset( binds, 0, sizeof( binds ) );
	binds[0].binding = 0;
	binds[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[0].descriptorCount = 1;
	binds[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[1].binding = 1;
	binds[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[1].descriptorCount = 1;
	binds[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[2].binding = 2;
	binds[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[2].descriptorCount = 1;
	binds[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[3].binding = 3;
	binds[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[3].descriptorCount = 1;
	binds[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	if ( !VRCS_CreateComputePipe( vk.modules.vrcs_pack_cs, binds, 4, sizeof( vrcs_pack_push_t ),
		&vrcs.pack_layout, &vrcs.pack_pl, &vrcs.pack_pipe, &vrcs.pack_pool, &vrcs.pack_set, "vrcs pack" ) ) {
		return qfalse;
	}

	/* Deblock: sri sampled + lit storage */
	Com_Memset( binds, 0, sizeof( binds ) );
	binds[0].binding = 0;
	binds[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[0].descriptorCount = 1;
	binds[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[1].binding = 1;
	binds[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	binds[1].descriptorCount = 1;
	binds[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	if ( !VRCS_CreateComputePipe( vk.modules.vrcs_deblock_cs, binds, 2, sizeof( vrcs_deblock_push_t ),
		&vrcs.deblock_layout, &vrcs.deblock_pl, &vrcs.deblock_pipe, &vrcs.deblock_pool, &vrcs.deblock_set,
		"vrcs deblock" ) ) {
		return qfalse;
	}

	/* Lighting VRCS: same as deferred + headers + coords */
	Com_Memset( binds, 0, sizeof( binds ) );
	binds[0].binding = 0;
	binds[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[0].descriptorCount = 1;
	binds[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[1].binding = 1;
	binds[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[1].descriptorCount = 1;
	binds[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[2].binding = 2;
	binds[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[2].descriptorCount = 1;
	binds[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[3].binding = 3;
	binds[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[3].descriptorCount = 1;
	binds[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[4].binding = 4;
	binds[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[4].descriptorCount = 1;
	binds[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[5].binding = 5;
	binds[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[5].descriptorCount = 1;
	binds[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[6].binding = 6;
	binds[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	binds[6].descriptorCount = 1;
	binds[6].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[7].binding = 7;
	binds[7].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[7].descriptorCount = 1;
	binds[7].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[8].binding = 8;
	binds[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[8].descriptorCount = 1;
	binds[8].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[9].binding = 9;
	binds[9].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[9].descriptorCount = 1;
	binds[9].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	if ( !VRCS_CreateComputePipe( vk.modules.deferred_lighting_vrcs_cs, binds, 10, sizeof( vrcs_light_push_t ),
		&vrcs.light_layout, &vrcs.light_pl, &vrcs.light_pipe, &vrcs.light_pool, &vrcs.light_set,
		"vrcs deferred lighting" ) ) {
		return qfalse;
	}

	return qtrue;
}

static qboolean VRCS_EnsureResources( uint32_t width, uint32_t height )
{
	uint32_t sriW, sriH, tilesX, tilesY;
	VkDeviceSize hdrBytes, coordBytes;

	if ( width < 1 || height < 1 ) {
		return qfalse;
	}

	sriW = ( width + 1u ) / 2u;
	sriH = ( height + 1u ) / 2u;
	tilesX = ( width + VRCS_TILE - 1u ) / VRCS_TILE;
	tilesY = ( height + VRCS_TILE - 1u ) / VRCS_TILE;

	if ( vrcs.ready && vrcs.width == width && vrcs.height == height &&
		vrcs.sri.image != VK_NULL_HANDLE && vrcs.tileHeaders.buffer != VK_NULL_HANDLE ) {
		return qtrue;
	}

	vrcs.width = width;
	vrcs.height = height;
	vrcs.sriW = sriW;
	vrcs.sriH = sriH;
	vrcs.tilesX = tilesX;
	vrcs.tilesY = tilesY;

	if ( !VRCS_CreateSriImage( sriW, sriH ) ) {
		return qfalse;
	}

	hdrBytes = (VkDeviceSize)tilesX * tilesY * sizeof( uint32_t ) * 2u;
	coordBytes = (VkDeviceSize)tilesX * tilesY * VRCS_MAX_PACK * sizeof( uint32_t );
	if ( !VRCS_CreateBuffer( hdrBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &vrcs.tileHeaders ) ) {
		return qfalse;
	}
	if ( !VRCS_CreateBuffer( coordBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &vrcs.tileCoords ) ) {
		return qfalse;
	}

	if ( !vrcs.light_pipe ) {
		if ( !VRCS_CreatePipelines() ) {
			return qfalse;
		}
	}

	vrcs.ready = qtrue;
	return qtrue;
}

qboolean vk_vrcs_active( void )
{
	if ( !r_vrcs || !r_vrcs->integer ) {
		return qfalse;
	}
	if ( !vk_deferred_lighting_active() ) {
		return qfalse;
	}
	if ( !vrcs.ready && ( vk.modules.vrcs_sri_cs == VK_NULL_HANDLE ||
		vk.modules.deferred_lighting_vrcs_cs == VK_NULL_HANDLE ) ) {
		return qfalse;
	}
	return qtrue;
}

void vk_vrcs_init( void )
{
	Com_Memset( &vrcs, 0, sizeof( vrcs ) );

	r_vrcs = ri.Cvar_Get( "r_vrcs", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_vrcs, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_vrcs,
		"Variable-Rate Compute Shading for deferred lighting (latched, vid_restart). "
		"Requires r_fbo + r_deferredLighting. Builds 2x2 SRI, packs 16x16 tiles, deblocks." );

	r_vrcs_quality = ri.Cvar_Get( "r_vrcs_quality", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vrcs_quality, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_vrcs_quality, "VRCS rate thresholds: 0=aggressive, 1=balanced, 2=quality." );

	r_vrcs_extraHalf = ri.Cvar_Get( "r_vrcs_extraHalf", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vrcs_extraHalf, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_vrcs_extraHalf, "Boost half-rate shading when SRI would choose 2x2 (reduces macro noise)." );

	r_vrcs_deblock = ri.Cvar_Get( "r_vrcs_deblock", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vrcs_deblock, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_vrcs_deblock, "Average primary/duplicate neighbors after VRCS lighting (fixes half-pixel offset)." );

	r_vrcs_debug = ri.Cvar_Get( "r_vrcs_debug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vrcs_debug, "0", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_vrcs_debug,
		"VRCS debug: 0=off, 1=SRI rates, 2=primary/dup mask, 3=packed-tile tint." );

	r_vrcs_edge = ri.Cvar_Get( "r_vrcs_edge", "0.08", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vrcs_edge, "0.001", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_vrcs_edge, "Luma gradient threshold for full-rate (1x1) shading." );

	r_vrcs_flat = ri.Cvar_Get( "r_vrcs_flat", "0.02", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vrcs_flat, "0.0001", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_vrcs_flat, "Luma gradient threshold below which 2x2 rate is allowed." );

	ri.Cmd_AddCommand( "vrcs_status", vk_vrcs_status_f );

	ri.Printf( PRINT_ALL, "[VRCS] Variable-Rate Compute Shading registered (r_vrcs, default 0)\n" );
}

void vk_vrcs_shutdown( void )
{
	VRCS_DestroyPipelines();
	VRCS_DestroyImage( &vrcs.sri );
	VRCS_DestroyBuffer( &vrcs.tileHeaders );
	VRCS_DestroyBuffer( &vrcs.tileCoords );
	Com_Memset( &vrcs, 0, sizeof( vrcs ) );
}

void vk_vrcs_frame_begin( void )
{
	vrcs.frame++;
}

static VkImageView VRCS_HistoryView( void )
{
	/* Prefer G-buffer albedo (stable mid-frame). Fall back to TAA history / color. */
	if ( vk.deferred_gbuffer_albedo_view != VK_NULL_HANDLE ) {
		return vk.deferred_gbuffer_albedo_view;
	}
	if ( r_taa && r_taa->integer && vk.taa_history_image_view[0] != VK_NULL_HANDLE ) {
		int idx = (int)( vrcs.frame & 1u );
		if ( vk.taa_history_image_view[idx] != VK_NULL_HANDLE ) {
			return vk.taa_history_image_view[idx];
		}
		return vk.taa_history_image_view[0];
	}
	return vk.color_image_view;
}

static void VRCS_FillLightPush( vrcs_light_push_t *push, uint32_t width, uint32_t height )
{
	const float *view = backEnd.viewParms.world.modelViewMatrix;
	const float *proj = backEnd.useFirstPersonProjection ? backEnd.firstPersonProjectionMatrix :
		backEnd.viewParms.projectionMatrix;
	float proj_vk[16];
	float aspect = ( backEnd.viewParms.viewportHeight > 0 ) ?
		( (float)backEnd.viewParms.viewportWidth / (float)backEnd.viewParms.viewportHeight ) : 1.0f;

	Com_Memcpy( push->viewMatrix, view, sizeof( push->viewMatrix ) );
	vk_get_projection_matrix_vk( proj, proj_vk );
	push->projInfo[0] = 1.0f / ( proj_vk[0] * aspect );
	push->projInfo[1] = 1.0f / proj_vk[5];
	push->projInfo[2] = proj_vk[10];
	push->projInfo[3] = proj_vk[14];
	push->extent[0] = width;
	push->extent[1] = height;
	push->tileGrid[0] = vk.forward_plus.tiles_x;
	push->tileGrid[1] = vk.forward_plus.tiles_y;
	push->strength = ( r_deferredLightingStrength && r_deferredLightingStrength->value > 0.0f ) ?
		Com_Clamp( 0.0f, 4.0f, r_deferredLightingStrength->value ) : 1.0f;
	push->maxPerTile = vk.forward_plus.max_per_tile;
	push->numLights = vk.forward_plus.last_packed_count;
	push->additive = ( r_deferredUnlitBase && r_deferredUnlitBase->integer ) ? 1u : 0u;
	push->specular = ( r_deferredSpecular && r_deferredSpecular->integer ) ? 1u : 0u;
	push->aoStrength = r_deferredAOCoupling ?
		Com_Clamp( 0.0f, 1.0f, r_deferredAOCoupling->value ) : 0.65f;
	push->specularStrength = r_deferredSpecularStrength ?
		Com_Clamp( 0.0f, 4.0f, r_deferredSpecularStrength->value ) : 1.0f;
	push->normalsAreWorld = vk.deferredGbufferDirectExport ? 1u : 0u;
	push->useMaterialClass = ( r_deferredMaterialClassify && r_deferredMaterialClassify->integer &&
		vk_material_classify_wanted() && vk.visibility_buffer_class_view != VK_NULL_HANDLE ) ? 1u : 0u;
	push->vrcsTileGrid[0] = vrcs.tilesX;
	push->vrcsTileGrid[1] = vrcs.tilesY;
	push->debugMode = r_vrcs_debug ? (uint32_t)r_vrcs_debug->integer : 0u;
	push->reserved = 0;
	push->zSlices = vk.forward_plus.z_slices > 0u ? vk.forward_plus.z_slices : 1u;
	push->zSliceMode = ( r_forwardPlusZSliceMode && r_forwardPlusZSliceMode->integer ) ? 1u : 0u;
	{
		float zn = ( r_znear && r_znear->value > 0.0f ) ? r_znear->value : 4.0f;
		float zf = backEnd.viewParms.zFar;
		if ( zn < 1e-3f ) {
			zn = 4.0f;
		}
		if ( zf <= zn + 1e-3f ) {
			zf = zn + 4000.0f;
		}
		push->zNear = zn;
		push->zFar = zf;
	}
}

qboolean vk_vrcs_dispatch_deferred_lighting( uint32_t width, uint32_t height )
{
	VkCommandBuffer cmd;
	VkImageAspectFlags depth_aspect;
	VkSampler sampler;
	VkDescriptorImageInfo imgInfo[8];
	VkDescriptorBufferInfo bufInfo[4];
	VkWriteDescriptorSet writes[12];
	vrcs_sri_push_t sriPush;
	vrcs_pack_push_t packPush;
	vrcs_deblock_push_t debPush;
	vrcs_light_push_t lightPush;
	VkImageView historyView;
	VkImageView depthView;
	VkImageView classView;
	Vk_Sampler_Def sd;
	uint32_t gx, gy;
	uint32_t nWrites;
	VkImageMemoryBarrier imgBarrier;
	VkMemoryBarrier memBarrier;

	if ( !vk_vrcs_active() ) {
		return qfalse;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return qfalse;
	}
	if ( vk.forward_plus.buffer == VK_NULL_HANDLE || vk.forward_plus.tile_buffer == VK_NULL_HANDLE ||
		vk.deferred_lighting_image == VK_NULL_HANDLE || vk.deferred_lighting_view == VK_NULL_HANDLE ) {
		return qfalse;
	}

	if ( !VRCS_EnsureResources( width, height ) ) {
		ri.Printf( PRINT_WARNING, "[VRCS] resource ensure failed — falling back\n" );
		return qfalse;
	}

	cmd = vk.cmd->command_buffer;
	historyView = VRCS_HistoryView();
	if ( historyView == VK_NULL_HANDLE ) {
		return qfalse;
	}
	depthView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	classView = ( vk.visibility_buffer_class_view != VK_NULL_HANDLE ) ?
		vk.visibility_buffer_class_view : vk.deferred_class_stub_view;

	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;
	sampler = vk_find_sampler( &sd );

	depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	record_depth_image_layout_transition( cmd, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	/* --- SRI --- */
	record_image_layout_transition( cmd, vrcs.sri.image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0 );

	Com_Memset( imgInfo, 0, sizeof( imgInfo ) );
	Com_Memset( writes, 0, sizeof( writes ) );
	imgInfo[0].sampler = sampler;
	imgInfo[0].imageView = historyView;
	imgInfo[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imgInfo[1].imageView = vrcs.sri.view;
	imgInfo[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = vrcs.sri_set;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &imgInfo[0];
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = vrcs.sri_set;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[1].pImageInfo = &imgInfo[1];
	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );

	Com_Memset( &sriPush, 0, sizeof( sriPush ) );
	sriPush.extent[0] = width;
	sriPush.extent[1] = height;
	sriPush.sriExtent[0] = vrcs.sriW;
	sriPush.sriExtent[1] = vrcs.sriH;
	sriPush.edgeThresh = r_vrcs_edge ? r_vrcs_edge->value : 0.08f;
	sriPush.flatThresh = r_vrcs_flat ? r_vrcs_flat->value : 0.02f;
	sriPush.extraHalf = ( r_vrcs_extraHalf && r_vrcs_extraHalf->integer ) ? 1u : 0u;
	sriPush.quality = r_vrcs_quality ? (uint32_t)r_vrcs_quality->integer : 1u;

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vrcs.sri_pipe );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vrcs.sri_pl, 0, 1, &vrcs.sri_set, 0, NULL );
	qvkCmdPushConstants( cmd, vrcs.sri_pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( sriPush ), &sriPush );
	gx = ( vrcs.sriW + 7u ) / 8u;
	gy = ( vrcs.sriH + 7u ) / 8u;
	qvkCmdDispatch( cmd, gx, gy, 1 );

	Com_Memset( &imgBarrier, 0, sizeof( imgBarrier ) );
	imgBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imgBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	imgBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imgBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	imgBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imgBarrier.image = vrcs.sri.image;
	imgBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imgBarrier.subresourceRange.levelCount = 1;
	imgBarrier.subresourceRange.layerCount = 1;
	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, NULL, 0, NULL, 1, &imgBarrier );

	/* --- Pack --- */
	Com_Memset( imgInfo, 0, sizeof( imgInfo ) );
	Com_Memset( bufInfo, 0, sizeof( bufInfo ) );
	Com_Memset( writes, 0, sizeof( writes ) );
	imgInfo[0].sampler = sampler;
	imgInfo[0].imageView = vrcs.sri.view;
	imgInfo[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imgInfo[1].sampler = sampler;
	imgInfo[1].imageView = depthView;
	imgInfo[1].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	bufInfo[0].buffer = vrcs.tileHeaders.buffer;
	bufInfo[0].range = VK_WHOLE_SIZE;
	bufInfo[1].buffer = vrcs.tileCoords.buffer;
	bufInfo[1].range = VK_WHOLE_SIZE;
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = vrcs.pack_set;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &imgInfo[0];
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = vrcs.pack_set;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].pImageInfo = &imgInfo[1];
	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = vrcs.pack_set;
	writes[2].dstBinding = 2;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[2].pBufferInfo = &bufInfo[0];
	writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[3].dstSet = vrcs.pack_set;
	writes[3].dstBinding = 3;
	writes[3].descriptorCount = 1;
	writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[3].pBufferInfo = &bufInfo[1];
	qvkUpdateDescriptorSets( vk.device, 4, writes, 0, NULL );

	Com_Memset( &packPush, 0, sizeof( packPush ) );
	packPush.extent[0] = width;
	packPush.extent[1] = height;
	packPush.tileGrid[0] = vrcs.tilesX;
	packPush.tileGrid[1] = vrcs.tilesY;
	packPush.rotatePrimary = ( vrcs.frame & 1u );

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vrcs.pack_pipe );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vrcs.pack_pl, 0, 1, &vrcs.pack_set, 0, NULL );
	qvkCmdPushConstants( cmd, vrcs.pack_pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( packPush ), &packPush );
	qvkCmdDispatch( cmd, vrcs.tilesX, vrcs.tilesY, 1 );

	Com_Memset( &memBarrier, 0, sizeof( memBarrier ) );
	memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 1, &memBarrier, 0, NULL, 0, NULL );

	/* --- Lighting --- */
	record_image_layout_transition( cmd, vk.deferred_lighting_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0 );

	Com_Memset( imgInfo, 0, sizeof( imgInfo ) );
	Com_Memset( bufInfo, 0, sizeof( bufInfo ) );
	Com_Memset( writes, 0, sizeof( writes ) );
	bufInfo[0].buffer = vk.forward_plus.buffer;
	bufInfo[0].range = VK_WHOLE_SIZE;
	bufInfo[1].buffer = vk.forward_plus.tile_buffer;
	bufInfo[1].range = VK_WHOLE_SIZE;
	bufInfo[2].buffer = vrcs.tileHeaders.buffer;
	bufInfo[2].range = VK_WHOLE_SIZE;
	bufInfo[3].buffer = vrcs.tileCoords.buffer;
	bufInfo[3].range = VK_WHOLE_SIZE;
	imgInfo[0].sampler = sampler;
	imgInfo[0].imageView = depthView;
	imgInfo[0].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	imgInfo[1].sampler = sampler;
	imgInfo[1].imageView = vk.deferred_gbuffer_albedo_view;
	imgInfo[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imgInfo[2].sampler = sampler;
	imgInfo[2].imageView = vk.deferred_gbuffer_normal_view;
	imgInfo[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imgInfo[3].sampler = sampler;
	imgInfo[3].imageView = vk.deferred_gbuffer_material_view;
	imgInfo[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imgInfo[4].imageView = vk.deferred_lighting_view;
	imgInfo[4].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imgInfo[5].sampler = sampler;
	imgInfo[5].imageView = classView;
	imgInfo[5].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	nWrites = 0;
	writes[nWrites].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[nWrites].dstSet = vrcs.light_set;
	writes[nWrites].dstBinding = 0;
	writes[nWrites].descriptorCount = 1;
	writes[nWrites].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[nWrites].pBufferInfo = &bufInfo[0];
	nWrites++;
	writes[nWrites].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[nWrites].dstSet = vrcs.light_set;
	writes[nWrites].dstBinding = 1;
	writes[nWrites].descriptorCount = 1;
	writes[nWrites].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[nWrites].pBufferInfo = &bufInfo[1];
	nWrites++;
	writes[nWrites].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[nWrites].dstSet = vrcs.light_set;
	writes[nWrites].dstBinding = 2;
	writes[nWrites].descriptorCount = 1;
	writes[nWrites].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[nWrites].pImageInfo = &imgInfo[0];
	nWrites++;
	writes[nWrites].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[nWrites].dstSet = vrcs.light_set;
	writes[nWrites].dstBinding = 3;
	writes[nWrites].descriptorCount = 1;
	writes[nWrites].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[nWrites].pImageInfo = &imgInfo[1];
	nWrites++;
	writes[nWrites].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[nWrites].dstSet = vrcs.light_set;
	writes[nWrites].dstBinding = 4;
	writes[nWrites].descriptorCount = 1;
	writes[nWrites].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[nWrites].pImageInfo = &imgInfo[2];
	nWrites++;
	writes[nWrites].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[nWrites].dstSet = vrcs.light_set;
	writes[nWrites].dstBinding = 5;
	writes[nWrites].descriptorCount = 1;
	writes[nWrites].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[nWrites].pImageInfo = &imgInfo[3];
	nWrites++;
	writes[nWrites].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[nWrites].dstSet = vrcs.light_set;
	writes[nWrites].dstBinding = 6;
	writes[nWrites].descriptorCount = 1;
	writes[nWrites].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[nWrites].pImageInfo = &imgInfo[4];
	nWrites++;
	writes[nWrites].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[nWrites].dstSet = vrcs.light_set;
	writes[nWrites].dstBinding = 7;
	writes[nWrites].descriptorCount = 1;
	writes[nWrites].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[nWrites].pImageInfo = &imgInfo[5];
	nWrites++;
	writes[nWrites].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[nWrites].dstSet = vrcs.light_set;
	writes[nWrites].dstBinding = 8;
	writes[nWrites].descriptorCount = 1;
	writes[nWrites].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[nWrites].pBufferInfo = &bufInfo[2];
	nWrites++;
	writes[nWrites].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[nWrites].dstSet = vrcs.light_set;
	writes[nWrites].dstBinding = 9;
	writes[nWrites].descriptorCount = 1;
	writes[nWrites].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[nWrites].pBufferInfo = &bufInfo[3];
	nWrites++;
	qvkUpdateDescriptorSets( vk.device, nWrites, writes, 0, NULL );

	VRCS_FillLightPush( &lightPush, width, height );
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vrcs.light_pipe );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vrcs.light_pl, 0, 1, &vrcs.light_set, 0, NULL );
	qvkCmdPushConstants( cmd, vrcs.light_pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( lightPush ), &lightPush );
	qvkCmdDispatch( cmd, vrcs.tilesX, vrcs.tilesY, 1 );

	/* --- Deblock --- */
	if ( ( r_vrcs_deblock && r_vrcs_deblock->integer ) || ( r_vrcs_debug && r_vrcs_debug->integer > 0 ) ) {
		Com_Memset( &imgBarrier, 0, sizeof( imgBarrier ) );
		imgBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imgBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		imgBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		imgBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		imgBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		imgBarrier.image = vk.deferred_lighting_image;
		imgBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imgBarrier.subresourceRange.levelCount = 1;
		imgBarrier.subresourceRange.layerCount = 1;
		qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0, 0, NULL, 0, NULL, 1, &imgBarrier );

		Com_Memset( imgInfo, 0, sizeof( imgInfo ) );
		Com_Memset( writes, 0, sizeof( writes ) );
		imgInfo[0].sampler = sampler;
		imgInfo[0].imageView = vrcs.sri.view;
		imgInfo[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imgInfo[1].imageView = vk.deferred_lighting_view;
		imgInfo[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = vrcs.deblock_set;
		writes[0].dstBinding = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[0].pImageInfo = &imgInfo[0];
		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet = vrcs.deblock_set;
		writes[1].dstBinding = 1;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[1].pImageInfo = &imgInfo[1];
		qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );

		Com_Memset( &debPush, 0, sizeof( debPush ) );
		debPush.extent[0] = width;
		debPush.extent[1] = height;
		debPush.debugMode = r_vrcs_debug ? (uint32_t)r_vrcs_debug->integer : 0u;
		if ( debPush.debugMode > 2u ) {
			debPush.debugMode = 0u; /* mode 3 handled in lighting */
		}

		qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vrcs.deblock_pipe );
		qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vrcs.deblock_pl, 0, 1, &vrcs.deblock_set, 0, NULL );
		qvkCmdPushConstants( cmd, vrcs.deblock_pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( debPush ), &debPush );
		gx = ( width + 7u ) / 8u;
		gy = ( height + 7u ) / 8u;
		qvkCmdDispatch( cmd, gx, gy, 1 );
	}

	record_image_layout_transition( cmd, vk.deferred_lighting_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );

	record_depth_image_layout_transition( cmd, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT );

	if ( !vrcs.logged ) {
		ri.Printf( PRINT_ALL,
			"[VRCS] deferred lighting path active (quality=%d extraHalf=%d deblock=%d tiles=%ux%u sri=%ux%u)\n",
			r_vrcs_quality ? r_vrcs_quality->integer : 1,
			r_vrcs_extraHalf ? r_vrcs_extraHalf->integer : 1,
			r_vrcs_deblock ? r_vrcs_deblock->integer : 1,
			vrcs.tilesX, vrcs.tilesY, vrcs.sriW, vrcs.sriH );
		vrcs.logged = qtrue;
	}

	return qtrue;
}

void vk_vrcs_status_f( void )
{
	ri.Printf( PRINT_ALL, "[VRCS] r_vrcs=%d active=%d ready=%d frame=%u\n",
		r_vrcs ? r_vrcs->integer : 0,
		vk_vrcs_active() ? 1 : 0,
		vrcs.ready ? 1 : 0,
		vrcs.frame );
	ri.Printf( PRINT_ALL, "[VRCS] ext=%ux%u sri=%ux%u tiles=%ux%u quality=%d extraHalf=%d deblock=%d debug=%d\n",
		vrcs.width, vrcs.height, vrcs.sriW, vrcs.sriH, vrcs.tilesX, vrcs.tilesY,
		r_vrcs_quality ? r_vrcs_quality->integer : 1,
		r_vrcs_extraHalf ? r_vrcs_extraHalf->integer : 1,
		r_vrcs_deblock ? r_vrcs_deblock->integer : 1,
		r_vrcs_debug ? r_vrcs_debug->integer : 0 );
	ri.Printf( PRINT_ALL, "[VRCS] edge=%.4f flat=%.4f deferredLighting=%d modules sri=%d pack=%d light=%d deblock=%d\n",
		r_vrcs_edge ? r_vrcs_edge->value : 0.0f,
		r_vrcs_flat ? r_vrcs_flat->value : 0.0f,
		r_deferredLighting ? r_deferredLighting->integer : 0,
		vk.modules.vrcs_sri_cs != VK_NULL_HANDLE,
		vk.modules.vrcs_pack_cs != VK_NULL_HANDLE,
		vk.modules.deferred_lighting_vrcs_cs != VK_NULL_HANDLE,
		vk.modules.vrcs_deblock_cs != VK_NULL_HANDLE );
}
