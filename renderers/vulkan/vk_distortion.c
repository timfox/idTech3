/*
===========================================================================
Raster Ultra 1.4 — screen-space distortion / heat-haze buffer.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_distortion.h"
#include "vk_gpu_particles.h"
#include "vk_util.h"
#include "vk_image_layout.h"
#include "vk_view_state.h"
#include "vk_reactive_mask.h"
#include "vk_pass_registry.h"
#include "vk_raster_ultra.h"

#include <math.h>

typedef struct {
	VkImage image;
	VkImageView view;
	VkDeviceMemory memory;
	VkImageLayout layout;
	uint32_t width, height;
} distImage_t;

typedef struct {
	qboolean ready;
	qboolean appliedThisFrame;
	qboolean pulseActive;
	uint32_t frameIndex;
	uint32_t width, height;
	float pulseStrength;
	float pulseRadius;

	distImage_t distort;

	VkShaderModule applyCS;
	VkDescriptorSetLayout applyLayout;
	VkPipelineLayout applyPL;
	VkPipeline applyPipe;
	VkDescriptorPool descriptorPool;
	VkDescriptorSet applySet;
} distState_t;

static distState_t dist;

static cvar_t *r_distortion;
static cvar_t *r_distortionMaxPx;
static cvar_t *r_distortionDepthTol;
static cvar_t *r_distortionDebug;

#include "vk_raster_fx_spirv.inc"

static void DIST_RegisterCvars( void )
{
	if ( r_distortion ) {
		return;
	}
	r_distortion = ri.Cvar_Get( "r_distortion", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_distortion, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_distortion,
		"Raster Ultra 1.4 screen-space distortion (latched).\n"
		" 0 - off (default)\n"
		" 1 - RG16F distortion buffer + depth-aware HDR warp\n"
		"Enable via: exec modern_raster_ultra.cfg; vid_restart" );
	ri.Cvar_SetGroup( r_distortion, CVG_RENDERER );

	r_distortionMaxPx = ri.Cvar_Get( "r_distortionMaxPx", "8", CVAR_ARCHIVE_ND );
	r_distortionDepthTol = ri.Cvar_Get( "r_distortionDepthTol", "0.02", CVAR_ARCHIVE_ND );
	r_distortionDebug = ri.Cvar_Get( "r_distortionDebug", "0", CVAR_ARCHIVE_ND );
}

static void DIST_DestroyImage( distImage_t *img )
{
	if ( img->view ) {
		qvkDestroyImageView( vk.device, img->view, NULL );
	}
	if ( img->image ) {
		qvkDestroyImage( vk.device, img->image, NULL );
	}
	if ( img->memory ) {
		qvkFreeMemory( vk.device, img->memory, NULL );
	}
	Com_Memset( img, 0, sizeof( *img ) );
}

static qboolean DIST_CreateImage( distImage_t *img, uint32_t width, uint32_t height, const char *name )
{
	VkImageCreateInfo ici;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo mai;
	VkImageViewCreateInfo vci;

	DIST_DestroyImage( img );
	Com_Memset( &ici, 0, sizeof( ici ) );
	ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ici.imageType = VK_IMAGE_TYPE_2D;
	ici.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	ici.extent.width = width;
	ici.extent.height = height;
	ici.extent.depth = 1;
	ici.mipLevels = 1;
	ici.arrayLayers = 1;
	ici.samples = VK_SAMPLE_COUNT_1_BIT;
	ici.tiling = VK_IMAGE_TILING_OPTIMAL;
	ici.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if ( qvkCreateImage( vk.device, &ici, NULL, &img->image ) != VK_SUCCESS ) {
		return qfalse;
	}
	qvkGetImageMemoryRequirements( vk.device, img->image, &req );
	Com_Memset( &mai, 0, sizeof( mai ) );
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.allocationSize = req.size;
	mai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	if ( qvkAllocateMemory( vk.device, &mai, NULL, &img->memory ) != VK_SUCCESS ||
		qvkBindImageMemory( vk.device, img->image, img->memory, 0 ) != VK_SUCCESS ) {
		DIST_DestroyImage( img );
		return qfalse;
	}
	Com_Memset( &vci, 0, sizeof( vci ) );
	vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	vci.image = img->image;
	vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
	vci.format = ici.format;
	vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vci.subresourceRange.levelCount = 1;
	vci.subresourceRange.layerCount = 1;
	if ( qvkCreateImageView( vk.device, &vci, NULL, &img->view ) != VK_SUCCESS ) {
		DIST_DestroyImage( img );
		return qfalse;
	}
	img->layout = VK_IMAGE_LAYOUT_UNDEFINED;
	img->width = width;
	img->height = height;
	SET_OBJECT_NAME( img->image, name, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	return qtrue;
}

static void DIST_Transition( VkCommandBuffer cmd, distImage_t *img, VkImageLayout layout )
{
	if ( !img->image || img->layout == layout ) {
		return;
	}
	record_image_layout_transition( cmd, img->image, VK_IMAGE_ASPECT_COLOR_BIT,
		img->layout, layout, 0, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	img->layout = layout;
}

static VkSampler DIST_Sampler( void )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;
	return vk_find_sampler( &sd );
}

static VkShaderModule DIST_Module( const uint8_t *bytes, uint32_t size, const char *name )
{
	VkShaderModuleCreateInfo ci;
	VkShaderModule module = VK_NULL_HANDLE;

	Com_Memset( &ci, 0, sizeof( ci ) );
	ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.codeSize = size;
	ci.pCode = (const uint32_t *)bytes;
	if ( qvkCreateShaderModule( vk.device, &ci, NULL, &module ) != VK_SUCCESS ) {
		return VK_NULL_HANDLE;
	}
	SET_OBJECT_NAME( module, name, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	return module;
}

static void DIST_DestroyPipelines( void )
{
#define DIST_DESTROY( fn, x ) do { if ( dist.x ) { fn( vk.device, dist.x, NULL ); dist.x = VK_NULL_HANDLE; } } while ( 0 )
	DIST_DESTROY( qvkDestroyPipeline, applyPipe );
	DIST_DESTROY( qvkDestroyPipelineLayout, applyPL );
	DIST_DESTROY( qvkDestroyDescriptorSetLayout, applyLayout );
	DIST_DESTROY( qvkDestroyShaderModule, applyCS );
	DIST_DESTROY( qvkDestroyDescriptorPool, descriptorPool );
#undef DIST_DESTROY
	dist.applySet = VK_NULL_HANDLE;
}

static qboolean DIST_CreatePipelines( void )
{
	static const VkDescriptorType types[] = {
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
	};
	VkDescriptorSetLayoutBinding bindings[5];
	VkDescriptorSetLayoutCreateInfo dci;
	VkPushConstantRange range;
	VkPipelineLayoutCreateInfo pci;
	VkComputePipelineCreateInfo ci;
	VkDescriptorPoolSize poolSizes[3];
	VkDescriptorPoolCreateInfo poolCI;
	VkDescriptorSetAllocateInfo ai;
	uint32_t i;

	DIST_DestroyPipelines();
	dist.applyCS = DIST_Module( vk_distortion_apply_cs_spv, VK_DISTORTION_APPLY_CS_SPV_SIZE, "distortion_apply_cs" );
	if ( !dist.applyCS ) {
		return qfalse;
	}
	Com_Memset( bindings, 0, sizeof( bindings ) );
	for ( i = 0; i < 5; ++i ) {
		bindings[i].binding = i;
		bindings[i].descriptorType = types[i];
		bindings[i].descriptorCount = 1;
		bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	}
	Com_Memset( &dci, 0, sizeof( dci ) );
	dci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dci.bindingCount = 5;
	dci.pBindings = bindings;
	if ( qvkCreateDescriptorSetLayout( vk.device, &dci, NULL, &dist.applyLayout ) != VK_SUCCESS ) {
		return qfalse;
	}
	Com_Memset( &range, 0, sizeof( range ) );
	range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	range.size = 32;
	Com_Memset( &pci, 0, sizeof( pci ) );
	pci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pci.setLayoutCount = 1;
	pci.pSetLayouts = &dist.applyLayout;
	pci.pushConstantRangeCount = 1;
	pci.pPushConstantRanges = &range;
	if ( qvkCreatePipelineLayout( vk.device, &pci, NULL, &dist.applyPL ) != VK_SUCCESS ) {
		return qfalse;
	}
	Com_Memset( &ci, 0, sizeof( ci ) );
	ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	ci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	ci.stage.module = dist.applyCS;
	ci.stage.pName = "main";
	ci.layout = dist.applyPL;
	if ( qvkCreateComputePipelines( vk.device, vk.pipelineCache, 1, &ci, NULL, &dist.applyPipe ) != VK_SUCCESS ) {
		return qfalse;
	}
	SET_OBJECT_NAME( dist.applyPipe, "distortion_apply", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	Com_Memset( poolSizes, 0, sizeof( poolSizes ) );
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[0].descriptorCount = 3;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSizes[1].descriptorCount = 2;
	Com_Memset( &poolCI, 0, sizeof( poolCI ) );
	poolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCI.maxSets = 1;
	poolCI.poolSizeCount = 2;
	poolCI.pPoolSizes = poolSizes;
	if ( qvkCreateDescriptorPool( vk.device, &poolCI, NULL, &dist.descriptorPool ) != VK_SUCCESS ) {
		return qfalse;
	}
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	ai.descriptorPool = dist.descriptorPool;
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &dist.applyLayout;
	return qvkAllocateDescriptorSets( vk.device, &ai, &dist.applySet ) == VK_SUCCESS;
}

static qboolean DIST_EnsureImage( uint32_t width, uint32_t height )
{
	if ( dist.distort.image && dist.width == width && dist.height == height ) {
		return qtrue;
	}
	if ( !DIST_CreateImage( &dist.distort, width, height, "distortion RG16F" ) ) {
		return qfalse;
	}
	dist.width = width;
	dist.height = height;
	return qtrue;
}

static void DIST_ProjectToScreen( const vec3_t world, float *sx, float *sy, float *depth )
{
	const float *viewMat = backEnd.viewParms.world.modelViewMatrix;
	const float *projection = backEnd.useFirstPersonProjection ?
		backEnd.firstPersonProjectionMatrix : backEnd.viewParms.projectionMatrix;
	float projVK[16];
	vec4_t clip, view4;

	view4[0] = world[0] * viewMat[0] + world[1] * viewMat[4] + world[2] * viewMat[8] + viewMat[12];
	view4[1] = world[0] * viewMat[1] + world[1] * viewMat[5] + world[2] * viewMat[9] + viewMat[13];
	view4[2] = world[0] * viewMat[2] + world[1] * viewMat[6] + world[2] * viewMat[10] + viewMat[14];
	view4[3] = 1.0f;
	vk_get_projection_matrix_vk( projection, projVK );
	clip[0] = view4[0] * projVK[0] + view4[3] * projVK[3];
	clip[1] = view4[1] * projVK[5] + view4[3] * projVK[7];
	clip[2] = view4[2] * projVK[10] + view4[3] * projVK[14];
	clip[3] = view4[2] * projVK[11] + view4[3] * projVK[15];
	if ( clip[3] <= 1e-4f ) {
		*sx = *sy = -1.0f;
		*depth = 0.0f;
		return;
	}
	*sx = clip[0] / clip[3] * 0.5f + 0.5f;
	*sy = clip[1] / clip[3] * 0.5f + 0.5f;
	*depth = clip[2] / clip[3];
}

static void DIST_SeedHeat( VkCommandBuffer cmd )
{
	VkClearColorValue clear;
	VkImageSubresourceRange range;
	void *mapped = NULL;
	uint32_t live = 0;
	vk_gp_particle_t *particles;
	uint32_t i, px, py, count = 0;
	float sx, sy, depth;
	uint16_t *staging;
	VkBuffer stagingBuf = VK_NULL_HANDLE;
	VkDeviceMemory stagingMem = VK_NULL_HANDLE;
	VkBufferCreateInfo bci;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo mai;
	VkBufferImageCopy region;
	size_t stagingBytes;

	Com_Memset( &clear, 0, sizeof( clear ) );
	Com_Memset( &range, 0, sizeof( range ) );
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.levelCount = 1;
	range.layerCount = 1;

	DIST_Transition( cmd, &dist.distort, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL );
	qvkCmdClearColorImage( cmd, dist.distort.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &range );
	DIST_Transition( cmd, &dist.distort, VK_IMAGE_LAYOUT_GENERAL );

	if ( !dist.pulseActive && !vk_gpu_particles_active() ) {
		return;
	}

	stagingBytes = (size_t)( 64 * 4 ) * sizeof( uint16_t );
	Com_Memset( &bci, 0, sizeof( bci ) );
	bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.size = stagingBytes;
	bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	if ( qvkCreateBuffer( vk.device, &bci, NULL, &stagingBuf ) != VK_SUCCESS ) {
		return;
	}
	qvkGetBufferMemoryRequirements( vk.device, stagingBuf, &req );
	Com_Memset( &mai, 0, sizeof( mai ) );
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.allocationSize = req.size;
	mai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	if ( qvkAllocateMemory( vk.device, &mai, NULL, &stagingMem ) != VK_SUCCESS ||
		qvkBindBufferMemory( vk.device, stagingBuf, stagingMem, 0 ) != VK_SUCCESS ||
		qvkMapMemory( vk.device, stagingMem, 0, stagingBytes, 0, &mapped ) != VK_SUCCESS ) {
		if ( stagingBuf ) qvkDestroyBuffer( vk.device, stagingBuf, NULL );
		if ( stagingMem ) qvkFreeMemory( vk.device, stagingMem, NULL );
		return;
	}
	Com_Memset( mapped, 0, stagingBytes );
	staging = (uint16_t *)mapped;

	if ( dist.pulseActive ) {
		vec3_t origin;
		float pulseSx, pulseSy, pulseDepth;
		VectorCopy( backEnd.refdef.vieworg, origin );
		DIST_ProjectToScreen( origin, &pulseSx, &pulseSy, &pulseDepth );
		if ( pulseSx >= 0.0f && pulseSx <= 1.0f && pulseSy >= 0.0f && pulseSy <= 1.0f ) {
			px = (uint32_t)( pulseSx * (float)dist.width );
			py = (uint32_t)( pulseSy * (float)dist.height );
			if ( px < dist.width && py < dist.height && count < 64 ) {
				staging[count * 4 + 0] = 0x4000; /* rg offset */
				staging[count * 4 + 1] = 0x4000;
				staging[count * 4 + 2] = 0x6666; /* mask */
				staging[count * 4 + 3] = 0;
				Com_Memset( &region, 0, sizeof( region ) );
				region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.imageSubresource.layerCount = 1;
				region.imageExtent.width = 1;
				region.imageExtent.height = 1;
				region.imageExtent.depth = 1;
				region.imageOffset.x = (int32_t)px;
				region.imageOffset.y = (int32_t)py;
				region.bufferOffset = (VkDeviceSize)count * 4u * sizeof( uint16_t );
				region.bufferRowLength = 1;
				region.bufferImageHeight = 1;
				qvkCmdCopyBufferToImage( cmd, stagingBuf, dist.distort.image,
					VK_IMAGE_LAYOUT_GENERAL, 1, &region );
				count++;
			}
		}
		dist.pulseActive = qfalse;
	}

	if ( vk_gpu_particles_mapped( (void **)&particles, &live ) && particles && live > 0 ) {
		for ( i = 0; i < VK_GP_MAX_PARTICLES && count < 48u; ++i ) {
			vec3_t wp;
			if ( particles[i].posLife[3] <= 0.05f ) {
				continue;
			}
			wp[0] = particles[i].posLife[0];
			wp[1] = particles[i].posLife[1];
			wp[2] = particles[i].posLife[2];
			DIST_ProjectToScreen( wp, &sx, &sy, &depth );
			if ( sx < 0.0f || sx > 1.0f || sy < 0.0f || sy > 1.0f ) {
				continue;
			}
			px = (uint32_t)( sx * (float)dist.width );
			py = (uint32_t)( sy * (float)dist.height );
			if ( px >= dist.width || py >= dist.height ) {
				continue;
			}
			staging[count * 4 + 0] = (uint16_t)( 16384 + (int)( crandom() * 8192.0f ) );
			staging[count * 4 + 1] = (uint16_t)( 16384 + (int)( crandom() * 8192.0f ) );
			staging[count * 4 + 2] = 0x3333;
			staging[count * 4 + 3] = 0;
			Com_Memset( &region, 0, sizeof( region ) );
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.layerCount = 1;
			region.imageExtent.width = 1;
			region.imageExtent.height = 1;
			region.imageExtent.depth = 1;
			region.imageOffset.x = (int32_t)px;
			region.imageOffset.y = (int32_t)py;
			region.bufferOffset = (VkDeviceSize)count * 4u * sizeof( uint16_t );
			region.bufferRowLength = 1;
			region.bufferImageHeight = 1;
			qvkCmdCopyBufferToImage( cmd, stagingBuf, dist.distort.image,
				VK_IMAGE_LAYOUT_GENERAL, 1, &region );
			count++;
		}
	}

	qvkUnmapMemory( vk.device, stagingMem );
	qvkDestroyBuffer( vk.device, stagingBuf, NULL );
	qvkFreeMemory( vk.device, stagingMem, NULL );
}

