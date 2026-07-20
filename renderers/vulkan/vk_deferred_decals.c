/*
===========================================================================
Raster Ultra 1.4 — deferred decals (G-buffer material property modification).
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_deferred_decals.h"
#include "vk_util.h"
#include "vk_image_layout.h"
#include "vk_view_state.h"
#include "vk_deferred_gbuffer.h"
#include "vk_pass_registry.h"
#include "vk_raster_ultra.h"

#include <math.h>

#define DD_MAX_DECALS 64

typedef struct {
	float centerRadius[4];
	float axisOpacity[4];
	float albedo[4];
	float props[4];
} ddDecalGpu_t;

typedef struct {
	qboolean ready;
	qboolean appliedThisFrame;
	qboolean fallbackLogged;
	qboolean formatsOk;
	uint32_t frameIndex;
	uint32_t decalCount;

	VkBuffer decalBuffer;
	VkDeviceMemory decalMemory;
	void *decalMapped;
	VkDeviceSize decalBufferSize;

	VkShaderModule applyCS;
	VkDescriptorSetLayout applyLayout;
	VkPipelineLayout applyPL;
	VkPipeline applyPipe;
	VkDescriptorPool descriptorPool;
	VkDescriptorSet applySet;
} ddState_t;

static ddState_t dd;

static cvar_t *r_deferredDecals;

#include "vk_raster_fx_spirv.inc"

static void DD_RegisterCvars( void )
{
	if ( r_deferredDecals ) {
		return;
	}
	r_deferredDecals = ri.Cvar_Get( "r_deferredDecals", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_deferredDecals, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_deferredDecals,
		"Raster Ultra 1.4 deferred decals (latched).\n"
		" 0 - off (default)\n"
		" 1 - compute decal stamp into G-buffer before deferred lighting\n"
		"Requires HDR G-buffer with STORAGE usage (Raster Ultra 1.4)." );
	ri.Cvar_SetGroup( r_deferredDecals, CVG_RENDERER );
}

static VkSampler DD_Sampler( void )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;
	return vk_find_sampler( &sd );
}

static VkShaderModule DD_Module( const uint8_t *bytes, uint32_t size, const char *name )
{
	VkShaderModuleCreateInfo ci;
	VkShaderModule module = VK_NULL_HANDLE;

	Com_Memset( &ci, 0, sizeof( ci ) );
	ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.codeSize = size;
	ci.pCode = (const uint32_t *)bytes;
	if ( qvkCreateShaderModule( vk.device, &ci, NULL, &module ) != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[DD] CreateShaderModule(%s) failed\n" S_COLOR_WHITE, name );
		return VK_NULL_HANDLE;
	}
	SET_OBJECT_NAME( module, name, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	return module;
}

static qboolean DD_GbufferFormatsOk( void )
{
	if ( !vk_deferred_gbuffer_fill_wanted() ) {
		return qfalse;
	}
	if ( vk.deferred_gbuffer_albedo == VK_NULL_HANDLE ||
		vk.deferred_gbuffer_normal == VK_NULL_HANDLE ||
		vk.deferred_gbuffer_material == VK_NULL_HANDLE ) {
		return qfalse;
	}
	/* Shader uses rgba16f image loads — require float16 G-buffer albedo. */
	if ( vk.color_format != VK_FORMAT_R16G16B16A16_SFLOAT ) {
		if ( !dd.fallbackLogged ) {
			ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
				"[DD] deferred decals need R16G16B16A16 albedo (have fmt=%d) — skipping\n" S_COLOR_WHITE,
				(int)vk.color_format );
			dd.fallbackLogged = qtrue;
		}
		return qfalse;
	}
	dd.formatsOk = qtrue;
	return qtrue;
}

