/*
===========================================================================
Raster Ultra 1.4 — GPU particles (compute sim + soft depth-aware splat).
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_gpu_particles.h"
#include "vk_util.h"
#include "vk_image_layout.h"
#include "vk_view_state.h"
#include "vk_deferred_gbuffer.h"
#include "vk_reactive_mask.h"
#include "vk_pass_registry.h"
#include "vk_raster_ultra.h"

#include <math.h>

#define GP_MAX_PARTICLES VK_GP_MAX_PARTICLES

typedef vk_gp_particle_t gpParticleGpu_t;

typedef struct {
	qboolean ready;
	qboolean appliedThisFrame;
	uint32_t frameIndex;
	uint32_t width, height;
	uint32_t liveCount;
	uint32_t spawnCursor;

	VkBuffer particleBuffer;
	VkDeviceMemory particleMemory;
	void *particleMapped;
	VkDeviceSize particleBufferSize;

	VkShaderModule updateCS, splatCS;
	VkDescriptorSetLayout updateLayout, splatLayout;
	VkPipelineLayout updatePL, splatPL;
	VkPipeline updatePipe, splatPipe;
	VkDescriptorPool descriptorPool;
	VkDescriptorSet updateSet, splatSet;
} gpState_t;

static gpState_t gp;

static cvar_t *r_gpuParticles;
static cvar_t *r_gpuParticlesDemo;
static cvar_t *r_gpuParticlesSoftScale;
static cvar_t *r_gpuParticlesEmissiveBoost;
static cvar_t *r_gpuParticlesGravity;
static cvar_t *r_gpuParticlesDrag;

#include "vk_raster_fx_spirv.inc"

static void GP_RegisterCvars( void )
{
	if ( r_gpuParticles ) {
		return;
	}
	r_gpuParticles = ri.Cvar_Get( "r_gpuParticles", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_gpuParticles, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_gpuParticles,
		"Raster Ultra 1.4 GPU particle sim + soft splat (latched, raster-only).\n"
		" 0 - off (default)\n"
		" 1 - compute update + depth-aware HDR splat\n"
		"Enable via: exec modern_raster_ultra.cfg; vid_restart" );
	ri.Cvar_SetGroup( r_gpuParticles, CVG_RENDERER );

	r_gpuParticlesDemo = ri.Cvar_Get( "r_gpuParticlesDemo", "0", CVAR_ARCHIVE_ND );
	r_gpuParticlesSoftScale = ri.Cvar_Get( "r_gpuParticlesSoftScale", "1", CVAR_ARCHIVE_ND );
	r_gpuParticlesEmissiveBoost = ri.Cvar_Get( "r_gpuParticlesEmissiveBoost", "2", CVAR_ARCHIVE_ND );
	r_gpuParticlesGravity = ri.Cvar_Get( "r_gpuParticlesGravity", "800", CVAR_ARCHIVE_ND );
	r_gpuParticlesDrag = ri.Cvar_Get( "r_gpuParticlesDrag", "0.35", CVAR_ARCHIVE_ND );
}

static VkSampler GP_Sampler( qboolean linear )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = linear ? GL_LINEAR : GL_NEAREST;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;
	return vk_find_sampler( &sd );
}

static VkShaderModule GP_Module( const uint8_t *bytes, uint32_t size, const char *name )
{
	VkShaderModuleCreateInfo ci;
	VkShaderModule module = VK_NULL_HANDLE;
	VkResult res;

	Com_Memset( &ci, 0, sizeof( ci ) );
	ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.codeSize = size;
	ci.pCode = (const uint32_t *)bytes;
	res = qvkCreateShaderModule( vk.device, &ci, NULL, &module );
	if ( res != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[GP] CreateShaderModule(%s) failed: %s\n" S_COLOR_WHITE, name, vk_result_string( res ) );
		return VK_NULL_HANDLE;
	}
	SET_OBJECT_NAME( module, name, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	return module;
}

static qboolean GP_CreateLayout( const VkDescriptorType *types, uint32_t count,
	uint32_t pushSize, VkDescriptorSetLayout *setLayout, VkPipelineLayout *pipelineLayout )
{
	VkDescriptorSetLayoutBinding bindings[8];
	VkDescriptorSetLayoutCreateInfo dci;
	VkPushConstantRange range;
	VkPipelineLayoutCreateInfo pci;
	uint32_t i;

	Com_Memset( bindings, 0, sizeof( bindings ) );
	for ( i = 0; i < count; ++i ) {
		bindings[i].binding = i;
		bindings[i].descriptorType = types[i];
		bindings[i].descriptorCount = 1;
		bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	}
	Com_Memset( &dci, 0, sizeof( dci ) );
	dci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dci.bindingCount = count;
	dci.pBindings = bindings;
	if ( qvkCreateDescriptorSetLayout( vk.device, &dci, NULL, setLayout ) != VK_SUCCESS ) {
		return qfalse;
	}
	Com_Memset( &range, 0, sizeof( range ) );
	range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	range.size = pushSize;
	Com_Memset( &pci, 0, sizeof( pci ) );
	pci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pci.setLayoutCount = 1;
	pci.pSetLayouts = setLayout;
	pci.pushConstantRangeCount = 1;
	pci.pPushConstantRanges = &range;
	if ( qvkCreatePipelineLayout( vk.device, &pci, NULL, pipelineLayout ) != VK_SUCCESS ) {
		qvkDestroyDescriptorSetLayout( vk.device, *setLayout, NULL );
		*setLayout = VK_NULL_HANDLE;
		return qfalse;
	}
	return qtrue;
}

static VkPipeline GP_CreateComputePipeline( VkShaderModule module, VkPipelineLayout layout, const char *name )
{
	VkComputePipelineCreateInfo ci;
	VkPipeline pipeline = VK_NULL_HANDLE;

	if ( !module || !layout ) {
		return VK_NULL_HANDLE;
	}
	Com_Memset( &ci, 0, sizeof( ci ) );
	ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	ci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	ci.stage.module = module;
	ci.stage.pName = "main";
	ci.layout = layout;
	if ( qvkCreateComputePipelines( vk.device, vk.pipelineCache, 1, &ci, NULL, &pipeline ) != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[GP] CreateComputePipelines(%s) failed\n" S_COLOR_WHITE, name );
		return VK_NULL_HANDLE;
	}
	SET_OBJECT_NAME( pipeline, name, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
	return pipeline;
}

static void GP_DestroyPipelines( void )
{
#define GP_DESTROY( fn, x ) do { if ( gp.x ) { fn( vk.device, gp.x, NULL ); gp.x = VK_NULL_HANDLE; } } while ( 0 )
	GP_DESTROY( qvkDestroyPipeline, updatePipe );
	GP_DESTROY( qvkDestroyPipeline, splatPipe );
	GP_DESTROY( qvkDestroyPipelineLayout, updatePL );
	GP_DESTROY( qvkDestroyPipelineLayout, splatPL );
	GP_DESTROY( qvkDestroyDescriptorSetLayout, updateLayout );
	GP_DESTROY( qvkDestroyDescriptorSetLayout, splatLayout );
	GP_DESTROY( qvkDestroyShaderModule, updateCS );
	GP_DESTROY( qvkDestroyShaderModule, splatCS );
	GP_DESTROY( qvkDestroyDescriptorPool, descriptorPool );
#undef GP_DESTROY
	gp.updateSet = VK_NULL_HANDLE;
	gp.splatSet = VK_NULL_HANDLE;
}

static void GP_DestroyBuffer( void )
{
	if ( gp.particleMapped ) {
		qvkUnmapMemory( vk.device, gp.particleMemory );
		gp.particleMapped = NULL;
	}
	if ( gp.particleBuffer ) {
		qvkDestroyBuffer( vk.device, gp.particleBuffer, NULL );
		gp.particleBuffer = VK_NULL_HANDLE;
	}
	if ( gp.particleMemory ) {
		qvkFreeMemory( vk.device, gp.particleMemory, NULL );
		gp.particleMemory = VK_NULL_HANDLE;
	}
	gp.particleBufferSize = 0;
	gp.liveCount = 0;
}

static qboolean GP_EnsureBuffer( void )
{
	VkBufferCreateInfo bci;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo mai;
	VkDeviceSize size = (VkDeviceSize)GP_MAX_PARTICLES * sizeof( gpParticleGpu_t );

	if ( gp.particleMapped && gp.particleBufferSize >= size ) {
		return qtrue;
	}
	GP_DestroyBuffer();
	Com_Memset( &bci, 0, sizeof( bci ) );
	bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.size = size;
	bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if ( qvkCreateBuffer( vk.device, &bci, NULL, &gp.particleBuffer ) != VK_SUCCESS ) {
		return qfalse;
	}
	qvkGetBufferMemoryRequirements( vk.device, gp.particleBuffer, &req );
	Com_Memset( &mai, 0, sizeof( mai ) );
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.allocationSize = req.size;
	mai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	if ( qvkAllocateMemory( vk.device, &mai, NULL, &gp.particleMemory ) != VK_SUCCESS ||
		qvkBindBufferMemory( vk.device, gp.particleBuffer, gp.particleMemory, 0 ) != VK_SUCCESS ||
		qvkMapMemory( vk.device, gp.particleMemory, 0, size, 0, &gp.particleMapped ) != VK_SUCCESS ) {
		GP_DestroyBuffer();
		return qfalse;
	}
	gp.particleBufferSize = size;
	Com_Memset( gp.particleMapped, 0, (size_t)size );
	SET_OBJECT_NAME( gp.particleBuffer, "GP particle SSBO", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	return qtrue;
}

static qboolean GP_CreatePipelines( void )
{
	static const VkDescriptorType updateTypes[] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER };
	static const VkDescriptorType splatTypes[] = {
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
	};
	VkDescriptorPoolSize poolSizes[4];
	VkDescriptorPoolCreateInfo pci;
	VkDescriptorSetLayout layouts[2];
	VkDescriptorSet sets[2];
	VkDescriptorSetAllocateInfo ai;

	GP_DestroyPipelines();
	gp.updateCS = GP_Module( vk_gp_update_cs_spv, VK_GP_UPDATE_CS_SPV_SIZE, "gp_update_cs" );
	gp.splatCS = GP_Module( vk_gp_soft_splat_cs_spv, VK_GP_SOFT_SPLAT_CS_SPV_SIZE, "gp_soft_splat_cs" );
	if ( !gp.updateCS || !gp.splatCS ) {
		return qfalse;
	}
	if ( !GP_CreateLayout( updateTypes, 1, 32, &gp.updateLayout, &gp.updatePL ) ||
		!GP_CreateLayout( splatTypes, 4, 160, &gp.splatLayout, &gp.splatPL ) ) {
		return qfalse;
	}
	gp.updatePipe = GP_CreateComputePipeline( gp.updateCS, gp.updatePL, "gp_update" );
	gp.splatPipe = GP_CreateComputePipeline( gp.splatCS, gp.splatPL, "gp_soft_splat" );
	if ( !gp.updatePipe || !gp.splatPipe ) {
		return qfalse;
	}
	Com_Memset( poolSizes, 0, sizeof( poolSizes ) );
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[0].descriptorCount = 2;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = 1;
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSizes[2].descriptorCount = 2;
	Com_Memset( &pci, 0, sizeof( pci ) );
	pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pci.maxSets = 2;
	pci.poolSizeCount = 3;
	pci.pPoolSizes = poolSizes;
	if ( qvkCreateDescriptorPool( vk.device, &pci, NULL, &gp.descriptorPool ) != VK_SUCCESS ) {
		return qfalse;
	}
	layouts[0] = gp.updateLayout;
	layouts[1] = gp.splatLayout;
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	ai.descriptorPool = gp.descriptorPool;
	ai.descriptorSetCount = 2;
	ai.pSetLayouts = layouts;
	if ( qvkAllocateDescriptorSets( vk.device, &ai, sets ) != VK_SUCCESS ) {
		return qfalse;
	}
	gp.updateSet = sets[0];
	gp.splatSet = sets[1];
	return GP_EnsureBuffer();
}

static void GP_SpawnBurst( int count, qboolean smoke )
{
	gpParticleGpu_t *p;
	vec3_t origin, forward, right, up;
	int i, base;
	float spread;

	if ( !gp.particleMapped || count <= 0 ) {
		return;
	}
	if ( count > GP_MAX_PARTICLES ) {
		count = GP_MAX_PARTICLES;
	}
	VectorCopy( backEnd.refdef.vieworg, origin );
	VectorCopy( backEnd.refdef.viewaxis[0], forward );
	VectorCopy( backEnd.refdef.viewaxis[1], right );
	VectorCopy( backEnd.refdef.viewaxis[2], up );
	base = (int)gp.spawnCursor;
	p = (gpParticleGpu_t *)gp.particleMapped;
	spread = smoke ? 48.0f : 24.0f;

	for ( i = 0; i < count; ++i ) {
		int idx = ( base + i ) % GP_MAX_PARTICLES;
		float t = (float)i / (float)count;
		float rx = crandom() * spread;
		float ry = crandom() * spread;
		float rz = crandom() * spread * 0.5f;
		Com_Memset( &p[idx], 0, sizeof( p[idx] ) );
		p[idx].posLife[0] = origin[0] + forward[0] * 32.0f + right[0] * rx + up[0] * ry;
		p[idx].posLife[1] = origin[1] + forward[1] * 32.0f + right[1] * rx + up[1] * ry;
		p[idx].posLife[2] = origin[2] + forward[2] * 32.0f + right[2] * rx + up[2] * rz;
		p[idx].posLife[3] = 1.0f;
		p[idx].velSize[0] = forward[0] * ( smoke ? 40.0f : 120.0f ) + crandom() * 30.0f;
		p[idx].velSize[1] = forward[1] * ( smoke ? 40.0f : 120.0f ) + crandom() * 30.0f;
		p[idx].velSize[2] = forward[2] * ( smoke ? 20.0f : 80.0f ) + crandom() * 40.0f + ( smoke ? 60.0f : 0.0f );
		p[idx].velSize[3] = smoke ? 24.0f + t * 16.0f : 8.0f + t * 6.0f;
		if ( smoke ) {
			p[idx].colorEmi[0] = 0.35f;
			p[idx].colorEmi[1] = 0.34f;
			p[idx].colorEmi[2] = 0.32f;
			p[idx].colorEmi[3] = 0.2f;
		} else {
			p[idx].colorEmi[0] = 1.0f;
			p[idx].colorEmi[1] = 0.55f + t * 0.3f;
			p[idx].colorEmi[2] = 0.1f;
			p[idx].colorEmi[3] = 1.5f;
		}
		p[idx].meta[0] = smoke ? 0.0f : 1.0f;
		p[idx].meta[1] = smoke ? 2.5f : 0.8f;
		p[idx].meta[2] = smoke ? 32.0f : 12.0f;
		p[idx].meta[3] = 1.0f;
	}
	gp.spawnCursor = (uint32_t)( ( base + count ) % GP_MAX_PARTICLES );
	gp.liveCount = GP_MAX_PARTICLES;
}

static void GP_SpawnDemo( void )
{
	if ( !r_gpuParticlesDemo || !r_gpuParticlesDemo->integer ) {
		return;
	}
	if ( ( gp.frameIndex % 120u ) == 1u ) {
		GP_SpawnBurst( 24, qtrue );
	}
	if ( ( gp.frameIndex % 180u ) == 60u ) {
		GP_SpawnBurst( 12, qfalse );
	}
}

static void GP_CountLive( void )
{
	gpParticleGpu_t *p = (gpParticleGpu_t *)gp.particleMapped;
	uint32_t i, n = 0;

	if ( !p ) {
		gp.liveCount = 0;
		return;
	}
	for ( i = 0; i < GP_MAX_PARTICLES; ++i ) {
		if ( p[i].posLife[3] > 0.01f ) {
			n++;
		}
	}
	gp.liveCount = n;
}

static void GP_Status_f( void )
{
	ri.Printf( PRINT_ALL,
		"[GP] ready=%d active=%d live=%u frame=%u size=%ux%u\n",
		gp.ready ? 1 : 0, vk_gpu_particles_active() ? 1 : 0,
		gp.liveCount, gp.frameIndex, gp.width, gp.height );
}

static void GP_Burst_f( void )
{
	GP_RegisterCvars();
	if ( !GP_EnsureBuffer() ) {
		ri.Printf( PRINT_ALL, "[GP] buffer unavailable\n" );
		return;
	}
	GP_SpawnBurst( 64, qtrue );
	GP_SpawnBurst( 32, qfalse );
	ri.Printf( PRINT_ALL, "[GP] burst spawned at view origin\n" );
}

static void GP_FillView( float view[16], float proj[16], float projInfo[4] )
{
	const float *viewMat = backEnd.viewParms.world.modelViewMatrix;
	const float *projection = backEnd.useFirstPersonProjection ?
		backEnd.firstPersonProjectionMatrix : backEnd.viewParms.projectionMatrix;
	float projVK[16];

	Com_Memcpy( view, viewMat, sizeof( float ) * 16 );
	vk_get_projection_matrix_vk( projection, projVK );
	Com_Memcpy( proj, projVK, sizeof( float ) * 16 );
	projInfo[0] = projVK[0] != 0.0f ? 1.0f / projVK[0] : 1.0f;
	projInfo[1] = projVK[5] != 0.0f ? 1.0f / projVK[5] : 1.0f;
	projInfo[2] = projVK[10];
	projInfo[3] = projVK[14];
}

static qboolean GP_ReactiveStamp( void )
{
	if ( vk_reactive_mask_active() &&
		( ( r_temporalReactiveMask && r_temporalReactiveMask->integer ) || VK_RasterUltra_Active() ) ) {
		return qtrue;
	}
	return qfalse;
}

void vk_gpu_particles_init( void )
{
	GP_RegisterCvars();
	if ( gp.ready || !vk.device || !vk.fboActive ) {
		return;
	}
	if ( !r_gpuParticles || !r_gpuParticles->integer ) {
		return;
	}
	if ( !GP_CreatePipelines() ) {
		GP_DestroyPipelines();
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[GP] pipeline create failed — GPU particles unavailable\n" S_COLOR_WHITE );
		return;
	}
	if ( ri.Cmd_AddCommand ) {
		ri.Cmd_AddCommand( "gpu_particles_status", GP_Status_f );
		ri.Cmd_AddCommand( "gpu_particles_burst", GP_Burst_f );
	}
	gp.ready = qtrue;
	ri.Printf( PRINT_ALL,
		"[GP] Raster Ultra GPU particles initialized (r_gpuParticles=%d; RT unused)\n",
		r_gpuParticles->integer );
}

void vk_gpu_particles_shutdown( void )
{
	if ( ri.Cmd_RemoveCommand ) {
		ri.Cmd_RemoveCommand( "gpu_particles_status" );
		ri.Cmd_RemoveCommand( "gpu_particles_burst" );
	}
	GP_DestroyPipelines();
	GP_DestroyBuffer();
	Com_Memset( &gp, 0, sizeof( gp ) );
}

void vk_gpu_particles_frame_begin( void )
{
	GP_RegisterCvars();
	if ( !gp.ready && vk.device && vk.fboActive && r_gpuParticles && r_gpuParticles->integer ) {
		vk_gpu_particles_init();
	}
	if ( !gp.ready ) {
		return;
	}
	gp.appliedThisFrame = qfalse;
	gp.frameIndex++;
	gp.width = vk_get_render_target_width();
	gp.height = vk_get_render_target_height();
	GP_SpawnDemo();
}

qboolean vk_gpu_particles_active( void )
{
	GP_RegisterCvars();
	return ( gp.ready && r_gpuParticles && r_gpuParticles->integer ) ? qtrue : qfalse;
}

qboolean vk_gpu_particles_mapped( void **mapped, uint32_t *liveCount )
{
	if ( !vk_gpu_particles_active() || !gp.particleMapped ) {
		return qfalse;
	}
	if ( mapped ) {
		*mapped = gp.particleMapped;
	}
	if ( liveCount ) {
		GP_CountLive();
		*liveCount = gp.liveCount;
	}
	return qtrue;
}

void vk_gpu_particles_apply_after_geometry( void )
{
	VkCommandBuffer cmd;
	VkImageAspectFlags depthAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	VkImageView depthView;
	VkImageView reactiveView;
	VkSampler nearest;
	VkDescriptorBufferInfo bufInfo;
	VkDescriptorImageInfo imgInfos[4];
	VkWriteDescriptorSet writes[4];
	VkImageView sceneView;
	uint32_t gx, gy;
	float view[16], proj[16], projInfo[4];
	qboolean stampReactive;

	struct {
		uint32_t count;
		float dt;
		float gravity;
		float drag;
		float wind[4];
	} updatePush;

	struct {
		uint32_t extentMeta[4];
		float projInfo[4];
		float view[16];
		float proj[16];
		float params0[4];
	} splatPush;

	if ( !vk_gpu_particles_active() ) {
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}
	if ( vk.color_image == VK_NULL_HANDLE || vk.depth_image == VK_NULL_HANDLE ) {
		return;
	}
	if ( !gp.width || !gp.height ) {
		gp.width = vk_get_render_target_width();
		gp.height = vk_get_render_target_height();
	}
	if ( !gp.width || !gp.height ) {
		return;
	}
	if ( vk.inRenderPass ) {
		vk_end_render_pass();
	}

	cmd = vk.cmd->command_buffer;
	GP_CountLive();

	/* Update sim */
	Com_Memset( &updatePush, 0, sizeof( updatePush ) );
	updatePush.count = GP_MAX_PARTICLES;
	{
		static int s_lastRefdefTime;
		updatePush.dt = 0.016f;
		if ( backEnd.refdef.time > s_lastRefdefTime && s_lastRefdefTime > 0 ) {
			updatePush.dt = (float)( backEnd.refdef.time - s_lastRefdefTime ) * 0.001f;
		}
		if ( updatePush.dt <= 0.0f || updatePush.dt > 0.25f ) {
			updatePush.dt = 0.016f;
		}
		s_lastRefdefTime = backEnd.refdef.time;
	}
	updatePush.gravity = r_gpuParticlesGravity ? r_gpuParticlesGravity->value : 800.0f;
	updatePush.drag = r_gpuParticlesDrag ? r_gpuParticlesDrag->value : 0.35f;

	Com_Memset( &bufInfo, 0, sizeof( bufInfo ) );
	bufInfo.buffer = gp.particleBuffer;
	bufInfo.offset = 0;
	bufInfo.range = gp.particleBufferSize;
	Com_Memset( &writes[0], 0, sizeof( writes[0] ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = gp.updateSet;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[0].pBufferInfo = &bufInfo;
	qvkUpdateDescriptorSets( vk.device, 1, &writes[0], 0, NULL );

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gp.updatePipe );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gp.updatePL, 0, 1, &gp.updateSet, 0, NULL );
	qvkCmdPushConstants( cmd, gp.updatePL, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( updatePush ), &updatePush );
	qvkCmdDispatch( cmd, ( GP_MAX_PARTICLES + 63u ) / 64u, 1, 1 );

	GP_CountLive();
	if ( gp.liveCount == 0 ) {
		return;
	}

	depthView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	nearest = GP_Sampler( qfalse );
	sceneView = vk.color_image_view;
	reactiveView = vk.reactive_mask_view ? vk.reactive_mask_view : vk.reactive_mask_stub_view;
	stampReactive = GP_ReactiveStamp();

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
	if ( stampReactive && reactiveView ) {
		vk_barrier_reactive_mask_for_storage( "gpu-particles-splat" );
	}

	GP_FillView( view, proj, projInfo );
	Com_Memset( imgInfos, 0, sizeof( imgInfos ) );
	imgInfos[0].sampler = nearest;
	imgInfos[0].imageView = depthView;
	imgInfos[0].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	imgInfos[1].imageView = sceneView;
	imgInfos[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imgInfos[2].imageView = reactiveView ? reactiveView : sceneView;
	imgInfos[2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = gp.splatSet;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[0].pBufferInfo = &bufInfo;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = gp.splatSet;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].pImageInfo = &imgInfos[0];
	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = gp.splatSet;
	writes[2].dstBinding = 2;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[2].pImageInfo = &imgInfos[1];
	writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[3].dstSet = gp.splatSet;
	writes[3].dstBinding = 3;
	writes[3].descriptorCount = 1;
	writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[3].pImageInfo = &imgInfos[2];
	qvkUpdateDescriptorSets( vk.device, 4, writes, 0, NULL );

	Com_Memset( &splatPush, 0, sizeof( splatPush ) );
	splatPush.extentMeta[0] = gp.width;
	splatPush.extentMeta[1] = gp.height;
	splatPush.extentMeta[2] = gp.liveCount > 0 ? GP_MAX_PARTICLES : 0u;
	Com_Memcpy( splatPush.projInfo, projInfo, sizeof( projInfo ) );
	Com_Memcpy( splatPush.view, view, sizeof( view ) );
	Com_Memcpy( splatPush.proj, proj, sizeof( proj ) );
	splatPush.params0[0] = r_gpuParticlesSoftScale ? r_gpuParticlesSoftScale->value : 1.0f;
	splatPush.params0[1] = r_gpuParticlesEmissiveBoost ? r_gpuParticlesEmissiveBoost->value : 2.0f;
	splatPush.params0[2] = 0.85f;
	splatPush.params0[3] = stampReactive ? 1.0f : 0.0f;

	vk_spine_note_write( VK_SPINE_RES_HDR_COLOR, VK_SPINE_PASS_NONE, VK_SPINE_ACCESS_COLOR_WRITE );
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gp.splatPipe );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gp.splatPL, 0, 1, &gp.splatSet, 0, NULL );
	qvkCmdPushConstants( cmd, gp.splatPL, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( splatPush ), &splatPush );
	gx = ( gp.width + 7u ) / 8u;
	gy = ( gp.height + 7u ) / 8u;
	qvkCmdDispatch( cmd, gx, gy, 1 );

	record_image_layout_transition( cmd, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0, 0 );
	record_depth_image_layout_transition( cmd, depthAspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT );
	if ( stampReactive ) {
		vk_barrier_reactive_mask_for_sampling( "gpu-particles-splat" );
	}

	gp.appliedThisFrame = qtrue;
}
