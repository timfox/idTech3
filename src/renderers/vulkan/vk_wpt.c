/*
===========================================================================
Wavefront path tracing experiment — queued primary rays + screen-space waves.
See docs/WAVEFRONT_PATH_TRACING.md and docs/NEURAL_RENDERER_PHASES.md.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_wpt.h"
#include "vk_util.h"
#include "vk_image_layout.h"
#include "vk_view_state.h"
#include "vk_deferred_gbuffer.h"
#include "vk_cmd.h"

typedef struct {
	uint32_t    rayCount;
} wptState_t;

static wptState_t wpt;

static cvar_t *r_wpt;
static cvar_t *r_wpt_strength;
static cvar_t *r_wpt_scale;
static cvar_t *r_wpt_bounces;
static cvar_t *r_wpt_stepScale;
static cvar_t *r_wpt_useGBuffer;
static cvar_t *r_wpt_skipSky;
static cvar_t *r_wpt_fsaBridge;
static cvar_t *r_wpt_debug;

typedef struct {
	float       invViewProj[16];
	float       viewOrigin[4];
	uint32_t    extent[2];
	uint32_t    maxRays;
	float       strength;
} vk_wpt_enqueue_push_t;

typedef struct {
	float       viewProj[16];
	uint32_t    extent[2];
	uint32_t    maxRays;
	uint32_t    waveIndex;
	float       stepScale;
	float       strength;
	uint32_t    useGBufferNormal;
	uint32_t    hasGBuffer;
} vk_wpt_wave_push_t;

typedef struct {
	uint32_t    extent[2];
	uint32_t    maxRays;
	float       strength;
	uint32_t    skipSky;
} vk_wpt_composite_push_t;

static VkSampler WPT_DepthSampler( void )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;
	return vk_find_sampler( &sd );
}

static VkSampler WPT_LinearSampler( void )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	return vk_find_sampler( &sd );
}

static void WPT_ClearGpu( void )
{
	if ( vk.wpt.enqueue_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.wpt.enqueue_pipeline, NULL );
		vk.wpt.enqueue_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.wpt.wave_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.wpt.wave_pipeline, NULL );
		vk.wpt.wave_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.wpt.composite_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.wpt.composite_pipeline, NULL );
		vk.wpt.composite_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.wpt.enqueue_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.wpt.enqueue_pipeline_layout, NULL );
		vk.wpt.enqueue_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.wpt.wave_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.wpt.wave_pipeline_layout, NULL );
		vk.wpt.wave_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.wpt.composite_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.wpt.composite_pipeline_layout, NULL );
		vk.wpt.composite_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.wpt.enqueue_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.wpt.enqueue_layout, NULL );
		vk.wpt.enqueue_layout = VK_NULL_HANDLE;
	}
	if ( vk.wpt.wave_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.wpt.wave_layout, NULL );
		vk.wpt.wave_layout = VK_NULL_HANDLE;
	}
	if ( vk.wpt.composite_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.wpt.composite_layout, NULL );
		vk.wpt.composite_layout = VK_NULL_HANDLE;
	}
	if ( vk.wpt.ray_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.wpt.ray_buffer, NULL );
		vk.wpt.ray_buffer = VK_NULL_HANDLE;
	}
	if ( vk.wpt.ray_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.wpt.ray_memory, NULL );
		vk.wpt.ray_memory = VK_NULL_HANDLE;
	}
	vk.wpt.enqueue_ready = qfalse;
	vk.wpt.wave_ready = qfalse;
	vk.wpt.composite_ready = qfalse;
	vk.wpt.buffers_ready = qfalse;
	vk.wptAllocated = qfalse;
	wpt.rayCount = 0;
}

static qboolean WPT_EnsureRayBuffer( uint32_t width, uint32_t height )
{
	VkDeviceSize bytes;
	VkBufferCreateInfo buf_ci;
	VkMemoryRequirements mem_req;
	VkMemoryAllocateInfo alloc_ci;

	bytes = (VkDeviceSize)width * (VkDeviceSize)height * 48u;
	if ( vk.wpt.ray_buffer != VK_NULL_HANDLE && wpt.rayCount == width * height ) {
		return qtrue;
	}

	if ( vk.wpt.ray_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.wpt.ray_buffer, NULL );
		qvkFreeMemory( vk.device, vk.wpt.ray_memory, NULL );
		vk.wpt.ray_buffer = VK_NULL_HANDLE;
		vk.wpt.ray_memory = VK_NULL_HANDLE;
	}

	Com_Memset( &buf_ci, 0, sizeof( buf_ci ) );
	buf_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_ci.size = bytes;
	buf_ci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &buf_ci, NULL, &vk.wpt.ray_buffer ) );
	qvkGetBufferMemoryRequirements( vk.device, vk.wpt.ray_buffer, &mem_req );
	Com_Memset( &alloc_ci, 0, sizeof( alloc_ci ) );
	alloc_ci.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_ci.allocationSize = mem_req.size;
	alloc_ci.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_ci, NULL, &vk.wpt.ray_memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.wpt.ray_buffer, vk.wpt.ray_memory, 0 ) );

	wpt.rayCount = width * height;
	vk.wpt.buffers_ready = qtrue;
	vk.wptAllocated = qtrue;
	return qtrue;
}

static void WPT_CreateEnqueuePipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[2];
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;

	if ( vk.wpt.enqueue_ready || vk.modules.wpt_enqueue_cs == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	Com_Memset( &layout_ci, 0, sizeof( layout_ci ) );
	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.bindingCount = 2;
	layout_ci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.wpt.enqueue_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_wpt_enqueue_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.wpt.enqueue_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.wpt.enqueue_pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.wpt_enqueue_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.wpt.enqueue_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.wpt.enqueue_pipeline ) );
	vk.wpt.enqueue_ready = qtrue;
}

static void WPT_CreateWavePipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[3];
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;

	if ( vk.wpt.wave_ready || vk.modules.wpt_wave_cs == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	Com_Memset( &layout_ci, 0, sizeof( layout_ci ) );
	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.bindingCount = 3;
	layout_ci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.wpt.wave_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_wpt_wave_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.wpt.wave_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.wpt.wave_pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.wpt_wave_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.wpt.wave_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.wpt.wave_pipeline ) );
	vk.wpt.wave_ready = qtrue;
}

static void WPT_CreateCompositePipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[4];
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;

	if ( vk.wpt.composite_ready || vk.modules.wpt_composite_cs == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
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
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.wpt.composite_layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_wpt_composite_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.wpt.composite_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.wpt.composite_pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.wpt_composite_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.wpt.composite_pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.wpt.composite_pipeline ) );
	vk.wpt.composite_ready = qtrue;
}

static void WPT_FillInvViewProj( float *out16 )
{
	float viewProj[16];
	const float *view;
	const float *projection;

	view = backEnd.viewParms.world.modelViewMatrix;
	projection = backEnd.useFirstPersonProjection ?
		backEnd.firstPersonProjectionMatrix : backEnd.viewParms.projectionMatrix;
	myGlMultMatrix( view, projection, viewProj );
	if ( !vk_mat4_inverse( viewProj, out16 ) ) {
		Com_Memcpy( out16, viewProj, sizeof( viewProj ) );
	}
}

static void WPT_FillViewProj( float *out16 )
{
	const float *view;
	const float *projection;

	view = backEnd.viewParms.world.modelViewMatrix;
	projection = backEnd.useFirstPersonProjection ?
		backEnd.firstPersonProjectionMatrix : backEnd.viewParms.projectionMatrix;
	myGlMultMatrix( view, projection, out16 );
}

static void WPT_Cmd_Status( void )
{
	ri.Printf( PRINT_ALL, "[WPT] active=%d rays=%u fsaBridge=%d\n",
		R_WPT_Active() ? 1 : 0,
		wpt.rayCount,
		( r_wpt_fsaBridge && r_wpt_fsaBridge->integer ) ? 1 : 0 );
}

void R_WPT_Init( void )
{
	r_wpt = ri.Cvar_Get( "r_wpt", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_wpt_strength = ri.Cvar_Get( "r_wpt_strength", "0.35", CVAR_ARCHIVE_ND );
	r_wpt_scale = ri.Cvar_Get( "r_wpt_scale", "0.5", CVAR_ARCHIVE_ND );
	r_wpt_bounces = ri.Cvar_Get( "r_wpt_bounces", "1", CVAR_ARCHIVE_ND );
	r_wpt_stepScale = ri.Cvar_Get( "r_wpt_stepScale", "1", CVAR_ARCHIVE_ND );
	r_wpt_useGBuffer = ri.Cvar_Get( "r_wpt_useGBuffer", "1", CVAR_ARCHIVE_ND );
	r_wpt_skipSky = ri.Cvar_Get( "r_wpt_skipSky", "1", CVAR_ARCHIVE_ND );
	r_wpt_fsaBridge = ri.Cvar_Get( "r_wpt_fsaBridge", "1", CVAR_ARCHIVE_ND );
	r_wpt_debug = ri.Cvar_Get( "r_wpt_debug", "0", CVAR_ARCHIVE_ND );

	ri.Cvar_CheckRange( r_wpt, "0", "1", CV_INTEGER );
	ri.Cvar_CheckRange( r_wpt_bounces, "0", "2", CV_INTEGER );
	ri.Cvar_CheckRange( r_wpt_scale, "0.25", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_wpt,
		"Wavefront path experiment: queued rays + screen-space bounce waves (0=off, 1=on)." );
	ri.Cvar_SetDescription( r_wpt_fsaBridge,
		"Document FSA pairing: enable r_fsa 1 before WPT for sparse RT experiments." );

	ri.Cmd_AddCommand( "wpt_status", WPT_Cmd_Status );

	if ( r_wpt->integer ) {
		ri.Printf( PRINT_ALL,
			"[WPT] Wavefront path experiment enabled. See docs/WAVEFRONT_PATH_TRACING.md\n" );
	}
}

void R_WPT_Shutdown( void )
{
	ri.Cmd_RemoveCommand( "wpt_status" );
	WPT_ClearGpu();
	Com_Memset( &wpt, 0, sizeof( wpt ) );
}

qboolean R_WPT_Active( void )
{
	return ( r_wpt && r_wpt->integer && vk.fboActive && vk.depth_image != VK_NULL_HANDLE ) ? qtrue : qfalse;
}

void vk_wpt_apply_after_geometry( void )
{
	uint32_t fullW, fullH, width, height;
	VkImageView depthView, normalView, albedoView;
	VkImageAspectFlags depth_aspect;
	int bounces, wave;
	qboolean useGbuf;

	if ( !r_wpt || !r_wpt->integer ) {
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE || !vk.inRenderPass ) {
		return;
	}
	if ( vk.color_image == VK_NULL_HANDLE || vk.depth_image == VK_NULL_HANDLE ) {
		return;
	}

	fullW = vk_get_render_target_width();
	fullH = vk_get_render_target_height();
	if ( fullW == 0 || fullH == 0 ) {
		return;
	}

	width = fullW;
	height = fullH;

	if ( !WPT_EnsureRayBuffer( width, height ) ) {
		return;
	}

	WPT_CreateEnqueuePipeline();
	WPT_CreateWavePipeline();
	WPT_CreateCompositePipeline();
	if ( !vk.wpt.enqueue_ready || !vk.wpt.wave_ready || !vk.wpt.composite_ready ) {
		return;
	}

	depthView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	useGbuf = ( r_wpt_useGBuffer && r_wpt_useGBuffer->integer &&
		vk.deferred_gbuffer_normal_view != VK_NULL_HANDLE &&
		vk_deferred_gbuffer_fill_wanted() ) ? qtrue : qfalse;
	normalView = useGbuf ? vk.deferred_gbuffer_normal_view :
		( tr.whiteImage ? tr.whiteImage->view : VK_NULL_HANDLE );
	albedoView = ( vk.deferred_gbuffer_albedo_view != VK_NULL_HANDLE && useGbuf ) ?
		vk.deferred_gbuffer_albedo_view : vk.color_image_view;

	depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	bounces = r_wpt_bounces ? r_wpt_bounces->integer : 1;
	if ( bounces < 0 ) {
		bounces = 0;
	}
	if ( bounces > 2 ) {
		bounces = 2;
	}

	/* Wave 0: enqueue */
	{
		VkDescriptorPoolSize ps[2];
		VkDescriptorPoolCreateInfo pool_ci;
		VkDescriptorSetAllocateInfo alloc_ci;
		VkDescriptorBufferInfo buf_info;
		VkDescriptorImageInfo depth_img;
		VkWriteDescriptorSet writes[2];
		VkDescriptorPool pool;
		VkDescriptorSet desc;
		vk_wpt_enqueue_push_t push;

		ps[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		ps[0].descriptorCount = 1;
		ps[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		ps[1].descriptorCount = 1;
		Com_Memset( &pool_ci, 0, sizeof( pool_ci ) );
		pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_ci.maxSets = 1;
		pool_ci.poolSizeCount = 2;
		pool_ci.pPoolSizes = ps;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &pool ) );
		alloc_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_ci.descriptorPool = pool;
		alloc_ci.descriptorSetCount = 1;
		alloc_ci.pSetLayouts = &vk.wpt.enqueue_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc_ci, &desc ) );

		buf_info.buffer = vk.wpt.ray_buffer;
		buf_info.offset = 0;
		buf_info.range = VK_WHOLE_SIZE;
		depth_img.sampler = WPT_DepthSampler();
		depth_img.imageView = depthView;
		depth_img.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = desc;
		writes[0].dstBinding = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[0].pBufferInfo = &buf_info;
		writes[1] = writes[0];
		writes[1].dstBinding = 1;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[1].pImageInfo = &depth_img;
		qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );

		Com_Memset( &push, 0, sizeof( push ) );
		WPT_FillInvViewProj( push.invViewProj );
		VectorCopy( backEnd.viewParms.or.origin, push.viewOrigin );
		push.viewOrigin[3] = 1.0f;
		push.extent[0] = width;
		push.extent[1] = height;
		push.maxRays = wpt.rayCount;
		push.strength = r_wpt_strength ? r_wpt_strength->value : 0.35f;

		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.wpt.enqueue_pipeline );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
			vk.wpt.enqueue_pipeline_layout, 0, 1, &desc, 0, NULL );
		qvkCmdPushConstants( vk.cmd->command_buffer, vk.wpt.enqueue_pipeline_layout,
			VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );
		qvkCmdDispatch( vk.cmd->command_buffer, ( width + 7 ) / 8, ( height + 7 ) / 8, 1 );
		qvkDestroyDescriptorPool( vk.device, pool, NULL );
	}

	for ( wave = 0; wave < bounces; wave++ ) {
		VkDescriptorPoolSize ps[3];
		VkDescriptorPoolCreateInfo pool_ci;
		VkDescriptorSetAllocateInfo alloc_ci;
		VkDescriptorBufferInfo buf_info;
		VkDescriptorImageInfo depth_img, normal_img;
		VkWriteDescriptorSet writes[3];
		VkDescriptorPool pool;
		VkDescriptorSet desc;
		vk_wpt_wave_push_t push;

		ps[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		ps[0].descriptorCount = 1;
		ps[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		ps[1].descriptorCount = 2;
		Com_Memset( &pool_ci, 0, sizeof( pool_ci ) );
		pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_ci.maxSets = 1;
		pool_ci.poolSizeCount = 2;
		pool_ci.pPoolSizes = ps;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &pool ) );
		alloc_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_ci.descriptorPool = pool;
		alloc_ci.descriptorSetCount = 1;
		alloc_ci.pSetLayouts = &vk.wpt.wave_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc_ci, &desc ) );

		buf_info.buffer = vk.wpt.ray_buffer;
		buf_info.offset = 0;
		buf_info.range = VK_WHOLE_SIZE;
		depth_img.sampler = WPT_DepthSampler();
		depth_img.imageView = depthView;
		depth_img.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		normal_img.sampler = WPT_LinearSampler();
		normal_img.imageView = normalView;
		normal_img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = desc;
		writes[0].dstBinding = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[0].pBufferInfo = &buf_info;
		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet = desc;
		writes[1].dstBinding = 1;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[1].pImageInfo = &depth_img;
		writes[2] = writes[1];
		writes[2].dstBinding = 2;
		writes[2].pImageInfo = &normal_img;
		qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );

		Com_Memset( &push, 0, sizeof( push ) );
		WPT_FillViewProj( push.viewProj );
		push.extent[0] = width;
		push.extent[1] = height;
		push.maxRays = wpt.rayCount;
		push.waveIndex = (uint32_t)wave;
		push.stepScale = r_wpt_stepScale ? r_wpt_stepScale->value : 1.0f;
		push.strength = r_wpt_strength ? r_wpt_strength->value : 0.35f;
		push.useGBufferNormal = useGbuf ? 1u : 0u;
		push.hasGBuffer = useGbuf ? 1u : 0u;

		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.wpt.wave_pipeline );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
			vk.wpt.wave_pipeline_layout, 0, 1, &desc, 0, NULL );
		qvkCmdPushConstants( vk.cmd->command_buffer, vk.wpt.wave_pipeline_layout,
			VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );
		qvkCmdDispatch( vk.cmd->command_buffer, ( width + 7 ) / 8, ( height + 7 ) / 8, 1 );
		qvkDestroyDescriptorPool( vk.device, pool, NULL );
	}

	/* Composite at full resolution */
	{
		VkDescriptorPoolSize ps[3];
		VkDescriptorPoolCreateInfo pool_ci;
		VkDescriptorSetAllocateInfo alloc_ci;
		VkDescriptorBufferInfo buf_info;
		VkDescriptorImageInfo depth_img, albedo_img, out_img;
		VkWriteDescriptorSet writes[4];
		VkDescriptorPool pool;
		VkDescriptorSet desc;
		vk_wpt_composite_push_t push;

		ps[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		ps[0].descriptorCount = 1;
		ps[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		ps[1].descriptorCount = 2;
		ps[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		ps[2].descriptorCount = 1;
		Com_Memset( &pool_ci, 0, sizeof( pool_ci ) );
		pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_ci.maxSets = 1;
		pool_ci.poolSizeCount = 3;
		pool_ci.pPoolSizes = ps;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &pool ) );
		alloc_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_ci.descriptorPool = pool;
		alloc_ci.descriptorSetCount = 1;
		alloc_ci.pSetLayouts = &vk.wpt.composite_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc_ci, &desc ) );

		buf_info.buffer = vk.wpt.ray_buffer;
		buf_info.offset = 0;
		buf_info.range = VK_WHOLE_SIZE;
		depth_img.sampler = WPT_DepthSampler();
		depth_img.imageView = depthView;
		depth_img.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		albedo_img.sampler = WPT_LinearSampler();
		albedo_img.imageView = albedoView;
		albedo_img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		out_img.imageView = vk.color_image_view;
		out_img.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = desc;
		writes[0].dstBinding = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[0].pBufferInfo = &buf_info;
		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet = desc;
		writes[1].dstBinding = 1;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[1].pImageInfo = &depth_img;
		writes[2] = writes[1];
		writes[2].dstBinding = 2;
		writes[2].pImageInfo = &albedo_img;
		writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[3].dstSet = desc;
		writes[3].dstBinding = 3;
		writes[3].descriptorCount = 1;
		writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[3].pImageInfo = &out_img;
		qvkUpdateDescriptorSets( vk.device, 4, writes, 0, NULL );

		Com_Memset( &push, 0, sizeof( push ) );
		push.extent[0] = width;
		push.extent[1] = height;
		push.maxRays = wpt.rayCount;
		push.strength = r_wpt_strength ? r_wpt_strength->value : 0.35f;
		push.skipSky = ( r_wpt_skipSky && r_wpt_skipSky->integer ) ? 1u : 0u;

		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.wpt.composite_pipeline );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
			vk.wpt.composite_pipeline_layout, 0, 1, &desc, 0, NULL );
		qvkCmdPushConstants( vk.cmd->command_buffer, vk.wpt.composite_pipeline_layout,
			VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );
		qvkCmdDispatch( vk.cmd->command_buffer, ( width + 7 ) / 8, ( height + 7 ) / 8, 1 );
		qvkDestroyDescriptorPool( vk.device, pool, NULL );
	}

	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );

	if ( r_wpt_debug && r_wpt_debug->integer ) {
		ri.Printf( PRINT_DEVELOPER, "[WPT] waves=%d res=%ux%u (queue %ux%u)\n",
			bounces, fullW, fullH, width, height );
	}
}