static void DD_DestroyPipelines( void )
{
#define DD_DESTROY( fn, x ) do { if ( dd.x ) { fn( vk.device, dd.x, NULL ); dd.x = VK_NULL_HANDLE; } } while ( 0 )
	DD_DESTROY( qvkDestroyPipeline, applyPipe );
	DD_DESTROY( qvkDestroyPipelineLayout, applyPL );
	DD_DESTROY( qvkDestroyDescriptorSetLayout, applyLayout );
	DD_DESTROY( qvkDestroyShaderModule, applyCS );
	DD_DESTROY( qvkDestroyDescriptorPool, descriptorPool );
#undef DD_DESTROY
	dd.applySet = VK_NULL_HANDLE;
}

static void DD_DestroyBuffer( void )
{
	if ( dd.decalMapped ) {
		qvkUnmapMemory( vk.device, dd.decalMemory );
		dd.decalMapped = NULL;
	}
	if ( dd.decalBuffer ) {
		qvkDestroyBuffer( vk.device, dd.decalBuffer, NULL );
		dd.decalBuffer = VK_NULL_HANDLE;
	}
	if ( dd.decalMemory ) {
		qvkFreeMemory( vk.device, dd.decalMemory, NULL );
		dd.decalMemory = VK_NULL_HANDLE;
	}
	dd.decalBufferSize = 0;
	dd.decalCount = 0;
}

