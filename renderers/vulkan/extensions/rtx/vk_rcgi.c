/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Radiance Cache GI (RcGI) — spatial-hash radiance cache + cascaded probes.
Chocolate RTX path; light grid / ray-query sample / cache shade / gather.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_rcgi.h"
#include "vk_hybrid1.h"
#include "vk_rtx.h"
#include "vk_rtx_bindless.h"
#include "vk_util.h"
#include "vk_image_layout.h"
#include "vk_view_state.h"
#include "vk_cmd.h"
#include "vk_ambient_visibility.h"

#ifdef USE_VULKAN_RTX
#include "vk_rcgi_spirv.inc"

#define RCGI_GRID_RES           12u
#define RCGI_CASCADES           4u
#define RCGI_MAX_LIGHT_IDS      64u
#define RCGI_MAX_LIGHTS         64u
#define RCGI_CACHE_CELLS        32768u
#define RCGI_CACHE_CELL_BYTES   48u
#define RCGI_PROBE_TILE         8u
#define RCGI_ATLAS_W            1024u
#define RCGI_ATLAS_H            1024u
#define RCGI_PROBES_PER_CASCADE ( RCGI_GRID_RES * RCGI_GRID_RES * RCGI_GRID_RES )
#define RCGI_GRID_CELLS         ( RCGI_CASCADES * RCGI_PROBES_PER_CASCADE )
#define RCGI_MAX_RAYS           64u
#define RCGI_ACTIVE_LIST_BYTES  ( 16u + RCGI_CACHE_CELLS * 4u )

typedef struct {
	float posRange[4];
	float color[4];
} RcGiLightGPU;

typedef struct {
	VkBuffer buffer;
	VkDeviceMemory memory;
	VkDeviceSize size;
} rcgi_buffer_t;

typedef struct {
	VkImage image;
	VkImageView view;
	VkDeviceMemory memory;
	uint32_t width, height;
} rcgi_image_t;

typedef struct {
	qboolean ready;
	qboolean logged_stub;
	uint32_t frame;
	uint32_t width, height;
	uint32_t gatherW, gatherH;
	int quality;

	rcgi_buffer_t lights;
	rcgi_buffer_t gridIds;
	rcgi_buffer_t gridCounts;
	rcgi_buffer_t cache;
	rcgi_buffer_t activeList;
	rcgi_buffer_t hitPack;
	rcgi_buffer_t dummy_albedo;
	rcgi_buffer_t dummy_normal;

	rcgi_image_t probeAtlas;
	rcgi_image_t gatherA;
	rcgi_image_t gatherB;
	rcgi_image_t shR;
	rcgi_image_t shG;
	rcgi_image_t shB;
	rcgi_image_t irradiance;
	rcgi_image_t history;

	VkShaderModule light_grid_cs;
	VkShaderModule sample_cs;
	VkShaderModule cache_shade_cs;
	VkShaderModule volume_update_cs;
	VkShaderModule final_gather_cs;
	VkShaderModule denoise_cs;
	VkShaderModule upscale_cs;
	VkShaderModule composite_cs;

	VkDescriptorSetLayout light_grid_layout;
	VkDescriptorSetLayout sample_layout;
	VkDescriptorSetLayout cache_shade_layout;
	VkDescriptorSetLayout volume_update_layout;
	VkDescriptorSetLayout final_gather_layout;
	VkDescriptorSetLayout denoise_layout;
	VkDescriptorSetLayout upscale_layout;
	VkDescriptorSetLayout composite_layout;

	VkPipelineLayout light_grid_pl;
	VkPipelineLayout sample_pl;
	VkPipelineLayout cache_shade_pl;
	VkPipelineLayout volume_update_pl;
	VkPipelineLayout final_gather_pl;
	VkPipelineLayout denoise_pl;
	VkPipelineLayout upscale_pl;
	VkPipelineLayout composite_pl;

	VkPipeline light_grid_pipe;
	VkPipeline sample_pipe;
	VkPipeline cache_shade_pipe;
	VkPipeline volume_update_pipe;
	VkPipeline final_gather_pipe;
	VkPipeline denoise_pipe;
	VkPipeline upscale_pipe;
	VkPipeline composite_pipe;

	VkDescriptorPool pool;
	VkDescriptorSet light_grid_set;
	VkDescriptorSet sample_set;
	VkDescriptorSet cache_shade_set;
	VkDescriptorSet volume_update_set;
	VkDescriptorSet final_gather_set;
	VkDescriptorSet denoise_set;
	VkDescriptorSet upscale_set;
	VkDescriptorSet composite_set;
} rcgi_state_t;

static rcgi_state_t rcgi;

static cvar_t *r_rcgi;
static cvar_t *r_rcgi_quality;
static cvar_t *r_rcgi_rays;
static cvar_t *r_rcgi_cellSize;
static cvar_t *r_rcgi_strength;
static cvar_t *r_rcgi_hybrid1Fusion;
static cvar_t *r_rcgi_debug;

static VkSampler RCGI_Nearest( void )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;
	return vk_find_sampler( &sd );
}

static VkSampler RCGI_Linear( void )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;
	return vk_find_sampler( &sd );
}

static float RCGI_CascadeHalfExtent( uint32_t cascade )
{
	float ext = 32.0f;
	while ( cascade > 0u ) {
		ext *= 2.0f;
		cascade--;
	}
	return ext;
}

static void RCGI_GatherExtent( uint32_t fullW, uint32_t fullH, int quality, uint32_t *outW, uint32_t *outH )
{
	uint32_t shift = ( quality == 0 ) ? 2u : 1u;
	*outW = fullW >> shift;
	*outH = fullH >> shift;
	if ( *outW < 1u ) {
		*outW = 1u;
	}
	if ( *outH < 1u ) {
		*outH = 1u;
	}
}

static void RCGI_DestroyBuffer( rcgi_buffer_t *b )
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

static qboolean RCGI_CreateBuffer( VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memFlags, rcgi_buffer_t *out )
{
	VkBufferCreateInfo bi;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo ai;
	VkMemoryAllocateFlagsInfo flagsInfo;

	Com_Memset( out, 0, sizeof( *out ) );
	Com_Memset( &bi, 0, sizeof( bi ) );
	bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bi.size = size;
	bi.usage = usage;
	VK_CHECK( qvkCreateBuffer( vk.device, &bi, NULL, &out->buffer ) );
	qvkGetBufferMemoryRequirements( vk.device, out->buffer, &req );

	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits, memFlags );
	if ( usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT ) {
		Com_Memset( &flagsInfo, 0, sizeof( flagsInfo ) );
		flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
		flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
		ai.pNext = &flagsInfo;
	}
	VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, &out->memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, out->buffer, out->memory, 0 ) );
	out->size = size;
	return qtrue;
}

static void RCGI_DestroyImage( rcgi_image_t *img )
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

static qboolean RCGI_CreateImage2D( rcgi_image_t *img, uint32_t w, uint32_t h, VkImageUsageFlags usage )
{
	VkImageCreateInfo ii;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo ai;
	VkImageViewCreateInfo vi;

	RCGI_DestroyImage( img );
	if ( w < 1 || h < 1 ) {
		return qfalse;
	}
	Com_Memset( &ii, 0, sizeof( ii ) );
	ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ii.imageType = VK_IMAGE_TYPE_2D;
	ii.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	ii.extent.width = w;
	ii.extent.height = h;
	ii.extent.depth = 1;
	ii.mipLevels = 1;
	ii.arrayLayers = 1;
	ii.samples = VK_SAMPLE_COUNT_1_BIT;
	ii.tiling = VK_IMAGE_TILING_OPTIMAL;
	ii.usage = usage;
	ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK( qvkCreateImage( vk.device, &ii, NULL, &img->image ) );
	qvkGetImageMemoryRequirements( vk.device, img->image, &req );
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, &img->memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, img->image, img->memory, 0 ) );
	Com_Memset( &vi, 0, sizeof( vi ) );
	vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	vi.image = img->image;
	vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
	vi.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vi.subresourceRange.levelCount = 1;
	vi.subresourceRange.layerCount = 1;
	VK_CHECK( qvkCreateImageView( vk.device, &vi, NULL, &img->view ) );
	img->width = w;
	img->height = h;
	return qtrue;
}

