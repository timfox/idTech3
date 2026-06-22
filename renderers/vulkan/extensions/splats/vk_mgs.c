/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Mobile-GS — compute 3D Gaussian splatting with mobile tier caps (no RTX).
See docs/MOBILE_GAUSSIAN_SPLATTING.md.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_mgs.h"
#include "vk_util.h"
#include "vk_image_layout.h"
#include "vk_view_state.h"
#include "vk_staging.h"
#include "vk_cmd.h"

#define MGS_MAX_GAUSSIANS       2048u
#define MGS_SPLAT_STRIDE        48u

typedef struct {
	float position[3];
	float opacity;
	float scale[3];
	float sigmaScale;
	float rotation[4];
	float color[3];
	float pad;
} mgsGaussian_t;

typedef struct {
	uint32_t tier;
	uint32_t maxGaussians;
	uint32_t maxFootprint;
	float scale;
	float strength;
} mgsTierParams_t;

typedef struct {
	qboolean loaded;
	char mapName[MAX_QPATH];
	uint32_t gaussianCount;
	uint32_t targetWidth;
	uint32_t targetHeight;
} mgsState_t;

static mgsState_t mgs;

static cvar_t *r_mgs;
static cvar_t *r_mgs_strength;
static cvar_t *r_mgs_maxSplats;
static cvar_t *r_mgs_scale;
static cvar_t *r_mgs_focal;
static cvar_t *r_mgs_skipSky;
static cvar_t *r_mgs_depthTest;
static cvar_t *r_mgs_debug;

static void MGS_CreatePreparePipeline( void );
static void MGS_CreateSplatPipeline( void );
static void MGS_CreateCompositePipeline( void );
static qboolean MGS_UploadGaussians( uint32_t count, const mgsGaussian_t *src );

typedef struct {
	float viewProj[16];
	float viewOrigin[4];
	float viewport[4];
	int gaussianCount;
	int pad0;
	int pad1;
	int pad2;
} vk_mgs_prepare_push_t;

typedef struct {
	float viewport[4];
	int splatCount;
	int maxFootprint;
	float strength;
	float pad0;
} vk_mgs_splat_push_t;

typedef struct {
	vec4_t extent;
	vec4_t params;
} vk_mgs_composite_push_t;

static VkSampler MGS_DepthSampler( void )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;
	return vk_find_sampler( &sd );
}

static VkSampler MGS_LinearSampler( void )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	return vk_find_sampler( &sd );
}

static mgsTierParams_t MGS_TierParams( void )
{
	mgsTierParams_t p;
	int tier;

	Com_Memset( &p, 0, sizeof( p ) );
	tier = R_MGS_EffectiveTier();
	if ( tier < 1 ) {
		tier = 1;
	}
	if ( tier > 3 ) {
		tier = 3;
	}
	p.tier = (uint32_t)tier;
	p.strength = r_mgs_strength ? r_mgs_strength->value : 0.85f;

	if ( tier == 1 ) {
		p.maxGaussians = 64u;
		p.maxFootprint = 12u;
		p.scale = 0.25f;
	} else if ( tier == 2 ) {
		p.maxGaussians = 256u;
		p.maxFootprint = 24u;
		p.scale = 0.5f;
	} else {
		p.maxGaussians = 1024u;
		p.maxFootprint = 48u;
		p.scale = 1.0f;
	}

	if ( r_mgs_maxSplats && r_mgs_maxSplats->integer > 0 ) {
		p.maxGaussians = (uint32_t)r_mgs_maxSplats->integer;
		if ( p.maxGaussians > MGS_MAX_GAUSSIANS ) {
			p.maxGaussians = MGS_MAX_GAUSSIANS;
		}
	}
	if ( r_mgs_scale && r_mgs_scale->value > 0.0f ) {
		p.scale = r_mgs_scale->value;
	}
	return p;
}