static qboolean DD_EnsureBuffer( void )
{
	VkBufferCreateInfo bci;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo mai;
	VkDeviceSize size = (VkDeviceSize)DD_MAX_DECALS * sizeof( ddDecalGpu_t );

	if ( dd.decalMapped && dd.decalBufferSize >= size ) {
		return qtrue;
	}
	DD_DestroyBuffer();
	Com_Memset( &bci, 0, sizeof( bci ) );
	bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.size = size;
	bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if ( qvkCreateBuffer( vk.device, &bci, NULL, &dd.decalBuffer ) != VK_SUCCESS ) {
		return qfalse;
	}
	qvkGetBufferMemoryRequirements( vk.device, dd.decalBuffer, &req );
	Com_Memset( &mai, 0, sizeof( mai ) );
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.allocationSize = req.size;
	mai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	if ( qvkAllocateMemory( vk.device, &mai, NULL, &dd.decalMemory ) != VK_SUCCESS ||
		qvkBindBufferMemory( vk.device, dd.decalBuffer, dd.decalMemory, 0 ) != VK_SUCCESS ||
		qvkMapMemory( vk.device, dd.decalMemory, 0, size, 0, &dd.decalMapped ) != VK_SUCCESS ) {
		DD_DestroyBuffer();
		return qfalse;
	}
	dd.decalBufferSize = size;
	Com_Memset( dd.decalMapped, 0, (size_t)size );
	SET_OBJECT_NAME( dd.decalBuffer, "DD decal SSBO", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	return qtrue;
}

static qboolean DD_CreatePipelines( void )
{
	static const VkDescriptorType types[] = {
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
	};
	VkDescriptorSetLayoutBinding bindings[6];
	VkDescriptorSetLayoutCreateInfo dci;
	VkPushConstantRange range;
	VkPipelineLayoutCreateInfo pci;
	VkComputePipelineCreateInfo ci;
	VkDescriptorPoolSize poolSizes[3];
	VkDescriptorPoolCreateInfo poolCI;
	VkDescriptorSetAllocateInfo ai;
	uint32_t i;

	DD_DestroyPipelines();
	dd.applyCS = DD_Module( vk_dd_apply_cs_spv, VK_DD_APPLY_CS_SPV_SIZE, "dd_apply_cs" );
	if ( !dd.applyCS ) {
		return qfalse;
	}
	Com_Memset( bindings, 0, sizeof( bindings ) );
	for ( i = 0; i < 6; ++i ) {
		bindings[i].binding = i;
		bindings[i].descriptorType = types[i];
		bindings[i].descriptorCount = 1;
		bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	}
	Com_Memset( &dci, 0, sizeof( dci ) );
	dci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dci.bindingCount = 6;
	dci.pBindings = bindings;
	if ( qvkCreateDescriptorSetLayout( vk.device, &dci, NULL, &dd.applyLayout ) != VK_SUCCESS ) {
		return qfalse;
	}
	Com_Memset( &range, 0, sizeof( range ) );
	range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	range.size = 96;
	Com_Memset( &pci, 0, sizeof( pci ) );
	pci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pci.setLayoutCount = 1;
	pci.pSetLayouts = &dd.applyLayout;
	pci.pushConstantRangeCount = 1;
	pci.pPushConstantRanges = &range;
	if ( qvkCreatePipelineLayout( vk.device, &pci, NULL, &dd.applyPL ) != VK_SUCCESS ) {
		return qfalse;
	}
	Com_Memset( &ci, 0, sizeof( ci ) );
	ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	ci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	ci.stage.module = dd.applyCS;
	ci.stage.pName = "main";
	ci.layout = dd.applyPL;
	if ( qvkCreateComputePipelines( vk.device, vk.pipelineCache, 1, &ci, NULL, &dd.applyPipe ) != VK_SUCCESS ) {
		return qfalse;
	}
	SET_OBJECT_NAME( dd.applyPipe, "dd_apply", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	Com_Memset( poolSizes, 0, sizeof( poolSizes ) );
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[0].descriptorCount = 1;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = 2;
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSizes[2].descriptorCount = 3;
	Com_Memset( &poolCI, 0, sizeof( poolCI ) );
	poolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCI.maxSets = 1;
	poolCI.poolSizeCount = 3;
	poolCI.pPoolSizes = poolSizes;
	if ( qvkCreateDescriptorPool( vk.device, &poolCI, NULL, &dd.descriptorPool ) != VK_SUCCESS ) {
		return qfalse;
	}
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	ai.descriptorPool = dd.descriptorPool;
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &dd.applyLayout;
	if ( qvkAllocateDescriptorSets( vk.device, &ai, &dd.applySet ) != VK_SUCCESS ) {
		return qfalse;
	}
	return DD_EnsureBuffer();
}

static void DD_SpawnDecalAtView( void )
{
	ddDecalGpu_t *d;
	vec3_t origin, forward;

	if ( !dd.decalMapped || dd.decalCount >= DD_MAX_DECALS ) {
		return;
	}
	d = ( (ddDecalGpu_t *)dd.decalMapped ) + dd.decalCount;
	VectorCopy( backEnd.refdef.vieworg, origin );
	VectorCopy( backEnd.refdef.viewaxis[0], forward );
	Com_Memset( d, 0, sizeof( *d ) );
	d->centerRadius[0] = origin[0] + forward[0] * 96.0f;
	d->centerRadius[1] = origin[1] + forward[1] * 96.0f;
	d->centerRadius[2] = origin[2] + forward[2] * 96.0f - 8.0f;
	d->centerRadius[3] = 64.0f;
	d->axisOpacity[0] = -forward[0];
	d->axisOpacity[1] = -forward[1];
	d->axisOpacity[2] = -forward[2];
	d->axisOpacity[3] = 0.85f;
	d->albedo[0] = 0.15f;
	d->albedo[1] = 0.55f;
	d->albedo[2] = 0.12f;
	d->props[0] = 0.15f;
	d->props[1] = 0.05f;
	d->props[2] = 0.0f;
	d->props[3] = (float)dd.decalCount;
	dd.decalCount++;
}

static void DD_Status_f( void )
{
	ri.Printf( PRINT_ALL,
		"[DD] ready=%d active=%d decals=%u formatsOk=%d gbufGen=%u\n",
		dd.ready ? 1 : 0, vk_deferred_decals_active() ? 1 : 0,
		dd.decalCount, dd.formatsOk ? 1 : 0, vk_deferred_gbuffer_generation() );
}

static void DD_Spawn_f( void )
{
	DD_RegisterCvars();
	if ( !DD_EnsureBuffer() ) {
		ri.Printf( PRINT_ALL, "[DD] buffer unavailable\n" );
		return;
	}
	DD_SpawnDecalAtView();
	ri.Printf( PRINT_ALL, "[DD] spawned decal #%u at view aim\n", dd.decalCount );
}

void vk_deferred_decals_init( void )
{
	DD_RegisterCvars();
	if ( dd.ready || !vk.device || !vk.fboActive ) {
		return;
	}
	if ( !r_deferredDecals || !r_deferredDecals->integer ) {
		return;
	}
	if ( !DD_CreatePipelines() ) {
		DD_DestroyPipelines();
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[DD] pipeline create failed — deferred decals unavailable\n" S_COLOR_WHITE );
		return;
	}
	if ( ri.Cmd_AddCommand ) {
		ri.Cmd_AddCommand( "deferred_decal_status", DD_Status_f );
		ri.Cmd_AddCommand( "deferred_decal_spawn", DD_Spawn_f );
	}
	dd.ready = qtrue;
	dd.formatsOk = DD_GbufferFormatsOk();
	ri.Printf( PRINT_ALL,
		"[DD] Raster Ultra deferred decals initialized (r_deferredDecals=%d formatsOk=%d)\n",
		r_deferredDecals->integer, dd.formatsOk ? 1 : 0 );
}

void vk_deferred_decals_shutdown( void )
{
	if ( ri.Cmd_RemoveCommand ) {
		ri.Cmd_RemoveCommand( "deferred_decal_status" );
		ri.Cmd_RemoveCommand( "deferred_decal_spawn" );
	}
	DD_DestroyPipelines();
	DD_DestroyBuffer();
	Com_Memset( &dd, 0, sizeof( dd ) );
}

void vk_deferred_decals_frame_begin( void )
{
	DD_RegisterCvars();
	if ( !dd.ready && vk.device && vk.fboActive && r_deferredDecals && r_deferredDecals->integer ) {
		vk_deferred_decals_init();
	}
	if ( !dd.ready ) {
		return;
	}
	dd.appliedThisFrame = qfalse;
	dd.frameIndex++;
	dd.formatsOk = DD_GbufferFormatsOk();
}

qboolean vk_deferred_decals_active( void )
{
	DD_RegisterCvars();
	return ( dd.ready && r_deferredDecals && r_deferredDecals->integer && dd.formatsOk ) ? qtrue : qfalse;
}

void vk_deferred_decals_apply_to_gbuffer( void )
{
	VkCommandBuffer cmd;
	VkImageAspectFlags depthAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	VkImageView depthView, normalView;
	VkSampler nearest;
	VkDescriptorBufferInfo bufInfo;
	VkDescriptorImageInfo imgInfos[5];
	VkWriteDescriptorSet writes[5];
	uint32_t width, height, gx, gy, normalsAreWorld;
	float invView[16], projInfo[4];

	struct {
		uint32_t extentMeta[4];
		float projInfo[4];
		float invView[16];
	} push;

	if ( !vk_deferred_decals_active() || dd.decalCount == 0 ) {
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}
	if ( !vk_deferred_gbuffer_fill_wanted() ) {
		return;
	}
	if ( vk.inRenderPass ) {
		vk_end_render_pass();
	}

	cmd = vk.cmd->command_buffer;
	width = vk_get_render_target_width();
	height = vk_get_render_target_height();
	if ( !width || !height ) {
		return;
	}

	depthView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	normalView = vk.deferred_gbuffer_normal_view;
	nearest = DD_Sampler();

	if ( glConfig.stencilBits > 0 ) {
		depthAspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	record_depth_image_layout_transition( cmd, depthAspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	record_image_layout_transition( cmd, vk.deferred_gbuffer_albedo, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0 );
	record_image_layout_transition( cmd, vk.deferred_gbuffer_normal, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0 );
	record_image_layout_transition( cmd, vk.deferred_gbuffer_material, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0 );

	{
		const float *viewMat = backEnd.viewParms.world.modelViewMatrix;
		const float *projection = backEnd.useFirstPersonProjection ?
			backEnd.firstPersonProjectionMatrix : backEnd.viewParms.projectionMatrix;
		float projVK[16];
		if ( !vk_mat4_inverse( viewMat, invView ) ) {
			Com_Memcpy( invView, viewMat, sizeof( float ) * 16 );
		}
		vk_get_projection_matrix_vk( projection, projVK );
		projInfo[0] = projVK[0] != 0.0f ? 1.0f / projVK[0] : 1.0f;
		projInfo[1] = projVK[5] != 0.0f ? 1.0f / projVK[5] : 1.0f;
		projInfo[2] = projVK[10];
		projInfo[3] = projVK[14];
	}
	normalsAreWorld = vk.deferredGbufferDirectExport ? 1u : 0u;

	Com_Memset( &bufInfo, 0, sizeof( bufInfo ) );
	bufInfo.buffer = dd.decalBuffer;
	bufInfo.offset = 0;
	bufInfo.range = dd.decalBufferSize;
	Com_Memset( imgInfos, 0, sizeof( imgInfos ) );
	imgInfos[0].sampler = nearest;
	imgInfos[0].imageView = depthView;
	imgInfos[0].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	imgInfos[1].sampler = nearest;
	imgInfos[1].imageView = normalView;
	imgInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imgInfos[2].imageView = vk.deferred_gbuffer_albedo_view;
	imgInfos[2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imgInfos[3].imageView = vk.deferred_gbuffer_normal_view;
	imgInfos[3].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imgInfos[4].imageView = vk.deferred_gbuffer_material_view;
	imgInfos[4].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = dd.applySet;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[0].pBufferInfo = &bufInfo;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = dd.applySet;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].pImageInfo = &imgInfos[0];
	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = dd.applySet;
	writes[2].dstBinding = 2;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[2].pImageInfo = &imgInfos[1];
	writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[3].dstSet = dd.applySet;
	writes[3].dstBinding = 3;
	writes[3].descriptorCount = 1;
	writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[3].pImageInfo = &imgInfos[2];
	writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[4].dstSet = dd.applySet;
	writes[4].dstBinding = 4;
	writes[4].descriptorCount = 1;
	writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[4].pImageInfo = &imgInfos[3];
	/* binding 5 material — reuse writes array slot via second batch */
	qvkUpdateDescriptorSets( vk.device, 5, writes, 0, NULL );
	writes[0].dstBinding = 5;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[0].pImageInfo = &imgInfos[4];
	qvkUpdateDescriptorSets( vk.device, 1, writes, 0, NULL );

	Com_Memset( &push, 0, sizeof( push ) );
	push.extentMeta[0] = width;
	push.extentMeta[1] = height;
	push.extentMeta[2] = dd.decalCount;
	push.extentMeta[3] = normalsAreWorld;
	Com_Memcpy( push.projInfo, projInfo, sizeof( projInfo ) );
	Com_Memcpy( push.invView, invView, sizeof( invView ) );

	vk_spine_note_write( VK_SPINE_RES_GBUFFER_ALBEDO, VK_SPINE_PASS_GBUFFER_FILL, VK_SPINE_ACCESS_STORAGE_WRITE );
	vk_spine_note_write( VK_SPINE_RES_GBUFFER_NORMAL, VK_SPINE_PASS_GBUFFER_FILL, VK_SPINE_ACCESS_STORAGE_WRITE );
	vk_spine_note_write( VK_SPINE_RES_GBUFFER_MATERIAL, VK_SPINE_PASS_GBUFFER_FILL, VK_SPINE_ACCESS_STORAGE_WRITE );
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, dd.applyPipe );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, dd.applyPL, 0, 1, &dd.applySet, 0, NULL );
	qvkCmdPushConstants( cmd, dd.applyPL, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );
	gx = ( width + 7u ) / 8u;
	gy = ( height + 7u ) / 8u;
	qvkCmdDispatch( cmd, gx, gy, 1 );

	record_image_layout_transition( cmd, vk.deferred_gbuffer_albedo, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	record_image_layout_transition( cmd, vk.deferred_gbuffer_normal, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	record_image_layout_transition( cmd, vk.deferred_gbuffer_material, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	record_depth_image_layout_transition( cmd, depthAspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT );

	dd.appliedThisFrame = qtrue;
}