static VkShaderModule RCGI_Module( const uint8_t *code, uint32_t size, const char *name )
{
	VkShaderModuleCreateInfo ci;
	VkShaderModule mod = VK_NULL_HANDLE;
	uint32_t *aligned = NULL;
	VkResult res;

	if ( !code || size < 4u || ( size & 3u ) != 0u ) {
		ri.Printf( PRINT_WARNING, "[RcGI] Invalid SPIR-V for %s (size=%u)\n", name, size );
		return VK_NULL_HANDLE;
	}

	/* NVIDIA CreateShaderModule requires 4-byte-aligned pCode; static uint8_t[] is not guaranteed. */
	aligned = (uint32_t *)ri.Hunk_AllocateTempMemory( (int)size );
	if ( !aligned ) {
		ri.Printf( PRINT_WARNING, "[RcGI] Out of temp memory for shader module %s\n", name );
		return VK_NULL_HANDLE;
	}
	Com_Memcpy( aligned, code, size );

	ri.Printf( PRINT_ALL, "[RcGI] creating shader module %s (%u bytes)\n", name, size );
	Com_Memset( &ci, 0, sizeof( ci ) );
	ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.codeSize = size;
	ci.pCode = aligned;
	res = qvkCreateShaderModule( vk.device, &ci, NULL, &mod );
	ri.Hunk_FreeTempMemory( aligned );
	if ( res != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, "[RcGI] Failed to create shader module %s (%s)\n",
			name, vk_result_string( res ) );
		return VK_NULL_HANDLE;
	}
	return mod;
}

static qboolean RCGI_CreateComputePipe( VkPipelineLayout layout, VkShaderModule module,
	const char *name, VkPipeline *outPipe )
{
	VkComputePipelineCreateInfo pci;
	VkPipelineShaderStageCreateInfo stage;
	VkResult res;

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = module;
	stage.pName = "main";
	Com_Memset( &pci, 0, sizeof( pci ) );
	pci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pci.stage = stage;
	pci.layout = layout;

	ri.Printf( PRINT_ALL, "[RcGI] creating compute pipeline %s\n", name );
	res = qvkCreateComputePipelines( vk.device, vk.pipelineCache, 1, &pci, NULL, outPipe );
	if ( res != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, "[RcGI] Failed to create compute pipeline %s (%s)\n",
			name, vk_result_string( res ) );
		*outPipe = VK_NULL_HANDLE;
		return qfalse;
	}
	return qtrue;
}

static void RCGI_DestroyPipelines( void )
{
	if ( rcgi.light_grid_pipe ) { qvkDestroyPipeline( vk.device, rcgi.light_grid_pipe, NULL ); rcgi.light_grid_pipe = VK_NULL_HANDLE; }
	if ( rcgi.sample_pipe ) { qvkDestroyPipeline( vk.device, rcgi.sample_pipe, NULL ); rcgi.sample_pipe = VK_NULL_HANDLE; }
	if ( rcgi.cache_shade_pipe ) { qvkDestroyPipeline( vk.device, rcgi.cache_shade_pipe, NULL ); rcgi.cache_shade_pipe = VK_NULL_HANDLE; }
	if ( rcgi.volume_update_pipe ) { qvkDestroyPipeline( vk.device, rcgi.volume_update_pipe, NULL ); rcgi.volume_update_pipe = VK_NULL_HANDLE; }
	if ( rcgi.final_gather_pipe ) { qvkDestroyPipeline( vk.device, rcgi.final_gather_pipe, NULL ); rcgi.final_gather_pipe = VK_NULL_HANDLE; }
	if ( rcgi.denoise_pipe ) { qvkDestroyPipeline( vk.device, rcgi.denoise_pipe, NULL ); rcgi.denoise_pipe = VK_NULL_HANDLE; }
	if ( rcgi.upscale_pipe ) { qvkDestroyPipeline( vk.device, rcgi.upscale_pipe, NULL ); rcgi.upscale_pipe = VK_NULL_HANDLE; }
	if ( rcgi.composite_pipe ) { qvkDestroyPipeline( vk.device, rcgi.composite_pipe, NULL ); rcgi.composite_pipe = VK_NULL_HANDLE; }

	if ( rcgi.light_grid_pl ) { qvkDestroyPipelineLayout( vk.device, rcgi.light_grid_pl, NULL ); rcgi.light_grid_pl = VK_NULL_HANDLE; }
	if ( rcgi.sample_pl ) { qvkDestroyPipelineLayout( vk.device, rcgi.sample_pl, NULL ); rcgi.sample_pl = VK_NULL_HANDLE; }
	if ( rcgi.cache_shade_pl ) { qvkDestroyPipelineLayout( vk.device, rcgi.cache_shade_pl, NULL ); rcgi.cache_shade_pl = VK_NULL_HANDLE; }
	if ( rcgi.volume_update_pl ) { qvkDestroyPipelineLayout( vk.device, rcgi.volume_update_pl, NULL ); rcgi.volume_update_pl = VK_NULL_HANDLE; }
	if ( rcgi.final_gather_pl ) { qvkDestroyPipelineLayout( vk.device, rcgi.final_gather_pl, NULL ); rcgi.final_gather_pl = VK_NULL_HANDLE; }
	if ( rcgi.denoise_pl ) { qvkDestroyPipelineLayout( vk.device, rcgi.denoise_pl, NULL ); rcgi.denoise_pl = VK_NULL_HANDLE; }
	if ( rcgi.upscale_pl ) { qvkDestroyPipelineLayout( vk.device, rcgi.upscale_pl, NULL ); rcgi.upscale_pl = VK_NULL_HANDLE; }
	if ( rcgi.composite_pl ) { qvkDestroyPipelineLayout( vk.device, rcgi.composite_pl, NULL ); rcgi.composite_pl = VK_NULL_HANDLE; }

	if ( rcgi.light_grid_layout ) { qvkDestroyDescriptorSetLayout( vk.device, rcgi.light_grid_layout, NULL ); rcgi.light_grid_layout = VK_NULL_HANDLE; }
	if ( rcgi.sample_layout ) { qvkDestroyDescriptorSetLayout( vk.device, rcgi.sample_layout, NULL ); rcgi.sample_layout = VK_NULL_HANDLE; }
	if ( rcgi.cache_shade_layout ) { qvkDestroyDescriptorSetLayout( vk.device, rcgi.cache_shade_layout, NULL ); rcgi.cache_shade_layout = VK_NULL_HANDLE; }
	if ( rcgi.volume_update_layout ) { qvkDestroyDescriptorSetLayout( vk.device, rcgi.volume_update_layout, NULL ); rcgi.volume_update_layout = VK_NULL_HANDLE; }
	if ( rcgi.final_gather_layout ) { qvkDestroyDescriptorSetLayout( vk.device, rcgi.final_gather_layout, NULL ); rcgi.final_gather_layout = VK_NULL_HANDLE; }
	if ( rcgi.denoise_layout ) { qvkDestroyDescriptorSetLayout( vk.device, rcgi.denoise_layout, NULL ); rcgi.denoise_layout = VK_NULL_HANDLE; }
	if ( rcgi.upscale_layout ) { qvkDestroyDescriptorSetLayout( vk.device, rcgi.upscale_layout, NULL ); rcgi.upscale_layout = VK_NULL_HANDLE; }
	if ( rcgi.composite_layout ) { qvkDestroyDescriptorSetLayout( vk.device, rcgi.composite_layout, NULL ); rcgi.composite_layout = VK_NULL_HANDLE; }

	if ( rcgi.pool ) { qvkDestroyDescriptorPool( vk.device, rcgi.pool, NULL ); rcgi.pool = VK_NULL_HANDLE; }
	rcgi.light_grid_set = rcgi.sample_set = rcgi.cache_shade_set = rcgi.volume_update_set = VK_NULL_HANDLE;
	rcgi.final_gather_set = rcgi.denoise_set = rcgi.upscale_set = rcgi.composite_set = VK_NULL_HANDLE;

	if ( rcgi.light_grid_cs ) { qvkDestroyShaderModule( vk.device, rcgi.light_grid_cs, NULL ); rcgi.light_grid_cs = VK_NULL_HANDLE; }
	if ( rcgi.sample_cs ) { qvkDestroyShaderModule( vk.device, rcgi.sample_cs, NULL ); rcgi.sample_cs = VK_NULL_HANDLE; }
	if ( rcgi.cache_shade_cs ) { qvkDestroyShaderModule( vk.device, rcgi.cache_shade_cs, NULL ); rcgi.cache_shade_cs = VK_NULL_HANDLE; }
	if ( rcgi.volume_update_cs ) { qvkDestroyShaderModule( vk.device, rcgi.volume_update_cs, NULL ); rcgi.volume_update_cs = VK_NULL_HANDLE; }
	if ( rcgi.final_gather_cs ) { qvkDestroyShaderModule( vk.device, rcgi.final_gather_cs, NULL ); rcgi.final_gather_cs = VK_NULL_HANDLE; }
	if ( rcgi.denoise_cs ) { qvkDestroyShaderModule( vk.device, rcgi.denoise_cs, NULL ); rcgi.denoise_cs = VK_NULL_HANDLE; }
	if ( rcgi.upscale_cs ) { qvkDestroyShaderModule( vk.device, rcgi.upscale_cs, NULL ); rcgi.upscale_cs = VK_NULL_HANDLE; }
	if ( rcgi.composite_cs ) { qvkDestroyShaderModule( vk.device, rcgi.composite_cs, NULL ); rcgi.composite_cs = VK_NULL_HANDLE; }
}

