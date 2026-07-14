/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Surfel GI (GIBS) — Global Illumination Based on Surfels.
Chocolate RTX path; spawn / ray-query update / resolve / composite.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_surfel_gi.h"
#include "vk_rtx.h"
#include "vk_util.h"
#include "vk_image_layout.h"
#include "vk_view_state.h"
#include "vk_cmd.h"

#ifdef USE_VULKAN_RTX
#include "vk_surfel_gi_spirv.inc"

#define SURFEL_GI_BYTES 64u
#define SURFEL_GI_DEFAULT_CAP 16384u

typedef struct {
	float pos[3];
	float radius;
	float normal[3];
	float confidence;
	float irradiance[3];
	float age;
	uint32_t flags;
	uint32_t pad[3];
} SurfelGPU;

typedef struct {
	VkBuffer buffer;
	VkDeviceMemory memory;
	VkDeviceSize size;
} sgi_buffer_t;

typedef struct {
	VkImage image;
	VkImageView view;
	VkDeviceMemory memory;
	uint32_t width, height;
} sgi_image_t;

typedef struct {
	qboolean ready;
	qboolean logged_stub;
	uint32_t capacity;
	uint32_t frame;
	uint32_t width, height;

	sgi_buffer_t surfels;
	sgi_buffer_t counters;
	sgi_image_t irradiance;

	VkShaderModule spawn_cs;
	VkShaderModule update_cs;
	VkShaderModule resolve_cs;
	VkShaderModule composite_cs;

	VkDescriptorSetLayout spawn_layout;
	VkDescriptorSetLayout update_layout;
	VkDescriptorSetLayout resolve_layout;
	VkDescriptorSetLayout composite_layout;

	VkPipelineLayout spawn_pl;
	VkPipelineLayout update_pl;
	VkPipelineLayout resolve_pl;
	VkPipelineLayout composite_pl;

	VkPipeline spawn_pipe;
	VkPipeline update_pipe;
	VkPipeline resolve_pipe;
	VkPipeline composite_pipe;

	VkDescriptorPool pool;
	VkDescriptorSet spawn_set;
	VkDescriptorSet update_set;
	VkDescriptorSet resolve_set;
	VkDescriptorSet composite_set;
} sgi_state_t;

static sgi_state_t sgi;

static cvar_t *r_surfelGi;
static cvar_t *r_surfelGi_max;
static cvar_t *r_surfelGi_radius;
static cvar_t *r_surfelGi_updateRate;
static cvar_t *r_surfelGi_samples;
static cvar_t *r_surfelGi_intensity;
static cvar_t *r_surfelGi_strength;
static cvar_t *r_surfelGi_spawn;
static cvar_t *r_surfelGi_blend;
static cvar_t *r_surfelGi_rayDist;
static cvar_t *r_surfelGi_debug;
static cvar_t *r_surfelGi_skipSky;

static VkSampler SGI_Nearest( void )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;
	return vk_find_sampler( &sd );
}

static void SGI_DestroyBuffer( sgi_buffer_t *b )
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

static qboolean SGI_CreateBuffer( VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memFlags, sgi_buffer_t *out )
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

static void SGI_DestroyImage( sgi_image_t *img )
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

static qboolean SGI_CreateIrradiance( uint32_t w, uint32_t h )
{
	VkImageCreateInfo ii;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo ai;
	VkImageViewCreateInfo vi;

	SGI_DestroyImage( &sgi.irradiance );
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
	ii.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK( qvkCreateImage( vk.device, &ii, NULL, &sgi.irradiance.image ) );
	qvkGetImageMemoryRequirements( vk.device, sgi.irradiance.image, &req );
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, &sgi.irradiance.memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, sgi.irradiance.image, sgi.irradiance.memory, 0 ) );
	Com_Memset( &vi, 0, sizeof( vi ) );
	vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	vi.image = sgi.irradiance.image;
	vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
	vi.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vi.subresourceRange.levelCount = 1;
	vi.subresourceRange.layerCount = 1;
	VK_CHECK( qvkCreateImageView( vk.device, &vi, NULL, &sgi.irradiance.view ) );
	sgi.irradiance.width = w;
	sgi.irradiance.height = h;
	return qtrue;
}