static void MGS_ClearGpu( void )
{
	if ( vk.mgs.prepare_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.mgs.prepare_pipeline, NULL );
		vk.mgs.prepare_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.mgs.splat_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.mgs.splat_pipeline, NULL );
		vk.mgs.splat_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.mgs.composite_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.mgs.composite_pipeline, NULL );
		vk.mgs.composite_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.mgs.prepare_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.mgs.prepare_pipeline_layout, NULL );
		vk.mgs.prepare_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.mgs.splat_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.mgs.splat_pipeline_layout, NULL );
		vk.mgs.splat_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.mgs.composite_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.mgs.composite_pipeline_layout, NULL );
		vk.mgs.composite_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.mgs.prepare_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.mgs.prepare_layout, NULL );
		vk.mgs.prepare_layout = VK_NULL_HANDLE;
	}
	if ( vk.mgs.splat_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.mgs.splat_layout, NULL );
		vk.mgs.splat_layout = VK_NULL_HANDLE;
	}
	if ( vk.mgs.composite_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.mgs.composite_layout, NULL );
		vk.mgs.composite_layout = VK_NULL_HANDLE;
	}
	if ( vk.mgs.prepare_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.mgs.prepare_pool, NULL );
		vk.mgs.prepare_pool = VK_NULL_HANDLE;
	}
	if ( vk.mgs.splat_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.mgs.splat_pool, NULL );
		vk.mgs.splat_pool = VK_NULL_HANDLE;
	}
	if ( vk.mgs.composite_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.mgs.composite_pool, NULL );
		vk.mgs.composite_pool = VK_NULL_HANDLE;
	}
	if ( vk.mgs.gaussian_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.mgs.gaussian_buffer, NULL );
		vk.mgs.gaussian_buffer = VK_NULL_HANDLE;
	}
	if ( vk.mgs.gaussian_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.mgs.gaussian_memory, NULL );
		vk.mgs.gaussian_memory = VK_NULL_HANDLE;
	}
	if ( vk.mgs.splat_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.mgs.splat_buffer, NULL );
		vk.mgs.splat_buffer = VK_NULL_HANDLE;
	}
	if ( vk.mgs.splat_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.mgs.splat_memory, NULL );
		vk.mgs.splat_memory = VK_NULL_HANDLE;
	}
	if ( vk.mgs.accum_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.mgs.accum_view, NULL );
		vk.mgs.accum_view = VK_NULL_HANDLE;
	}
	if ( vk.mgs.accum_image != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, vk.mgs.accum_image, NULL );
		vk.mgs.accum_image = VK_NULL_HANDLE;
	}
	if ( vk.mgs.accum_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.mgs.accum_memory, NULL );
		vk.mgs.accum_memory = VK_NULL_HANDLE;
	}
	vk.mgs.prepare_ready = qfalse;
	vk.mgs.splat_ready = qfalse;
	vk.mgs.composite_ready = qfalse;
	vk.mgs.gaussian_count = 0u;
	vk.mgsAllocated = qfalse;
}

static void MGS_FillProceduralGaussians( mgsGaussian_t *out, uint32_t count, float sigmaScale )
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
		mgsGaussian_t *g = &out[i];

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

extern qboolean R_SQZ_Active( void );
extern int R_SQZ_EffectiveMgsTier( void );

