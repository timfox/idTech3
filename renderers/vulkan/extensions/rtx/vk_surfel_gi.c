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
#include "vk_hybrid1.h"
#include "vk_rcgi.h"
#include "vk_rtx.h"
#include "vk_rtx_bindless.h"
#include "vk_util.h"
#include "vk_image_layout.h"
#include "vk_view_state.h"
#include "vk_cmd.h"
#include "vk_ambient_visibility.h"
#include "vk_pass_registry.h"

#ifdef USE_VULKAN_RTX
#include "vk_surfel_gi_spirv.inc"

#define SURFEL_GI_BYTES 64u
#define SURFEL_GI_DEFAULT_CAP 16384u
#define SURFEL_HASH_CELLS  4096u
#define SURFEL_HASH_BUCKET 16u

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
	sgi_buffer_t hash_grid;
	sgi_buffer_t dummy_albedo;
	sgi_buffer_t dummy_normal;
	sgi_image_t irradiance;

	VkShaderModule spawn_cs;
	VkShaderModule update_cs;
	VkShaderModule hash_cs;
	VkShaderModule resolve_cs;
	VkShaderModule composite_cs;

	VkDescriptorSetLayout spawn_layout;
	VkDescriptorSetLayout update_layout;
	VkDescriptorSetLayout hash_layout;
	VkDescriptorSetLayout resolve_layout;
	VkDescriptorSetLayout composite_layout;

	VkPipelineLayout spawn_pl;
	VkPipelineLayout update_pl;
	VkPipelineLayout hash_pl;
	VkPipelineLayout resolve_pl;
	VkPipelineLayout composite_pl;

	VkPipeline spawn_pipe;
	VkPipeline update_pipe;
	VkPipeline hash_pipe;
	VkPipeline resolve_pipe;
	VkPipeline composite_pipe;

	VkDescriptorPool pool;
	VkDescriptorSet spawn_set;
	VkDescriptorSet update_set;
	VkDescriptorSet hash_set;
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
static cvar_t *r_surfelGi_maxAge;
static cvar_t *r_surfelGi_hash;
static cvar_t *r_surfelGi_cellSize;
static cvar_t *r_surfelGi_hybrid1Fusion;
static cvar_t *r_surfelGiDensity;
static cvar_t *r_surfelGi_adaptiveSpawn;
static cvar_t *r_surfelGi_minSep;
static int s_surfelAppliedDensity = -1;

static VkSampler SGI_Nearest( void )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;
	return vk_find_sampler( &sd );
}

/*
===============
SGI_ApplyDensityPreset

r_surfelGiDensity 1–3 rewrites live spawn/radius/cell/sample knobs.
0 = custom (leave individual cvars). Latched r_surfelGi_max is unchanged.
===============
*/
static void SGI_ApplyDensityPreset( void )
{
	int d;
	const char *name;

	if ( !r_surfelGiDensity ) {
		return;
	}
	d = r_surfelGiDensity->integer;
	if ( d < 0 ) {
		d = 0;
	} else if ( d > 3 ) {
		d = 3;
	}
	if ( d == 0 ) {
		if ( s_surfelAppliedDensity != 0 ) {
			s_surfelAppliedDensity = 0;
			ri.Printf( PRINT_DEVELOPER, "[SurfelGI] density=custom (individual r_surfelGi_* knobs)\n" );
		}
		r_surfelGiDensity->modified = qfalse;
		return;
	}
	if ( d == s_surfelAppliedDensity && !r_surfelGiDensity->modified ) {
		return;
	}

	ri.Cvar_Set( "r_surfelGi_hash", "1" );
	ri.Cvar_Set( "r_surfelGi_adaptiveSpawn", "1" );

	if ( d == 1 ) {
		name = "sparse";
		ri.Cvar_Set( "r_surfelGi_spawn", "512" );
		ri.Cvar_Set( "r_surfelGi_radius", "0.55" );
		ri.Cvar_Set( "r_surfelGi_cellSize", "96" );
		ri.Cvar_Set( "r_surfelGi_samples", "2" );
		ri.Cvar_Set( "r_surfelGi_updateRate", "6" );
		ri.Cvar_Set( "r_surfelGi_minSep", "0.45" );
	} else if ( d == 2 ) {
		name = "balanced";
		ri.Cvar_Set( "r_surfelGi_spawn", "1024" );
		ri.Cvar_Set( "r_surfelGi_radius", "0.35" );
		ri.Cvar_Set( "r_surfelGi_cellSize", "64" );
		ri.Cvar_Set( "r_surfelGi_samples", "4" );
		ri.Cvar_Set( "r_surfelGi_updateRate", "4" );
		ri.Cvar_Set( "r_surfelGi_minSep", "0.22" );
	} else {
		name = "dense";
		ri.Cvar_Set( "r_surfelGi_spawn", "3072" );
		ri.Cvar_Set( "r_surfelGi_radius", "0.22" );
		ri.Cvar_Set( "r_surfelGi_cellSize", "40" );
		ri.Cvar_Set( "r_surfelGi_samples", "4" );
		ri.Cvar_Set( "r_surfelGi_updateRate", "3" );
		ri.Cvar_Set( "r_surfelGi_minSep", "0.14" );
	}

	s_surfelAppliedDensity = d;
	r_surfelGiDensity->modified = qfalse;
	ri.Printf( PRINT_ALL, "[SurfelGI] density preset=%d (%s)\n", d, name );
}