static VkShaderModule SGI_Module( const uint8_t *code, uint32_t size, const char *name )
{
	VkShaderModuleCreateInfo ci;
	VkShaderModule mod = VK_NULL_HANDLE;
	Com_Memset( &ci, 0, sizeof( ci ) );
	ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.codeSize = size;
	ci.pCode = (const uint32_t *)code;
	if ( qvkCreateShaderModule( vk.device, &ci, NULL, &mod ) != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, "[SurfelGI] Failed to create shader module %s\n", name );
		return VK_NULL_HANDLE;
	}
	return mod;
}

static void SGI_DestroyPipelines( void )
{
	if ( sgi.spawn_pipe ) { qvkDestroyPipeline( vk.device, sgi.spawn_pipe, NULL ); sgi.spawn_pipe = VK_NULL_HANDLE; }
	if ( sgi.update_pipe ) { qvkDestroyPipeline( vk.device, sgi.update_pipe, NULL ); sgi.update_pipe = VK_NULL_HANDLE; }
	if ( sgi.resolve_pipe ) { qvkDestroyPipeline( vk.device, sgi.resolve_pipe, NULL ); sgi.resolve_pipe = VK_NULL_HANDLE; }
	if ( sgi.composite_pipe ) { qvkDestroyPipeline( vk.device, sgi.composite_pipe, NULL ); sgi.composite_pipe = VK_NULL_HANDLE; }
	if ( sgi.spawn_pl ) { qvkDestroyPipelineLayout( vk.device, sgi.spawn_pl, NULL ); sgi.spawn_pl = VK_NULL_HANDLE; }
	if ( sgi.update_pl ) { qvkDestroyPipelineLayout( vk.device, sgi.update_pl, NULL ); sgi.update_pl = VK_NULL_HANDLE; }
	if ( sgi.resolve_pl ) { qvkDestroyPipelineLayout( vk.device, sgi.resolve_pl, NULL ); sgi.resolve_pl = VK_NULL_HANDLE; }
	if ( sgi.composite_pl ) { qvkDestroyPipelineLayout( vk.device, sgi.composite_pl, NULL ); sgi.composite_pl = VK_NULL_HANDLE; }
	if ( sgi.spawn_layout ) { qvkDestroyDescriptorSetLayout( vk.device, sgi.spawn_layout, NULL ); sgi.spawn_layout = VK_NULL_HANDLE; }
	if ( sgi.update_layout ) { qvkDestroyDescriptorSetLayout( vk.device, sgi.update_layout, NULL ); sgi.update_layout = VK_NULL_HANDLE; }
	if ( sgi.resolve_layout ) { qvkDestroyDescriptorSetLayout( vk.device, sgi.resolve_layout, NULL ); sgi.resolve_layout = VK_NULL_HANDLE; }
	if ( sgi.composite_layout ) { qvkDestroyDescriptorSetLayout( vk.device, sgi.composite_layout, NULL ); sgi.composite_layout = VK_NULL_HANDLE; }
	if ( sgi.pool ) { qvkDestroyDescriptorPool( vk.device, sgi.pool, NULL ); sgi.pool = VK_NULL_HANDLE; }
	sgi.spawn_set = sgi.update_set = sgi.resolve_set = sgi.composite_set = VK_NULL_HANDLE;
	if ( sgi.spawn_cs ) { qvkDestroyShaderModule( vk.device, sgi.spawn_cs, NULL ); sgi.spawn_cs = VK_NULL_HANDLE; }
	if ( sgi.update_cs ) { qvkDestroyShaderModule( vk.device, sgi.update_cs, NULL ); sgi.update_cs = VK_NULL_HANDLE; }
	if ( sgi.resolve_cs ) { qvkDestroyShaderModule( vk.device, sgi.resolve_cs, NULL ); sgi.resolve_cs = VK_NULL_HANDLE; }
	if ( sgi.composite_cs ) { qvkDestroyShaderModule( vk.device, sgi.composite_cs, NULL ); sgi.composite_cs = VK_NULL_HANDLE; }
}