static qboolean MGS_UploadGaussians( uint32_t count, const mgsGaussian_t *src )
{
	VkBufferCreateInfo bi;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo ai;
	VkDeviceSize size;
	void *mapped;

	if ( vk.mgs.gaussian_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.mgs.gaussian_buffer, NULL );
		qvkFreeMemory( vk.device, vk.mgs.gaussian_memory, NULL );
		vk.mgs.gaussian_buffer = VK_NULL_HANDLE;
		vk.mgs.gaussian_memory = VK_NULL_HANDLE;
	}

	size = (VkDeviceSize)count * sizeof( mgsGaussian_t );
	Com_Memset( &bi, 0, sizeof( bi ) );
	bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bi.size = size;
	bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &bi, NULL, &vk.mgs.gaussian_buffer ) );
	qvkGetBufferMemoryRequirements( vk.device, vk.mgs.gaussian_buffer, &req );
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, &vk.mgs.gaussian_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.mgs.gaussian_buffer, vk.mgs.gaussian_memory, 0 ) );
	VK_CHECK( qvkMapMemory( vk.device, vk.mgs.gaussian_memory, 0, size, 0, &mapped ) );
	Com_Memcpy( mapped, src, (size_t)size );
	qvkUnmapMemory( vk.device, vk.mgs.gaussian_memory );

	size = (VkDeviceSize)count * MGS_SPLAT_STRIDE;
	Com_Memset( &bi, 0, sizeof( bi ) );
	bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bi.size = size;
	bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &bi, NULL, &vk.mgs.splat_buffer ) );
	qvkGetBufferMemoryRequirements( vk.device, vk.mgs.splat_buffer, &req );
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &ai, NULL, &vk.mgs.splat_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.mgs.splat_buffer, vk.mgs.splat_memory, 0 ) );

	vk.mgs.gaussian_count = count;
	return qtrue;
}

int R_MGS_EffectiveTier( void )
{
	if ( R_SQZ_Active() ) {
		return R_SQZ_EffectiveMgsTier();
	}
	if ( r_mgs && r_mgs->integer > 0 ) {
		return r_mgs->integer;
	}
	return 0;
}

void R_MGS_MarkLoaded( const char *mapBaseName, uint32_t gaussianCount )
{
	if ( mapBaseName && mapBaseName[0] ) {
		Q_strncpyz( mgs.mapName, mapBaseName, sizeof( mgs.mapName ) );
	}
	mgs.gaussianCount = gaussianCount;
	mgs.loaded = qtrue;
}

qboolean R_MGS_UploadGaussians( uint32_t count, const void *src, size_t srcStride )
{
	size_t stride = srcStride ? srcStride : sizeof( mgsGaussian_t );

	if ( !src || count == 0 || stride < sizeof( mgsGaussian_t ) ) {
		return qfalse;
	}
	if ( stride == sizeof( mgsGaussian_t ) ) {
		return MGS_UploadGaussians( count, (const mgsGaussian_t *)src );
	}
	{
		mgsGaussian_t *tmp = (mgsGaussian_t *)ri.Malloc( (size_t)count * sizeof( mgsGaussian_t ) );
		uint32_t i;
		qboolean ok;

		for ( i = 0; i < count; i++ ) {
			Com_Memcpy( &tmp[i], (const byte *)src + i * stride, sizeof( mgsGaussian_t ) );
		}
		ok = MGS_UploadGaussians( count, tmp );
		ri.Free( tmp );
		return ok;
	}
}

void R_MGS_EnsurePipelines( void )
{
	MGS_CreatePreparePipeline();
	MGS_CreateSplatPipeline();
	MGS_CreateCompositePipeline();
	vk.mgsAllocated = qtrue;
}

static qboolean MGS_EnsureAccumTarget( uint32_t width, uint32_t height )
{
	if ( vk.mgs.accum_image != VK_NULL_HANDLE &&
		mgs.targetWidth == width && mgs.targetHeight == height ) {
		return qtrue;
	}

	if ( vk.mgs.accum_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.mgs.accum_view, NULL );
		qvkDestroyImage( vk.device, vk.mgs.accum_image, NULL );
		qvkFreeMemory( vk.device, vk.mgs.accum_memory, NULL );
		vk.mgs.accum_view = VK_NULL_HANDLE;
		vk.mgs.accum_image = VK_NULL_HANDLE;
		vk.mgs.accum_memory = VK_NULL_HANDLE;
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
		if ( qvkCreateImage( vk.device, &image_desc, NULL, &vk.mgs.accum_image ) != VK_SUCCESS ) {
			return qfalse;
		}
		qvkGetImageMemoryRequirements( vk.device, vk.mgs.accum_image, &mem_req );
		Com_Memset( &alloc_info, 0, sizeof( alloc_info ) );
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = mem_req.size;
		alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
		if ( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.mgs.accum_memory ) != VK_SUCCESS ||
			qvkBindImageMemory( vk.device, vk.mgs.accum_image, vk.mgs.accum_memory, 0 ) != VK_SUCCESS ) {
			qvkDestroyImage( vk.device, vk.mgs.accum_image, NULL );
			vk.mgs.accum_image = VK_NULL_HANDLE;
			return qfalse;
		}
		Com_Memset( &view_desc, 0, sizeof( view_desc ) );
		view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_desc.image = vk.mgs.accum_image;
		view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_desc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view_desc.subresourceRange.levelCount = 1;
		view_desc.subresourceRange.layerCount = 1;
		if ( qvkCreateImageView( vk.device, &view_desc, NULL, &vk.mgs.accum_view ) != VK_SUCCESS ) {
			return qfalse;
		}
	}

	mgs.targetWidth = width;
	mgs.targetHeight = height;
	return qtrue;
}

