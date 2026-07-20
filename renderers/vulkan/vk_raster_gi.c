/*
===========================================================================
Raster Ultra 1.3 — Dynamic irradiance probes + screen-space indirect diffuse.
Raster-only; RT locked by Raster Ultra. Ownership: docs/RASTER_ULTRA_1.3.md.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_raster_gi.h"
#include "vk_util.h"
#include "vk_image_layout.h"
#include "vk_view_state.h"
#include "vk_deferred_gbuffer.h"
#include "vk_ambient_visibility.h"
#include "vk_pass_registry.h"
#include "vk_cmd.h"
#include "vk_raster_ultra.h"
#include "vk_pathtrace.h"

#include <math.h>

#define RGI_MAX_PROBES          4096
#define RGI_CACHE_MAGIC         0x31494752 /* 'RGI1' */
#define RGI_CACHE_VERSION       1

typedef struct {
	vec3_t origin;
	float valid;           /* 0..1 confidence */
	vec3_t L0;             /* ambient / dynamic delta irradiance */
	float skyVis;
	vec3_t L1x;
	float age;
	vec3_t L1y;
	float interior;
	vec3_t L1z;
	float relocateMag;
	vec3_t staticL0;       /* lightmap/SH baseline (not injected when delta mode) */
	float dirty;
	int gridX, gridY, gridZ;
	uint32_t lastUpdateFrame;
	uint32_t invalidReason; /* bitfield for debug */
} rgiProbe_t;

/* GPU mirror of RgiProbe (5x vec4). */
typedef struct {
	float posValid[4];
	float L0_sky[4];
	float L1x[4];
	float L1y[4];
	float L1z[4];
} rgiProbeGpu_t;

typedef struct {
	VkImage image;
	VkDeviceMemory memory;
	VkImageView view;
	VkImageLayout layout;
	uint32_t width, height;
} rgiImage_t;

typedef struct {
	qboolean ready;
	qboolean probesReady;
	qboolean appliedThisFrame;
	char mapName[MAX_QPATH];

	rgiProbe_t *probes;
	int probeCount;
	int gridX, gridY, gridZ;
	vec3_t worldMin, worldMax;
	float spacing;

	/* Budget / metrics */
	int requested;
	int updated;
	int deferred;
	uint32_t frameIndex;
	double lastCpuUpdateMs;

	/* GPU */
	VkBuffer probeBuffer;
	VkDeviceMemory probeMemory;
	void *probeMapped;
	VkDeviceSize probeBufferSize;

	rgiImage_t probeIrr;
	rgiImage_t probeMeta;
	rgiImage_t ssgiRad;
	rgiImage_t ssgiMeta;

	VkShaderModule probeCS, ssgiCS, resolveCS;
	VkDescriptorSetLayout probeLayout, ssgiLayout, resolveLayout;
	VkPipelineLayout probePL, ssgiPL, resolvePL;
	VkPipeline probePipe, ssgiPipe, resolvePipe;
	VkDescriptorPool descriptorPool;
	VkDescriptorSet probeSet, ssgiSet, resolveSet;

	uint32_t gbufferGeneration;
	uint32_t width, height;
} rgiState_t;

static rgiState_t rgi;

static cvar_t *r_probeGi;
static cvar_t *r_probeGiStrength;
static cvar_t *r_probeGiSpacing;
static cvar_t *r_probeGiMax;
static cvar_t *r_probeGiBudget;
static cvar_t *r_probeGiMode;          /* 0=delta (default), 1=full ambient when no lightgrid */
static cvar_t *r_probeGiStaticScale;
static cvar_t *r_probeGiDynamicScale;
static cvar_t *r_probeGiMinVis;
static cvar_t *r_probeGiAuto;
static cvar_t *r_probeGiCache;
static cvar_t *r_ssgi;
static cvar_t *r_ssgiStrength;
static cvar_t *r_ssgiSteps;
static cvar_t *r_ssgiThickness;
static cvar_t *r_ssgiDistance;
static cvar_t *r_ssgiNearScale;
static cvar_t *r_ssgiEdgeFade;
static cvar_t *r_rasterGiDebug;
static cvar_t *r_rasterGiAoStrength;
static cvar_t *r_rasterGiLightmapDelta;
static cvar_t *r_havenrpDiffuseGiOwner;

/* ---- helpers ---- */