static const char *SGI_DensityName( void )
{
	int d = r_surfelGiDensity ? r_surfelGiDensity->integer : 0;
	if ( d <= 0 ) {
		return "custom";
	}
	if ( d == 1 ) {
		return "sparse";
	}
	if ( d == 2 ) {
		return "balanced";
	}
	return "dense";
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
	if ( sgi.hash_pipe ) { qvkDestroyPipeline( vk.device, sgi.hash_pipe, NULL ); sgi.hash_pipe = VK_NULL_HANDLE; }
	if ( sgi.resolve_pipe ) { qvkDestroyPipeline( vk.device, sgi.resolve_pipe, NULL ); sgi.resolve_pipe = VK_NULL_HANDLE; }
	if ( sgi.composite_pipe ) { qvkDestroyPipeline( vk.device, sgi.composite_pipe, NULL ); sgi.composite_pipe = VK_NULL_HANDLE; }
	if ( sgi.spawn_pl ) { qvkDestroyPipelineLayout( vk.device, sgi.spawn_pl, NULL ); sgi.spawn_pl = VK_NULL_HANDLE; }
	if ( sgi.update_pl ) { qvkDestroyPipelineLayout( vk.device, sgi.update_pl, NULL ); sgi.update_pl = VK_NULL_HANDLE; }
	if ( sgi.hash_pl ) { qvkDestroyPipelineLayout( vk.device, sgi.hash_pl, NULL ); sgi.hash_pl = VK_NULL_HANDLE; }
	if ( sgi.resolve_pl ) { qvkDestroyPipelineLayout( vk.device, sgi.resolve_pl, NULL ); sgi.resolve_pl = VK_NULL_HANDLE; }
	if ( sgi.composite_pl ) { qvkDestroyPipelineLayout( vk.device, sgi.composite_pl, NULL ); sgi.composite_pl = VK_NULL_HANDLE; }
	if ( sgi.spawn_layout ) { qvkDestroyDescriptorSetLayout( vk.device, sgi.spawn_layout, NULL ); sgi.spawn_layout = VK_NULL_HANDLE; }
	if ( sgi.update_layout ) { qvkDestroyDescriptorSetLayout( vk.device, sgi.update_layout, NULL ); sgi.update_layout = VK_NULL_HANDLE; }
	if ( sgi.hash_layout ) { qvkDestroyDescriptorSetLayout( vk.device, sgi.hash_layout, NULL ); sgi.hash_layout = VK_NULL_HANDLE; }
	if ( sgi.resolve_layout ) { qvkDestroyDescriptorSetLayout( vk.device, sgi.resolve_layout, NULL ); sgi.resolve_layout = VK_NULL_HANDLE; }
	if ( sgi.composite_layout ) { qvkDestroyDescriptorSetLayout( vk.device, sgi.composite_layout, NULL ); sgi.composite_layout = VK_NULL_HANDLE; }
	if ( sgi.pool ) { qvkDestroyDescriptorPool( vk.device, sgi.pool, NULL ); sgi.pool = VK_NULL_HANDLE; }
	sgi.spawn_set = sgi.update_set = sgi.hash_set = sgi.resolve_set = sgi.composite_set = VK_NULL_HANDLE;
	if ( sgi.spawn_cs ) { qvkDestroyShaderModule( vk.device, sgi.spawn_cs, NULL ); sgi.spawn_cs = VK_NULL_HANDLE; }
	if ( sgi.update_cs ) { qvkDestroyShaderModule( vk.device, sgi.update_cs, NULL ); sgi.update_cs = VK_NULL_HANDLE; }
	if ( sgi.hash_cs ) { qvkDestroyShaderModule( vk.device, sgi.hash_cs, NULL ); sgi.hash_cs = VK_NULL_HANDLE; }
	if ( sgi.resolve_cs ) { qvkDestroyShaderModule( vk.device, sgi.resolve_cs, NULL ); sgi.resolve_cs = VK_NULL_HANDLE; }
	if ( sgi.composite_cs ) { qvkDestroyShaderModule( vk.device, sgi.composite_cs, NULL ); sgi.composite_cs = VK_NULL_HANDLE; }
}