static void MGS_CreatePreparePipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[2];
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;

	if ( vk.mgs.prepare_ready || vk.modules.mgs_prepare_cs == VK_NULL_HANDLE ) {
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
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.mgs.prepare_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_mgs_prepare_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.mgs.prepare_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.mgs.prepare_pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.mgs_prepare_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.mgs.prepare_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.mgs.prepare_pipeline ) );

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
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &pci, NULL, &vk.mgs.prepare_pool ) );
		Com_Memset( &ai, 0, sizeof( ai ) );
		ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		ai.descriptorPool = vk.mgs.prepare_pool;
		ai.descriptorSetCount = 1;
		ai.pSetLayouts = &vk.mgs.prepare_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &ai, &vk.mgs.prepare_descriptor ) );
	}

	vk.mgs.prepare_ready = qtrue;
}

static void MGS_CreateSplatPipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[2];
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;

	if ( vk.mgs.splat_ready || vk.modules.mgs_splat_cs == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	Com_Memset( &layout_ci, 0, sizeof( layout_ci ) );
	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.bindingCount = 2;
	layout_ci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.mgs.splat_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_mgs_splat_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.mgs.splat_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.mgs.splat_pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.mgs_splat_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.mgs.splat_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.mgs.splat_pipeline ) );

	{
		VkDescriptorPoolSize ps[2];
		VkDescriptorPoolCreateInfo pci;
		VkDescriptorSetAllocateInfo ai;

		Com_Memset( ps, 0, sizeof( ps ) );
		ps[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		ps[0].descriptorCount = 1;
		ps[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		ps[1].descriptorCount = 1;
		Com_Memset( &pci, 0, sizeof( pci ) );
		pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pci.maxSets = 1;
		pci.poolSizeCount = 2;
		pci.pPoolSizes = ps;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &pci, NULL, &vk.mgs.splat_pool ) );
		Com_Memset( &ai, 0, sizeof( ai ) );
		ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		ai.descriptorPool = vk.mgs.splat_pool;
		ai.descriptorSetCount = 1;
		ai.pSetLayouts = &vk.mgs.splat_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &ai, &vk.mgs.splat_descriptor ) );
	}

	vk.mgs.splat_ready = qtrue;
}

static void MGS_CreateCompositePipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[3];
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;

	if ( vk.mgs.composite_ready || vk.modules.mgs_composite_cs == VK_NULL_HANDLE ) {
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
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.mgs.composite_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_mgs_composite_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.mgs.composite_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.mgs.composite_pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.mgs_composite_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.mgs.composite_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.mgs.composite_pipeline ) );

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
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &pci, NULL, &vk.mgs.composite_pool ) );
		Com_Memset( &ai, 0, sizeof( ai ) );
		ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		ai.descriptorPool = vk.mgs.composite_pool;
		ai.descriptorSetCount = 1;
		ai.pSetLayouts = &vk.mgs.composite_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &ai, &vk.mgs.composite_descriptor ) );
	}

	vk.mgs.composite_ready = qtrue;
}