static qboolean SGI_CreatePipelines( void )
{
	VkDescriptorSetLayoutBinding binds[5];
	VkDescriptorSetLayoutCreateInfo lci;
	VkPushConstantRange pcr;
	VkPipelineLayoutCreateInfo plci;
	VkComputePipelineCreateInfo pci;
	VkPipelineShaderStageCreateInfo stage;
	VkDescriptorPoolSize sizes[4];
	VkDescriptorPoolCreateInfo poolCi;
	VkDescriptorSetAllocateInfo alloc;
	VkDescriptorSetLayout layouts[4];
	uint32_t i;

	sgi.spawn_cs = SGI_Module( vk_surfel_spawn_cs_spv, VK_SURFEL_SPAWN_CS_SPV_SIZE, "surfel_spawn" );
	sgi.update_cs = SGI_Module( vk_surfel_update_cs_spv, VK_SURFEL_UPDATE_CS_SPV_SIZE, "surfel_update" );
	sgi.resolve_cs = SGI_Module( vk_surfel_resolve_cs_spv, VK_SURFEL_RESOLVE_CS_SPV_SIZE, "surfel_resolve" );
	sgi.composite_cs = SGI_Module( vk_surfel_composite_cs_spv, VK_SURFEL_COMPOSITE_CS_SPV_SIZE, "surfel_composite" );
	if ( !sgi.spawn_cs || !sgi.update_cs || !sgi.resolve_cs || !sgi.composite_cs ) {
		return qfalse;
	}

	/* spawn: SSBO surfel, SSBO counter, depth, normal */
	Com_Memset( binds, 0, sizeof( binds ) );
	binds[0].binding = 0; binds[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; binds[0].descriptorCount = 1; binds[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[1].binding = 1; binds[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; binds[1].descriptorCount = 1; binds[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[2].binding = 2; binds[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; binds[2].descriptorCount = 1; binds[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[3].binding = 3; binds[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; binds[3].descriptorCount = 1; binds[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	Com_Memset( &lci, 0, sizeof( lci ) );
	lci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	lci.bindingCount = 4;
	lci.pBindings = binds;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &lci, NULL, &sgi.spawn_layout ) );

	/* update: AS @0, SSBO surfel @1, SSBO counter @2 */
	binds[0].binding = 0; binds[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR; binds[0].descriptorCount = 1; binds[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[1].binding = 1; binds[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; binds[1].descriptorCount = 1; binds[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[2].binding = 2; binds[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; binds[2].descriptorCount = 1; binds[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	lci.bindingCount = 3;
	lci.pBindings = binds;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &lci, NULL, &sgi.update_layout ) );

	/* resolve: SSBO RO, SSBO RO, depth, normal, storage image */
	binds[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[3].binding = 3;
	binds[4].binding = 4; binds[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; binds[4].descriptorCount = 1; binds[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	lci.bindingCount = 5;
	lci.pBindings = binds;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &lci, NULL, &sgi.resolve_layout ) );

	/* composite: depth, albedo, irradiance, color storage */
	binds[0].binding = 0; binds[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[1].binding = 1; binds[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[2].binding = 2; binds[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[3].binding = 3; binds[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	lci.bindingCount = 4;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &lci, NULL, &sgi.composite_layout ) );

	Com_Memset( &pcr, 0, sizeof( pcr ) );
	pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pcr.offset = 0;
	Com_Memset( &plci, 0, sizeof( plci ) );
	plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plci.setLayoutCount = 1;
	plci.pushConstantRangeCount = 1;
	plci.pPushConstantRanges = &pcr;

	pcr.size = sizeof( float ) * 16 + sizeof( float ) * 4 + sizeof( uint32_t ) * 4; /* mat4+vec4+uvec4 */
	plci.pSetLayouts = &sgi.spawn_layout;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &sgi.spawn_pl ) );

	pcr.size = sizeof( uint32_t ) * 4 + sizeof( float ) * 4;
	plci.pSetLayouts = &sgi.update_layout;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &sgi.update_pl ) );

	pcr.size = sizeof( float ) * 16 + sizeof( float ) * 4 + sizeof( uint32_t ) * 4;
	plci.pSetLayouts = &sgi.resolve_layout;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &sgi.resolve_pl ) );

	pcr.size = sizeof( uint32_t ) * 8; /* uvec2 + float + uints padded */
	plci.pSetLayouts = &sgi.composite_layout;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &sgi.composite_pl ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.pName = "main";
	Com_Memset( &pci, 0, sizeof( pci ) );
	pci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pci.stage = stage;

	pci.layout = sgi.spawn_pl; pci.stage.module = sgi.spawn_cs;
	VK_CHECK( qvkCreateComputePipelines( vk.device, vk.pipelineCache, 1, &pci, NULL, &sgi.spawn_pipe ) );
	pci.layout = sgi.update_pl; pci.stage.module = sgi.update_cs;
	VK_CHECK( qvkCreateComputePipelines( vk.device, vk.pipelineCache, 1, &pci, NULL, &sgi.update_pipe ) );
	pci.layout = sgi.resolve_pl; pci.stage.module = sgi.resolve_cs;
	VK_CHECK( qvkCreateComputePipelines( vk.device, vk.pipelineCache, 1, &pci, NULL, &sgi.resolve_pipe ) );
	pci.layout = sgi.composite_pl; pci.stage.module = sgi.composite_cs;
	VK_CHECK( qvkCreateComputePipelines( vk.device, vk.pipelineCache, 1, &pci, NULL, &sgi.composite_pipe ) );

	Com_Memset( sizes, 0, sizeof( sizes ) );
	sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; sizes[0].descriptorCount = 8;
	sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; sizes[1].descriptorCount = 12;
	sizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; sizes[2].descriptorCount = 4;
	sizes[3].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR; sizes[3].descriptorCount = 2;
	Com_Memset( &poolCi, 0, sizeof( poolCi ) );
	poolCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCi.maxSets = 4;
	poolCi.poolSizeCount = 4;
	poolCi.pPoolSizes = sizes;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &poolCi, NULL, &sgi.pool ) );

	layouts[0] = sgi.spawn_layout;
	layouts[1] = sgi.update_layout;
	layouts[2] = sgi.resolve_layout;
	layouts[3] = sgi.composite_layout;
	Com_Memset( &alloc, 0, sizeof( alloc ) );
	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.descriptorPool = sgi.pool;
	alloc.descriptorSetCount = 4;
	alloc.pSetLayouts = layouts;
	{
		VkDescriptorSet sets[4];
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, sets ) );
		sgi.spawn_set = sets[0];
		sgi.update_set = sets[1];
		sgi.resolve_set = sets[2];
		sgi.composite_set = sets[3];
	}
	(void)i;
	return qtrue;
}

static void SGI_FillInvViewProj( float out[16] )
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

static void SGI_Status_f( void )
{
	ri.Printf( PRINT_ALL, "[SurfelGI] ready=%d active=%d capacity=%u frame=%u ext=%ux%u rayQuery=%d rtx=%d\n",
		sgi.ready ? 1 : 0,
		vk_surfel_gi_active() ? 1 : 0,
		sgi.capacity,
		sgi.frame,
		sgi.width, sgi.height,
		( vk.rayQueryAvailable ? 1 : 0 ),
		( vk.rtxAvailable ? 1 : 0 ) );
}

void vk_surfel_gi_shutdown( void )
{
	if ( !sgi.ready && sgi.surfels.buffer == VK_NULL_HANDLE ) {
		SGI_DestroyPipelines();
		SGI_DestroyImage( &sgi.irradiance );
		SGI_DestroyBuffer( &sgi.surfels );
		SGI_DestroyBuffer( &sgi.counters );
		return;
	}
	SGI_DestroyPipelines();
	SGI_DestroyImage( &sgi.irradiance );
	SGI_DestroyBuffer( &sgi.surfels );
	SGI_DestroyBuffer( &sgi.counters );
	Com_Memset( &sgi, 0, sizeof( sgi ) );
}

void vk_surfel_gi_init( void )
{
	uint32_t cap;
	uint32_t zeros[4] = { 0, 0, 0, 0 };
	void *mapped;

	if ( sgi.ready ) {
		return;
	}
	if ( !r_surfelGi ) {
		r_surfelGi = ri.Cvar_Get( "r_surfelGi", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
		r_surfelGi_max = ri.Cvar_Get( "r_surfelGi_max", "16384", CVAR_ARCHIVE_ND | CVAR_LATCH );
		r_surfelGi_radius = ri.Cvar_Get( "r_surfelGi_radius", "0.35", CVAR_ARCHIVE_ND );
		r_surfelGi_updateRate = ri.Cvar_Get( "r_surfelGi_updateRate", "4", CVAR_ARCHIVE_ND );
		r_surfelGi_samples = ri.Cvar_Get( "r_surfelGi_samples", "4", CVAR_ARCHIVE_ND );
		r_surfelGi_intensity = ri.Cvar_Get( "r_surfelGi_intensity", "1.0", CVAR_ARCHIVE_ND );
		r_surfelGi_strength = ri.Cvar_Get( "r_surfelGi_strength", "0.85", CVAR_ARCHIVE_ND );
		r_surfelGi_spawn = ri.Cvar_Get( "r_surfelGi_spawn", "1024", CVAR_ARCHIVE_ND );
		r_surfelGi_blend = ri.Cvar_Get( "r_surfelGi_blend", "0.15", CVAR_ARCHIVE_ND );
		r_surfelGi_rayDist = ri.Cvar_Get( "r_surfelGi_rayDist", "512", CVAR_ARCHIVE_ND );
		r_surfelGi_debug = ri.Cvar_Get( "r_surfelGi_debug", "0", CVAR_ARCHIVE_ND );
		r_surfelGi_skipSky = ri.Cvar_Get( "r_surfelGi_skipSky", "1", CVAR_ARCHIVE_ND );
		ri.Cvar_CheckRange( r_surfelGi, "0", "1", CV_INTEGER );
		ri.Cvar_CheckRange( r_surfelGi_max, "256", "262144", CV_INTEGER );
		ri.Cvar_CheckRange( r_surfelGi_samples, "1", "16", CV_INTEGER );
		ri.Cvar_CheckRange( r_surfelGi_debug, "0", "2", CV_INTEGER );
		ri.Cvar_SetDescription( r_surfelGi,
			"Surfel GI (GIBS): cache indirect lighting in surfels (needs USE_VULKAN_RTX + ray query + vid_restart)." );
		ri.Cmd_AddCommand( "surfel_gi_status", SGI_Status_f );
	}

	if ( !r_surfelGi->integer ) {
		return;
	}
	if ( !vk.rtxAvailable || !vk.fboActive ) {
		ri.Printf( PRINT_WARNING, "[SurfelGI] needs rtxAvailable + FBO (set r_rtx/r_hybrid1/r_surfelGi + vid_restart)\n" );
		return;
	}
	if ( !vk.rayQueryAvailable ) {
		ri.Printf( PRINT_WARNING, "[SurfelGI] VK_KHR_ray_query not available on this device\n" );
		return;
	}

	cap = (uint32_t)r_surfelGi_max->integer;
	if ( cap < 256u ) {
		cap = 256u;
	}
	sgi.capacity = cap;

	if ( !SGI_CreateBuffer( (VkDeviceSize)cap * SURFEL_GI_BYTES,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &sgi.surfels ) ) {
		return;
	}
	if ( !SGI_CreateBuffer( 16,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &sgi.counters ) ) {
		vk_surfel_gi_shutdown();
		return;
	}
	VK_CHECK( qvkMapMemory( vk.device, sgi.counters.memory, 0, 16, 0, &mapped ) );
	Com_Memcpy( mapped, zeros, 16 );
	qvkUnmapMemory( vk.device, sgi.counters.memory );

	if ( !SGI_CreatePipelines() ) {
		vk_surfel_gi_shutdown();
		return;
	}

	sgi.ready = qtrue;
	ri.Printf( PRINT_ALL, "[SurfelGI] Global Illumination Based on Surfels initialized (r_surfelGi=1, cap=%u)\n", cap );
}

qboolean vk_surfel_gi_active( void )
{
	return ( r_surfelGi && r_surfelGi->integer && sgi.ready && vk.rtxAvailable && vk.rayQueryAvailable && vk.fboActive ) ? qtrue : qfalse;
}

void vk_surfel_gi_frame_begin( void )
{
	uint32_t w = 0, h = 0;
	if ( r_surfelGi && r_surfelGi->integer > 0 && !sgi.ready && vk.rtxAvailable && vk.fboActive ) {
		vk_surfel_gi_init();
	}
	if ( !sgi.ready ) {
		return;
	}
	vk_rtx_scene_extent( &w, &h );
	if ( w < 1 || h < 1 ) {
		w = glConfig.vidWidth;
		h = glConfig.vidHeight;
	}
	if ( w != sgi.width || h != sgi.height ) {
		if ( SGI_CreateIrradiance( w, h ) ) {
			sgi.width = w;
			sgi.height = h;
		}
	}
}

void vk_surfel_gi_apply_after_geometry( void )
{
	VkCommandBuffer cmd;
	VkSampler nearest;
	VkImageView depthView, normalView, albedoView;
	VkDescriptorBufferInfo bSurfel, bCounter;
	VkDescriptorImageInfo img[5];
	VkWriteDescriptorSet writes[8];
	VkMemoryBarrier memBarrier;
	float invViewProj[16];
	uint32_t attempts, gx, gy;
	struct {
		float invViewProj[16];
		float extent[4];
		uint32_t params[4];
	} spawnPush;
	struct {
		uint32_t params[4];
		float ray[4];
	} updatePush;
	struct {
		float invViewProj[16];
		float extent[4];
		uint32_t params[4];
	} resolvePush;
	struct {
		uint32_t extent[2];
		float strength;
		uint32_t skipSky;
		uint32_t debugMode;
		uint32_t pad[3];
	} compPush;

	if ( !vk_surfel_gi_active() || !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}
	if ( vk.color_image == VK_NULL_HANDLE || vk.depth_image == VK_NULL_HANDLE ) {
		return;
	}
	if ( sgi.irradiance.image == VK_NULL_HANDLE ) {
		vk_surfel_gi_frame_begin();
		if ( sgi.irradiance.image == VK_NULL_HANDLE ) {
			return;
		}
	}

	vk_rtx_scene_prepare();
	if ( !vk_rtx_scene_ready() ) {
		return;
	}

	cmd = vk.cmd->command_buffer;
	nearest = SGI_Nearest();
	depthView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	normalView = vk.deferred_gbuffer_normal_view ? vk.deferred_gbuffer_normal_view :
		( tr.whiteImage ? tr.whiteImage->view : depthView );
	albedoView = vk.deferred_gbuffer_albedo_view ? vk.deferred_gbuffer_albedo_view :
		( tr.whiteImage ? tr.whiteImage->view : vk.color_image_view );

	SGI_FillInvViewProj( invViewProj );

	Com_Memset( &bSurfel, 0, sizeof( bSurfel ) );
	bSurfel.buffer = sgi.surfels.buffer;
	bSurfel.range = VK_WHOLE_SIZE;
	Com_Memset( &bCounter, 0, sizeof( bCounter ) );
	bCounter.buffer = sgi.counters.buffer;
	bCounter.range = VK_WHOLE_SIZE;

	/* spawn descriptors */
	Com_Memset( img, 0, sizeof( img ) );
	Com_Memset( writes, 0, sizeof( writes ) );
	img[0].sampler = nearest; img[0].imageView = depthView;
	img[0].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	img[1].sampler = nearest; img[1].imageView = normalView;
	img[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = sgi.spawn_set; writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1; writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[0].pBufferInfo = &bSurfel;
	writes[1] = writes[0]; writes[1].dstBinding = 1; writes[1].pBufferInfo = &bCounter;
	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = sgi.spawn_set; writes[2].dstBinding = 2;
	writes[2].descriptorCount = 1; writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[2].pImageInfo = &img[0];
	writes[3] = writes[2]; writes[3].dstBinding = 3; writes[3].pImageInfo = &img[1];
	qvkUpdateDescriptorSets( vk.device, 4, writes, 0, NULL );

	/* update descriptors + TLAS (binding 0) */
	writes[0].dstSet = sgi.update_set; writes[0].dstBinding = 1; writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; writes[0].pBufferInfo = &bSurfel;
	writes[1].dstSet = sgi.update_set; writes[1].dstBinding = 2; writes[1].pBufferInfo = &bCounter;
	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );
	vk_rtx_bind_tlas_descriptor( sgi.update_set );

	/* resolve */
	img[2].imageView = sgi.irradiance.view;
	img[2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	writes[0].dstSet = sgi.resolve_set; writes[0].dstBinding = 0; writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; writes[0].pBufferInfo = &bSurfel;
	writes[1].dstSet = sgi.resolve_set; writes[1].dstBinding = 1; writes[1].pBufferInfo = &bCounter;
	writes[2].dstSet = sgi.resolve_set; writes[2].dstBinding = 2; writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; writes[2].pImageInfo = &img[0];
	writes[3].dstSet = sgi.resolve_set; writes[3].dstBinding = 3; writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; writes[3].pImageInfo = &img[1];
	writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[4].dstSet = sgi.resolve_set; writes[4].dstBinding = 4;
	writes[4].descriptorCount = 1; writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[4].pImageInfo = &img[2];
	qvkUpdateDescriptorSets( vk.device, 5, writes, 0, NULL );

	/* composite */
	img[3].sampler = nearest; img[3].imageView = albedoView;
	img[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	img[4].sampler = nearest; img[4].imageView = sgi.irradiance.view;
	img[4].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	{
		VkDescriptorImageInfo colorInfo;
		Com_Memset( &colorInfo, 0, sizeof( colorInfo ) );
		colorInfo.imageView = vk.color_image_view;
		colorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		writes[0].dstSet = sgi.composite_set; writes[0].dstBinding = 0;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; writes[0].pImageInfo = &img[0];
		writes[1].dstSet = sgi.composite_set; writes[1].dstBinding = 1; writes[1].pImageInfo = &img[3];
		writes[2].dstSet = sgi.composite_set; writes[2].dstBinding = 2; writes[2].pImageInfo = &img[4];
		writes[3].dstSet = sgi.composite_set; writes[3].dstBinding = 3;
		writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; writes[3].pImageInfo = &colorInfo;
		qvkUpdateDescriptorSets( vk.device, 4, writes, 0, NULL );
	}

	attempts = (uint32_t)( r_surfelGi_spawn && r_surfelGi_spawn->integer > 0 ? r_surfelGi_spawn->integer : 1024 );
	Com_Memcpy( spawnPush.invViewProj, invViewProj, sizeof( invViewProj ) );
	spawnPush.extent[0] = (float)sgi.width;
	spawnPush.extent[1] = (float)sgi.height;
	spawnPush.extent[2] = r_surfelGi_radius ? r_surfelGi_radius->value : 0.35f;
	spawnPush.extent[3] = 0.0f;
	spawnPush.params[0] = sgi.capacity;
	spawnPush.params[1] = sgi.frame;
	spawnPush.params[2] = attempts;
	spawnPush.params[3] = 0;

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sgi.spawn_pipe );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sgi.spawn_pl, 0, 1, &sgi.spawn_set, 0, NULL );
	qvkCmdPushConstants( cmd, sgi.spawn_pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( spawnPush ), &spawnPush );
	qvkCmdDispatch( cmd, ( attempts + 63u ) / 64u, 1, 1 );

	Com_Memset( &memBarrier, 0, sizeof( memBarrier ) );
	memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 1, &memBarrier, 0, NULL, 0, NULL );

	updatePush.params[0] = sgi.capacity;
	updatePush.params[1] = sgi.frame;
	updatePush.params[2] = (uint32_t)( r_surfelGi_samples ? r_surfelGi_samples->integer : 4 );
	updatePush.params[3] = (uint32_t)( r_surfelGi_updateRate && r_surfelGi_updateRate->integer > 0 ? r_surfelGi_updateRate->integer : 4 );
	updatePush.ray[0] = r_surfelGi_rayDist ? r_surfelGi_rayDist->value : 512.0f;
	updatePush.ray[1] = r_surfelGi_intensity ? r_surfelGi_intensity->value : 1.0f;
	updatePush.ray[2] = r_surfelGi_blend ? r_surfelGi_blend->value : 0.15f;
	updatePush.ray[3] = 0.0f;

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sgi.update_pipe );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sgi.update_pl, 0, 1, &sgi.update_set, 0, NULL );
	qvkCmdPushConstants( cmd, sgi.update_pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( updatePush ), &updatePush );
	qvkCmdDispatch( cmd, ( sgi.capacity + 63u ) / 64u, 1, 1 );

	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 1, &memBarrier, 0, NULL, 0, NULL );

	record_image_layout_transition( cmd, sgi.irradiance.image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	Com_Memcpy( resolvePush.invViewProj, invViewProj, sizeof( invViewProj ) );
	resolvePush.extent[0] = (float)sgi.width;
	resolvePush.extent[1] = (float)sgi.height;
	resolvePush.extent[2] = 1.0f;
	resolvePush.extent[3] = r_surfelGi_intensity ? r_surfelGi_intensity->value : 1.0f;
	resolvePush.params[0] = sgi.capacity;
	resolvePush.params[1] = 48u;
	resolvePush.params[2] = ( r_surfelGi_skipSky && r_surfelGi_skipSky->integer ) ? 1u : 0u;
	resolvePush.params[3] = 0;

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sgi.resolve_pipe );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sgi.resolve_pl, 0, 1, &sgi.resolve_set, 0, NULL );
	qvkCmdPushConstants( cmd, sgi.resolve_pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( resolvePush ), &resolvePush );
	gx = ( sgi.width + 7u ) / 8u;
	gy = ( sgi.height + 7u ) / 8u;
	qvkCmdDispatch( cmd, gx, gy, 1 );

	record_image_layout_transition( cmd, sgi.irradiance.image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	record_image_layout_transition( cmd, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		0, 0 );

	compPush.extent[0] = sgi.width;
	compPush.extent[1] = sgi.height;
	compPush.strength = r_surfelGi_strength ? r_surfelGi_strength->value : 0.85f;
	compPush.skipSky = ( r_surfelGi_skipSky && r_surfelGi_skipSky->integer ) ? 1u : 0u;
	compPush.debugMode = (uint32_t)( r_surfelGi_debug ? r_surfelGi_debug->integer : 0 );
	compPush.pad[0] = compPush.pad[1] = compPush.pad[2] = 0;

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sgi.composite_pipe );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sgi.composite_pl, 0, 1, &sgi.composite_set, 0, NULL );
	qvkCmdPushConstants( cmd, sgi.composite_pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( compPush ), &compPush );
	qvkCmdDispatch( cmd, gx, gy, 1 );

	record_image_layout_transition( cmd, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0, 0 );

	sgi.frame++;
}

#else /* !USE_VULKAN_RTX */

void vk_surfel_gi_init( void )
{
	static qboolean logged;
	if ( !logged ) {
		ri.Printf( PRINT_ALL, "[SurfelGI] chocolate stub (build with -DUSE_VULKAN_RTX=ON)\n" );
		logged = qtrue;
	}
	ri.Cvar_Get( "r_surfelGi", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
}

void vk_surfel_gi_shutdown( void ) {}
void vk_surfel_gi_frame_begin( void ) {}
qboolean vk_surfel_gi_active( void ) { return qfalse; }
void vk_surfel_gi_apply_after_geometry( void ) {}

#endif /* USE_VULKAN_RTX */