static void DIST_Status_f( void )
{
	ri.Printf( PRINT_ALL,
		"[DIST] ready=%d active=%d size=%ux%u pulse=%.2f\n",
		dist.ready ? 1 : 0, vk_distortion_active() ? 1 : 0,
		dist.width, dist.height, dist.pulseStrength );
}

static void DIST_Pulse_f( void )
{
	dist.pulseActive = qtrue;
	dist.pulseStrength = 1.0f;
	dist.pulseRadius = 96.0f;
	ri.Printf( PRINT_ALL, "[DIST] distortion pulse queued at view origin\n" );
}

static qboolean DIST_ReactiveStamp( void )
{
	if ( vk_reactive_mask_active() &&
		( ( r_temporalReactiveMask && r_temporalReactiveMask->integer ) || VK_RasterUltra_Active() ) ) {
		return qtrue;
	}
	return qfalse;
}

void vk_distortion_init( void )
{
	DIST_RegisterCvars();
	if ( dist.ready || !vk.device || !vk.fboActive ) {
		return;
	}
	if ( !r_distortion || !r_distortion->integer ) {
		return;
	}
	if ( !DIST_CreatePipelines() ) {
		DIST_DestroyPipelines();
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[DIST] pipeline create failed — distortion unavailable\n" S_COLOR_WHITE );
		return;
	}
	if ( ri.Cmd_AddCommand ) {
		ri.Cmd_AddCommand( "distortion_status", DIST_Status_f );
		ri.Cmd_AddCommand( "distortion_pulse", DIST_Pulse_f );
	}
	dist.ready = qtrue;
	ri.Printf( PRINT_ALL,
		"[DIST] Raster Ultra distortion initialized (r_distortion=%d; samples HDR scene only)\n",
		r_distortion->integer );
}