static void MGS_Cmd_Status( void )
{
	mgsTierParams_t tier = MGS_TierParams();
	ri.Printf( PRINT_ALL,
		"[MGS] active=%d loaded=%d gaussians=%u tier=%u scale=%.2f accum=%ux%u\n",
		R_MGS_Active() ? 1 : 0,
		mgs.loaded ? 1 : 0,
		vk.mgs.gaussian_count,
		tier.tier,
		tier.scale,
		mgs.targetWidth, mgs.targetHeight );
}

void R_MGS_Init( void )
{
	r_mgs = ri.Cvar_Get( "r_mgs", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_mgs_strength = ri.Cvar_Get( "r_mgs_strength", "0.85", CVAR_ARCHIVE_ND );
	r_mgs_maxSplats = ri.Cvar_Get( "r_mgs_maxSplats", "0", CVAR_ARCHIVE_ND );
	r_mgs_scale = ri.Cvar_Get( "r_mgs_scale", "0", CVAR_ARCHIVE_ND );
	r_mgs_focal = ri.Cvar_Get( "r_mgs_focal", "512", CVAR_ARCHIVE_ND );
	r_mgs_skipSky = ri.Cvar_Get( "r_mgs_skipSky", "1", CVAR_ARCHIVE_ND );
	r_mgs_depthTest = ri.Cvar_Get( "r_mgs_depthTest", "1", CVAR_ARCHIVE_ND );
	r_mgs_debug = ri.Cvar_Get( "r_mgs_debug", "0", CVAR_ARCHIVE_ND );

	ri.Cvar_CheckRange( r_mgs, "0", "3", CV_INTEGER );
	ri.Cvar_CheckRange( r_mgs_maxSplats, "0", "2048", CV_INTEGER );
	ri.Cvar_CheckRange( r_mgs_scale, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_mgs,
		"Mobile-GS Gaussian splatting: 0=off, 1=mobile tier, 2=balanced, 3=high (latched; vid_restart)." );
	ri.Cvar_SetDescription( r_mgs_strength,
		"Splat composite strength (alpha scale)." );
	ri.Cvar_SetDescription( r_mgs_maxSplats,
		"Override max splats (0=tier default: 64/256/1024)." );
	ri.Cvar_SetDescription( r_mgs_scale,
		"Override splat buffer resolution scale (0=tier: 0.25/0.5/1)." );
	ri.Cvar_SetDescription( r_mgs_focal,
		"Screen-radius focal scale for projected splat size." );

	ri.Cmd_AddCommand( "mgs_status", MGS_Cmd_Status );

	if ( r_mgs->integer > 0 ) {
		ri.Printf( PRINT_ALL,
			"[MGS] Mobile-GS enabled (tier %d). See docs/MOBILE_GAUSSIAN_SPLATTING.md\n",
			r_mgs->integer );
#ifdef __ANDROID__
		ri.Printf( PRINT_ALL,
			"[MGS] Android: tier 1 recommended for backgrounds/vistas; pair with r_renderScale if needed.\n" );
#endif
	}
}

void R_MGS_Shutdown( void )
{
	ri.Cmd_RemoveCommand( "mgs_status" );
	MGS_ClearGpu();
	Com_Memset( &mgs, 0, sizeof( mgs ) );
}

void R_MGS_OnMapLoad( const char *mapBaseName )
{
	mgsGaussian_t *gaussians;
	mgsTierParams_t tier;
	uint32_t count;
	float sigma;

	MGS_ClearGpu();
	Com_Memset( &mgs, 0, sizeof( mgs ) );

	if ( !r_mgs || r_mgs->integer <= 0 ) {
		return;
	}
	if ( !mapBaseName || !mapBaseName[0] ) {
		return;
	}

	Q_strncpyz( mgs.mapName, mapBaseName, sizeof( mgs.mapName ) );
	tier = MGS_TierParams();
	count = tier.maxGaussians;
	sigma = 2.5f;

	gaussians = (mgsGaussian_t *)ri.Malloc( (size_t)count * sizeof( mgsGaussian_t ) );
	MGS_FillProceduralGaussians( gaussians, count, sigma );
	if ( MGS_UploadGaussians( count, gaussians ) ) {
		mgs.gaussianCount = count;
		mgs.loaded = qtrue;
		ri.Printf( PRINT_ALL, "[MGS] Loaded %u procedural Gaussians for '%s' (tier %u)\n",
			count, mgs.mapName, tier.tier );
	}
	ri.Free( gaussians );

	MGS_CreatePreparePipeline();
	MGS_CreateSplatPipeline();
	MGS_CreateCompositePipeline();
	vk.mgsAllocated = qtrue;
}

qboolean R_MGS_Active( void )
{
	return ( R_MGS_EffectiveTier() > 0 && mgs.loaded && vk.mgs.gaussian_count > 0 &&
		vk.fboActive && vk.color_image != VK_NULL_HANDLE &&
		vk.mgs.prepare_ready && vk.mgs.splat_ready && vk.mgs.composite_ready ) ? qtrue : qfalse;
}

void vk_mgs_apply_after_geometry( void )
{
	VkCommandBuffer cmd;
	mgsTierParams_t tier;
	uint32_t fullW, fullH, splatW, splatH;
	float viewProj[16];
	const float *view;
	const float *projection;
	float proj_vk[16];
	vk_mgs_prepare_push_t prepPush;
	vk_mgs_splat_push_t splatPush;
	vk_mgs_composite_push_t compPush;
	VkDescriptorBufferInfo gaussInfo, splatInfo;
	VkDescriptorImageInfo accumInfo, depthInfo, colorInfo;
	VkWriteDescriptorSet writes[6];
	VkImageMemoryBarrier barriers[2];
	VkImageAspectFlags depthAspect;
	uint32_t prepGroups;

	if ( !R_MGS_Active() || !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}

	cmd = vk.cmd->command_buffer;
	tier = MGS_TierParams();
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

	if ( !MGS_EnsureAccumTarget( splatW, splatH ) ) {
		return;
	}

	view = backEnd.viewParms.world.modelViewMatrix;
	projection = backEnd.useFirstPersonProjection ?
		backEnd.firstPersonProjectionMatrix : backEnd.viewParms.projectionMatrix;
	vk_get_projection_matrix_vk( projection, proj_vk );
	myGlMultMatrix( view, proj_vk, viewProj );

	Com_Memset( &prepPush, 0, sizeof( prepPush ) );
	Com_Memcpy( prepPush.viewProj, viewProj, sizeof( prepPush.viewProj ) );
	prepPush.viewOrigin[0] = backEnd.viewParms.or.origin[0];
	prepPush.viewOrigin[1] = backEnd.viewParms.or.origin[1];
	prepPush.viewOrigin[2] = backEnd.viewParms.or.origin[2];
	prepPush.viewOrigin[3] = 1.0f;
	prepPush.viewport[0] = (float)splatW;
	prepPush.viewport[1] = (float)splatH;
	prepPush.viewport[2] = r_mgs_focal ? r_mgs_focal->value : 512.0f;
	prepPush.viewport[3] = (float)tier.maxFootprint;
	prepPush.gaussianCount = (int)vk.mgs.gaussian_count;

	gaussInfo.buffer = vk.mgs.gaussian_buffer;
	gaussInfo.offset = 0;
	gaussInfo.range = VK_WHOLE_SIZE;
	splatInfo.buffer = vk.mgs.splat_buffer;
	splatInfo.offset = 0;
	splatInfo.range = VK_WHOLE_SIZE;

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = vk.mgs.prepare_descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[0].pBufferInfo = &gaussInfo;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = vk.mgs.prepare_descriptor;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[1].pBufferInfo = &splatInfo;
	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vk.mgs.prepare_pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vk.mgs.prepare_pipeline_layout,
		0, 1, &vk.mgs.prepare_descriptor, 0, NULL );
	qvkCmdPushConstants( cmd, vk.mgs.prepare_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof( prepPush ), &prepPush );
	prepGroups = ( vk.mgs.gaussian_count + 63u ) / 64u;
	qvkCmdDispatch( cmd, prepGroups, 1, 1 );

	Com_Memset( barriers, 0, sizeof( barriers ) );
	barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barriers[0].image = vk.mgs.accum_image;
	barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barriers[0].subresourceRange.levelCount = 1;
	barriers[0].subresourceRange.layerCount = 1;
	barriers[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, NULL, 0, NULL, 1, barriers );

	Com_Memset( &accumInfo, 0, sizeof( accumInfo ) );
	accumInfo.imageView = vk.mgs.accum_view;
	accumInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = vk.mgs.splat_descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[0].pBufferInfo = &splatInfo;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = vk.mgs.splat_descriptor;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[1].pImageInfo = &accumInfo;
	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );

	Com_Memset( &splatPush, 0, sizeof( splatPush ) );
	splatPush.viewport[0] = (float)splatW;
	splatPush.viewport[1] = (float)splatH;
	splatPush.splatCount = (int)vk.mgs.gaussian_count;
	splatPush.maxFootprint = (int)tier.maxFootprint;
	splatPush.strength = tier.strength;

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vk.mgs.splat_pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vk.mgs.splat_pipeline_layout,
		0, 1, &vk.mgs.splat_descriptor, 0, NULL );
	qvkCmdPushConstants( cmd, vk.mgs.splat_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof( splatPush ), &splatPush );
	qvkCmdDispatch( cmd, vk.mgs.gaussian_count, 1, 1 );

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
	depthInfo.sampler = MGS_DepthSampler();
	depthInfo.imageView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	Com_Memset( &accumInfo, 0, sizeof( accumInfo ) );
	accumInfo.sampler = MGS_LinearSampler();
	accumInfo.imageView = vk.mgs.accum_view;
	accumInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	Com_Memset( &colorInfo, 0, sizeof( colorInfo ) );
	colorInfo.imageView = vk.color_image_view;
	colorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = vk.mgs.composite_descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].descriptorCount = 1;
	writes[0].pImageInfo = &depthInfo;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = vk.mgs.composite_descriptor;
	writes[1].dstBinding = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].descriptorCount = 1;
	writes[1].pImageInfo = &accumInfo;
	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = vk.mgs.composite_descriptor;
	writes[2].dstBinding = 2;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[2].descriptorCount = 1;
	writes[2].pImageInfo = &colorInfo;
	qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );

	Com_Memset( &compPush, 0, sizeof( compPush ) );
	compPush.extent[0] = (float)fullW;
	compPush.extent[1] = (float)fullH;
	compPush.params[0] = tier.strength;
	compPush.params[1] = ( r_mgs_depthTest && r_mgs_depthTest->integer ) ? 1.0f : 0.0f;
	compPush.params[2] = ( r_mgs_skipSky && r_mgs_skipSky->integer ) ? 1.0f : 0.0f;
	compPush.params[3] = 0.002f;

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vk.mgs.composite_pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vk.mgs.composite_pipeline_layout,
		0, 1, &vk.mgs.composite_descriptor, 0, NULL );
	qvkCmdPushConstants( cmd, vk.mgs.composite_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof( compPush ), &compPush );
	qvkCmdDispatch( cmd, ( fullW + 7u ) / 8u, ( fullH + 7u ) / 8u, 1 );

	record_image_layout_transition( cmd, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, 0 );

	if ( r_mgs_debug && r_mgs_debug->integer ) {
		ri.Printf( PRINT_DEVELOPER, "[MGS] splat %ux%u -> composite %ux%u count=%u tier=%u\n",
			splatW, splatH, fullW, fullH, vk.mgs.gaussian_count, tier.tier );
	}
}