static qboolean RCGI_CreatePipelines( void )
{
	VkDescriptorSetLayoutBinding binds[10];
	VkDescriptorSetLayoutCreateInfo lci;
	VkPushConstantRange pcr;
	VkPipelineLayoutCreateInfo plci;
	VkDescriptorPoolSize sizes[4];
	VkDescriptorPoolCreateInfo poolCi;
	VkDescriptorSetAllocateInfo alloc;
	VkDescriptorSetLayout layouts[8];

	rcgi.light_grid_cs = RCGI_Module( vk_rcgi_light_grid_cs_spv, VK_RCGI_LIGHT_GRID_CS_SPV_SIZE, "rcgi_light_grid" );
	rcgi.sample_cs = RCGI_Module( vk_rcgi_sample_cs_spv, VK_RCGI_SAMPLE_CS_SPV_SIZE, "rcgi_sample" );
	rcgi.cache_shade_cs = RCGI_Module( vk_rcgi_cache_shade_cs_spv, VK_RCGI_CACHE_SHADE_CS_SPV_SIZE, "rcgi_cache_shade" );
	rcgi.volume_update_cs = RCGI_Module( vk_rcgi_volume_update_cs_spv, VK_RCGI_VOLUME_UPDATE_CS_SPV_SIZE, "rcgi_volume_update" );
	rcgi.final_gather_cs = RCGI_Module( vk_rcgi_final_gather_cs_spv, VK_RCGI_FINAL_GATHER_CS_SPV_SIZE, "rcgi_final_gather" );
	rcgi.denoise_cs = RCGI_Module( vk_rcgi_denoise_cs_spv, VK_RCGI_DENOISE_CS_SPV_SIZE, "rcgi_denoise" );
	rcgi.upscale_cs = RCGI_Module( vk_rcgi_upscale_cs_spv, VK_RCGI_UPSCALE_CS_SPV_SIZE, "rcgi_upscale" );
	rcgi.composite_cs = RCGI_Module( vk_rcgi_composite_cs_spv, VK_RCGI_COMPOSITE_CS_SPV_SIZE, "rcgi_composite" );
	if ( !rcgi.light_grid_cs || !rcgi.sample_cs || !rcgi.cache_shade_cs || !rcgi.volume_update_cs ||
	     !rcgi.final_gather_cs || !rcgi.denoise_cs || !rcgi.upscale_cs || !rcgi.composite_cs ) {
		return qfalse;
	}

	Com_Memset( binds, 0, sizeof( binds ) );
	binds[0].binding = 0; binds[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; binds[0].descriptorCount = 1; binds[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[1].binding = 1; binds[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; binds[1].descriptorCount = 1; binds[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[2].binding = 2; binds[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; binds[2].descriptorCount = 1; binds[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	Com_Memset( &lci, 0, sizeof( lci ) );
	lci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	lci.bindingCount = 3;
	lci.pBindings = binds;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &lci, NULL, &rcgi.light_grid_layout ) );

	/* sample: AS@0 cache@1 active@2 hit@3 worldAlb@4 worldN@5 */
	binds[0].binding = 0; binds[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	binds[0].descriptorCount = 1; binds[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[1].binding = 1; binds[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[2].binding = 2; binds[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[3].binding = 3; binds[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[4].binding = 4; binds[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[5].binding = 5; binds[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	lci.bindingCount = 6;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &lci, NULL, &rcgi.sample_layout ) );

	/* cache_shade: cache@0 active@1 lights@2 gridIds@3 gridCounts@4
	 * Reset binds[0] — leftover ACCELERATION_STRUCTURE from sample crashes NVIDIA glvkspirv. */
	binds[0].binding = 0; binds[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[0].descriptorCount = 1; binds[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[1].binding = 1; binds[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[2].binding = 2; binds[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[3].binding = 3; binds[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[4].binding = 4; binds[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	lci.bindingCount = 5;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &lci, NULL, &rcgi.cache_shade_layout ) );

	/* volume_update: cache@0 probeAtlas@1 */
	binds[0].binding = 0; binds[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[1].binding = 1; binds[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	lci.bindingCount = 2;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &lci, NULL, &rcgi.volume_update_layout ) );

	/* final_gather: AS@0 depth@1 normal@2 cache@3 atlas@4 shR@5 shG@6 shB@7 gather@8 */
	binds[0].binding = 0; binds[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	binds[1].binding = 1; binds[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[2].binding = 2; binds[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[3].binding = 3; binds[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[4].binding = 4; binds[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[5].binding = 5; binds[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	binds[6].binding = 6; binds[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	binds[7].binding = 7; binds[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	binds[8].binding = 8; binds[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	lci.bindingCount = 9;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &lci, NULL, &rcgi.final_gather_layout ) );

	/* denoise: depth@0 normal@1 gatherIn@2 gatherOut@3 */
	binds[0].binding = 0; binds[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[1].binding = 1; binds[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[2].binding = 2; binds[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[3].binding = 3; binds[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	lci.bindingCount = 4;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &lci, NULL, &rcgi.denoise_layout ) );

	/* upscale: depth@0 normal@1 gatherLow@2 history@3 irrOut@4 histOut@5 */
	binds[0].binding = 0; binds[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[1].binding = 1; binds[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[2].binding = 2; binds[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[3].binding = 3; binds[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[4].binding = 4; binds[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	binds[4].descriptorCount = 1; binds[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[5].binding = 5; binds[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	binds[5].descriptorCount = 1; binds[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	lci.bindingCount = 6;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &lci, NULL, &rcgi.upscale_layout ) );

	/* composite: depth@0 albedo@1 irradiance@2 colorOut@3 ambient visibility@4 */
	binds[0].binding = 0; binds[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[1].binding = 1; binds[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[2].binding = 2; binds[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[3].binding = 3; binds[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	binds[4].binding = 4; binds[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[4].descriptorCount = 1; binds[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	lci.bindingCount = 5;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &lci, NULL, &rcgi.composite_layout ) );

	Com_Memset( &pcr, 0, sizeof( pcr ) );
	pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	Com_Memset( &plci, 0, sizeof( plci ) );
	plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plci.setLayoutCount = 1;
	plci.pushConstantRangeCount = 1;
	plci.pPushConstantRanges = &pcr;

	pcr.size = sizeof( uint32_t ) * 4 + sizeof( float ) * 4;
	plci.pSetLayouts = &rcgi.light_grid_layout;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &rcgi.light_grid_pl ) );

	pcr.size = sizeof( uint32_t ) * 4 + sizeof( float ) * 12;
	plci.pSetLayouts = &rcgi.sample_layout;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &rcgi.sample_pl ) );

	pcr.size = sizeof( uint32_t ) * 4 + sizeof( float ) * 8;
	plci.pSetLayouts = &rcgi.cache_shade_layout;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &rcgi.cache_shade_pl ) );

	pcr.size = sizeof( uint32_t ) * 4 + sizeof( float ) * 4;
	plci.pSetLayouts = &rcgi.volume_update_layout;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &rcgi.volume_update_pl ) );

	pcr.size = sizeof( uint32_t ) * 4 + sizeof( float ) * 16 + sizeof( float ) * 8;
	plci.pSetLayouts = &rcgi.final_gather_layout;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &rcgi.final_gather_pl ) );

	pcr.size = sizeof( uint32_t ) * 4 + sizeof( float ) * 4;
	plci.pSetLayouts = &rcgi.denoise_layout;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &rcgi.denoise_pl ) );

	pcr.size = sizeof( uint32_t ) * 4 + sizeof( float ) * 4;
	plci.pSetLayouts = &rcgi.upscale_layout;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &rcgi.upscale_pl ) );

	pcr.size = sizeof( uint32_t ) * 8;
	plci.pSetLayouts = &rcgi.composite_layout;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &rcgi.composite_pl ) );

	if ( !RCGI_CreateComputePipe( rcgi.light_grid_pl, rcgi.light_grid_cs, "rcgi_light_grid", &rcgi.light_grid_pipe ) ||
	     !RCGI_CreateComputePipe( rcgi.sample_pl, rcgi.sample_cs, "rcgi_sample", &rcgi.sample_pipe ) ||
	     !RCGI_CreateComputePipe( rcgi.cache_shade_pl, rcgi.cache_shade_cs, "rcgi_cache_shade", &rcgi.cache_shade_pipe ) ||
	     !RCGI_CreateComputePipe( rcgi.volume_update_pl, rcgi.volume_update_cs, "rcgi_volume_update", &rcgi.volume_update_pipe ) ||
	     !RCGI_CreateComputePipe( rcgi.final_gather_pl, rcgi.final_gather_cs, "rcgi_final_gather", &rcgi.final_gather_pipe ) ||
	     !RCGI_CreateComputePipe( rcgi.denoise_pl, rcgi.denoise_cs, "rcgi_denoise", &rcgi.denoise_pipe ) ||
	     !RCGI_CreateComputePipe( rcgi.upscale_pl, rcgi.upscale_cs, "rcgi_upscale", &rcgi.upscale_pipe ) ||
	     !RCGI_CreateComputePipe( rcgi.composite_pl, rcgi.composite_cs, "rcgi_composite", &rcgi.composite_pipe ) ) {
		return qfalse;
	}

	Com_Memset( sizes, 0, sizeof( sizes ) );
	sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; sizes[0].descriptorCount = 48;
	sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; sizes[1].descriptorCount = 24;
	sizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; sizes[2].descriptorCount = 16;
	sizes[3].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR; sizes[3].descriptorCount = 4;
	Com_Memset( &poolCi, 0, sizeof( poolCi ) );
	poolCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCi.maxSets = 8;
	poolCi.poolSizeCount = 4;
	poolCi.pPoolSizes = sizes;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &poolCi, NULL, &rcgi.pool ) );

	layouts[0] = rcgi.light_grid_layout;
	layouts[1] = rcgi.sample_layout;
	layouts[2] = rcgi.cache_shade_layout;
	layouts[3] = rcgi.volume_update_layout;
	layouts[4] = rcgi.final_gather_layout;
	layouts[5] = rcgi.denoise_layout;
	layouts[6] = rcgi.upscale_layout;
	layouts[7] = rcgi.composite_layout;
	Com_Memset( &alloc, 0, sizeof( alloc ) );
	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.descriptorPool = rcgi.pool;
	alloc.descriptorSetCount = 8;
	alloc.pSetLayouts = layouts;
	{
		VkDescriptorSet sets[8];
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, sets ) );
		rcgi.light_grid_set = sets[0];
		rcgi.sample_set = sets[1];
		rcgi.cache_shade_set = sets[2];
		rcgi.volume_update_set = sets[3];
		rcgi.final_gather_set = sets[4];
		rcgi.denoise_set = sets[5];
		rcgi.upscale_set = sets[6];
		rcgi.composite_set = sets[7];
	}
	return qtrue;
}

static qboolean RCGI_CreateStaticResources( void )
{
	VkDeviceSize gridIdsBytes = (VkDeviceSize)RCGI_GRID_CELLS * RCGI_MAX_LIGHT_IDS * sizeof( uint32_t );
	VkDeviceSize gridCountsBytes = (VkDeviceSize)RCGI_GRID_CELLS * sizeof( uint32_t );
	VkDeviceSize cacheBytes = (VkDeviceSize)RCGI_CACHE_CELLS * RCGI_CACHE_CELL_BYTES;
	VkDeviceSize hitBytes = (VkDeviceSize)RCGI_PROBES_PER_CASCADE * RCGI_MAX_RAYS * sizeof( uint32_t ) * 4u;
	void *mapped = NULL;
	float gray[3] = { 0.72f, 0.70f, 0.66f };
	float up[3] = { 0.0f, 0.0f, 1.0f };

	if ( !RCGI_CreateBuffer( (VkDeviceSize)RCGI_MAX_LIGHTS * sizeof( RcGiLightGPU ),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &rcgi.lights ) ) {
		return qfalse;
	}
	if ( !RCGI_CreateBuffer( gridIdsBytes,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &rcgi.gridIds ) ) {
		return qfalse;
	}
	if ( !RCGI_CreateBuffer( gridCountsBytes,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &rcgi.gridCounts ) ) {
		return qfalse;
	}
	if ( !RCGI_CreateBuffer( cacheBytes,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &rcgi.cache ) ) {
		return qfalse;
	}
	if ( !RCGI_CreateBuffer( RCGI_ACTIVE_LIST_BYTES,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &rcgi.activeList ) ) {
		return qfalse;
	}
	if ( !RCGI_CreateBuffer( hitBytes,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &rcgi.hitPack ) ) {
		return qfalse;
	}
	if ( !RCGI_CreateBuffer( sizeof( gray ), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &rcgi.dummy_albedo ) ) {
		return qfalse;
	}
	VK_CHECK( qvkMapMemory( vk.device, rcgi.dummy_albedo.memory, 0, sizeof( gray ), 0, &mapped ) );
	Com_Memcpy( mapped, gray, sizeof( gray ) );
	qvkUnmapMemory( vk.device, rcgi.dummy_albedo.memory );
	if ( !RCGI_CreateBuffer( sizeof( up ), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &rcgi.dummy_normal ) ) {
		return qfalse;
	}
	VK_CHECK( qvkMapMemory( vk.device, rcgi.dummy_normal.memory, 0, sizeof( up ), 0, &mapped ) );
	Com_Memcpy( mapped, up, sizeof( up ) );
	qvkUnmapMemory( vk.device, rcgi.dummy_normal.memory );

	if ( !RCGI_CreateImage2D( &rcgi.probeAtlas, RCGI_ATLAS_W, RCGI_ATLAS_H,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT ) ) {
		return qfalse;
	}
	return qtrue;
}

static qboolean RCGI_EnsureFrameImages( uint32_t fullW, uint32_t fullH, int quality )
{
	uint32_t gW, gH;
	VkImageUsageFlags gatherUsage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	RCGI_GatherExtent( fullW, fullH, quality, &gW, &gH );
	if ( rcgi.width == fullW && rcgi.height == fullH && rcgi.gatherW == gW && rcgi.gatherH == gH && rcgi.quality == quality &&
	     rcgi.irradiance.image != VK_NULL_HANDLE ) {
		return qtrue;
	}

	if ( !RCGI_CreateImage2D( &rcgi.gatherA, gW, gH, gatherUsage ) ) {
		return qfalse;
	}
	if ( !RCGI_CreateImage2D( &rcgi.gatherB, gW, gH, gatherUsage ) ) {
		return qfalse;
	}
	if ( !RCGI_CreateImage2D( &rcgi.shR, gW, gH, gatherUsage ) ) {
		return qfalse;
	}
	if ( !RCGI_CreateImage2D( &rcgi.shG, gW, gH, gatherUsage ) ) {
		return qfalse;
	}
	if ( !RCGI_CreateImage2D( &rcgi.shB, gW, gH, gatherUsage ) ) {
		return qfalse;
	}
	if ( !RCGI_CreateImage2D( &rcgi.irradiance, fullW, fullH, gatherUsage ) ) {
		return qfalse;
	}
	if ( !RCGI_CreateImage2D( &rcgi.history, fullW, fullH, gatherUsage ) ) {
		return qfalse;
	}

	rcgi.width = fullW;
	rcgi.height = fullH;
	rcgi.gatherW = gW;
	rcgi.gatherH = gH;
	rcgi.quality = quality;
	return qtrue;
}

static void RCGI_FillInvViewProj( float out[16] )
{
	const float *view = backEnd.viewParms.world.modelViewMatrix;
	const float *projection = backEnd.useFirstPersonProjection
		? backEnd.firstPersonProjectionMatrix
		: backEnd.viewParms.projectionMatrix;
	float proj_vk[16];
	float viewProj[16];

	vk_get_projection_matrix_vk( projection, proj_vk );
	myGlMultMatrix( view, proj_vk, viewProj );
	if ( !vk_mat4_inverse( viewProj, out ) ) {
		Com_Memcpy( out, viewProj, sizeof( float ) * 16 );
	}
}

static uint32_t RCGI_UploadLights( void )
{
	RcGiLightGPU *dst;
	uint32_t n, i;
	void *mapped = NULL;

	n = tr.refdef.num_dlights;
	if ( n > RCGI_MAX_LIGHTS ) {
		n = RCGI_MAX_LIGHTS;
	}
	if ( rcgi.lights.memory == VK_NULL_HANDLE ) {
		return 0u;
	}
	VK_CHECK( qvkMapMemory( vk.device, rcgi.lights.memory, 0, rcgi.lights.size, 0, &mapped ) );
	dst = (RcGiLightGPU *)mapped;
	Com_Memset( dst, 0, (size_t)n * sizeof( RcGiLightGPU ) );
	if ( tr.refdef.dlights ) {
		for ( i = 0; i < n; i++ ) {
			const dlight_t *dl = &tr.refdef.dlights[i];
			vec3_t color;
			R_DynamicLightColor( dl, color );
			dst[i].posRange[0] = dl->origin[0];
			dst[i].posRange[1] = dl->origin[1];
			dst[i].posRange[2] = dl->origin[2];
			dst[i].posRange[3] = dl->radius;
			dst[i].color[0] = MAX( color[0], 0.0f );
			dst[i].color[1] = MAX( color[1], 0.0f );
			dst[i].color[2] = MAX( color[2], 0.0f );
			dst[i].color[3] = 1.0f;
		}
	}
	qvkUnmapMemory( vk.device, rcgi.lights.memory );
	return n;
}

static void RCGI_Status_f( void )
{
	ri.Printf( PRINT_ALL, "[RcGI] ready=%d active=%d frame=%u ext=%ux%u gather=%ux%u quality=%d rayQuery=%d rtx=%d\n",
		rcgi.ready ? 1 : 0,
		vk_rcgi_active() ? 1 : 0,
		rcgi.frame,
		rcgi.width, rcgi.height,
		rcgi.gatherW, rcgi.gatherH,
		r_rcgi_quality ? r_rcgi_quality->integer : 1,
		( vk.rayQueryAvailable ? 1 : 0 ),
		( vk.rtxAvailable ? 1 : 0 ) );
	ri.Printf( PRINT_ALL, "[RcGI] rays=%d cellSize=%.3f strength=%.2f albedoPrims=%u normalPrims=%u\n",
		r_rcgi_rays ? r_rcgi_rays->integer : 32,
		r_rcgi_cellSize ? r_rcgi_cellSize->value : 0.25f,
		r_rcgi_strength ? r_rcgi_strength->value : 0.85f,
		vk_rtx_world_albedo_count(),
		vk_rtx_world_normal_count() );
	ri.Printf( PRINT_ALL, "[RcGI] hybrid1Fusion cvar=%d active=%d (Hybrid1 owns diffuse GI; RcGI skips scene composite)\n",
		( r_rcgi_hybrid1Fusion && r_rcgi_hybrid1Fusion->integer ) ? 1 : 0,
		vk_rcgi_hybrid1_fusion_active() ? 1 : 0 );
}

void vk_rcgi_shutdown( void )
{
	RCGI_DestroyPipelines();
	RCGI_DestroyImage( &rcgi.probeAtlas );
	RCGI_DestroyImage( &rcgi.gatherA );
	RCGI_DestroyImage( &rcgi.gatherB );
	RCGI_DestroyImage( &rcgi.shR );
	RCGI_DestroyImage( &rcgi.shG );
	RCGI_DestroyImage( &rcgi.shB );
	RCGI_DestroyImage( &rcgi.irradiance );
	RCGI_DestroyImage( &rcgi.history );
	RCGI_DestroyBuffer( &rcgi.lights );
	RCGI_DestroyBuffer( &rcgi.gridIds );
	RCGI_DestroyBuffer( &rcgi.gridCounts );
	RCGI_DestroyBuffer( &rcgi.cache );
	RCGI_DestroyBuffer( &rcgi.activeList );
	RCGI_DestroyBuffer( &rcgi.hitPack );
	RCGI_DestroyBuffer( &rcgi.dummy_albedo );
	RCGI_DestroyBuffer( &rcgi.dummy_normal );
	Com_Memset( &rcgi, 0, sizeof( rcgi ) );
}

void vk_rcgi_init( void )
{
	if ( rcgi.ready ) {
		return;
	}
	if ( !r_rcgi ) {
		r_rcgi = ri.Cvar_Get( "r_rcgi", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
		r_rcgi_quality = ri.Cvar_Get( "r_rcgi_quality", "1", CVAR_ARCHIVE_ND );
		r_rcgi_rays = ri.Cvar_Get( "r_rcgi_rays", "32", CVAR_ARCHIVE_ND );
		r_rcgi_cellSize = ri.Cvar_Get( "r_rcgi_cellSize", "0.25", CVAR_ARCHIVE_ND );
		r_rcgi_strength = ri.Cvar_Get( "r_rcgi_strength", "0.85", CVAR_ARCHIVE_ND );
		r_rcgi_hybrid1Fusion = ri.Cvar_Get( "r_rcgi_hybrid1Fusion", "1", CVAR_ARCHIVE_ND );
		r_rcgi_debug = ri.Cvar_Get( "r_rcgi_debug", "0", CVAR_ARCHIVE_ND );
		ri.Cvar_CheckRange( r_rcgi, "0", "1", CV_INTEGER );
		ri.Cvar_CheckRange( r_rcgi_quality, "0", "1", CV_INTEGER );
		ri.Cvar_CheckRange( r_rcgi_rays, "1", "64", CV_INTEGER );
		ri.Cvar_CheckRange( r_rcgi_hybrid1Fusion, "0", "1", CV_INTEGER );
		ri.Cvar_CheckRange( r_rcgi_debug, "0", "3", CV_INTEGER );
		ri.Cvar_SetDescription( r_rcgi,
			"Radiance Cache GI: spatial-hash radiance cache + cascaded probes (needs USE_VULKAN_RTX + ray query + vid_restart)." );
		ri.Cvar_SetDescription( r_rcgi_quality,
			"RcGI gather resolution: 0=quarter-res, 1=half-res." );
		ri.Cvar_SetDescription( r_rcgi_hybrid1Fusion,
			"RcGI: when Hybrid1 is active, skip RcGI scene composite and feed irradiance into Hybrid1." );
		ri.Cmd_AddCommand( "rcgi_status", RCGI_Status_f );
		ri.Printf( PRINT_ALL, "[RcGI] r_rcgi=%d quality=%d fusion=%d\n",
			r_rcgi->integer,
			r_rcgi_quality->integer,
			r_rcgi_hybrid1Fusion->integer );
	}

	if ( !r_rcgi->integer ) {
		return;
	}
	if ( !vk.rtxAvailable || !vk.fboActive ) {
		ri.Printf( PRINT_WARNING, "[RcGI] needs rtxAvailable + FBO (set r_rtx/r_hybrid1/r_rcgi + vid_restart)\n" );
		return;
	}
	if ( !vk.rayQueryAvailable ) {
		ri.Printf( PRINT_WARNING, "[RcGI] VK_KHR_ray_query not available on this device\n" );
		return;
	}

	if ( !RCGI_CreateStaticResources() ) {
		ri.Printf( PRINT_WARNING, "[RcGI] static resource create failed — disabling r_rcgi\n" );
		ri.Cvar_Set( "r_rcgi", "0" );
		vk_rcgi_shutdown();
		return;
	}
	if ( !RCGI_CreatePipelines() ) {
		ri.Printf( PRINT_WARNING, "[RcGI] pipeline create failed — disabling r_rcgi (see prior [RcGI] lines)\n" );
		ri.Cvar_Set( "r_rcgi", "0" );
		vk_rcgi_shutdown();
		return;
	}

	rcgi.ready = qtrue;
	ri.Printf( PRINT_ALL, "[RcGI] Radiance Cache GI initialized (grid=%u cascades=%u cacheCells=%u atlas=%ux%u)\n",
		RCGI_GRID_RES, RCGI_CASCADES, RCGI_CACHE_CELLS, RCGI_ATLAS_W, RCGI_ATLAS_H );
}

qboolean vk_rcgi_active( void )
{
	return ( r_rcgi && r_rcgi->integer && rcgi.ready && vk.rtxAvailable && vk.rayQueryAvailable && vk.fboActive ) ? qtrue : qfalse;
}

qboolean vk_rcgi_hybrid1_fusion_active( void )
{
	if ( !r_rcgi_hybrid1Fusion || !r_rcgi_hybrid1Fusion->integer ) {
		return qfalse;
	}
	if ( !vk_rcgi_active() || !vk_hybrid1_active() ) {
		return qfalse;
	}
	if ( rcgi.irradiance.view == VK_NULL_HANDLE ) {
		return qfalse;
	}
	return qtrue;
}

VkImageView vk_rcgi_irradiance_view( void )
{
	return rcgi.irradiance.view;
}

float vk_rcgi_fusion_strength( void )
{
	return r_rcgi_strength ? r_rcgi_strength->value : 0.85f;
}

void vk_rcgi_frame_begin( void )
{
	uint32_t w = 0, h = 0;
	int quality;

	if ( r_rcgi && r_rcgi->integer > 0 && !rcgi.ready && vk.rtxAvailable && vk.fboActive ) {
		vk_rcgi_init();
	}
	if ( !rcgi.ready ) {
		return;
	}

	vk_rtx_scene_extent( &w, &h );
	if ( w < 1 || h < 1 ) {
		w = glConfig.vidWidth;
		h = glConfig.vidHeight;
	}
	quality = r_rcgi_quality ? r_rcgi_quality->integer : 1;
	if ( quality < 0 ) {
		quality = 0;
	} else if ( quality > 1 ) {
		quality = 1;
	}
	RCGI_EnsureFrameImages( w, h, quality );
}

void vk_rcgi_apply_after_geometry( void )
{
	VkCommandBuffer cmd;
	VkSampler nearest;
	VkSampler linear;
	VkImageView depthView, normalView, albedoView;
	VkDescriptorBufferInfo bInfo[8];
	VkDescriptorImageInfo imgInfo[8];
	VkWriteDescriptorSet writes[10];
	VkMemoryBarrier memBarrier;
	VkBufferMemoryBarrier bufBarrier;
	float invViewProj[16];
	uint32_t lightCount;
	uint32_t cascade;
	uint32_t rays;
	uint32_t probes;
	uint32_t gx, gy;
	float halfExt;
	float cellSize;
	vec3_t cascadeOrigin;

	struct {
		uint32_t params[4];
		float cascadeOrigin[4];
	} gridPush;

	struct {
		uint32_t params[4];
		float cascadeOrigin[4];
		float camera[4];
		float cache[4];
	} samplePush;

	struct {
		uint32_t params[4];
		float cascadeOrigin[4];
		float sun[4];
	} shadePush;

	struct {
		uint32_t params[4];
		float blend[4];
	} volPush;

	struct {
		uint32_t params[4];
		float invViewProj[16];
		float camera[4];
		float cache[4];
	} gatherPush;

	struct {
		uint32_t params[4];
		float sigma[4];
	} denoisePush;

	struct {
		uint32_t params[4];
		float temporal[4];
	} upscalePush;

	struct {
		uint32_t extent[2];
		float strength;
		uint32_t skipSky;
		uint32_t debugMode;
		float avStrength;
		uint32_t pad[2];
	} compPush;

	if ( !vk_rcgi_active() || !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}
	if ( vk.color_image == VK_NULL_HANDLE || vk.depth_image == VK_NULL_HANDLE ) {
		return;
	}
	if ( rcgi.irradiance.image == VK_NULL_HANDLE ) {
		vk_rcgi_frame_begin();
		if ( rcgi.irradiance.image == VK_NULL_HANDLE ) {
			return;
		}
	}

	vk_rtx_scene_prepare();
	if ( !vk_rtx_scene_ready() ) {
		return;
	}

	cmd = vk.cmd->command_buffer;
	nearest = RCGI_Nearest();
	linear = RCGI_Linear();
	depthView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	normalView = vk.deferred_gbuffer_normal_view ? vk.deferred_gbuffer_normal_view :
		( tr.whiteImage ? tr.whiteImage->view : depthView );
	albedoView = vk.deferred_gbuffer_albedo_view ? vk.deferred_gbuffer_albedo_view :
		( tr.whiteImage ? tr.whiteImage->view : vk.color_image_view );

	cascade = rcgi.frame % RCGI_CASCADES;
	probes = RCGI_PROBES_PER_CASCADE;
	rays = (uint32_t)( r_rcgi_rays && r_rcgi_rays->integer > 0 ? r_rcgi_rays->integer : 32 );
	if ( rays > RCGI_MAX_RAYS ) {
		rays = RCGI_MAX_RAYS;
	}
	cellSize = r_rcgi_cellSize ? r_rcgi_cellSize->value : 0.25f;
	halfExt = RCGI_CascadeHalfExtent( cascade );
	VectorCopy( backEnd.viewParms.or.origin, cascadeOrigin );

	lightCount = RCGI_UploadLights();

	qvkCmdFillBuffer( cmd, rcgi.activeList.buffer, 0, 4, 0 );
	Com_Memset( &bufBarrier, 0, sizeof( bufBarrier ) );
	bufBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	bufBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	bufBarrier.buffer = rcgi.activeList.buffer;
	bufBarrier.size = 4;
	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, NULL, 1, &bufBarrier, 0, NULL );

	RCGI_FillInvViewProj( invViewProj );

	Com_Memset( bInfo, 0, sizeof( bInfo ) );
	Com_Memset( imgInfo, 0, sizeof( imgInfo ) );
	Com_Memset( writes, 0, sizeof( writes ) );

	/* light grid descriptors */
	bInfo[0].buffer = rcgi.lights.buffer; bInfo[0].range = VK_WHOLE_SIZE;
	bInfo[1].buffer = rcgi.gridIds.buffer; bInfo[1].range = VK_WHOLE_SIZE;
	bInfo[2].buffer = rcgi.gridCounts.buffer; bInfo[2].range = VK_WHOLE_SIZE;
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = rcgi.light_grid_set; writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1; writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[0].pBufferInfo = &bInfo[0];
	writes[1] = writes[0]; writes[1].dstBinding = 1; writes[1].pBufferInfo = &bInfo[1];
	writes[2] = writes[0]; writes[2].dstBinding = 2; writes[2].pBufferInfo = &bInfo[2];
	qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );

	/* sample descriptors */
	bInfo[0].buffer = rcgi.cache.buffer;
	bInfo[1].buffer = rcgi.activeList.buffer;
	bInfo[2].buffer = rcgi.hitPack.buffer;
	bInfo[3].buffer = rcgi.dummy_albedo.buffer;
	bInfo[4].buffer = rcgi.dummy_normal.buffer;
	writes[0].dstSet = rcgi.sample_set; writes[0].dstBinding = 1; writes[0].pBufferInfo = &bInfo[0];
	writes[1].dstSet = rcgi.sample_set; writes[1].dstBinding = 2; writes[1].pBufferInfo = &bInfo[1];
	writes[2].dstSet = rcgi.sample_set; writes[2].dstBinding = 3; writes[2].pBufferInfo = &bInfo[2];
	writes[3].dstSet = rcgi.sample_set; writes[3].dstBinding = 4; writes[3].pBufferInfo = &bInfo[3];
	writes[4].dstSet = rcgi.sample_set; writes[4].dstBinding = 5; writes[4].pBufferInfo = &bInfo[4];
	qvkUpdateDescriptorSets( vk.device, 5, writes, 0, NULL );
	vk_rtx_bind_tlas_descriptor( rcgi.sample_set );
	if ( vk_rtx_world_albedo_count() > 0u ) {
		vk_rtx_bind_world_albedo_ssbo( rcgi.sample_set, 4 );
	}
	if ( vk_rtx_world_normal_count() > 0u ) {
		vk_rtx_bind_world_normal_ssbo( rcgi.sample_set, 5 );
	}

	/* cache shade descriptors */
	bInfo[0].buffer = rcgi.cache.buffer;
	bInfo[1].buffer = rcgi.activeList.buffer;
	bInfo[2].buffer = rcgi.lights.buffer;
	bInfo[3].buffer = rcgi.gridIds.buffer;
	bInfo[4].buffer = rcgi.gridCounts.buffer;
	writes[0].dstSet = rcgi.cache_shade_set; writes[0].dstBinding = 0; writes[0].pBufferInfo = &bInfo[0];
	writes[1].dstSet = rcgi.cache_shade_set; writes[1].dstBinding = 1; writes[1].pBufferInfo = &bInfo[1];
	writes[2].dstSet = rcgi.cache_shade_set; writes[2].dstBinding = 2; writes[2].pBufferInfo = &bInfo[2];
	writes[3].dstSet = rcgi.cache_shade_set; writes[3].dstBinding = 3; writes[3].pBufferInfo = &bInfo[3];
	writes[4].dstSet = rcgi.cache_shade_set; writes[4].dstBinding = 4; writes[4].pBufferInfo = &bInfo[4];
	qvkUpdateDescriptorSets( vk.device, 5, writes, 0, NULL );

	/* volume update */
	imgInfo[0].imageView = rcgi.probeAtlas.view;
	imgInfo[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	writes[0].dstSet = rcgi.volume_update_set; writes[0].dstBinding = 0;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; writes[0].pBufferInfo = &bInfo[0];
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = rcgi.volume_update_set; writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1; writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[1].pImageInfo = &imgInfo[0];
	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );

	/* final gather */
	imgInfo[0].sampler = nearest; imgInfo[0].imageView = depthView;
	imgInfo[0].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	imgInfo[1].sampler = nearest; imgInfo[1].imageView = normalView;
	imgInfo[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imgInfo[2].sampler = linear; imgInfo[2].imageView = rcgi.probeAtlas.view;
	imgInfo[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imgInfo[3].imageView = rcgi.gatherA.view; imgInfo[3].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imgInfo[4].imageView = rcgi.shR.view; imgInfo[4].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imgInfo[5].imageView = rcgi.shG.view; imgInfo[5].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imgInfo[6].imageView = rcgi.shB.view; imgInfo[6].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	writes[0].dstSet = rcgi.final_gather_set; writes[0].dstBinding = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; writes[0].pImageInfo = &imgInfo[0];
	writes[1].dstSet = rcgi.final_gather_set; writes[1].dstBinding = 2;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; writes[1].pImageInfo = &imgInfo[1];
	writes[2].dstSet = rcgi.final_gather_set; writes[2].dstBinding = 3;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; writes[2].pBufferInfo = &bInfo[0];
	writes[3].dstSet = rcgi.final_gather_set; writes[3].dstBinding = 4;
	writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; writes[3].pImageInfo = &imgInfo[2];
	writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[4].dstSet = rcgi.final_gather_set; writes[4].dstBinding = 5;
	writes[4].descriptorCount = 1; writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[4].pImageInfo = &imgInfo[4];
	writes[5] = writes[4]; writes[5].dstBinding = 6; writes[5].pImageInfo = &imgInfo[5];
	writes[6] = writes[4]; writes[6].dstBinding = 7; writes[6].pImageInfo = &imgInfo[6];
	writes[7] = writes[4]; writes[7].dstBinding = 8; writes[7].pImageInfo = &imgInfo[3];
	qvkUpdateDescriptorSets( vk.device, 8, writes, 0, NULL );
	vk_rtx_bind_tlas_descriptor( rcgi.final_gather_set );

	/* denoise ping-pong uses gatherA/B */
	imgInfo[0].sampler = nearest; imgInfo[0].imageView = depthView;
	imgInfo[0].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	imgInfo[1].sampler = nearest; imgInfo[1].imageView = normalView;
	imgInfo[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imgInfo[2].sampler = linear; imgInfo[2].imageView = rcgi.gatherA.view;
	imgInfo[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imgInfo[3].imageView = rcgi.gatherB.view; imgInfo[3].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	writes[0].dstSet = rcgi.denoise_set; writes[0].dstBinding = 0;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; writes[0].pImageInfo = &imgInfo[0];
	writes[1].dstSet = rcgi.denoise_set; writes[1].dstBinding = 1; writes[1].pImageInfo = &imgInfo[1];
	writes[2].dstSet = rcgi.denoise_set; writes[2].dstBinding = 2; writes[2].pImageInfo = &imgInfo[2];
	writes[3].dstSet = rcgi.denoise_set; writes[3].dstBinding = 3;
	writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; writes[3].pImageInfo = &imgInfo[3];
	qvkUpdateDescriptorSets( vk.device, 4, writes, 0, NULL );

	/* upscale */
	imgInfo[2].sampler = linear; imgInfo[2].imageView = rcgi.gatherA.view;
	imgInfo[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imgInfo[3].sampler = linear; imgInfo[3].imageView = rcgi.history.view;
	imgInfo[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imgInfo[4].imageView = rcgi.irradiance.view; imgInfo[4].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imgInfo[5].imageView = rcgi.history.view; imgInfo[5].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	writes[0].dstSet = rcgi.upscale_set; writes[0].dstBinding = 0; writes[0].pImageInfo = &imgInfo[0];
	writes[1].dstSet = rcgi.upscale_set; writes[1].dstBinding = 1; writes[1].pImageInfo = &imgInfo[1];
	writes[2].dstSet = rcgi.upscale_set; writes[2].dstBinding = 2; writes[2].pImageInfo = &imgInfo[2];
	writes[3].dstSet = rcgi.upscale_set; writes[3].dstBinding = 3; writes[3].pImageInfo = &imgInfo[3];
	writes[4].dstSet = rcgi.upscale_set; writes[4].dstBinding = 4;
	writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; writes[4].pImageInfo = &imgInfo[4];
	writes[5].dstSet = rcgi.upscale_set; writes[5].dstBinding = 5; writes[5].pImageInfo = &imgInfo[5];
	qvkUpdateDescriptorSets( vk.device, 6, writes, 0, NULL );

	/* composite */
	imgInfo[1].sampler = nearest; imgInfo[1].imageView = albedoView;
	imgInfo[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imgInfo[2].sampler = linear; imgInfo[2].imageView = rcgi.irradiance.view;
	imgInfo[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imgInfo[3].imageView = vk.color_image_view; imgInfo[3].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imgInfo[4].sampler = linear;
	imgInfo[4].imageView = vk_ambient_visibility_available() ? vk_ambient_visibility_view() : tr.whiteImage->view;
	imgInfo[4].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	writes[0].dstSet = rcgi.composite_set; writes[0].dstBinding = 0; writes[0].pImageInfo = &imgInfo[0];
	writes[1].dstSet = rcgi.composite_set; writes[1].dstBinding = 1; writes[1].pImageInfo = &imgInfo[1];
	writes[2].dstSet = rcgi.composite_set; writes[2].dstBinding = 2; writes[2].pImageInfo = &imgInfo[2];
	writes[3].dstSet = rcgi.composite_set; writes[3].dstBinding = 3;
	writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; writes[3].pImageInfo = &imgInfo[3];
	writes[4] = writes[0]; writes[4].dstSet = rcgi.composite_set; writes[4].dstBinding = 4;
	writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; writes[4].pImageInfo = &imgInfo[4];
	qvkUpdateDescriptorSets( vk.device, 5, writes, 0, NULL );

	Com_Memset( &memBarrier, 0, sizeof( memBarrier ) );
	memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
	memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

	Com_Memset( &bufBarrier, 0, sizeof( bufBarrier ) );
	bufBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	bufBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	bufBarrier.buffer = rcgi.activeList.buffer;
	bufBarrier.size = VK_WHOLE_SIZE;

	/* 1. Light grid */
	gridPush.params[0] = cascade;
	gridPush.params[1] = lightCount;
	gridPush.params[2] = RCGI_GRID_RES;
	gridPush.params[3] = RCGI_MAX_LIGHT_IDS;
	gridPush.cascadeOrigin[0] = cascadeOrigin[0];
	gridPush.cascadeOrigin[1] = cascadeOrigin[1];
	gridPush.cascadeOrigin[2] = cascadeOrigin[2];
	gridPush.cascadeOrigin[3] = halfExt;
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rcgi.light_grid_pipe );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rcgi.light_grid_pl, 0, 1, &rcgi.light_grid_set, 0, NULL );
	qvkCmdPushConstants( cmd, rcgi.light_grid_pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( gridPush ), &gridPush );
	qvkCmdDispatch( cmd, RCGI_GRID_RES / 4u, RCGI_GRID_RES / 4u, RCGI_GRID_RES / 4u );

	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 1, &memBarrier, 0, NULL, 0, NULL );

	/* 2. Sample */
	samplePush.params[0] = cascade;
	samplePush.params[1] = RCGI_GRID_RES;
	samplePush.params[2] = rays;
	samplePush.params[3] = rcgi.frame;
	samplePush.cascadeOrigin[0] = cascadeOrigin[0];
	samplePush.cascadeOrigin[1] = cascadeOrigin[1];
	samplePush.cascadeOrigin[2] = cascadeOrigin[2];
	samplePush.cascadeOrigin[3] = halfExt;
	samplePush.camera[0] = backEnd.viewParms.or.origin[0];
	samplePush.camera[1] = backEnd.viewParms.or.origin[1];
	samplePush.camera[2] = backEnd.viewParms.or.origin[2];
	samplePush.camera[3] = 512.0f;
	samplePush.cache[0] = cellSize;
	samplePush.cache[1] = halfExt;
	samplePush.cache[2] = (float)rcgi.frame;
	samplePush.cache[3] = 0.0f;
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rcgi.sample_pipe );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rcgi.sample_pl, 0, 1, &rcgi.sample_set, 0, NULL );
	qvkCmdPushConstants( cmd, rcgi.sample_pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( samplePush ), &samplePush );
	qvkCmdDispatch( cmd, ( probes + 63u ) / 64u, 1, 1 );

	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 1, &memBarrier, 0, NULL, 0, NULL );

	/* 3. Cache shade */
	shadePush.params[0] = cascade;
	shadePush.params[1] = RCGI_GRID_RES;
	shadePush.params[2] = lightCount;
	shadePush.params[3] = rcgi.frame;
	shadePush.cascadeOrigin[0] = cascadeOrigin[0];
	shadePush.cascadeOrigin[1] = cascadeOrigin[1];
	shadePush.cascadeOrigin[2] = cascadeOrigin[2];
	shadePush.cascadeOrigin[3] = halfExt;
	shadePush.sun[0] = tr.sunDirection[0];
	shadePush.sun[1] = tr.sunDirection[1];
	shadePush.sun[2] = tr.sunDirection[2];
	shadePush.sun[3] = ( tr.sunLight[0] + tr.sunLight[1] + tr.sunLight[2] ) * ( 1.0f / 3.0f );
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rcgi.cache_shade_pipe );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rcgi.cache_shade_pl, 0, 1, &rcgi.cache_shade_set, 0, NULL );
	qvkCmdPushConstants( cmd, rcgi.cache_shade_pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( shadePush ), &shadePush );
	qvkCmdDispatch( cmd, ( RCGI_CACHE_CELLS + 63u ) / 64u, 1, 1 );

	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 1, &memBarrier, 0, NULL, 0, NULL );

	/* 4. Volume update */
	record_image_layout_transition( cmd, rcgi.probeAtlas.image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	volPush.params[0] = cascade;
	volPush.params[1] = RCGI_GRID_RES;
	volPush.params[2] = rcgi.frame;
	volPush.params[3] = RCGI_PROBE_TILE;
	volPush.blend[0] = 0.15f;
	volPush.blend[1] = volPush.blend[2] = volPush.blend[3] = 0.0f;
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rcgi.volume_update_pipe );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rcgi.volume_update_pl, 0, 1, &rcgi.volume_update_set, 0, NULL );
	qvkCmdPushConstants( cmd, rcgi.volume_update_pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( volPush ), &volPush );
	qvkCmdDispatch( cmd, probes, 1, 1 );
	record_image_layout_transition( cmd, rcgi.probeAtlas.image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	/* 5. Final gather */
	record_image_layout_transition( cmd, rcgi.gatherA.image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	record_image_layout_transition( cmd, rcgi.shR.image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	record_image_layout_transition( cmd, rcgi.shG.image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	record_image_layout_transition( cmd, rcgi.shB.image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	gatherPush.params[0] = rcgi.gatherW;
	gatherPush.params[1] = rcgi.gatherH;
	gatherPush.params[2] = rcgi.frame;
	gatherPush.params[3] = 0u;
	Com_Memcpy( gatherPush.invViewProj, invViewProj, sizeof( invViewProj ) );
	gatherPush.camera[0] = backEnd.viewParms.or.origin[0];
	gatherPush.camera[1] = backEnd.viewParms.or.origin[1];
	gatherPush.camera[2] = backEnd.viewParms.or.origin[2];
	gatherPush.camera[3] = 512.0f;
	gatherPush.cache[0] = cellSize;
	gatherPush.cache[1] = halfExt;
	gatherPush.cache[2] = (float)rcgi.frame;
	gatherPush.cache[3] = 0.0f;
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rcgi.final_gather_pipe );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rcgi.final_gather_pl, 0, 1, &rcgi.final_gather_set, 0, NULL );
	qvkCmdPushConstants( cmd, rcgi.final_gather_pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( gatherPush ), &gatherPush );
	gx = ( rcgi.gatherW + 7u ) / 8u;
	gy = ( rcgi.gatherH + 7u ) / 8u;
	qvkCmdDispatch( cmd, gx, gy, 1 );

	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 1, &memBarrier, 0, NULL, 0, NULL );

	/* 6. Denoise H then V */
	denoisePush.params[0] = rcgi.gatherW;
	denoisePush.params[1] = rcgi.gatherH;
	denoisePush.sigma[0] = 1.0f;
	denoisePush.sigma[1] = 40.0f;
	denoisePush.sigma[2] = 8.0f;
	denoisePush.sigma[3] = 0.0f;
	record_image_layout_transition( cmd, rcgi.gatherA.image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	record_image_layout_transition( cmd, rcgi.gatherB.image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	denoisePush.params[2] = 0u;
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rcgi.denoise_pipe );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rcgi.denoise_pl, 0, 1, &rcgi.denoise_set, 0, NULL );
	qvkCmdPushConstants( cmd, rcgi.denoise_pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( denoisePush ), &denoisePush );
	qvkCmdDispatch( cmd, gx, gy, 1 );

	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 1, &memBarrier, 0, NULL, 0, NULL );

	/* swap gatherIn to gatherB for vertical pass */
	imgInfo[2].imageView = rcgi.gatherB.view;
	imgInfo[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imgInfo[3].imageView = rcgi.gatherA.view;
	imgInfo[3].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	writes[2].dstSet = rcgi.denoise_set; writes[2].dstBinding = 2; writes[2].pImageInfo = &imgInfo[2];
	writes[3].dstSet = rcgi.denoise_set; writes[3].dstBinding = 3; writes[3].pImageInfo = &imgInfo[3];
	qvkUpdateDescriptorSets( vk.device, 2, writes + 2, 0, NULL );
	record_image_layout_transition( cmd, rcgi.gatherB.image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	denoisePush.params[2] = 1u;
	qvkCmdPushConstants( cmd, rcgi.denoise_pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( denoisePush ), &denoisePush );
	qvkCmdDispatch( cmd, gx, gy, 1 );

	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 1, &memBarrier, 0, NULL, 0, NULL );

	/* 7. Upscale */
	record_image_layout_transition( cmd, rcgi.gatherA.image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	record_image_layout_transition( cmd, rcgi.irradiance.image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	record_image_layout_transition( cmd, rcgi.history.image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	upscalePush.params[0] = rcgi.width;
	upscalePush.params[1] = rcgi.height;
	upscalePush.params[2] = rcgi.gatherW;
	upscalePush.params[3] = rcgi.gatherH;
	upscalePush.temporal[0] = 0.12f;
	upscalePush.temporal[1] = upscalePush.temporal[2] = upscalePush.temporal[3] = 0.0f;
	gx = ( rcgi.width + 7u ) / 8u;
	gy = ( rcgi.height + 7u ) / 8u;
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rcgi.upscale_pipe );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rcgi.upscale_pl, 0, 1, &rcgi.upscale_set, 0, NULL );
	qvkCmdPushConstants( cmd, rcgi.upscale_pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( upscalePush ), &upscalePush );
	qvkCmdDispatch( cmd, gx, gy, 1 );

	record_image_layout_transition( cmd, rcgi.irradiance.image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	/* 8. Composite or Hybrid1 fusion handoff */
	if ( vk_rcgi_hybrid1_fusion_active() ) {
		static qboolean fusionLogged;
		if ( !fusionLogged ) {
			ri.Printf( PRINT_ALL, "[RcGI] Hybrid1 fusion active — irradiance handed to Hybrid1 composite\n" );
			fusionLogged = qtrue;
		}
		rcgi.frame++;
		return;
	}

	record_image_layout_transition( cmd, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		0, 0 );

	compPush.extent[0] = rcgi.width;
	compPush.extent[1] = rcgi.height;
	compPush.strength = r_rcgi_strength ? r_rcgi_strength->value : 0.85f;
	compPush.skipSky = 1u;
	compPush.debugMode = (uint32_t)( r_rcgi_debug ? r_rcgi_debug->integer : 0 );
	compPush.avStrength = vk_ambient_visibility_strength();
	compPush.pad[0] = compPush.pad[1] = 0u;
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rcgi.composite_pipe );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rcgi.composite_pl, 0, 1, &rcgi.composite_set, 0, NULL );
	qvkCmdPushConstants( cmd, rcgi.composite_pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( compPush ), &compPush );
	qvkCmdDispatch( cmd, gx, gy, 1 );

	record_image_layout_transition( cmd, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0, 0 );

	rcgi.frame++;
}

#else /* !USE_VULKAN_RTX */

void vk_rcgi_init( void )
{
	static qboolean logged;
	if ( !logged ) {
		ri.Printf( PRINT_ALL, "[RcGI] chocolate stub (build with -DUSE_VULKAN_RTX=ON)\n" );
		logged = qtrue;
	}
	ri.Cvar_Get( "r_rcgi", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
}

void vk_rcgi_shutdown( void ) {}
void vk_rcgi_frame_begin( void ) {}
qboolean vk_rcgi_active( void ) { return qfalse; }
void vk_rcgi_apply_after_geometry( void ) {}
qboolean vk_rcgi_hybrid1_fusion_active( void ) { return qfalse; }
VkImageView vk_rcgi_irradiance_view( void ) { return VK_NULL_HANDLE; }
float vk_rcgi_fusion_strength( void ) { return 0.0f; }

#endif /* USE_VULKAN_RTX */