static qboolean SGI_CreatePipelines( void )
{
	VkDescriptorSetLayoutBinding binds[10];
	VkDescriptorSetLayoutCreateInfo lci;
	VkDescriptorBindingFlags bindingFlags[10];
	VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo;
	VkPushConstantRange pcr;
	VkPipelineLayoutCreateInfo plci;
	VkComputePipelineCreateInfo pci;
	VkPipelineShaderStageCreateInfo stage;
	VkDescriptorPoolSize sizes[4];
	VkDescriptorPoolCreateInfo poolCi;
	VkDescriptorSetAllocateInfo alloc;
	VkDescriptorSetLayout layouts[5];
	uint32_t bindlessCap = 1u;
	uint32_t bi;

	sgi.spawn_cs = SGI_Module( vk_surfel_spawn_cs_spv, VK_SURFEL_SPAWN_CS_SPV_SIZE, "surfel_spawn" );
	sgi.update_cs = SGI_Module( vk_surfel_update_cs_spv, VK_SURFEL_UPDATE_CS_SPV_SIZE, "surfel_update" );
	sgi.hash_cs = SGI_Module( vk_surfel_hash_cs_spv, VK_SURFEL_HASH_CS_SPV_SIZE, "surfel_hash" );
	sgi.resolve_cs = SGI_Module( vk_surfel_resolve_cs_spv, VK_SURFEL_RESOLVE_CS_SPV_SIZE, "surfel_resolve" );
	sgi.composite_cs = SGI_Module( vk_surfel_composite_cs_spv, VK_SURFEL_COMPOSITE_CS_SPV_SIZE, "surfel_composite" );
	if ( !sgi.spawn_cs || !sgi.update_cs || !sgi.hash_cs || !sgi.resolve_cs || !sgi.composite_cs ) {
		return qfalse;
	}

	if ( vk_rtx_bindless_indexing_supported() ) {
		bindlessCap = vk_rtx_bindless_cap();
		if ( bindlessCap < 1u ) {
			bindlessCap = 1u;
		}
		if ( bindlessCap > 4096u ) {
			bindlessCap = 4096u;
		}
	}

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

	/* update: AS@0 surfel@1 counter@2 worldAlb@3 worldN@4 entAlb@5 entN@6 bindless@7 primMat@8 primUv@9 */
	binds[0].binding = 0; binds[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	binds[0].descriptorCount = 1; binds[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[1].binding = 1; binds[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[1].descriptorCount = 1; binds[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[2].binding = 2; binds[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[2].descriptorCount = 1; binds[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[3].binding = 3; binds[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[3].descriptorCount = 1; binds[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[4].binding = 4; binds[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[4].descriptorCount = 1; binds[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[5].binding = 5; binds[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[5].descriptorCount = 1; binds[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[6].binding = 6; binds[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[6].descriptorCount = 1; binds[6].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[7].binding = 7; binds[7].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[7].descriptorCount = bindlessCap; binds[7].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[8].binding = 8; binds[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[8].descriptorCount = 1; binds[8].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[9].binding = 9; binds[9].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[9].descriptorCount = 1; binds[9].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	Com_Memset( bindingFlags, 0, sizeof( bindingFlags ) );
	for ( bi = 0; bi < 10; bi++ ) {
		bindingFlags[bi] = 0;
	}
	if ( bindlessCap > 1u ) {
		bindingFlags[7] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
	}
	Com_Memset( &bindingFlagsInfo, 0, sizeof( bindingFlagsInfo ) );
	bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	bindingFlagsInfo.bindingCount = 10;
	bindingFlagsInfo.pBindingFlags = bindingFlags;
	lci.bindingCount = 10;
	lci.pBindings = binds;
	lci.pNext = ( bindlessCap > 1u ) ? &bindingFlagsInfo : NULL;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &lci, NULL, &sgi.update_layout ) );
	lci.pNext = NULL;

	/* hash: surfel @0, counter @1, hash @2 */
	binds[0].binding = 0; binds[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[1].binding = 1; binds[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[2].binding = 2; binds[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	lci.bindingCount = 3;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &lci, NULL, &sgi.hash_layout ) );

	/* resolve: surfel, counter, depth, normal, irradiance image, hash */
	binds[0].binding = 0; binds[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[1].binding = 1; binds[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[2].binding = 2; binds[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[3].binding = 3; binds[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[4].binding = 4; binds[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; binds[4].descriptorCount = 1; binds[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[5].binding = 5; binds[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; binds[5].descriptorCount = 1; binds[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	lci.bindingCount = 6;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &lci, NULL, &sgi.resolve_layout ) );

	binds[0].binding = 0; binds[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[1].binding = 1; binds[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[2].binding = 2; binds[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[3].binding = 3; binds[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	binds[4].binding = 4; binds[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[4].descriptorCount = 1; binds[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	lci.bindingCount = 5;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &lci, NULL, &sgi.composite_layout ) );

	Com_Memset( &pcr, 0, sizeof( pcr ) );
	pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	Com_Memset( &plci, 0, sizeof( plci ) );
	plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plci.setLayoutCount = 1;
	plci.pushConstantRangeCount = 1;
	plci.pPushConstantRanges = &pcr;

	pcr.size = sizeof( float ) * 16 + sizeof( float ) * 4 + sizeof( uint32_t ) * 4;
	plci.pSetLayouts = &sgi.spawn_layout;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &sgi.spawn_pl ) );

	pcr.size = sizeof( uint32_t ) * 4 + sizeof( float ) * 4 + sizeof( uint32_t ) * 4;
	plci.pSetLayouts = &sgi.update_layout;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &sgi.update_pl ) );

	pcr.size = sizeof( uint32_t ) * 4 + sizeof( float ) * 4;
	plci.pSetLayouts = &sgi.hash_layout;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &sgi.hash_pl ) );

	pcr.size = sizeof( float ) * 16 + sizeof( float ) * 4 + sizeof( uint32_t ) * 4;
	plci.pSetLayouts = &sgi.resolve_layout;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &plci, NULL, &sgi.resolve_pl ) );

	pcr.size = sizeof( uint32_t ) * 8;
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
	pci.layout = sgi.hash_pl; pci.stage.module = sgi.hash_cs;
	VK_CHECK( qvkCreateComputePipelines( vk.device, vk.pipelineCache, 1, &pci, NULL, &sgi.hash_pipe ) );
	pci.layout = sgi.resolve_pl; pci.stage.module = sgi.resolve_cs;
	VK_CHECK( qvkCreateComputePipelines( vk.device, vk.pipelineCache, 1, &pci, NULL, &sgi.resolve_pipe ) );
	pci.layout = sgi.composite_pl; pci.stage.module = sgi.composite_cs;
	VK_CHECK( qvkCreateComputePipelines( vk.device, vk.pipelineCache, 1, &pci, NULL, &sgi.composite_pipe ) );

	Com_Memset( sizes, 0, sizeof( sizes ) );
	sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; sizes[0].descriptorCount = 32;
	sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; sizes[1].descriptorCount = 16u + bindlessCap;
	sizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; sizes[2].descriptorCount = 4;
	sizes[3].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR; sizes[3].descriptorCount = 2;
	Com_Memset( &poolCi, 0, sizeof( poolCi ) );
	poolCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCi.maxSets = 5;
	poolCi.poolSizeCount = 4;
	poolCi.pPoolSizes = sizes;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &poolCi, NULL, &sgi.pool ) );

	layouts[0] = sgi.spawn_layout;
	layouts[1] = sgi.update_layout;
	layouts[2] = sgi.hash_layout;
	layouts[3] = sgi.resolve_layout;
	layouts[4] = sgi.composite_layout;
	Com_Memset( &alloc, 0, sizeof( alloc ) );
	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.descriptorPool = sgi.pool;
	alloc.descriptorSetCount = 5;
	alloc.pSetLayouts = layouts;
	{
		VkDescriptorSet sets[5];
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, sets ) );
		sgi.spawn_set = sets[0];
		sgi.update_set = sets[1];
		sgi.hash_set = sets[2];
		sgi.resolve_set = sets[3];
		sgi.composite_set = sets[4];
	}
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
	uint32_t counters[4] = { 0, 0, 0, 0 };
	uint32_t albedoPrims = vk_rtx_world_albedo_count();
	uint32_t normalPrims = vk_rtx_world_normal_count();

	if ( sgi.counters.memory != VK_NULL_HANDLE ) {
		void *mapped = NULL;
		if ( qvkMapMemory( vk.device, sgi.counters.memory, 0, 16, 0, &mapped ) == VK_SUCCESS && mapped ) {
			Com_Memcpy( counters, mapped, 16 );
			qvkUnmapMemory( vk.device, sgi.counters.memory );
		}
	}
	ri.Printf( PRINT_ALL, "[SurfelGI] ready=%d active=%d capacity=%u frame=%u ext=%ux%u rayQuery=%d rtx=%d\n",
		sgi.ready ? 1 : 0,
		vk_surfel_gi_active() ? 1 : 0,
		sgi.capacity,
		sgi.frame,
		sgi.width, sgi.height,
		( vk.rayQueryAvailable ? 1 : 0 ),
		( vk.rtxAvailable ? 1 : 0 ) );
	ri.Printf( PRINT_ALL, "[SurfelGI] scan=%u recycled=%u liveSpawn=%u albedoPrims=%u normalPrims=%u hash=%d cell=%.2f\n",
		counters[0], counters[2], counters[3], albedoPrims, normalPrims,
		( r_surfelGi_hash && r_surfelGi_hash->integer ) ? 1 : 0,
		r_surfelGi_cellSize ? r_surfelGi_cellSize->value : 1.0f );
	ri.Printf( PRINT_ALL, "[SurfelGI] density=%d(%s) adaptiveSpawn=%d minSep=%.2f hashBucket=%u\n",
		r_surfelGiDensity ? r_surfelGiDensity->integer : 0,
		SGI_DensityName(),
		( r_surfelGi_adaptiveSpawn && r_surfelGi_adaptiveSpawn->integer ) ? 1 : 0,
		r_surfelGi_minSep ? r_surfelGi_minSep->value : 0.0f,
		SURFEL_HASH_BUCKET );
	ri.Printf( PRINT_ALL, "[SurfelGI] hybrid1Fusion cvar=%d active=%d (Hybrid1 owns diffuse GI; Surfel skips scene composite)\n",
		( r_surfelGi_hybrid1Fusion && r_surfelGi_hybrid1Fusion->integer ) ? 1 : 0,
		vk_surfel_gi_hybrid1_fusion_active() ? 1 : 0 );
}

void vk_surfel_gi_shutdown( void )
{
	SGI_DestroyPipelines();
	SGI_DestroyImage( &sgi.irradiance );
	SGI_DestroyBuffer( &sgi.surfels );
	SGI_DestroyBuffer( &sgi.counters );
	SGI_DestroyBuffer( &sgi.hash_grid );
	SGI_DestroyBuffer( &sgi.dummy_albedo );
	SGI_DestroyBuffer( &sgi.dummy_normal );
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
		r_surfelGi = ri.Cvar_Get( "r_surfelGi", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
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
		r_surfelGi_maxAge = ri.Cvar_Get( "r_surfelGi_maxAge", "240", CVAR_ARCHIVE_ND );
		r_surfelGi_hash = ri.Cvar_Get( "r_surfelGi_hash", "1", CVAR_ARCHIVE_ND );
		r_surfelGi_cellSize = ri.Cvar_Get( "r_surfelGi_cellSize", "64", CVAR_ARCHIVE_ND );
		r_surfelGi_hybrid1Fusion = ri.Cvar_Get( "r_surfelGi_hybrid1Fusion", "1", CVAR_ARCHIVE_ND );
		r_surfelGiDensity = ri.Cvar_Get( "r_surfelGiDensity", "0", CVAR_ARCHIVE_ND );
		r_surfelGi_adaptiveSpawn = ri.Cvar_Get( "r_surfelGi_adaptiveSpawn", "1", CVAR_ARCHIVE_ND );
		r_surfelGi_minSep = ri.Cvar_Get( "r_surfelGi_minSep", "0.22", CVAR_ARCHIVE_ND );
		ri.Cvar_CheckRange( r_surfelGi, "0", "1", CV_INTEGER );
		ri.Cvar_CheckRange( r_surfelGi_max, "256", "262144", CV_INTEGER );
		ri.Cvar_CheckRange( r_surfelGi_samples, "1", "16", CV_INTEGER );
		ri.Cvar_CheckRange( r_surfelGi_debug, "0", "2", CV_INTEGER );
		ri.Cvar_CheckRange( r_surfelGi_hash, "0", "1", CV_INTEGER );
		ri.Cvar_CheckRange( r_surfelGi_hybrid1Fusion, "0", "1", CV_INTEGER );
		ri.Cvar_CheckRange( r_surfelGiDensity, "0", "3", CV_INTEGER );
		ri.Cvar_CheckRange( r_surfelGi_adaptiveSpawn, "0", "1", CV_INTEGER );
		ri.Cvar_SetDescription( r_surfelGi,
			"Surfel GI (GIBS): cache indirect lighting in surfels (needs USE_VULKAN_RTX + ray query + vid_restart)." );
		ri.Cvar_SetDescription( r_surfelGi_hash,
			"Surfel GI: 1=fixed-bucket spatial hash for resolve gather; 0=strided scan fallback." );
		ri.Cvar_SetDescription( r_surfelGi_hybrid1Fusion,
			"Surfel GI: when Hybrid1 is active, skip Surfel scene composite and feed irradiance into Hybrid1 (avoids double diffuse GI)." );
		ri.Cvar_SetDescription( r_surfelGiDensity,
			"Surfel GI density preset: 0=custom, 1=sparse, 2=balanced, 3=dense (live; sets spawn/radius/cell/minSep)." );
		ri.Cvar_SetDescription( r_surfelGi_adaptiveSpawn,
			"Surfel GI: scale spawn attempts by pool fill (boost when sparse, ease when near capacity)." );
		ri.Cvar_SetDescription( r_surfelGi_minSep,
			"Surfel GI: reject new spawns within this world-unit distance of an existing surfel (0=off)." );
		ri.Cmd_AddCommand( "surfel_gi_status", SGI_Status_f );
		ri.Printf( PRINT_ALL, "[SurfelGI] r_surfelGi_hybrid1Fusion=%d density=%d adaptiveSpawn=%d minSep=%.2f hashBucket=%u\n",
			r_surfelGi_hybrid1Fusion->integer,
			r_surfelGiDensity->integer,
			r_surfelGi_adaptiveSpawn->integer,
			r_surfelGi_minSep->value,
			SURFEL_HASH_BUCKET );
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

	if ( !SGI_CreateBuffer( (VkDeviceSize)SURFEL_HASH_CELLS * SURFEL_HASH_BUCKET * sizeof( uint32_t ),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &sgi.hash_grid ) ) {
		vk_surfel_gi_shutdown();
		return;
	}
	{
		float gray[3] = { 0.72f, 0.70f, 0.66f };
		float up[3] = { 0.0f, 0.0f, 1.0f };
		void *amap = NULL;
		if ( !SGI_CreateBuffer( sizeof( gray ),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &sgi.dummy_albedo ) ) {
			vk_surfel_gi_shutdown();
			return;
		}
		VK_CHECK( qvkMapMemory( vk.device, sgi.dummy_albedo.memory, 0, sizeof( gray ), 0, &amap ) );
		Com_Memcpy( amap, gray, sizeof( gray ) );
		qvkUnmapMemory( vk.device, sgi.dummy_albedo.memory );
		if ( !SGI_CreateBuffer( sizeof( up ),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &sgi.dummy_normal ) ) {
			vk_surfel_gi_shutdown();
			return;
		}
		VK_CHECK( qvkMapMemory( vk.device, sgi.dummy_normal.memory, 0, sizeof( up ), 0, &amap ) );
		Com_Memcpy( amap, up, sizeof( up ) );
		qvkUnmapMemory( vk.device, sgi.dummy_normal.memory );
	}

	if ( !SGI_CreatePipelines() ) {
		vk_surfel_gi_shutdown();
		return;
	}

	sgi.ready = qtrue;
	ri.Printf( PRINT_ALL, "[SurfelGI] Global Illumination Based on Surfels initialized (r_surfelGi=1, cap=%u, hash=%u)\n",
		cap, SURFEL_HASH_CELLS );
}

qboolean vk_surfel_gi_active( void )
{
	return ( r_surfelGi && r_surfelGi->integer && sgi.ready && vk.rtxAvailable && vk.rayQueryAvailable && vk.fboActive ) ? qtrue : qfalse;
}

qboolean vk_surfel_gi_hybrid1_fusion_active( void )
{
	if ( !r_surfelGi_hybrid1Fusion || !r_surfelGi_hybrid1Fusion->integer ) {
		return qfalse;
	}
	if ( !vk_surfel_gi_active() || !vk_hybrid1_active() ) {
		return qfalse;
	}
	if ( sgi.irradiance.view == VK_NULL_HANDLE ) {
		return qfalse;
	}
	return qtrue;
}

VkImageView vk_surfel_gi_irradiance_view( void )
{
	return sgi.irradiance.view;
}

float vk_surfel_gi_fusion_strength( void )
{
	return r_surfelGi_strength ? r_surfelGi_strength->value : 0.85f;
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
	SGI_ApplyDensityPreset();
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
	VkDescriptorImageInfo img[6];
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
		uint32_t bindless[4]; /* x=texCount y=worldPrimCount */
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
		float avStrength;
		uint32_t pad[2];
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

	/* update descriptors + TLAS (binding 0) + world albedo/normal (3/4) + entity albedo/normal (5/6) */
	{
		VkDescriptorBufferInfo bAlbedo;
		VkDescriptorBufferInfo bNormal;
		Com_Memset( &bAlbedo, 0, sizeof( bAlbedo ) );
		bAlbedo.buffer = sgi.dummy_albedo.buffer;
		bAlbedo.range = VK_WHOLE_SIZE;
		Com_Memset( &bNormal, 0, sizeof( bNormal ) );
		bNormal.buffer = sgi.dummy_normal.buffer;
		bNormal.range = VK_WHOLE_SIZE;
		writes[0].dstSet = sgi.update_set; writes[0].dstBinding = 1; writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; writes[0].pBufferInfo = &bSurfel;
		writes[1].dstSet = sgi.update_set; writes[1].dstBinding = 2; writes[1].pBufferInfo = &bCounter;
		writes[2].dstSet = sgi.update_set; writes[2].dstBinding = 3; writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; writes[2].pBufferInfo = &bAlbedo;
		writes[3].dstSet = sgi.update_set; writes[3].dstBinding = 4; writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; writes[3].pBufferInfo = &bNormal;
		writes[4].dstSet = sgi.update_set; writes[4].dstBinding = 5; writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; writes[4].pBufferInfo = &bAlbedo;
		writes[5].dstSet = sgi.update_set; writes[5].dstBinding = 6; writes[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; writes[5].pBufferInfo = &bNormal;
		qvkUpdateDescriptorSets( vk.device, 6, writes, 0, NULL );
		vk_rtx_bind_tlas_descriptor( sgi.update_set );
		if ( vk_rtx_world_albedo_count() > 0u ) {
			vk_rtx_bind_world_albedo_ssbo( sgi.update_set, 3 );
		}
		if ( vk_rtx_world_normal_count() > 0u ) {
			vk_rtx_bind_world_normal_ssbo( sgi.update_set, 4 );
		}
		if ( vk_rtx_entity_albedo_count() > 0u ) {
			vk_rtx_bind_entity_albedo_ssbo( sgi.update_set, 5 );
		}
		if ( vk_rtx_entity_normal_count() > 0u ) {
			vk_rtx_bind_entity_normal_ssbo( sgi.update_set, 6 );
		}
		vk_rtx_bindless_set_descriptor_array_count( vk_rtx_bindless_indexing_supported() ?
			vk_rtx_bindless_cap() : 1u );
		vk_rtx_bindless_bind_textures( sgi.update_set, 7 );
		vk_rtx_bindless_bind_prim_material( sgi.update_set, 8 );
		{
			VkDescriptorBufferInfo bUv;
			VkWriteDescriptorSet wUv;
			Com_Memset( &bUv, 0, sizeof( bUv ) );
			bUv.buffer = sgi.dummy_albedo.buffer;
			bUv.range = VK_WHOLE_SIZE;
			Com_Memset( &wUv, 0, sizeof( wUv ) );
			wUv.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			wUv.dstSet = sgi.update_set;
			wUv.dstBinding = 9;
			wUv.descriptorCount = 1;
			wUv.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			wUv.pBufferInfo = &bUv;
			qvkUpdateDescriptorSets( vk.device, 1, &wUv, 0, NULL );
		}
		vk_rtx_bind_prim_uv_ssbo( sgi.update_set, 9 );
	}

	/* hash descriptors */
	{
		VkDescriptorBufferInfo bHash;
		Com_Memset( &bHash, 0, sizeof( bHash ) );
		bHash.buffer = sgi.hash_grid.buffer;
		bHash.range = VK_WHOLE_SIZE;
		writes[0].dstSet = sgi.hash_set; writes[0].dstBinding = 0; writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; writes[0].pBufferInfo = &bSurfel;
		writes[1].dstSet = sgi.hash_set; writes[1].dstBinding = 1; writes[1].pBufferInfo = &bCounter;
		writes[2].dstSet = sgi.hash_set; writes[2].dstBinding = 2; writes[2].pBufferInfo = &bHash;
		qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );
	}

	/* resolve */
	img[2].imageView = sgi.irradiance.view;
	img[2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	{
		VkDescriptorBufferInfo bHash;
		Com_Memset( &bHash, 0, sizeof( bHash ) );
		bHash.buffer = sgi.hash_grid.buffer;
		bHash.range = VK_WHOLE_SIZE;
		writes[0].dstSet = sgi.resolve_set; writes[0].dstBinding = 0; writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; writes[0].pBufferInfo = &bSurfel;
		writes[1].dstSet = sgi.resolve_set; writes[1].dstBinding = 1; writes[1].pBufferInfo = &bCounter;
		writes[2].dstSet = sgi.resolve_set; writes[2].dstBinding = 2; writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; writes[2].pImageInfo = &img[0];
		writes[3].dstSet = sgi.resolve_set; writes[3].dstBinding = 3; writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; writes[3].pImageInfo = &img[1];
		writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[4].dstSet = sgi.resolve_set; writes[4].dstBinding = 4;
		writes[4].descriptorCount = 1; writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[4].pImageInfo = &img[2];
		writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[5].dstSet = sgi.resolve_set; writes[5].dstBinding = 5;
		writes[5].descriptorCount = 1; writes[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[5].pBufferInfo = &bHash;
		qvkUpdateDescriptorSets( vk.device, 6, writes, 0, NULL );
	}

	/* composite */
	img[3].sampler = nearest; img[3].imageView = albedoView;
	img[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	img[4].sampler = nearest; img[4].imageView = sgi.irradiance.view;
	img[4].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	img[5].sampler = nearest;
	img[5].imageView = vk_ambient_visibility_available() ? vk_ambient_visibility_view() : tr.whiteImage->view;
	img[5].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
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
		writes[4] = writes[0]; writes[4].dstSet = sgi.composite_set; writes[4].dstBinding = 4;
		writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; writes[4].pImageInfo = &img[5];
		qvkUpdateDescriptorSets( vk.device, 5, writes, 0, NULL );
	}

	attempts = (uint32_t)( r_surfelGi_spawn && r_surfelGi_spawn->integer > 0 ? r_surfelGi_spawn->integer : 1024 );
	/* Adaptive spawn: boost when pool is sparse, ease when near capacity. */
	if ( r_surfelGi_adaptiveSpawn && r_surfelGi_adaptiveSpawn->integer && sgi.counters.memory != VK_NULL_HANDLE ) {
		uint32_t counters[4] = { 0, 0, 0, 0 };
		void *mapped = NULL;
		if ( qvkMapMemory( vk.device, sgi.counters.memory, 0, 16, 0, &mapped ) == VK_SUCCESS && mapped ) {
			Com_Memcpy( counters, mapped, 16 );
			/* Per-frame spawn stats (activeCount high-water is preserved). */
			counters[2] = 0u;
			counters[3] = 0u;
			Com_Memcpy( mapped, counters, 16 );
			qvkUnmapMemory( vk.device, sgi.counters.memory );
			if ( sgi.capacity > 0u ) {
				float fill = (float)counters[0] / (float)sgi.capacity;
				if ( fill < 0.25f ) {
					attempts *= 2u;
				} else if ( fill < 0.50f ) {
					attempts = ( attempts * 3u ) / 2u;
				} else if ( fill > 0.85f ) {
					attempts = attempts / 2u;
					if ( attempts < 128u ) {
						attempts = 128u;
					}
				}
			}
		}
	}
	if ( attempts > sgi.capacity ) {
		attempts = sgi.capacity;
	}
	Com_Memcpy( spawnPush.invViewProj, invViewProj, sizeof( invViewProj ) );
	spawnPush.extent[0] = (float)sgi.width;
	spawnPush.extent[1] = (float)sgi.height;
	spawnPush.extent[2] = r_surfelGi_radius ? r_surfelGi_radius->value : 0.35f;
	spawnPush.extent[3] = r_surfelGi_minSep ? r_surfelGi_minSep->value : 0.0f;
	spawnPush.params[0] = sgi.capacity;
	spawnPush.params[1] = sgi.frame;
	spawnPush.params[2] = attempts;
	spawnPush.params[3] = 0;

	vk_spine_pass_begin( VK_SPINE_PASS_SURFEL_GI_UPDATE );
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
	updatePush.ray[3] = r_surfelGi_maxAge ? r_surfelGi_maxAge->value : 240.0f;
	updatePush.bindless[0] = updatePush.bindless[1] = updatePush.bindless[2] = updatePush.bindless[3] = 0u;
	if ( vk_rtx_bindless_active() ) {
		updatePush.bindless[0] = vk_rtx_bindless_texture_count();
		updatePush.bindless[1] = vk_rtx_world_albedo_count();
	}

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sgi.update_pipe );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sgi.update_pl, 0, 1, &sgi.update_set, 0, NULL );
	qvkCmdPushConstants( cmd, sgi.update_pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( updatePush ), &updatePush );
	qvkCmdDispatch( cmd, ( sgi.capacity + 63u ) / 64u, 1, 1 );
	vk_spine_pass_end( VK_SPINE_PASS_SURFEL_GI_UPDATE );

	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 1, &memBarrier, 0, NULL, 0, NULL );

	if ( r_surfelGi_hash && r_surfelGi_hash->integer && sgi.hash_pipe != VK_NULL_HANDLE ) {
		struct {
			uint32_t params[4];
			float grid[4];
		} hashPush;
		uint32_t hashEntries = SURFEL_HASH_CELLS * SURFEL_HASH_BUCKET;

		hashPush.params[0] = sgi.capacity;
		hashPush.params[1] = 1u; /* clear */
		hashPush.params[2] = hashPush.params[3] = 0;
		hashPush.grid[0] = r_surfelGi_cellSize ? r_surfelGi_cellSize->value : 64.0f;
		hashPush.grid[1] = hashPush.grid[2] = hashPush.grid[3] = 0.0f;
		vk_spine_pass_begin( VK_SPINE_PASS_SURFEL_GI_HASH );
		qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sgi.hash_pipe );
		qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sgi.hash_pl, 0, 1, &sgi.hash_set, 0, NULL );
		qvkCmdPushConstants( cmd, sgi.hash_pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( hashPush ), &hashPush );
		qvkCmdDispatch( cmd, ( hashEntries + 63u ) / 64u, 1, 1 );

		qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0, 1, &memBarrier, 0, NULL, 0, NULL );

		hashPush.params[1] = 0u; /* scatter */
		qvkCmdPushConstants( cmd, sgi.hash_pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( hashPush ), &hashPush );
		qvkCmdDispatch( cmd, ( sgi.capacity + 63u ) / 64u, 1, 1 );
		vk_spine_pass_end( VK_SPINE_PASS_SURFEL_GI_HASH );

		qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0, 1, &memBarrier, 0, NULL, 0, NULL );
	}

	record_image_layout_transition( cmd, sgi.irradiance.image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	Com_Memcpy( resolvePush.invViewProj, invViewProj, sizeof( invViewProj ) );
	resolvePush.extent[0] = (float)sgi.width;
	resolvePush.extent[1] = (float)sgi.height;
	resolvePush.extent[2] = r_surfelGi_cellSize ? r_surfelGi_cellSize->value : 64.0f;
	resolvePush.extent[3] = r_surfelGi_intensity ? r_surfelGi_intensity->value : 1.0f;
	resolvePush.params[0] = sgi.capacity;
	resolvePush.params[1] = ( r_surfelGi_hash && r_surfelGi_hash->integer ) ? 1u : 0u;
	resolvePush.params[2] = ( r_surfelGi_skipSky && r_surfelGi_skipSky->integer ) ? 1u : 0u;
	resolvePush.params[3] = 0;

	vk_spine_pass_begin( VK_SPINE_PASS_SURFEL_GI_RESOLVE );
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sgi.resolve_pipe );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sgi.resolve_pl, 0, 1, &sgi.resolve_set, 0, NULL );
	qvkCmdPushConstants( cmd, sgi.resolve_pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( resolvePush ), &resolvePush );
	gx = ( sgi.width + 7u ) / 8u;
	gy = ( sgi.height + 7u ) / 8u;
	qvkCmdDispatch( cmd, gx, gy, 1 );

	record_image_layout_transition( cmd, sgi.irradiance.image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	vk_spine_pass_end( VK_SPINE_PASS_SURFEL_GI_RESOLVE );

	/* Hybrid1 fusion: keep irradiance for Hybrid1 composite; skip Surfel scene add (no double GI).
	 * Radiance Cache GI also owns diffuse when active (prefer RcGI over Surfel composite). */
	if ( vk_surfel_gi_hybrid1_fusion_active() || vk_rcgi_active() ) {
		static qboolean fusionLogged;
		if ( !fusionLogged ) {
			if ( vk_rcgi_active() ) {
				ri.Printf( PRINT_ALL, "[SurfelGI] RcGI active — skipping Surfel scene composite\n" );
			} else {
				ri.Printf( PRINT_ALL, "[SurfelGI] Hybrid1 fusion active — irradiance handed to Hybrid1 composite\n" );
			}
			fusionLogged = qtrue;
		}
		sgi.frame++;
		return;
	}

	record_image_layout_transition( cmd, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		0, 0 );

	compPush.extent[0] = sgi.width;
	compPush.extent[1] = sgi.height;
	compPush.strength = r_surfelGi_strength ? r_surfelGi_strength->value : 0.85f;
	compPush.skipSky = ( r_surfelGi_skipSky && r_surfelGi_skipSky->integer ) ? 1u : 0u;
	compPush.debugMode = (uint32_t)( r_surfelGi_debug ? r_surfelGi_debug->integer : 0 );
	compPush.avStrength = vk_ambient_visibility_strength();
	compPush.pad[0] = compPush.pad[1] = 0;

	vk_spine_pass_begin( VK_SPINE_PASS_SURFEL_GI_COMPOSITE );
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sgi.composite_pipe );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sgi.composite_pl, 0, 1, &sgi.composite_set, 0, NULL );
	qvkCmdPushConstants( cmd, sgi.composite_pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( compPush ), &compPush );
	qvkCmdDispatch( cmd, gx, gy, 1 );

	record_image_layout_transition( cmd, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0, 0 );
	vk_spine_pass_end( VK_SPINE_PASS_SURFEL_GI_COMPOSITE );

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
	ri.Cvar_Get( "r_surfelGi", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
}

void vk_surfel_gi_shutdown( void ) {}
void vk_surfel_gi_frame_begin( void ) {}
qboolean vk_surfel_gi_active( void ) { return qfalse; }
void vk_surfel_gi_apply_after_geometry( void ) {}
qboolean vk_surfel_gi_hybrid1_fusion_active( void ) { return qfalse; }
VkImageView vk_surfel_gi_irradiance_view( void ) { return VK_NULL_HANDLE; }
float vk_surfel_gi_fusion_strength( void ) { return 0.0f; }

#endif /* USE_VULKAN_RTX */