static void RGI_RegisterCvars( void )
{
	if ( r_probeGi ) {
		return;
	}
	r_probeGi = ri.Cvar_Get( "r_probeGi", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_probeGi, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_probeGi,
		"Raster Ultra 1.3 dynamic irradiance probes (latched, raster-only).\n"
		" 0 - off (default; certified boot unchanged)\n"
		" 1 - enable probe GI + optional SSGI composition\n"
		"Enable via: exec modern_raster_ultra.cfg; vid_restart" );
	ri.Cvar_SetGroup( r_probeGi, CVG_RENDERER );

	r_probeGiStrength = ri.Cvar_Get( "r_probeGiStrength", "1", CVAR_ARCHIVE_ND );
	r_probeGiSpacing = ri.Cvar_Get( "r_probeGiSpacing", "256", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_probeGiMax = ri.Cvar_Get( "r_probeGiMax", "2048", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_probeGiBudget = ri.Cvar_Get( "r_probeGiBudget", "64", CVAR_ARCHIVE_ND );
	r_probeGiMode = ri.Cvar_Get( "r_probeGiMode", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_probeGiMode,
		"Probe energy model:\n"
		" 0 - static baseline + dynamic delta (default; avoids LM double-count)\n"
		" 1 - full ambient when lightgrid absent" );
	r_probeGiStaticScale = ri.Cvar_Get( "r_probeGiStaticScale", "0", CVAR_ARCHIVE_ND );
	r_probeGiDynamicScale = ri.Cvar_Get( "r_probeGiDynamicScale", "1", CVAR_ARCHIVE_ND );
	r_probeGiMinVis = ri.Cvar_Get( "r_probeGiMinVis", "0.02", CVAR_ARCHIVE_ND );
	r_probeGiAuto = ri.Cvar_Get( "r_probeGiAuto", "1", CVAR_ARCHIVE_ND );
	r_probeGiCache = ri.Cvar_Get( "r_probeGiCache", "1", CVAR_ARCHIVE_ND );

	r_ssgi = ri.Cvar_Get( "r_ssgi", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_ssgi, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_ssgi,
		"Raster Ultra 1.3 screen-space indirect diffuse (latched).\n"
		"Requires r_probeGi 1. Current-frame only; confidence-routed." );
	ri.Cvar_SetGroup( r_ssgi, CVG_RENDERER );
	r_ssgiStrength = ri.Cvar_Get( "r_ssgiStrength", "0.65", CVAR_ARCHIVE_ND );
	r_ssgiSteps = ri.Cvar_Get( "r_ssgiSteps", "12", CVAR_ARCHIVE_ND );
	r_ssgiThickness = ri.Cvar_Get( "r_ssgiThickness", "24", CVAR_ARCHIVE_ND );
	r_ssgiDistance = ri.Cvar_Get( "r_ssgiDistance", "192", CVAR_ARCHIVE_ND );
	r_ssgiNearScale = ri.Cvar_Get( "r_ssgiNearScale", "1", CVAR_ARCHIVE_ND );
	r_ssgiEdgeFade = ri.Cvar_Get( "r_ssgiEdgeFade", "32", CVAR_ARCHIVE_ND );

	r_rasterGiDebug = ri.Cvar_Get( "r_rasterGiDebug", "0", CVAR_ARCHIVE_ND );
	r_rasterGiAoStrength = ri.Cvar_Get( "r_rasterGiAoStrength", "1", CVAR_ARCHIVE_ND );
	r_rasterGiLightmapDelta = ri.Cvar_Get( "r_rasterGiLightmapDelta", "1", CVAR_ARCHIVE_ND );
	r_havenrpDiffuseGiOwner = ri.Cvar_Get( "r_havenrpDiffuseGiOwner", "auto", CVAR_ARCHIVE_ND );
}

static void RGI_DestroyImage( rgiImage_t *img )
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

static qboolean RGI_CreateImage( rgiImage_t *img, uint32_t width, uint32_t height, const char *name )
{
	VkImageCreateInfo ici;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo mai;
	VkImageViewCreateInfo vci;
	VkResult res;

	RGI_DestroyImage( img );
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
	ici.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	res = qvkCreateImage( vk.device, &ici, NULL, &img->image );
	if ( res != VK_SUCCESS ) {
		return qfalse;
	}
	qvkGetImageMemoryRequirements( vk.device, img->image, &req );
	Com_Memset( &mai, 0, sizeof( mai ) );
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.allocationSize = req.size;
	mai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	res = qvkAllocateMemory( vk.device, &mai, NULL, &img->memory );
	if ( res != VK_SUCCESS ) {
		RGI_DestroyImage( img );
		return qfalse;
	}
	if ( qvkBindImageMemory( vk.device, img->image, img->memory, 0 ) != VK_SUCCESS ) {
		RGI_DestroyImage( img );
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
	res = qvkCreateImageView( vk.device, &vci, NULL, &img->view );
	if ( res != VK_SUCCESS ) {
		RGI_DestroyImage( img );
		return qfalse;
	}
	img->layout = VK_IMAGE_LAYOUT_UNDEFINED;
	img->width = width;
	img->height = height;
	SET_OBJECT_NAME( img->image, name, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	return qtrue;
}

static void RGI_Transition( VkCommandBuffer cmd, rgiImage_t *img, VkImageLayout layout )
{
	if ( !img->image || img->layout == layout ) {
		return;
	}
	record_image_layout_transition( cmd, img->image, VK_IMAGE_ASPECT_COLOR_BIT,
		img->layout, layout, 0, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	img->layout = layout;
}

static VkSampler RGI_Sampler( qboolean linear )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = linear ? GL_LINEAR : GL_NEAREST;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;
	return vk_find_sampler( &sd );
}

static VkShaderModule RGI_Module( const uint8_t *bytes, uint32_t size, const char *name )
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
			"[RGI] CreateShaderModule(%s) failed: %s\n" S_COLOR_WHITE, name, vk_result_string( res ) );
		return VK_NULL_HANDLE;
	}
	SET_OBJECT_NAME( module, name, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	return module;
}

static qboolean RGI_CreateLayout( const VkDescriptorType *types, uint32_t count,
	uint32_t pushSize, VkDescriptorSetLayout *setLayout, VkPipelineLayout *pipelineLayout )
{
	VkDescriptorSetLayoutBinding bindings[12];
	VkDescriptorSetLayoutCreateInfo dci;
	VkPushConstantRange range;
	VkPipelineLayoutCreateInfo pci;
	VkResult res;
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
	res = qvkCreateDescriptorSetLayout( vk.device, &dci, NULL, setLayout );
	if ( res != VK_SUCCESS ) {
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
	res = qvkCreatePipelineLayout( vk.device, &pci, NULL, pipelineLayout );
	if ( res != VK_SUCCESS ) {
		qvkDestroyDescriptorSetLayout( vk.device, *setLayout, NULL );
		*setLayout = VK_NULL_HANDLE;
		return qfalse;
	}
	return qtrue;
}

static VkPipeline RGI_CreateComputePipeline( VkShaderModule module, VkPipelineLayout layout, const char *name )
{
	VkComputePipelineCreateInfo ci;
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkResult res;

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
	res = qvkCreateComputePipelines( vk.device, vk.pipelineCache, 1, &ci, NULL, &pipeline );
	if ( res != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[RGI] CreateComputePipelines(%s) failed: %s\n" S_COLOR_WHITE, name, vk_result_string( res ) );
		return VK_NULL_HANDLE;
	}
	SET_OBJECT_NAME( pipeline, name, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
	return pipeline;
}

#include "vk_raster_gi_spirv.inc"

static void RGI_DestroyPipelines( void )
{
#define RGI_DESTROY( fn, x ) do { if ( rgi.x ) { fn( vk.device, rgi.x, NULL ); rgi.x = VK_NULL_HANDLE; } } while ( 0 )
	RGI_DESTROY( qvkDestroyPipeline, probePipe );
	RGI_DESTROY( qvkDestroyPipeline, ssgiPipe );
	RGI_DESTROY( qvkDestroyPipeline, resolvePipe );
	RGI_DESTROY( qvkDestroyPipelineLayout, probePL );
	RGI_DESTROY( qvkDestroyPipelineLayout, ssgiPL );
	RGI_DESTROY( qvkDestroyPipelineLayout, resolvePL );
	RGI_DESTROY( qvkDestroyDescriptorSetLayout, probeLayout );
	RGI_DESTROY( qvkDestroyDescriptorSetLayout, ssgiLayout );
	RGI_DESTROY( qvkDestroyDescriptorSetLayout, resolveLayout );
	RGI_DESTROY( qvkDestroyShaderModule, probeCS );
	RGI_DESTROY( qvkDestroyShaderModule, ssgiCS );
	RGI_DESTROY( qvkDestroyShaderModule, resolveCS );
	RGI_DESTROY( qvkDestroyDescriptorPool, descriptorPool );
#undef RGI_DESTROY
	rgi.probeSet = VK_NULL_HANDLE;
	rgi.ssgiSet = VK_NULL_HANDLE;
	rgi.resolveSet = VK_NULL_HANDLE;
}

static qboolean RGI_CreatePipelines( void )
{
	static const VkDescriptorType probeTypes[] = {
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
	};
	static const VkDescriptorType ssgiTypes[] = {
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
	};
	static const VkDescriptorType resolveTypes[] = {
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
	};
	VkDescriptorPoolSize sizes[3];
	VkDescriptorPoolCreateInfo poolCI;
	VkDescriptorSetAllocateInfo ai;
	VkDescriptorSetLayout layouts[3];
	VkDescriptorSet sets[3];
	VkResult res;

	rgi.probeCS = RGI_Module( vk_rgi_probe_sample_cs_spv, VK_RGI_PROBE_SAMPLE_CS_SPV_SIZE, "RGI probe sample" );
	rgi.ssgiCS = RGI_Module( vk_rgi_ssgi_cs_spv, VK_RGI_SSGI_CS_SPV_SIZE, "RGI SSGI" );
	rgi.resolveCS = RGI_Module( vk_rgi_resolve_cs_spv, VK_RGI_RESOLVE_CS_SPV_SIZE, "RGI resolve" );
	if ( !rgi.probeCS || !rgi.ssgiCS || !rgi.resolveCS ) {
		return qfalse;
	}
	/* push sizes must match GLSL push_constant blocks (std430-ish aligned). */
	if ( !RGI_CreateLayout( probeTypes, ARRAY_LEN( probeTypes ), 160u, &rgi.probeLayout, &rgi.probePL ) ||
		!RGI_CreateLayout( ssgiTypes, ARRAY_LEN( ssgiTypes ), 192u, &rgi.ssgiLayout, &rgi.ssgiPL ) ||
		!RGI_CreateLayout( resolveTypes, ARRAY_LEN( resolveTypes ), 48u, &rgi.resolveLayout, &rgi.resolvePL ) ) {
		return qfalse;
	}
	rgi.probePipe = RGI_CreateComputePipeline( rgi.probeCS, rgi.probePL, "RGI probe pipeline" );
	rgi.ssgiPipe = RGI_CreateComputePipeline( rgi.ssgiCS, rgi.ssgiPL, "RGI SSGI pipeline" );
	rgi.resolvePipe = RGI_CreateComputePipeline( rgi.resolveCS, rgi.resolvePL, "RGI resolve pipeline" );
	if ( !rgi.probePipe || !rgi.ssgiPipe || !rgi.resolvePipe ) {
		return qfalse;
	}

	Com_Memset( sizes, 0, sizeof( sizes ) );
	sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; sizes[0].descriptorCount = 24;
	sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; sizes[1].descriptorCount = 12;
	sizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; sizes[2].descriptorCount = 4;
	Com_Memset( &poolCI, 0, sizeof( poolCI ) );
	poolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCI.maxSets = 3;
	poolCI.poolSizeCount = 3;
	poolCI.pPoolSizes = sizes;
	res = qvkCreateDescriptorPool( vk.device, &poolCI, NULL, &rgi.descriptorPool );
	if ( res != VK_SUCCESS ) {
		return qfalse;
	}
	layouts[0] = rgi.probeLayout;
	layouts[1] = rgi.ssgiLayout;
	layouts[2] = rgi.resolveLayout;
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	ai.descriptorPool = rgi.descriptorPool;
	ai.descriptorSetCount = 3;
	ai.pSetLayouts = layouts;
	res = qvkAllocateDescriptorSets( vk.device, &ai, sets );
	if ( res != VK_SUCCESS ) {
		return qfalse;
	}
	rgi.probeSet = sets[0];
	rgi.ssgiSet = sets[1];
	rgi.resolveSet = sets[2];
	return qtrue;
}

static void RGI_DestroyProbeBuffer( void )
{
	if ( rgi.probeMapped ) {
		qvkUnmapMemory( vk.device, rgi.probeMemory );
		rgi.probeMapped = NULL;
	}
	if ( rgi.probeBuffer ) {
		qvkDestroyBuffer( vk.device, rgi.probeBuffer, NULL );
		rgi.probeBuffer = VK_NULL_HANDLE;
	}
	if ( rgi.probeMemory ) {
		qvkFreeMemory( vk.device, rgi.probeMemory, NULL );
		rgi.probeMemory = VK_NULL_HANDLE;
	}
	rgi.probeBufferSize = 0;
}

static qboolean RGI_EnsureProbeBuffer( int count )
{
	VkBufferCreateInfo bci;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo mai;
	VkResult res;
	VkDeviceSize size;

	if ( count < 1 ) {
		count = 1;
	}
	size = (VkDeviceSize)count * sizeof( rgiProbeGpu_t );
	if ( rgi.probeMapped && rgi.probeBufferSize >= size ) {
		return qtrue;
	}
	RGI_DestroyProbeBuffer();
	Com_Memset( &bci, 0, sizeof( bci ) );
	bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.size = size;
	bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	res = qvkCreateBuffer( vk.device, &bci, NULL, &rgi.probeBuffer );
	if ( res != VK_SUCCESS ) {
		return qfalse;
	}
	qvkGetBufferMemoryRequirements( vk.device, rgi.probeBuffer, &req );
	Com_Memset( &mai, 0, sizeof( mai ) );
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.allocationSize = req.size;
	mai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	res = qvkAllocateMemory( vk.device, &mai, NULL, &rgi.probeMemory );
	if ( res != VK_SUCCESS ) {
		RGI_DestroyProbeBuffer();
		return qfalse;
	}
	if ( qvkBindBufferMemory( vk.device, rgi.probeBuffer, rgi.probeMemory, 0 ) != VK_SUCCESS ||
		qvkMapMemory( vk.device, rgi.probeMemory, 0, size, 0, &rgi.probeMapped ) != VK_SUCCESS ) {
		RGI_DestroyProbeBuffer();
		return qfalse;
	}
	rgi.probeBufferSize = size;
	Com_Memset( rgi.probeMapped, 0, (size_t)size );
	SET_OBJECT_NAME( rgi.probeBuffer, "RGI probe SSBO", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	return qtrue;
}

static qboolean RGI_EnsureImages( uint32_t width, uint32_t height )
{
	if ( rgi.probeIrr.image && rgi.width == width && rgi.height == height ) {
		return qtrue;
	}
	if ( !RGI_CreateImage( &rgi.probeIrr, width, height, "RGI probe irradiance" ) ||
		!RGI_CreateImage( &rgi.probeMeta, width, height, "RGI probe meta" ) ||
		!RGI_CreateImage( &rgi.ssgiRad, width, height, "RGI SSGI radiance" ) ||
		!RGI_CreateImage( &rgi.ssgiMeta, width, height, "RGI SSGI meta" ) ) {
		return qfalse;
	}
	rgi.width = width;
	rgi.height = height;
	return qtrue;
}

/* ---- probe placement / update ---- */

static mnode_t *RGI_PointInLeaf( const vec3_t p )
{
	mnode_t *node;

	if ( !tr.world || !tr.world->nodes ) {
		return NULL;
	}
	node = tr.world->nodes;
	while ( 1 ) {
		if ( node->contents != (int)CONTENTS_NODE ) {
			return node;
		}
		{
			cplane_t *plane = node->plane;
			float d = DotProduct( p, plane->normal ) - plane->dist;
			node = ( d > 0 ) ? node->children[0] : node->children[1];
		}
	}
}

static qboolean RGI_PointEmpty( const vec3_t p )
{
	mnode_t *leaf = RGI_PointInLeaf( p );
	if ( !leaf ) {
		return qfalse;
	}
	if ( leaf->contents & CONTENTS_SOLID ) {
		return qfalse;
	}
	return qtrue;
}

static qboolean RGI_RelocateProbe( vec3_t pos )
{
	const float offsets[6][3] = {
		{ 32, 0, 0 }, { -32, 0, 0 }, { 0, 32, 0 },
		{ 0, -32, 0 }, { 0, 0, 32 }, { 0, 0, -32 }
	};
	vec3_t tryPos;
	int i, s;

	if ( RGI_PointEmpty( pos ) ) {
		return qtrue;
	}
	for ( s = 1; s <= 4; ++s ) {
		for ( i = 0; i < 6; ++i ) {
			VectorMA( pos, (float)s, offsets[i], tryPos );
			if ( RGI_PointEmpty( tryPos ) ) {
				VectorCopy( tryPos, pos );
				return qtrue;
			}
		}
	}
	return qfalse;
}

static void RGI_FreeProbes( void )
{
	if ( rgi.probes ) {
		ri.Free( rgi.probes );
		rgi.probes = NULL;
	}
	rgi.probeCount = 0;
	rgi.probesReady = qfalse;
	rgi.gridX = rgi.gridY = rgi.gridZ = 0;
}

static void RGI_UploadProbes( void )
{
	rgiProbeGpu_t *dst;
	int i;

	if ( !rgi.probeMapped || !rgi.probes || rgi.probeCount <= 0 ) {
		return;
	}
	dst = (rgiProbeGpu_t *)rgi.probeMapped;
	for ( i = 0; i < rgi.probeCount; ++i ) {
		const rgiProbe_t *p = &rgi.probes[i];
		float staticScale = r_probeGiStaticScale ? r_probeGiStaticScale->value : 0.0f;
		float dynamicScale = r_probeGiDynamicScale ? r_probeGiDynamicScale->value : 1.0f;
		vec3_t L0;

		/* Delta mode: L0 = static*staticScale + dynamic (stored in L0). */
		L0[0] = p->staticL0[0] * staticScale + p->L0[0] * dynamicScale;
		L0[1] = p->staticL0[1] * staticScale + p->L0[1] * dynamicScale;
		L0[2] = p->staticL0[2] * staticScale + p->L0[2] * dynamicScale;

		dst[i].posValid[0] = p->origin[0];
		dst[i].posValid[1] = p->origin[1];
		dst[i].posValid[2] = p->origin[2];
		dst[i].posValid[3] = p->valid;
		dst[i].L0_sky[0] = L0[0];
		dst[i].L0_sky[1] = L0[1];
		dst[i].L0_sky[2] = L0[2];
		dst[i].L0_sky[3] = p->skyVis;
		dst[i].L1x[0] = p->L1x[0] * dynamicScale;
		dst[i].L1x[1] = p->L1x[1] * dynamicScale;
		dst[i].L1x[2] = p->L1x[2] * dynamicScale;
		dst[i].L1x[3] = p->age;
		dst[i].L1y[0] = p->L1y[0] * dynamicScale;
		dst[i].L1y[1] = p->L1y[1] * dynamicScale;
		dst[i].L1y[2] = p->L1y[2] * dynamicScale;
		dst[i].L1y[3] = p->interior;
		dst[i].L1z[0] = p->L1z[0] * dynamicScale;
		dst[i].L1z[1] = p->L1z[1] * dynamicScale;
		dst[i].L1z[2] = p->L1z[2] * dynamicScale;
		dst[i].L1z[3] = p->relocateMag;
	}
}

static void RGI_UpdateProbeLighting( rgiProbe_t *p, const trRefdef_t *refdef )
{
	vec3_t ambient, directed, lightDir;
	vec3_t dyn = { 0, 0, 0 };
	vec3_t L1x = { 0, 0, 0 }, L1y = { 0, 0, 0 }, L1z = { 0, 0, 0 };
	int i;
	float inv255 = 1.0f / 255.0f;

	VectorClear( p->staticL0 );
	if ( tr.world && tr.world->lightGridData &&
		R_LightForPoint( p->origin, ambient, directed, lightDir ) ) {
		p->staticL0[0] = ambient[0] * inv255;
		p->staticL0[1] = ambient[1] * inv255;
		p->staticL0[2] = ambient[2] * inv255;
		/* Pack directed as weak L1 toward lightDir. */
		{
			float s = ( directed[0] + directed[1] + directed[2] ) * ( 1.0f / 3.0f ) * inv255 * 0.35f;
			L1x[0] = L1x[1] = L1x[2] = lightDir[0] * s;
			L1y[0] = L1y[1] = L1y[2] = lightDir[1] * s;
			L1z[0] = L1z[1] = L1z[2] = lightDir[2] * s;
		}
	} else if ( r_probeGiMode && r_probeGiMode->integer == 1 ) {
		/* Classic fallback ambient when lightgrid missing. */
		p->staticL0[0] = p->staticL0[1] = p->staticL0[2] = 0.15f;
	}

	/* Dynamic lights → dynamic delta (ownership: not re-adding baked direct). */
	if ( refdef ) {
		for ( i = 0; i < (int)refdef->num_dlights; ++i ) {
			const dlight_t *dl = &refdef->dlights[i];
			vec3_t dir;
			float dist, atten, power;
			VectorSubtract( dl->origin, p->origin, dir );
			dist = VectorNormalize( dir );
			if ( dist < 1.0f ) {
				dist = 1.0f;
			}
			if ( dist > dl->radius * 1.5f ) {
				continue;
			}
			power = dl->radius * dl->radius;
			atten = power / ( dist * dist );
			atten = Com_Clamp( 0.0f, 4.0f, atten * 0.15f );
			dyn[0] += dl->color[0] * atten;
			dyn[1] += dl->color[1] * atten;
			dyn[2] += dl->color[2] * atten;
			L1x[0] += dir[0] * dl->color[0] * atten * 0.5f;
			L1y[1] += dir[1] * dl->color[1] * atten * 0.5f;
			L1z[2] += dir[2] * dl->color[2] * atten * 0.5f;
		}
	}

	/* Soft sun sky irradiance (not recursive). */
	if ( tr.sunDirection[0] || tr.sunDirection[1] || tr.sunDirection[2] ) {
		float sky = Com_Clamp( 0.0f, 1.0f, tr.sunDirection[2] * 0.5f + 0.5f );
		p->skyVis = sky;
		if ( p->interior < 0.5f ) {
			dyn[0] += 0.02f * sky;
			dyn[1] += 0.02f * sky;
			dyn[2] += 0.025f * sky;
		}
	} else {
		p->skyVis = 0.5f;
	}

	VectorCopy( dyn, p->L0 );
	VectorCopy( L1x, p->L1x );
	VectorCopy( L1y, p->L1y );
	VectorCopy( L1z, p->L1z );
	p->age = 0.0f;
	p->dirty = 0.0f;
	p->lastUpdateFrame = rgi.frameIndex;
}

static int RGI_ProbePriority( const rgiProbe_t *p, const vec3_t viewOrg )
{
	float dist = Distance( p->origin, viewOrg );
	int score = (int)( dist );
	score += (int)( p->age * 8.0f );
	if ( p->dirty > 0.5f ) {
		score -= 5000;
	}
	return score;
}

static void RGI_BudgetUpdate( const trRefdef_t *refdef )
{
	int budget = r_probeGiBudget ? r_probeGiBudget->integer : 64;
	int i, updated = 0;
	vec3_t viewOrg;
	int *order;
	int n;

	if ( !rgi.probes || rgi.probeCount <= 0 || !refdef ) {
		return;
	}
	if ( budget < 1 ) {
		budget = 1;
	}
	if ( budget > rgi.probeCount ) {
		budget = rgi.probeCount;
	}

	VectorCopy( refdef->vieworg, viewOrg );
	rgi.requested = rgi.probeCount;
	order = (int *)ri.Hunk_AllocateTempMemory( sizeof( int ) * rgi.probeCount );
	if ( !order ) {
		for ( i = 0; i < budget; ++i ) {
			RGI_UpdateProbeLighting( &rgi.probes[i], refdef );
			updated++;
		}
		rgi.updated = updated;
		rgi.deferred = rgi.probeCount - updated;
		return;
	}
	n = rgi.probeCount;
	for ( i = 0; i < n; ++i ) {
		order[i] = i;
		rgi.probes[i].age += 1.0f;
	}
	/* Simple selection sort of first `budget` by priority (low score first). */
	for ( i = 0; i < budget; ++i ) {
		int best = i;
		int j;
		for ( j = i + 1; j < n; ++j ) {
			if ( RGI_ProbePriority( &rgi.probes[order[j]], viewOrg ) <
				RGI_ProbePriority( &rgi.probes[order[best]], viewOrg ) ) {
				best = j;
			}
		}
		if ( best != i ) {
			int tmp = order[i];
			order[i] = order[best];
			order[best] = tmp;
		}
		RGI_UpdateProbeLighting( &rgi.probes[order[i]], refdef );
		updated++;
	}
	ri.Hunk_FreeTempMemory( order );
	rgi.updated = updated;
	rgi.deferred = rgi.probeCount - updated;
}

static qboolean RGI_GenerateGrid( void )
{
	vec3_t mins, maxs;
	float spacing;
	int gx, gy, gz, maxProbes;
	int x, y, z, count;
	int validCount = 0;

	if ( !tr.world ) {
		return qfalse;
	}
	VectorCopy( tr.world->bmodels[0].bounds[0], mins );
	VectorCopy( tr.world->bmodels[0].bounds[1], maxs );
	spacing = r_probeGiSpacing ? r_probeGiSpacing->value : 256.0f;
	if ( spacing < 64.0f ) {
		spacing = 64.0f;
	}
	maxProbes = r_probeGiMax ? r_probeGiMax->integer : 2048;
	if ( maxProbes > RGI_MAX_PROBES ) {
		maxProbes = RGI_MAX_PROBES;
	}
	if ( maxProbes < 8 ) {
		maxProbes = 8;
	}

	gx = (int)ceil( ( maxs[0] - mins[0] ) / spacing );
	gy = (int)ceil( ( maxs[1] - mins[1] ) / spacing );
	gz = (int)ceil( ( maxs[2] - mins[2] ) / spacing );
	if ( gx < 1 ) gx = 1;
	if ( gy < 1 ) gy = 1;
	if ( gz < 1 ) gz = 1;

	/* Shrink until under budget. */
	while ( gx * gy * gz > maxProbes ) {
		if ( gx >= gy && gx >= gz && gx > 1 ) {
			gx--;
		} else if ( gy >= gz && gy > 1 ) {
			gy--;
		} else if ( gz > 1 ) {
			gz--;
		} else {
			break;
		}
	}

	RGI_FreeProbes();
	count = gx * gy * gz;
	rgi.probes = (rgiProbe_t *)ri.Malloc( sizeof( rgiProbe_t ) * count );
	if ( !rgi.probes ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[RGI] probe allocation failed\n" S_COLOR_WHITE );
		return qfalse;
	}
	Com_Memset( rgi.probes, 0, sizeof( rgiProbe_t ) * count );
	rgi.gridX = gx;
	rgi.gridY = gy;
	rgi.gridZ = gz;
	rgi.spacing = spacing;
	VectorCopy( mins, rgi.worldMin );
	VectorCopy( maxs, rgi.worldMax );
	rgi.probeCount = count;

	for ( z = 0; z < gz; ++z ) {
		for ( y = 0; y < gy; ++y ) {
			for ( x = 0; x < gx; ++x ) {
				int idx = x + y * gx + z * gx * gy;
				rgiProbe_t *p = &rgi.probes[idx];
				vec3_t pos;
				float relocate = 0.0f;

				pos[0] = mins[0] + ( x + 0.5f ) * ( maxs[0] - mins[0] ) / (float)gx;
				pos[1] = mins[1] + ( y + 0.5f ) * ( maxs[1] - mins[1] ) / (float)gy;
				pos[2] = mins[2] + ( z + 0.5f ) * ( maxs[2] - mins[2] ) / (float)gz;
				p->gridX = x;
				p->gridY = y;
				p->gridZ = z;
				if ( !RGI_PointEmpty( pos ) ) {
					vec3_t before;
					VectorCopy( pos, before );
					if ( RGI_RelocateProbe( pos ) ) {
						relocate = Distance( before, pos );
						p->valid = 0.85f;
						p->invalidReason = 0;
					} else {
						p->valid = 0.0f;
						p->invalidReason = 1; /* solid */
						VectorCopy( pos, p->origin );
						continue;
					}
				} else {
					p->valid = 1.0f;
				}
				VectorCopy( pos, p->origin );
				p->relocateMag = relocate;
				p->interior = ( pos[2] < ( mins[2] + maxs[2] ) * 0.5f ) ? 1.0f : 0.0f;
				p->dirty = 1.0f;
				p->age = 1000.0f;
				validCount++;
			}
		}
	}

	if ( !RGI_EnsureProbeBuffer( count ) ) {
		RGI_FreeProbes();
		return qfalse;
	}
	rgi.probesReady = ( validCount > 0 ) ? qtrue : qfalse;
	Q_strncpyz( rgi.mapName, tr.world->name[0] ? tr.world->name : "unknown", sizeof( rgi.mapName ) );
	ri.Printf( PRINT_ALL,
		"[RGI] generated probe grid %dx%dx%d (%d valid / %d) spacing=%.0f map=%s\n",
		gx, gy, gz, validCount, count, spacing, rgi.mapName );
	return rgi.probesReady;
}

typedef struct {
	int magic;
	int version;
	int gridX, gridY, gridZ;
	float spacing;
	float worldMin[3], worldMax[3];
	int probeCount;
} rgiCacheHeader_t;

static void RGI_CachePath( char *out, int outSize )
{
	const char *map = ( tr.world && tr.world->baseName[0] ) ? tr.world->baseName :
		( tr.world && tr.world->name[0] ) ? tr.world->name : "unknown";
	Com_sprintf( out, outSize, "maps/%s.rgi", map );
}

static qboolean RGI_SaveCache( void )
{
	char path[MAX_QPATH];
	rgiCacheHeader_t hdr;
	byte *buf;
	int size, i;
	rgiProbeGpu_t *body;

	if ( !rgi.probesReady || !rgi.probes || !( r_probeGiCache && r_probeGiCache->integer ) ) {
		return qfalse;
	}
	RGI_CachePath( path, sizeof( path ) );
	Com_Memset( &hdr, 0, sizeof( hdr ) );
	hdr.magic = RGI_CACHE_MAGIC;
	hdr.version = RGI_CACHE_VERSION;
	hdr.gridX = rgi.gridX;
	hdr.gridY = rgi.gridY;
	hdr.gridZ = rgi.gridZ;
	hdr.spacing = rgi.spacing;
	VectorCopy( rgi.worldMin, hdr.worldMin );
	VectorCopy( rgi.worldMax, hdr.worldMax );
	hdr.probeCount = rgi.probeCount;
	size = (int)( sizeof( hdr ) + sizeof( rgiProbeGpu_t ) * rgi.probeCount );
	buf = (byte *)ri.Hunk_AllocateTempMemory( size );
	if ( !buf ) {
		return qfalse;
	}
	Com_Memcpy( buf, &hdr, sizeof( hdr ) );
	body = (rgiProbeGpu_t *)( buf + sizeof( hdr ) );
	for ( i = 0; i < rgi.probeCount; ++i ) {
		const rgiProbe_t *p = &rgi.probes[i];
		body[i].posValid[0] = p->origin[0];
		body[i].posValid[1] = p->origin[1];
		body[i].posValid[2] = p->origin[2];
		body[i].posValid[3] = p->valid;
		body[i].L0_sky[0] = p->staticL0[0];
		body[i].L0_sky[1] = p->staticL0[1];
		body[i].L0_sky[2] = p->staticL0[2];
		body[i].L0_sky[3] = p->skyVis;
		body[i].L1x[3] = p->age;
		body[i].L1y[3] = p->interior;
		body[i].L1z[3] = p->relocateMag;
	}
	ri.FS_WriteFile( path, buf, size );
	ri.Hunk_FreeTempMemory( buf );
	ri.Printf( PRINT_ALL, "[RGI] saved probe cache %s (%d probes)\n", path, rgi.probeCount );
	return qtrue;
}

static qboolean RGI_LoadCache( void )
{
	char path[MAX_QPATH];
	rgiCacheHeader_t *hdr;
	byte *buf = NULL;
	int len, i;
	rgiProbeGpu_t *body;

	if ( !( r_probeGiCache && r_probeGiCache->integer ) || !tr.world ) {
		return qfalse;
	}
	RGI_CachePath( path, sizeof( path ) );
	len = ri.FS_ReadFile( path, (void **)&buf );
	if ( len <= 0 || !buf ) {
		return qfalse;
	}
	if ( len < (int)sizeof( rgiCacheHeader_t ) ) {
		ri.FS_FreeFile( buf );
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[RGI] corrupt cache %s (short)\n" S_COLOR_WHITE, path );
		return qfalse;
	}
	hdr = (rgiCacheHeader_t *)buf;
	if ( hdr->magic != RGI_CACHE_MAGIC || hdr->version != RGI_CACHE_VERSION ||
		hdr->probeCount <= 0 || hdr->probeCount > RGI_MAX_PROBES ) {
		ri.FS_FreeFile( buf );
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[RGI] corrupt cache %s (header)\n" S_COLOR_WHITE, path );
		return qfalse;
	}
	if ( len < (int)( sizeof( *hdr ) + sizeof( rgiProbeGpu_t ) * hdr->probeCount ) ) {
		ri.FS_FreeFile( buf );
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[RGI] corrupt cache %s (body)\n" S_COLOR_WHITE, path );
		return qfalse;
	}
	RGI_FreeProbes();
	rgi.probes = (rgiProbe_t *)ri.Malloc( sizeof( rgiProbe_t ) * hdr->probeCount );
	if ( !rgi.probes ) {
		ri.FS_FreeFile( buf );
		return qfalse;
	}
	Com_Memset( rgi.probes, 0, sizeof( rgiProbe_t ) * hdr->probeCount );
	rgi.probeCount = hdr->probeCount;
	rgi.gridX = hdr->gridX;
	rgi.gridY = hdr->gridY;
	rgi.gridZ = hdr->gridZ;
	rgi.spacing = hdr->spacing;
	VectorCopy( hdr->worldMin, rgi.worldMin );
	VectorCopy( hdr->worldMax, rgi.worldMax );
	body = (rgiProbeGpu_t *)( buf + sizeof( *hdr ) );
	for ( i = 0; i < rgi.probeCount; ++i ) {
		rgi.probes[i].origin[0] = body[i].posValid[0];
		rgi.probes[i].origin[1] = body[i].posValid[1];
		rgi.probes[i].origin[2] = body[i].posValid[2];
		rgi.probes[i].valid = body[i].posValid[3];
		rgi.probes[i].staticL0[0] = body[i].L0_sky[0];
		rgi.probes[i].staticL0[1] = body[i].L0_sky[1];
		rgi.probes[i].staticL0[2] = body[i].L0_sky[2];
		rgi.probes[i].skyVis = body[i].L0_sky[3];
		rgi.probes[i].interior = body[i].L1y[3];
		rgi.probes[i].relocateMag = body[i].L1z[3];
		rgi.probes[i].dirty = 1.0f;
		rgi.probes[i].age = 100.0f;
	}
	ri.FS_FreeFile( buf );
	if ( !RGI_EnsureProbeBuffer( rgi.probeCount ) ) {
		RGI_FreeProbes();
		return qfalse;
	}
	rgi.probesReady = qtrue;
	Q_strncpyz( rgi.mapName, tr.world->name[0] ? tr.world->name : "unknown", sizeof( rgi.mapName ) );
	ri.Printf( PRINT_ALL, "[RGI] loaded probe cache %s (%d probes)\n", path, rgi.probeCount );
	return qtrue;
}

static void RGI_EnsureProbes( void )
{
	if ( rgi.probesReady && tr.world && rgi.mapName[0] &&
		!Q_stricmp( rgi.mapName, tr.world->name ) ) {
		return;
	}
	if ( !tr.world ) {
		return;
	}
	if ( RGI_LoadCache() ) {
		return;
	}
	if ( r_probeGiAuto && r_probeGiAuto->integer ) {
		if ( RGI_GenerateGrid() ) {
			RGI_SaveCache();
		}
	}
}

/* ---- console ---- */

static void RGI_Status_f( void )
{
	ri.Printf( PRINT_ALL,
		"rasterGi: ready=%d probes=%d/%d grid=%dx%dx%d spacing=%.0f "
		"updated=%d deferred=%d cpu=%.2fms ssgi=%d debug=%d owner=%s\n",
		rgi.ready ? 1 : 0,
		rgi.probesReady ? rgi.probeCount : 0,
		r_probeGiMax ? r_probeGiMax->integer : 0,
		rgi.gridX, rgi.gridY, rgi.gridZ, rgi.spacing,
		rgi.updated, rgi.deferred, rgi.lastCpuUpdateMs,
		r_ssgi ? r_ssgi->integer : 0,
		r_rasterGiDebug ? r_rasterGiDebug->integer : 0,
		r_havenrpDiffuseGiOwner ? r_havenrpDiffuseGiOwner->string : "auto" );
}

static void RGI_Generate_f( void )
{
	if ( RGI_GenerateGrid() ) {
		RGI_SaveCache();
	}
}

static void RGI_Save_f( void )
{
	RGI_SaveCache();
}

static void RGI_Invalidate_f( void )
{
	char path[MAX_QPATH];
	RGI_CachePath( path, sizeof( path ) );
	RGI_FreeProbes();
	ri.Printf( PRINT_ALL, "[RGI] invalidated probes (delete %s manually if present)\n", path );
}

static void RGI_Inspect_f( void )
{
	int i, shown = 0;
	if ( !rgi.probes ) {
		ri.Printf( PRINT_ALL, "[RGI] no probes\n" );
		return;
	}
	for ( i = 0; i < rgi.probeCount && shown < 32; ++i ) {
		const rgiProbe_t *p = &rgi.probes[i];
		if ( p->valid <= 0.0f && ri.Cmd_Argc() < 2 ) {
			continue;
		}
		ri.Printf( PRINT_ALL,
			"  [%d] pos=(%.0f %.0f %.0f) valid=%.2f age=%.0f dyn=(%.3f %.3f %.3f) reloc=%.1f\n",
			i, p->origin[0], p->origin[1], p->origin[2], p->valid, p->age,
			p->L0[0], p->L0[1], p->L0[2], p->relocateMag );
		shown++;
	}
}

/* ---- public API ---- */

void vk_raster_gi_invalidate( void )
{
	RGI_FreeProbes();
	rgi.gbufferGeneration = 0;
}

void vk_raster_gi_on_map_load( void )
{
	vk_raster_gi_invalidate();
	if ( r_probeGi && r_probeGi->integer && rgi.ready ) {
		RGI_EnsureProbes();
	}
}

qboolean vk_raster_gi_active( void )
{
	return ( rgi.ready && r_probeGi && r_probeGi->integer && vk.fboActive &&
		vk_deferred_gbuffer_active() ) ? qtrue : qfalse;
}

qboolean vk_raster_gi_probes_ready( void )
{
	return ( vk_raster_gi_active() && rgi.probesReady ) ? qtrue : qfalse;
}

qboolean vk_raster_gi_sample_entity( const vec3_t origin, const vec3_t normal,
	vec3_t ambientOut, float *confidenceOut )
{
	vec3_t local, extent, gridF, frac;
	int base[3];
	int x, y, z;
	vec3_t irrSum = { 0, 0, 0 };
	float wSum = 0.0f;
	float confSum = 0.0f;
	float staticScale, dynamicScale;

	VectorClear( ambientOut );
	if ( confidenceOut ) {
		*confidenceOut = 0.0f;
	}
	if ( !vk_raster_gi_probes_ready() || rgi.gridX < 1 ) {
		return qfalse;
	}

	extent[0] = rgi.worldMax[0] - rgi.worldMin[0];
	extent[1] = rgi.worldMax[1] - rgi.worldMin[1];
	extent[2] = rgi.worldMax[2] - rgi.worldMin[2];
	if ( extent[0] < 1.0f ) extent[0] = 1.0f;
	if ( extent[1] < 1.0f ) extent[1] = 1.0f;
	if ( extent[2] < 1.0f ) extent[2] = 1.0f;
	local[0] = ( origin[0] - rgi.worldMin[0] ) / extent[0];
	local[1] = ( origin[1] - rgi.worldMin[1] ) / extent[1];
	local[2] = ( origin[2] - rgi.worldMin[2] ) / extent[2];
	gridF[0] = local[0] * rgi.gridX - 0.5f;
	gridF[1] = local[1] * rgi.gridY - 0.5f;
	gridF[2] = local[2] * rgi.gridZ - 0.5f;
	base[0] = (int)floor( gridF[0] );
	base[1] = (int)floor( gridF[1] );
	base[2] = (int)floor( gridF[2] );
	frac[0] = gridF[0] - base[0];
	frac[1] = gridF[1] - base[1];
	frac[2] = gridF[2] - base[2];
	staticScale = r_probeGiStaticScale ? r_probeGiStaticScale->value : 0.0f;
	dynamicScale = r_probeGiDynamicScale ? r_probeGiDynamicScale->value : 1.0f;

	for ( z = 0; z < 2; ++z ) {
		for ( y = 0; y < 2; ++y ) {
			for ( x = 0; x < 2; ++x ) {
				int cx = base[0] + x;
				int cy = base[1] + y;
				int cz = base[2] + z;
				int idx;
				rgiProbe_t *p;
				vec3_t toP, L;
				float dist, facing, trilin, w;

				if ( cx < 0 ) cx = 0;
				if ( cy < 0 ) cy = 0;
				if ( cz < 0 ) cz = 0;
				if ( cx >= rgi.gridX ) cx = rgi.gridX - 1;
				if ( cy >= rgi.gridY ) cy = rgi.gridY - 1;
				if ( cz >= rgi.gridZ ) cz = rgi.gridZ - 1;
				idx = cx + cy * rgi.gridX + cz * rgi.gridX * rgi.gridY;
				if ( idx < 0 || idx >= rgi.probeCount ) {
					continue;
				}
				p = &rgi.probes[idx];
				if ( p->valid <= 1e-4f ) {
					continue;
				}
				VectorSubtract( p->origin, origin, toP );
				dist = VectorNormalize( toP );
				facing = DotProduct( normal, toP );
				if ( facing < 0.0f ) {
					facing = 0.0f;
				}
				trilin = ( x ? frac[0] : 1.0f - frac[0] ) *
					( y ? frac[1] : 1.0f - frac[1] ) *
					( z ? frac[2] : 1.0f - frac[2] );
				w = trilin * ( 0.05f + facing ) * p->valid;
				L[0] = p->staticL0[0] * staticScale + p->L0[0] * dynamicScale;
				L[1] = p->staticL0[1] * staticScale + p->L0[1] * dynamicScale;
				L[2] = p->staticL0[2] * staticScale + p->L0[2] * dynamicScale;
				/* L1 directional */
				L[0] += ( p->L1x[0] * normal[0] + p->L1y[0] * normal[1] + p->L1z[0] * normal[2] ) * dynamicScale;
				L[1] += ( p->L1x[1] * normal[0] + p->L1y[1] * normal[1] + p->L1z[1] * normal[2] ) * dynamicScale;
				L[2] += ( p->L1x[2] * normal[0] + p->L1y[2] * normal[1] + p->L1z[2] * normal[2] ) * dynamicScale;
				VectorMA( irrSum, w, L, irrSum );
				wSum += w;
				confSum += p->valid * w;
				(void)dist;
			}
		}
	}
	if ( wSum <= 1e-5f ) {
		return qfalse;
	}
	VectorScale( irrSum, 1.0f / wSum, ambientOut );
	/* Convert to classic 0..255 entity light space. */
	VectorScale( ambientOut, 255.0f, ambientOut );
	if ( confidenceOut ) {
		*confidenceOut = confSum / wSum;
	}
	return qtrue;
}

void vk_raster_gi_shutdown( void )
{
	if ( ri.Cmd_RemoveCommand ) {
		ri.Cmd_RemoveCommand( "probe_gi_status" );
		ri.Cmd_RemoveCommand( "probe_gi_generate" );
		ri.Cmd_RemoveCommand( "probe_gi_save" );
		ri.Cmd_RemoveCommand( "probe_gi_invalidate" );
		ri.Cmd_RemoveCommand( "probe_gi_inspect" );
		ri.Cmd_RemoveCommand( "raster_gi_status" );
	}
	RGI_DestroyPipelines();
	RGI_DestroyProbeBuffer();
	RGI_DestroyImage( &rgi.probeIrr );
	RGI_DestroyImage( &rgi.probeMeta );
	RGI_DestroyImage( &rgi.ssgiRad );
	RGI_DestroyImage( &rgi.ssgiMeta );
	RGI_FreeProbes();
	Com_Memset( &rgi, 0, sizeof( rgi ) );
}

void vk_raster_gi_init( void )
{
	RGI_RegisterCvars();
	if ( rgi.ready || !vk.device || !vk.fboActive ) {
		return;
	}
	if ( !r_probeGi || !r_probeGi->integer ) {
		return;
	}
	if ( !RGI_CreatePipelines() ) {
		RGI_DestroyPipelines();
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[RGI] pipeline create failed — probe GI unavailable\n" S_COLOR_WHITE );
		return;
	}
	if ( ri.Cmd_RemoveCommand ) {
		ri.Cmd_RemoveCommand( "probe_gi_status" );
		ri.Cmd_RemoveCommand( "probe_gi_generate" );
		ri.Cmd_RemoveCommand( "probe_gi_save" );
		ri.Cmd_RemoveCommand( "probe_gi_invalidate" );
		ri.Cmd_RemoveCommand( "probe_gi_inspect" );
		ri.Cmd_RemoveCommand( "raster_gi_status" );
	}
	if ( ri.Cmd_AddCommand ) {
		ri.Cmd_AddCommand( "probe_gi_status", RGI_Status_f );
		ri.Cmd_AddCommand( "raster_gi_status", RGI_Status_f );
		ri.Cmd_AddCommand( "probe_gi_generate", RGI_Generate_f );
		ri.Cmd_AddCommand( "probe_gi_save", RGI_Save_f );
		ri.Cmd_AddCommand( "probe_gi_invalidate", RGI_Invalidate_f );
		ri.Cmd_AddCommand( "probe_gi_inspect", RGI_Inspect_f );
	}
	rgi.ready = qtrue;
	rgi.gbufferGeneration = vk_deferred_gbuffer_generation();
	ri.Printf( PRINT_ALL,
		"[RGI] Raster Ultra probe GI + SSGI initialized (r_probeGi=%d r_ssgi=%d; RT unused)\n",
		r_probeGi->integer, r_ssgi ? r_ssgi->integer : 0 );
}

void vk_raster_gi_frame_begin( void )
{
	uint32_t w, h;

	RGI_RegisterCvars();
	if ( !rgi.ready && vk.device && vk.fboActive && r_probeGi && r_probeGi->integer ) {
		vk_raster_gi_init();
	}
	if ( !rgi.ready ) {
		return;
	}
	rgi.appliedThisFrame = qfalse;
	rgi.frameIndex++;
	w = vk_get_render_target_width();
	h = vk_get_render_target_height();
	if ( w && h ) {
		RGI_EnsureImages( w, h );
	}
	if ( vk_deferred_gbuffer_generation() != rgi.gbufferGeneration ) {
		rgi.gbufferGeneration = vk_deferred_gbuffer_generation();
	}
}

static void RGI_ImageWrite( VkWriteDescriptorSet *w, VkDescriptorImageInfo *info,
	VkDescriptorSet set, uint32_t binding, VkDescriptorType type, VkSampler sampler,
	VkImageView view, VkImageLayout layout )
{
	Com_Memset( info, 0, sizeof( *info ) );
	info->sampler = sampler;
	info->imageView = view;
	info->imageLayout = layout;
	Com_Memset( w, 0, sizeof( *w ) );
	w->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	w->dstSet = set;
	w->dstBinding = binding;
	w->descriptorCount = 1;
	w->descriptorType = type;
	w->pImageInfo = info;
}

static void RGI_FillView( float invView[16], float view[16], float projInfo[4], uint32_t *normalsAreWorld )
{
	const float *viewMat = backEnd.viewParms.world.modelViewMatrix;
	const float *projection = backEnd.useFirstPersonProjection ?
		backEnd.firstPersonProjectionMatrix : backEnd.viewParms.projectionMatrix;
	float projVK[16];

	Com_Memcpy( view, viewMat, sizeof( float ) * 16 );
	if ( !vk_mat4_inverse( viewMat, invView ) ) {
		Com_Memcpy( invView, viewMat, sizeof( float ) * 16 );
	}
	vk_get_projection_matrix_vk( projection, projVK );
	projInfo[0] = projVK[0] != 0.0f ? 1.0f / projVK[0] : 1.0f;
	projInfo[1] = projVK[5] != 0.0f ? 1.0f / projVK[5] : 1.0f;
	projInfo[2] = projVK[10];
	projInfo[3] = projVK[14];
	*normalsAreWorld = vk.deferredGbufferDirectExport ? 1u : 0u;
}

void vk_raster_gi_apply_after_geometry( void )
{
	VkCommandBuffer cmd;
	VkImageAspectFlags depthAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	VkImageView depthView, normalView, albedoView, aoView;
	VkSampler nearest, linear;
	VkDescriptorImageInfo infos[12];
	VkWriteDescriptorSet writes[12];
	VkDescriptorBufferInfo bufInfo;
	float invView[16], view[16], projInfo[4];
	uint32_t normalsAreWorld, gx, gy;
	qboolean hasAO;
	qboolean doSsgi;
	int t0;

	struct {
		uint32_t extentMeta[4];
		uint32_t gridDim[4];
		float worldMin[4];
		float worldMax[4];
		float projInfo[4];
		float invView[16];
		float params0[4];
	} probePush;

	struct {
		uint32_t extentMeta[4];
		float projInfo[4];
		float invView[16];
		float view[16];
		float params0[4];
		float params1[4];
	} ssgiPush;

	struct {
		uint32_t extentMeta[4];
		float params0[4];
		float params1[4];
	} resolvePush;

	if ( rgi.appliedThisFrame || !vk_raster_gi_active() || !vk.cmd || !backEnd.doneSurfaces ) {
		return;
	}
	if ( vk_classify_current_view() != VK_VIEW_CLASS_MAIN_WORLD ) {
		return;
	}
	if ( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return;
	}
	/* Ultra RT lock: never run neural/RT GI alongside. */
	if ( VK_RasterUltra_Active() ) {
		VK_RasterUltra_Enforce();
	}
	if ( vk_pathtrace_active() ) {
		return;
	}

	RGI_EnsureProbes();
	if ( !rgi.probesReady || !rgi.probeIrr.image || !vk.depth_image || !vk.color_image ) {
		return;
	}

	t0 = ri.Milliseconds ? ri.Milliseconds() : 0;
	RGI_BudgetUpdate( &backEnd.refdef );
	RGI_UploadProbes();
	if ( ri.Milliseconds ) {
		rgi.lastCpuUpdateMs = (double)( ri.Milliseconds() - t0 );
	}

	vk_spine_pass_begin( VK_SPINE_PASS_RASTER_GI );
	cmd = vk.cmd->command_buffer;
	if ( glConfig.stencilBits > 0 ) {
		depthAspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	vk_end_render_pass();
	record_depth_image_layout_transition( cmd, depthAspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		0, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	depthView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	normalView = vk.deferred_gbuffer_normal_view ? vk.deferred_gbuffer_normal_view : tr.whiteImage->view;
	albedoView = vk.deferred_gbuffer_albedo_view ? vk.deferred_gbuffer_albedo_view : tr.whiteImage->view;
	nearest = RGI_Sampler( qfalse );
	linear = RGI_Sampler( qtrue );
	RGI_FillView( invView, view, projInfo, &normalsAreWorld );

	hasAO = qfalse;
	aoView = tr.whiteImage->view;
	if ( vk_ambient_visibility_available() ) {
		hasAO = qtrue;
		aoView = vk_ambient_visibility_view();
	}

	/* Probe sample */
	RGI_Transition( cmd, &rgi.probeIrr, VK_IMAGE_LAYOUT_GENERAL );
	RGI_Transition( cmd, &rgi.probeMeta, VK_IMAGE_LAYOUT_GENERAL );
	RGI_ImageWrite( &writes[0], &infos[0], rgi.probeSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		nearest, depthView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL );
	RGI_ImageWrite( &writes[1], &infos[1], rgi.probeSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		nearest, normalView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	Com_Memset( &bufInfo, 0, sizeof( bufInfo ) );
	bufInfo.buffer = rgi.probeBuffer;
	bufInfo.range = VK_WHOLE_SIZE;
	Com_Memset( &writes[2], 0, sizeof( writes[2] ) );
	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = rgi.probeSet;
	writes[2].dstBinding = 2;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[2].pBufferInfo = &bufInfo;
	RGI_ImageWrite( &writes[3], &infos[3], rgi.probeSet, 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		VK_NULL_HANDLE, rgi.probeIrr.view, VK_IMAGE_LAYOUT_GENERAL );
	RGI_ImageWrite( &writes[4], &infos[4], rgi.probeSet, 4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		VK_NULL_HANDLE, rgi.probeMeta.view, VK_IMAGE_LAYOUT_GENERAL );
	qvkUpdateDescriptorSets( vk.device, 5, writes, 0, NULL );

	Com_Memset( &probePush, 0, sizeof( probePush ) );
	probePush.extentMeta[0] = rgi.width;
	probePush.extentMeta[1] = rgi.height;
	probePush.extentMeta[2] = (uint32_t)rgi.probeCount;
	probePush.extentMeta[3] = normalsAreWorld;
	probePush.gridDim[0] = (uint32_t)rgi.gridX;
	probePush.gridDim[1] = (uint32_t)rgi.gridY;
	probePush.gridDim[2] = (uint32_t)rgi.gridZ;
	VectorCopy( rgi.worldMin, probePush.worldMin );
	VectorCopy( rgi.worldMax, probePush.worldMax );
	Com_Memcpy( probePush.projInfo, projInfo, sizeof( projInfo ) );
	Com_Memcpy( probePush.invView, invView, sizeof( invView ) );
	probePush.params0[0] = rgi.spacing;
	probePush.params0[1] = 1.0f;
	probePush.params0[2] = 1.0f;
	probePush.params0[3] = r_probeGiMinVis ? r_probeGiMinVis->value : 0.02f;

	vk_spine_note_write( VK_SPINE_RES_PROBE_IRRADIANCE, VK_SPINE_PASS_RASTER_GI, VK_SPINE_ACCESS_STORAGE_WRITE );
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rgi.probePipe );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rgi.probePL, 0, 1, &rgi.probeSet, 0, NULL );
	qvkCmdPushConstants( cmd, rgi.probePL, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( probePush ), &probePush );
	gx = ( rgi.width + 7u ) / 8u;
	gy = ( rgi.height + 7u ) / 8u;
	qvkCmdDispatch( cmd, gx, gy, 1 );
	RGI_Transition( cmd, &rgi.probeIrr, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	RGI_Transition( cmd, &rgi.probeMeta, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );

	doSsgi = ( r_ssgi && r_ssgi->integer ) ? qtrue : qfalse;
	RGI_Transition( cmd, &rgi.ssgiRad, VK_IMAGE_LAYOUT_GENERAL );
	RGI_Transition( cmd, &rgi.ssgiMeta, VK_IMAGE_LAYOUT_GENERAL );
	if ( doSsgi ) {
		/* Scene color must be readable; transition HDR. */
		record_image_layout_transition( cmd, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			0, 0 );
		RGI_ImageWrite( &writes[0], &infos[0], rgi.ssgiSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			nearest, depthView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL );
		RGI_ImageWrite( &writes[1], &infos[1], rgi.ssgiSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			nearest, normalView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		RGI_ImageWrite( &writes[2], &infos[2], rgi.ssgiSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			linear, vk.color_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		RGI_ImageWrite( &writes[3], &infos[3], rgi.ssgiSet, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			linear, albedoView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		RGI_ImageWrite( &writes[4], &infos[4], rgi.ssgiSet, 4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			VK_NULL_HANDLE, rgi.ssgiRad.view, VK_IMAGE_LAYOUT_GENERAL );
		RGI_ImageWrite( &writes[5], &infos[5], rgi.ssgiSet, 5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			VK_NULL_HANDLE, rgi.ssgiMeta.view, VK_IMAGE_LAYOUT_GENERAL );
		qvkUpdateDescriptorSets( vk.device, 6, writes, 0, NULL );

		Com_Memset( &ssgiPush, 0, sizeof( ssgiPush ) );
		ssgiPush.extentMeta[0] = rgi.width;
		ssgiPush.extentMeta[1] = rgi.height;
		ssgiPush.extentMeta[2] = rgi.frameIndex;
		ssgiPush.extentMeta[3] = normalsAreWorld;
		Com_Memcpy( ssgiPush.projInfo, projInfo, sizeof( projInfo ) );
		Com_Memcpy( ssgiPush.invView, invView, sizeof( invView ) );
		Com_Memcpy( ssgiPush.view, view, sizeof( view ) );
		ssgiPush.params0[0] = r_ssgiDistance ? r_ssgiDistance->value : 192.0f;
		ssgiPush.params0[1] = r_ssgiThickness ? r_ssgiThickness->value : 24.0f;
		ssgiPush.params0[2] = (float)( r_ssgiSteps ? r_ssgiSteps->integer : 12 );
		ssgiPush.params0[3] = r_ssgiNearScale ? r_ssgiNearScale->value : 1.0f;
		ssgiPush.params1[0] = r_ssgiEdgeFade ? r_ssgiEdgeFade->value : 32.0f;
		ssgiPush.params1[1] = 0.5f;

		vk_spine_note_write( VK_SPINE_RES_SSGI_RADIANCE, VK_SPINE_PASS_RASTER_GI, VK_SPINE_ACCESS_STORAGE_WRITE );
		qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rgi.ssgiPipe );
		qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rgi.ssgiPL, 0, 1, &rgi.ssgiSet, 0, NULL );
		qvkCmdPushConstants( cmd, rgi.ssgiPL, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( ssgiPush ), &ssgiPush );
		qvkCmdDispatch( cmd, gx, gy, 1 );
	} else {
		/* Clear SSGI outputs to zero confidence when disabled. */
		VkClearColorValue clear;
		VkImageSubresourceRange range;
		Com_Memset( &clear, 0, sizeof( clear ) );
		Com_Memset( &range, 0, sizeof( range ) );
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.levelCount = 1;
		range.layerCount = 1;
		qvkCmdClearColorImage( cmd, rgi.ssgiRad.image, VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &range );
		qvkCmdClearColorImage( cmd, rgi.ssgiMeta.image, VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &range );
	}
	RGI_Transition( cmd, &rgi.ssgiRad, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	RGI_Transition( cmd, &rgi.ssgiMeta, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );

	/* Resolve into HDR */
	record_image_layout_transition( cmd, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		0, 0 );
	RGI_ImageWrite( &writes[0], &infos[0], rgi.resolveSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		nearest, depthView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL );
	RGI_ImageWrite( &writes[1], &infos[1], rgi.resolveSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		linear, albedoView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	RGI_ImageWrite( &writes[2], &infos[2], rgi.resolveSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		linear, rgi.probeIrr.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	RGI_ImageWrite( &writes[3], &infos[3], rgi.resolveSet, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		linear, rgi.probeMeta.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	RGI_ImageWrite( &writes[4], &infos[4], rgi.resolveSet, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		linear, rgi.ssgiRad.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	RGI_ImageWrite( &writes[5], &infos[5], rgi.resolveSet, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		linear, rgi.ssgiMeta.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	RGI_ImageWrite( &writes[6], &infos[6], rgi.resolveSet, 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		linear, aoView, hasAO ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	RGI_ImageWrite( &writes[7], &infos[7], rgi.resolveSet, 7, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		VK_NULL_HANDLE, vk.color_image_view, VK_IMAGE_LAYOUT_GENERAL );
	qvkUpdateDescriptorSets( vk.device, 8, writes, 0, NULL );

	Com_Memset( &resolvePush, 0, sizeof( resolvePush ) );
	resolvePush.extentMeta[0] = rgi.width;
	resolvePush.extentMeta[1] = rgi.height;
	resolvePush.extentMeta[2] = (uint32_t)( r_rasterGiDebug ? r_rasterGiDebug->integer : 0 );
	resolvePush.extentMeta[3] = hasAO ? 1u : 0u;
	resolvePush.params0[0] = r_probeGiStrength ? r_probeGiStrength->value : 1.0f;
	resolvePush.params0[1] = ( doSsgi && r_ssgiStrength ) ? r_ssgiStrength->value : 0.0f;
	resolvePush.params0[2] = r_rasterGiAoStrength ? r_rasterGiAoStrength->value : 1.0f;
	resolvePush.params0[3] = r_rasterGiLightmapDelta ? r_rasterGiLightmapDelta->value : 1.0f;
	resolvePush.params1[2] = 1.0f; /* duplicate-energy soft clamp on */

	vk_spine_note_write( VK_SPINE_RES_INDIRECT_DIFFUSE, VK_SPINE_PASS_RASTER_GI, VK_SPINE_ACCESS_STORAGE_WRITE );
	vk_spine_note_write( VK_SPINE_RES_HDR_COLOR, VK_SPINE_PASS_RASTER_GI, VK_SPINE_ACCESS_COLOR_WRITE );
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rgi.resolvePipe );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rgi.resolvePL, 0, 1, &rgi.resolveSet, 0, NULL );
	qvkCmdPushConstants( cmd, rgi.resolvePL, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( resolvePush ), &resolvePush );
	qvkCmdDispatch( cmd, gx, gy, 1 );

	record_image_layout_transition( cmd, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0, 0 );
	record_depth_image_layout_transition( cmd, depthAspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT );

	vk_spine_pass_end( VK_SPINE_PASS_RASTER_GI );
	rgi.appliedThisFrame = qtrue;
}