void vk_distortion_shutdown( void )
{
	if ( ri.Cmd_RemoveCommand ) {
		ri.Cmd_RemoveCommand( "distortion_status" );
		ri.Cmd_RemoveCommand( "distortion_pulse" );
	}
	DIST_DestroyPipelines();
	DIST_DestroyImage( &dist.distort );
	Com_Memset( &dist, 0, sizeof( dist ) );
}

void vk_distortion_frame_begin( void )
{
	VkCommandBuffer cmd;
	uint32_t w, h;

	DIST_RegisterCvars();
	if ( !dist.ready && vk.device && vk.fboActive && r_distortion && r_distortion->integer ) {
		vk_distortion_init();
	}
	if ( !dist.ready ) {
		return;
	}
	dist.appliedThisFrame = qfalse;
	dist.frameIndex++;
	w = vk_get_render_target_width();
	h = vk_get_render_target_height();
	if ( !w || !h ) {
		return;
	}
	if ( !DIST_EnsureImage( w, h ) ) {
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}
	if ( vk.inRenderPass ) {
		vk_end_render_pass();
	}
	cmd = vk.cmd->command_buffer;
	DIST_SeedHeat( cmd );
	DIST_Transition( cmd, &dist.distort, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
}

qboolean vk_distortion_active( void )
{
	DIST_RegisterCvars();
	return ( dist.ready && r_distortion && r_distortion->integer && dist.distort.image ) ? qtrue : qfalse;
}

void vk_distortion_apply( void )
{
	VkCommandBuffer cmd;
	VkImageAspectFlags depthAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	VkImageView depthView, reactiveView;
	VkSampler linear;
	VkDescriptorImageInfo imgInfos[5];
	VkWriteDescriptorSet writes[5];
	uint32_t gx, gy, debugMode, stampReactive;

	struct {
		uint32_t extentMeta[4];
		float params0[4];
	} push;

	if ( !vk_distortion_active() ) {
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}
	if ( vk.color_image == VK_NULL_HANDLE || vk.depth_image == VK_NULL_HANDLE ) {
		return;
	}
	if ( !dist.width || !dist.height ) {
		return;
	}
	if ( vk.inRenderPass ) {
		vk_end_render_pass();
	}

	cmd = vk.cmd->command_buffer;
	depthView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	reactiveView = vk.reactive_mask_view ? vk.reactive_mask_view : vk.reactive_mask_stub_view;
	linear = DIST_Sampler();
	stampReactive = DIST_ReactiveStamp();
	debugMode = r_distortionDebug ? (uint32_t)r_distortionDebug->integer : 0u;

	if ( glConfig.stencilBits > 0 ) {
		depthAspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	record_depth_image_layout_transition( cmd, depthAspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	record_image_layout_transition( cmd, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		0, 0 );
	DIST_Transition( cmd, &dist.distort, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	if ( stampReactive && reactiveView ) {
		vk_barrier_reactive_mask_for_storage( "distortion-apply" );
	}

	Com_Memset( imgInfos, 0, sizeof( imgInfos ) );
	/* Same-image sample+store: both must use GENERAL once color is GENERAL.
	 * SHADER_READ sample layout against a GENERAL image produced tile/band UB. */
	imgInfos[0].sampler = linear;
	imgInfos[0].imageView = vk.color_image_view;
	imgInfos[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imgInfos[1].sampler = linear;
	imgInfos[1].imageView = depthView;
	imgInfos[1].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	imgInfos[2].sampler = linear;
	imgInfos[2].imageView = dist.distort.view;
	imgInfos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imgInfos[3].imageView = vk.color_image_view;
	imgInfos[3].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imgInfos[4].imageView = reactiveView ? reactiveView : vk.color_image_view;
	imgInfos[4].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = dist.applySet;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &imgInfos[0];
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = dist.applySet;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].pImageInfo = &imgInfos[1];
	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = dist.applySet;
	writes[2].dstBinding = 2;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[2].pImageInfo = &imgInfos[2];
	writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[3].dstSet = dist.applySet;
	writes[3].dstBinding = 3;
	writes[3].descriptorCount = 1;
	writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[3].pImageInfo = &imgInfos[3];
	writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[4].dstSet = dist.applySet;
	writes[4].dstBinding = 4;
	writes[4].descriptorCount = 1;
	writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[4].pImageInfo = &imgInfos[4];
	qvkUpdateDescriptorSets( vk.device, 5, writes, 0, NULL );

	Com_Memset( &push, 0, sizeof( push ) );
	push.extentMeta[0] = dist.width;
	push.extentMeta[1] = dist.height;
	push.extentMeta[2] = debugMode;
	push.extentMeta[3] = stampReactive ? 1u : 0u;
	push.params0[0] = r_distortionMaxPx ? r_distortionMaxPx->value : 8.0f;
	push.params0[1] = r_distortionDepthTol ? r_distortionDepthTol->value : 0.02f;

	vk_spine_note_write( VK_SPINE_RES_HDR_COLOR, VK_SPINE_PASS_NONE, VK_SPINE_ACCESS_COLOR_WRITE );
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, dist.applyPipe );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, dist.applyPL, 0, 1, &dist.applySet, 0, NULL );
	qvkCmdPushConstants( cmd, dist.applyPL, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );
	gx = ( dist.width + 7u ) / 8u;
	gy = ( dist.height + 7u ) / 8u;
	qvkCmdDispatch( cmd, gx, gy, 1 );

	record_image_layout_transition( cmd, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0, 0 );
	record_depth_image_layout_transition( cmd, depthAspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT );
	if ( stampReactive ) {
		vk_barrier_reactive_mask_for_sampling( "distortion-apply" );
	}

	dist.appliedThisFrame = qtrue;
}
