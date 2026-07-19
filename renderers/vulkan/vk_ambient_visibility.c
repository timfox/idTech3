/*
===========================================================================
Directional Ambient Visibility (GTAO + Hybrid1 TLAS ray-query RTAO).

Output encoding (RGBA16F): R diffuse visibility; GB octahedral world-space
bent normal; A open visibility-cone measure. Auxiliary RGBA16F: normalized
average hit distance, variance, temporal confidence, rejection reason.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_ambient_visibility.h"
#include "vk_deferred_gbuffer.h"
#include "vk_image_layout.h"
#include "vk_pathtrace.h"
#include "vk_rtx.h"
#include "vk_temporal.h"
#include "vk_util.h"
#include "vk_view_state.h"
#include "vk_ambient_visibility_spirv.inc"

#define AV_QUERY_COUNT 8u
#define AV_QUERY_RAW_BEGIN 0u
#define AV_QUERY_RAW_END 1u
#define AV_QUERY_TEMPORAL_END 2u
#define AV_QUERY_FILTER_END 3u
#define AV_QUERY_COMPOSITE_END 4u

typedef struct {
	VkImage image;
	VkImageView view;
	VkDeviceMemory memory;
	VkImageLayout layout;
	uint32_t width, height;
} av_image_t;

typedef struct {
	qboolean ready;
	qboolean historyValid;
	qboolean appliedThisFrame;
	qboolean fallbackLogged;
	uint32_t frame;
	uint32_t width, height;
	uint32_t traceWidth, traceHeight;
	uint32_t historyIndex;
	uint32_t gbufferGeneration;
	uint32_t historyGeneration;
	int lastMode;
	float lastRadius;

	av_image_t raw;
	av_image_t rawAux;
	av_image_t gtao;
	av_image_t gtaoAux;
	av_image_t history[2];
	av_image_t historyGeo[2];
	av_image_t historyAux[2];
	av_image_t filtered;
	av_image_t reference;
	av_image_t referenceAux;

	VkShaderModule gtaoCS, rtaoCS, temporalCS, filterCS, compositeCS;
	VkDescriptorSetLayout gtaoLayout, rtaoLayout, temporalLayout, filterLayout, compositeLayout;
	VkPipelineLayout gtaoPL, rtaoPL, temporalPL, filterPL, compositePL;
	VkPipeline gtaoPipe, rtaoPipe, temporalPipe, filterPipe, compositePipe;
	VkDescriptorPool descriptorPool;
	VkDescriptorSet gtaoSet, rtaoSet, temporalSet, filterSet, compositeSet;
	VkQueryPool queryPool;
	VkBuffer metricBuffer;
	VkDeviceMemory metricMemory;
	void *metricMapped;
	double timings[4]; /* raw, temporal, filter, total ms */
	double errors[2];  /* scalar visibility MAE, bent-normal angular MAE (degrees) */
	uint32_t errorSamples;
} av_state_t;

static av_state_t av;

static cvar_t *r_ambientVisibilityMode;
static cvar_t *r_ambientVisibilityStrength;
static cvar_t *r_rtaoRaysPerPixel;
static cvar_t *r_rtaoRadius;
static cvar_t *r_rtaoMinRadius;
static cvar_t *r_rtaoRayBias;
static cvar_t *r_rtaoNormalBias;
static cvar_t *r_rtaoResolutionScale;
static cvar_t *r_rtaoContactVisibility;
static cvar_t *r_rtaoTemporal;
static cvar_t *r_rtaoSpatialFilter;
static cvar_t *r_rtaoMaxHistory;
static cvar_t *r_rtaoDebug;
static cvar_t *r_referenceAORays;
static cvar_t *r_referenceAOSeed;
static cvar_t *r_gtaoSlices;
static cvar_t *r_gtaoSteps;
static cvar_t *r_gtaoThickness;
static cvar_t *r_gtaoHalfRes;

static qboolean AV_RayQueryAvailable( void )
{
#ifdef USE_VULKAN_RTX
	return vk.rayQueryAvailable;
#else
	return qfalse;
#endif
}

static VkSampler AV_Sampler( qboolean linear )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = linear ? GL_LINEAR : GL_NEAREST;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;
	return vk_find_sampler( &sd );
}

static void AV_DestroyImage( av_image_t *img )
{
	if ( img->view ) qvkDestroyImageView( vk.device, img->view, NULL );
	if ( img->image ) qvkDestroyImage( vk.device, img->image, NULL );
	if ( img->memory ) qvkFreeMemory( vk.device, img->memory, NULL );
	Com_Memset( img, 0, sizeof( *img ) );
}

static qboolean AV_CreateImage( av_image_t *img, uint32_t width, uint32_t height, const char *name )
{
	VkImageCreateInfo ici;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo mai;
	VkImageViewCreateInfo vci;

	AV_DestroyImage( img );
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
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK( qvkCreateImage( vk.device, &ici, NULL, &img->image ) );
	qvkGetImageMemoryRequirements( vk.device, img->image, &req );
	Com_Memset( &mai, 0, sizeof( mai ) );
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.allocationSize = req.size;
	mai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &mai, NULL, &img->memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, img->image, img->memory, 0 ) );
	Com_Memset( &vci, 0, sizeof( vci ) );
	vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	vci.image = img->image;
	vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
	vci.format = ici.format;
	vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vci.subresourceRange.levelCount = 1;
	vci.subresourceRange.layerCount = 1;
	VK_CHECK( qvkCreateImageView( vk.device, &vci, NULL, &img->view ) );
	img->layout = VK_IMAGE_LAYOUT_UNDEFINED;
	img->width = width;
	img->height = height;
	SET_OBJECT_NAME( img->image, name, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	SET_OBJECT_NAME( img->view, va( "%s view", name ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	return qtrue;
}

static void AV_Transition( VkCommandBuffer cmd, av_image_t *img, VkImageLayout layout )
{
	if ( !img->image || img->layout == layout ) return;
	record_image_layout_transition( cmd, img->image, VK_IMAGE_ASPECT_COLOR_BIT,
		img->layout, layout, 0, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	img->layout = layout;
}

static VkShaderModule AV_Module( const uint8_t *bytes, uint32_t size, const char *name )
{
	VkShaderModuleCreateInfo ci;
	VkShaderModule module = VK_NULL_HANDLE;
	Com_Memset( &ci, 0, sizeof( ci ) );
	ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.codeSize = size;
	ci.pCode = (const uint32_t *)bytes;
	VK_CHECK( qvkCreateShaderModule( vk.device, &ci, NULL, &module ) );
	SET_OBJECT_NAME( module, name, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	return module;
}

static qboolean AV_CreateLayout( const VkDescriptorType *types, uint32_t count,
	uint32_t pushSize, VkDescriptorSetLayout *setLayout, VkPipelineLayout *pipelineLayout )
{
	VkDescriptorSetLayoutBinding bindings[12];
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
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &dci, NULL, setLayout ) );
	Com_Memset( &range, 0, sizeof( range ) );
	range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	range.size = pushSize;
	Com_Memset( &pci, 0, sizeof( pci ) );
	pci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pci.setLayoutCount = 1;
	pci.pSetLayouts = setLayout;
	pci.pushConstantRangeCount = 1;
	pci.pPushConstantRanges = &range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pci, NULL, pipelineLayout ) );
	return qtrue;
}

static VkPipeline AV_CreateComputePipeline( VkShaderModule module, VkPipelineLayout layout, const char *name )
{
	VkComputePipelineCreateInfo ci;
	VkPipeline pipeline = VK_NULL_HANDLE;
	Com_Memset( &ci, 0, sizeof( ci ) );
	ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	ci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	ci.stage.module = module;
	ci.stage.pName = "main";
	ci.layout = layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, vk.pipelineCache, 1, &ci, NULL, &pipeline ) );
	SET_OBJECT_NAME( pipeline, name, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
	return pipeline;
}

static void AV_DestroyPipelines( void )
{
#define AV_DESTROY( fn, x ) do { if ( av.x ) { fn( vk.device, av.x, NULL ); av.x = VK_NULL_HANDLE; } } while ( 0 )
	AV_DESTROY( qvkDestroyPipeline, gtaoPipe ); AV_DESTROY( qvkDestroyPipeline, rtaoPipe );
	AV_DESTROY( qvkDestroyPipeline, temporalPipe ); AV_DESTROY( qvkDestroyPipeline, filterPipe );
	AV_DESTROY( qvkDestroyPipeline, compositePipe );
	AV_DESTROY( qvkDestroyPipelineLayout, gtaoPL ); AV_DESTROY( qvkDestroyPipelineLayout, rtaoPL );
	AV_DESTROY( qvkDestroyPipelineLayout, temporalPL ); AV_DESTROY( qvkDestroyPipelineLayout, filterPL );
	AV_DESTROY( qvkDestroyPipelineLayout, compositePL );
	AV_DESTROY( qvkDestroyDescriptorSetLayout, gtaoLayout ); AV_DESTROY( qvkDestroyDescriptorSetLayout, rtaoLayout );
	AV_DESTROY( qvkDestroyDescriptorSetLayout, temporalLayout ); AV_DESTROY( qvkDestroyDescriptorSetLayout, filterLayout );
	AV_DESTROY( qvkDestroyDescriptorSetLayout, compositeLayout );
	AV_DESTROY( qvkDestroyShaderModule, gtaoCS ); AV_DESTROY( qvkDestroyShaderModule, rtaoCS );
	AV_DESTROY( qvkDestroyShaderModule, temporalCS ); AV_DESTROY( qvkDestroyShaderModule, filterCS );
	AV_DESTROY( qvkDestroyShaderModule, compositeCS );
	AV_DESTROY( qvkDestroyDescriptorPool, descriptorPool );
#undef AV_DESTROY
}

static qboolean AV_CreatePipelines( void )
{
	static const VkDescriptorType gtaoTypes[] = {
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE };
	static const VkDescriptorType rtaoTypes[] = {
		VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE };
	static const VkDescriptorType temporalTypes[] = {
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE };
	static const VkDescriptorType filterTypes[] = {
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE };
	static const VkDescriptorType compositeTypes[] = {
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER };
	VkDescriptorPoolSize sizes[4];
	VkDescriptorPoolCreateInfo poolCI;
	VkDescriptorSetAllocateInfo ai;
	VkDescriptorSetLayout layouts[5];
	VkDescriptorSet sets[5];
	uint32_t setCount = AV_RayQueryAvailable() ? 5u : 4u;

	av.gtaoCS = AV_Module( vk_av_gtao_cs_spv, VK_AV_GTAO_CS_SPV_SIZE, "Ambient Visibility GTAO" );
	av.temporalCS = AV_Module( vk_av_temporal_cs_spv, VK_AV_TEMPORAL_CS_SPV_SIZE, "Ambient Visibility temporal" );
	av.filterCS = AV_Module( vk_av_filter_cs_spv, VK_AV_FILTER_CS_SPV_SIZE, "Ambient Visibility filter" );
	av.compositeCS = AV_Module( vk_av_composite_cs_spv, VK_AV_COMPOSITE_CS_SPV_SIZE, "Ambient Visibility composite" );
	if ( AV_RayQueryAvailable() )
		av.rtaoCS = AV_Module( vk_av_rtao_cs_spv, VK_AV_RTAO_CS_SPV_SIZE, "Ambient Visibility ray query" );

	AV_CreateLayout( gtaoTypes, ARRAY_LEN( gtaoTypes ), 128u, &av.gtaoLayout, &av.gtaoPL );
	if ( AV_RayQueryAvailable() ) AV_CreateLayout( rtaoTypes, ARRAY_LEN( rtaoTypes ), 128u, &av.rtaoLayout, &av.rtaoPL );
	AV_CreateLayout( temporalTypes, ARRAY_LEN( temporalTypes ), 112u, &av.temporalLayout, &av.temporalPL );
	AV_CreateLayout( filterTypes, ARRAY_LEN( filterTypes ), 96u, &av.filterLayout, &av.filterPL );
	AV_CreateLayout( compositeTypes, ARRAY_LEN( compositeTypes ), 32u, &av.compositeLayout, &av.compositePL );
	av.gtaoPipe = AV_CreateComputePipeline( av.gtaoCS, av.gtaoPL, "Ambient Visibility GTAO pipeline" );
	if ( AV_RayQueryAvailable() ) av.rtaoPipe = AV_CreateComputePipeline( av.rtaoCS, av.rtaoPL, "Ambient Visibility RTAO pipeline" );
	av.temporalPipe = AV_CreateComputePipeline( av.temporalCS, av.temporalPL, "Ambient Visibility temporal pipeline" );
	av.filterPipe = AV_CreateComputePipeline( av.filterCS, av.filterPL, "Ambient Visibility filter pipeline" );
	av.compositePipe = AV_CreateComputePipeline( av.compositeCS, av.compositePL, "Ambient Visibility composite pipeline" );

	Com_Memset( sizes, 0, sizeof( sizes ) );
	sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; sizes[0].descriptorCount = 40;
	sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; sizes[1].descriptorCount = 16;
	sizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; sizes[2].descriptorCount = 2;
	sizes[3].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR; sizes[3].descriptorCount = AV_RayQueryAvailable() ? 2u : 0u;
	Com_Memset( &poolCI, 0, sizeof( poolCI ) );
	poolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCI.maxSets = 5;
	poolCI.poolSizeCount = AV_RayQueryAvailable() ? 4u : 3u;
	poolCI.pPoolSizes = sizes;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &poolCI, NULL, &av.descriptorPool ) );
	layouts[0] = av.gtaoLayout;
	layouts[1] = av.temporalLayout;
	layouts[2] = av.filterLayout;
	layouts[3] = av.compositeLayout;
	if ( AV_RayQueryAvailable() ) layouts[4] = av.rtaoLayout;
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	ai.descriptorPool = av.descriptorPool;
	ai.descriptorSetCount = setCount;
	ai.pSetLayouts = layouts;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &ai, sets ) );
	av.gtaoSet = sets[0]; av.temporalSet = sets[1]; av.filterSet = sets[2]; av.compositeSet = sets[3];
	if ( AV_RayQueryAvailable() ) av.rtaoSet = sets[4];
	return qtrue;
}

static qboolean AV_CreateMetricBuffer( void )
{
	VkBufferCreateInfo bci;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo mai;
	VkDeviceSize size = (VkDeviceSize)NUM_COMMAND_BUFFERS * sizeof( uint32_t ) * 4u;

	Com_Memset( &bci, 0, sizeof( bci ) );
	bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.size = size;
	bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &bci, NULL, &av.metricBuffer ) );
	qvkGetBufferMemoryRequirements( vk.device, av.metricBuffer, &req );
	Com_Memset( &mai, 0, sizeof( mai ) );
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.allocationSize = req.size;
	mai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &mai, NULL, &av.metricMemory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, av.metricBuffer, av.metricMemory, 0 ) );
	VK_CHECK( qvkMapMemory( vk.device, av.metricMemory, 0, size, 0, &av.metricMapped ) );
	Com_Memset( av.metricMapped, 0, (size_t)size );
	SET_OBJECT_NAME( av.metricBuffer, "Ambient Visibility reference error reduction", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	return qtrue;
}

static void AV_RegisterCvars( void )
{
	if ( r_ambientVisibilityMode ) return;
	/* These three values change image extents. Latching them prevents a live cvar
	 * edit from destroying images that an older command buffer can still sample. */
	r_ambientVisibilityMode = ri.Cvar_Get( "r_ambientVisibilityMode", "2", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_ambientVisibilityStrength = ri.Cvar_Get( "r_ambientVisibilityStrength", "1", CVAR_ARCHIVE_ND );
	r_rtaoRaysPerPixel = ri.Cvar_Get( "r_rtaoRaysPerPixel", "1", CVAR_ARCHIVE_ND );
	r_rtaoRadius = ri.Cvar_Get( "r_rtaoRadius", "96", CVAR_ARCHIVE_ND );
	r_rtaoMinRadius = ri.Cvar_Get( "r_rtaoMinRadius", "12", CVAR_ARCHIVE_ND );
	r_rtaoRayBias = ri.Cvar_Get( "r_rtaoRayBias", "0.02", CVAR_ARCHIVE_ND );
	r_rtaoNormalBias = ri.Cvar_Get( "r_rtaoNormalBias", "0.05", CVAR_ARCHIVE_ND );
	r_rtaoResolutionScale = ri.Cvar_Get( "r_rtaoResolutionScale", "0.5", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_rtaoContactVisibility = ri.Cvar_Get( "r_rtaoContactVisibility", "1", CVAR_ARCHIVE_ND );
	r_rtaoTemporal = ri.Cvar_Get( "r_rtaoTemporal", "1", CVAR_ARCHIVE_ND );
	r_rtaoSpatialFilter = ri.Cvar_Get( "r_rtaoSpatialFilter", "1", CVAR_ARCHIVE_ND );
	r_rtaoMaxHistory = ri.Cvar_Get( "r_rtaoMaxHistory", "16", CVAR_ARCHIVE_ND );
	r_rtaoDebug = ri.Cvar_Get( "r_rtaoDebug", "0", CVAR_ARCHIVE_ND );
	r_referenceAORays = ri.Cvar_Get( "r_referenceAORays", "64", CVAR_ARCHIVE_ND );
	r_referenceAOSeed = ri.Cvar_Get( "r_referenceAOSeed", "1", CVAR_ARCHIVE_ND );
	r_gtaoSlices = ri.Cvar_Get( "r_gtaoSlices", "4", CVAR_ARCHIVE_ND );
	r_gtaoSteps = ri.Cvar_Get( "r_gtaoSteps", "6", CVAR_ARCHIVE_ND );
	r_gtaoThickness = ri.Cvar_Get( "r_gtaoThickness", "2", CVAR_ARCHIVE_ND );
	r_gtaoHalfRes = ri.Cvar_Get( "r_gtaoHalfRes", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_ambientVisibilityMode, "0", "5", CV_INTEGER );
	ri.Cvar_CheckRange( r_ambientVisibilityStrength, "0", "1", CV_FLOAT );
	ri.Cvar_CheckRange( r_rtaoRaysPerPixel, "1", "8", CV_INTEGER );
	ri.Cvar_CheckRange( r_rtaoRadius, "1", "2048", CV_FLOAT );
	ri.Cvar_CheckRange( r_rtaoMinRadius, "0.1", "512", CV_FLOAT );
	ri.Cvar_CheckRange( r_rtaoResolutionScale, "0.25", "1", CV_FLOAT );
	ri.Cvar_CheckRange( r_rtaoMaxHistory, "1", "64", CV_INTEGER );
	ri.Cvar_CheckRange( r_rtaoDebug, "0", "20", CV_INTEGER );
	ri.Cvar_CheckRange( r_referenceAORays, "16", "256", CV_INTEGER );
	ri.Cvar_CheckRange( r_gtaoSlices, "2", "8", CV_INTEGER );
	ri.Cvar_CheckRange( r_gtaoSteps, "2", "8", CV_INTEGER );
	ri.Cvar_SetDescription( r_ambientVisibilityMode,
		"Ambient Visibility: 0 off, 1 legacy SSAO, 2 GTAO, 3 ray-query RTAO, 4 adaptive hybrid, 5 Reference AO." );
	ri.Cvar_SetDescription( r_rtaoDebug,
		"Ambient Visibility debug: 1 GTAO, 2 raw/final RTAO, 4 bent normal, 5 cone, 6 hit distance, 7 variance, 8 confidence, 10 rejection, 14 difference, 15 reference error, 16 bent error, 17 specular occlusion, 20 edge fallback." );
}

static void AV_Status_f( void )
{
	const char *tlasMode = "n/a", *tlasReason = "not requested";
	int requested = r_ambientVisibilityMode ? r_ambientVisibilityMode->integer : 0;
	if ( requested >= 3 ) vk_rtx_tlas_status( &tlasMode, &tlasReason );
	ri.Printf( PRINT_ALL,
		"[AV] requested=%d effective=%s ready=%d output=%ux%u trace=%ux%u rays=%d history=%s bentNormal=on\n",
		requested, requested == 0 ? "off" : requested == 1 ? "legacy" : requested == 2 ? "GTAO" :
		( av.rtaoPipe && vk_rtx_scene_ready() ? ( requested == 5 ? "Reference AO" : requested == 4 ? "Hybrid" : "RTAO" ) : "GTAO fallback" ),
		av.ready, av.width, av.height, av.traceWidth, av.traceHeight,
		requested == 5 ? r_referenceAORays->integer : r_rtaoRaysPerPixel->integer,
		av.historyValid ? "valid" : "reset(unoccluded)" );
	ri.Printf( PRINT_ALL,
		"[AV] gen gbuffer=%u history=%u match=%s viewClass=%s\n",
		av.gbufferGeneration, av.historyGeneration,
		( av.gbufferGeneration == vk_deferred_gbuffer_generation() &&
		  av.historyGeneration == av.gbufferGeneration ) ? "yes" : "no",
		vk_view_class_name( vk_classify_current_view() ) );
	ri.Printf( PRINT_ALL,
		"[AV] rayQuery=%d TLAS=%s (%s) memory~%.2f MiB timings raw=%.3f temporal=%.3f filter=%.3f total=%.3f ms\n",
		AV_RayQueryAvailable(), tlasMode, tlasReason,
		( (double)av.traceWidth * av.traceHeight * 8.0 * 11.0 +
		  (double)av.width * av.height * 8.0 * 2.0 ) / ( 1024.0 * 1024.0 ),
		av.timings[0], av.timings[1], av.timings[2], av.timings[3] );
	ri.Printf( PRINT_ALL,
		"[AV] reference error samples=%u visibility_MAE=%.6f bent_normal_MAE=%.3f degrees%s\n",
		av.errorSamples, av.errors[0], av.errors[1],
		av.errorSamples ? "" : " (enable r_rtaoDebug 15 or 16)" );
}

static void AV_Reset_f( void )
{
	vk_ambient_visibility_reset_history();
	ri.Printf( PRINT_ALL, "[AV] Ambient Visibility History reset\n" );
}

static qboolean AV_EnsureImages( uint32_t width, uint32_t height )
{
	float scale;
	uint32_t tw, th;
	int mode = r_ambientVisibilityMode ? r_ambientVisibilityMode->integer : 0;
	int i;
	if ( mode == 2 ) scale = ( r_gtaoHalfRes && r_gtaoHalfRes->integer ) ? 0.5f : 1.0f;
	else scale = r_rtaoResolutionScale ? r_rtaoResolutionScale->value : 0.5f;
	if ( mode == 5 ) scale = 1.0f;
	tw = MAX( 1u, (uint32_t)( width * scale + 0.5f ) );
	th = MAX( 1u, (uint32_t)( height * scale + 0.5f ) );
	if ( av.width == width && av.height == height && av.traceWidth == tw && av.traceHeight == th && av.raw.image ) return qtrue;
	AV_CreateImage( &av.raw, tw, th, "Ambient Visibility raw" );
	AV_CreateImage( &av.rawAux, tw, th, "Ambient Visibility raw auxiliary" );
	AV_CreateImage( &av.gtao, tw, th, "Ambient Visibility GTAO" );
	AV_CreateImage( &av.gtaoAux, tw, th, "Ambient Visibility GTAO auxiliary" );
	for ( i = 0; i < 2; ++i ) {
		AV_CreateImage( &av.history[i], tw, th, va( "Ambient Visibility History %d", i ) );
		AV_CreateImage( &av.historyGeo[i], tw, th, va( "Ambient Visibility History Geometry %d", i ) );
		AV_CreateImage( &av.historyAux[i], tw, th, va( "Ambient Visibility History Auxiliary %d", i ) );
	}
	AV_CreateImage( &av.filtered, tw, th, "Ambient Visibility filtered" );
	AV_CreateImage( &av.reference, width, height, "Reference AO" );
	AV_CreateImage( &av.referenceAux, width, height, "Reference AO auxiliary" );
	av.width = width; av.height = height; av.traceWidth = tw; av.traceHeight = th;
	vk_ambient_visibility_reset_history();
	return qtrue;
}

static void AV_FillView( float invView[16], float projInfo[4], uint32_t *normalsAreWorld )
{
	const float *view = backEnd.viewParms.world.modelViewMatrix;
	const float *projection = backEnd.useFirstPersonProjection ? backEnd.firstPersonProjectionMatrix : backEnd.viewParms.projectionMatrix;
	float projVK[16];
	if ( !vk_mat4_inverse( view, invView ) ) Com_Memcpy( invView, view, sizeof( float ) * 16 );
	vk_get_projection_matrix_vk( projection, projVK );
	projInfo[0] = projVK[0] != 0.0f ? 1.0f / projVK[0] : 1.0f;
	projInfo[1] = projVK[5] != 0.0f ? 1.0f / projVK[5] : 1.0f;
	projInfo[2] = projVK[10]; projInfo[3] = projVK[14];
	*normalsAreWorld = vk.deferredGbufferDirectExport ? 1u : 0u;
}

static void AV_ImageWrite( VkWriteDescriptorSet *w, VkDescriptorImageInfo *info,
	VkDescriptorSet set, uint32_t binding, VkDescriptorType type, VkSampler sampler,
	VkImageView view, VkImageLayout layout )
{
	Com_Memset( info, 0, sizeof( *info ) );
	info->sampler = sampler; info->imageView = view; info->imageLayout = layout;
	Com_Memset( w, 0, sizeof( *w ) );
	w->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	w->dstSet = set; w->dstBinding = binding; w->descriptorCount = 1;
	w->descriptorType = type; w->pImageInfo = info;
}

static void AV_Barrier( VkCommandBuffer cmd )
{
	VkMemoryBarrier b;
	Com_Memset( &b, 0, sizeof( b ) );
	b.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	b.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 1, &b, 0, NULL, 0, NULL );
}

static void AV_WriteTimestamp( VkCommandBuffer cmd, uint32_t index )
{
	if ( av.queryPool && qvkCmdWriteTimestamp )
		qvkCmdWriteTimestamp( cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, av.queryPool,
			( vk.cmd_index * AV_QUERY_COUNT ) + index );
}

static void AV_ReadTimings( void )
{
	uint64_t q[AV_QUERY_COUNT * 2];
	VkResult result;
	uint32_t base;
	double ms;
	if ( !av.queryPool || !qvkGetQueryPoolResults ) return;
	base = vk.cmd_index * AV_QUERY_COUNT;
	Com_Memset( q, 0, sizeof( q ) );
	result = qvkGetQueryPoolResults( vk.device, av.queryPool, base, AV_QUERY_COUNT, sizeof( q ), q,
		sizeof( uint64_t ) * 2, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT );
	if ( result != VK_SUCCESS && result != VK_NOT_READY ) return;
	if ( !q[1] || !q[3] || !q[5] || !q[7] || !q[9] ) return;
	ms = (double)vk.volumetric_timestamp_period_ns * 1e-6;
	av.timings[0] = (double)( q[2] - q[0] ) * ms;
	av.timings[1] = (double)( q[4] - q[2] ) * ms;
	av.timings[2] = (double)( q[6] - q[4] ) * ms;
	av.timings[3] = (double)( q[8] - q[0] ) * ms;
	if ( av.metricMapped ) {
		const uint32_t *m = (const uint32_t *)av.metricMapped + vk.cmd_index * 4u;
		av.errorSamples = m[2];
		if ( m[2] > 0u ) {
			av.errors[0] = (double)m[0] / ( (double)m[2] * 2048.0 );
			av.errors[1] = (double)m[1] / ( (double)m[2] * 2048.0 ) * 180.0;
		}
	}
}

void vk_ambient_visibility_reset_history( void )
{
	/* historyValid=false forces temporal CS to seed unoccluded defaults (not black AO). */
	av.historyValid = qfalse;
	av.historyIndex = 0u;
	av.historyGeneration = vk_deferred_gbuffer_generation();
}

qboolean vk_ambient_visibility_active( void )
{
	return av.ready && r_ambientVisibilityMode && r_ambientVisibilityMode->integer >= 2 &&
		vk.fboActive && vk_deferred_gbuffer_active() &&
		vk_deferred_gbuffer_generation_valid() ? qtrue : qfalse;
}

qboolean vk_ambient_visibility_blocks_legacy_post( void )
{
	if ( vk_pathtrace_active() ) return qtrue;
	/* Mode 0 = AV off (do not block r_ssao). Mode 1 = explicit legacy SSAO owner.
	 * Mode >= 2 = AV owns AO and suppresses the post SSAO pass. */
	return ( r_ambientVisibilityMode && r_ambientVisibilityMode->integer >= 2 ) ? qtrue : qfalse;
}

qboolean vk_ambient_visibility_available( void )
{
	return vk_ambient_visibility_active() && av.appliedThisFrame ? qtrue : qfalse;
}

VkImageView vk_ambient_visibility_view( void )
{
	if ( !vk_ambient_visibility_available() ) return VK_NULL_HANDLE;
	return ( r_ambientVisibilityMode && r_ambientVisibilityMode->integer == 5 ) ? av.reference.view : av.filtered.view;
}

float vk_ambient_visibility_strength( void )
{
	return vk_ambient_visibility_available() && r_ambientVisibilityStrength ?
		r_ambientVisibilityStrength->value : 0.0f;
}

void vk_ambient_visibility_init( void )
{
	VkQueryPoolCreateInfo qci;
	cvar_t *failInject;

	AV_RegisterCvars();
	ri.Cvar_Get( "r_avFailInject", "0", CVAR_TEMP | CVAR_CHEAT );
	if ( av.ready || !vk.device || !vk.fboActive ) return;
	failInject = ri.Cvar_Get( "r_avFailInject", "0", CVAR_TEMP | CVAR_CHEAT );
	if ( failInject && failInject->string &&
		( !Q_stricmp( failInject->string, "history" ) || !Q_stricmp( failInject->string, "rtao" ) ||
		  !Q_stricmp( failInject->string, "all" ) ) ) {
		static qboolean s_avFailLogged;
		if ( !s_avFailLogged ) {
			ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
				"[AV] r_avFailInject=%s — skipping AV init (stable AO owner remains)\n" S_COLOR_WHITE,
				failInject->string );
			s_avFailLogged = qtrue;
		}
		vk_deferred_gbuffer_set_fallback( va( "r_avFailInject=%s", failInject->string ) );
		if ( ri.Cvar_VariableIntegerValue( "r_ambientVisibilityMode" ) >= 2 ) {
			ri.Cvar_Set( "r_ambientVisibilityMode", "1" );
		}
		if ( r_ssao && !r_ssao->integer ) {
			ri.Cvar_Set( "r_ssao", "1" );
		}
		return;
	}
	if ( !AV_CreatePipelines() ) { AV_DestroyPipelines(); return; }
	AV_CreateMetricBuffer();
	if ( qvkCreateQueryPool && qvkCmdWriteTimestamp && qvkGetQueryPoolResults ) {
		Com_Memset( &qci, 0, sizeof( qci ) );
		qci.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		qci.queryType = VK_QUERY_TYPE_TIMESTAMP;
		qci.queryCount = NUM_COMMAND_BUFFERS * AV_QUERY_COUNT;
		if ( qvkCreateQueryPool( vk.device, &qci, NULL, &av.queryPool ) == VK_SUCCESS )
			SET_OBJECT_NAME( av.queryPool, "Ambient Visibility timestamps", VK_DEBUG_REPORT_OBJECT_TYPE_QUERY_POOL_EXT );
	}
	if ( ri.Cmd_RemoveCommand ) ri.Cmd_RemoveCommand( "ambient_visibility_status" );
	if ( ri.Cmd_AddCommand ) {
		ri.Cmd_AddCommand( "ambient_visibility_status", AV_Status_f );
		ri.Cmd_AddCommand( "ambient_visibility_reset", AV_Reset_f );
	}
	av.ready = qtrue;
	av.lastMode = -1;
	av.gbufferGeneration = vk_deferred_gbuffer_generation();
	av.historyGeneration = av.gbufferGeneration;
	ri.Printf( PRINT_ALL, "[AV] directional Ambient Visibility initialized (GTAO%s)\n",
		av.rtaoPipe ? " + Hybrid1 TLAS ray query" : "; RTAO unavailable, safe GTAO fallback" );
}

void vk_ambient_visibility_shutdown( void )
{
	int i;
	if ( ri.Cmd_RemoveCommand ) {
		ri.Cmd_RemoveCommand( "ambient_visibility_status" );
		ri.Cmd_RemoveCommand( "ambient_visibility_reset" );
	}
	if ( av.queryPool ) qvkDestroyQueryPool( vk.device, av.queryPool, NULL );
	if ( av.metricMapped ) qvkUnmapMemory( vk.device, av.metricMemory );
	if ( av.metricBuffer ) qvkDestroyBuffer( vk.device, av.metricBuffer, NULL );
	if ( av.metricMemory ) qvkFreeMemory( vk.device, av.metricMemory, NULL );
	AV_DestroyPipelines();
	AV_DestroyImage( &av.raw ); AV_DestroyImage( &av.rawAux );
	AV_DestroyImage( &av.gtao ); AV_DestroyImage( &av.gtaoAux );
	for ( i = 0; i < 2; ++i ) {
		AV_DestroyImage( &av.history[i] ); AV_DestroyImage( &av.historyGeo[i] ); AV_DestroyImage( &av.historyAux[i] );
	}
	AV_DestroyImage( &av.filtered ); AV_DestroyImage( &av.reference ); AV_DestroyImage( &av.referenceAux );
	Com_Memset( &av, 0, sizeof( av ) );
}

void vk_ambient_visibility_frame_begin( void )
{
	uint32_t width, height;
	uint32_t gbufGen;

	AV_RegisterCvars();
	if ( !av.ready && vk.device && vk.fboActive ) vk_ambient_visibility_init();
	if ( !av.ready ) return;
	AV_ReadTimings();
	av.appliedThisFrame = qfalse;
	width = vk_get_render_target_width(); height = vk_get_render_target_height();
	if ( width && height ) AV_EnsureImages( width, height );

	gbufGen = vk_deferred_gbuffer_generation();
	/*
	 * Do not reset on weapon/UI view-class flicker — RDF_NOWORLDMODEL flips every
	 * frame after the world pass (same rule as TAA). Apply is already main-world-only.
	 * Portal/mirror never own AV history because apply refuses those view classes.
	 */
	if ( av.gbufferGeneration != gbufGen ||
		av.historyGeneration != gbufGen ||
		av.lastMode != r_ambientVisibilityMode->integer ||
		av.lastRadius != r_rtaoRadius->value ||
		vk.temporal.appliedResetReasons != 0u ) {
		vk_ambient_visibility_reset_history();
		av.gbufferGeneration = gbufGen;
		av.lastMode = r_ambientVisibilityMode->integer;
		av.lastRadius = r_rtaoRadius->value;
	}
}

void vk_ambient_visibility_apply_after_geometry( void )
{
	VkCommandBuffer cmd;
	VkImageAspectFlags depthAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	VkImageView depthView, normalView, classView, motionView;
	VkSampler nearest = AV_Sampler( qfalse ), linear = AV_Sampler( qtrue );
	VkDescriptorImageInfo infos[12];
	VkWriteDescriptorSet writes[12];
	VkDescriptorBufferInfo metricInfo;
	float invView[16], projInfo[4];
	uint32_t normalsAreWorld, readIndex, writeIndex, gx, gy, mode, effectiveMode;
	qboolean rtReady = qfalse, needReference = qfalse;
	av_image_t *current, *currentAux, *finalImage;

	struct { uint32_t ef[4]; float proj[4]; float inv[16]; float p0[4]; float p1[4]; } gtaoPush;
	struct { uint32_t ef[4]; float proj[4]; float inv[16]; float p0[4]; uint32_t p1[4]; } rtaoPush;
	struct { uint32_t ef[4]; float inv[16]; float p0[4]; float p1[4]; } temporalPush;
	struct { uint32_t ef[4]; float inv[16]; float p[4]; } filterPush;
	struct { uint32_t em[4]; float p[4]; } compositePush;

	if ( av.appliedThisFrame || !vk_ambient_visibility_active() || !vk.cmd || !backEnd.doneSurfaces ) return;
	if ( vk_classify_current_view() != VK_VIEW_CLASS_MAIN_WORLD ) return;
	if ( !vk_deferred_gbuffer_generation_valid() ||
		av.gbufferGeneration != vk_deferred_gbuffer_generation() ) {
		vk_ambient_visibility_reset_history();
		return;
	}
	if ( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) return;
	if ( vk_pathtrace_active() ) {
		static qboolean warned;
		if ( !warned ) {
			ri.Printf( PRINT_WARNING, "[AV] full path tracing disables standalone ambient visibility to avoid double-darkening traced transport\n" );
			warned = qtrue;
		}
		return;
	}
	if ( !av.raw.image || !vk.depth_image ) return;
	cmd = vk.cmd->command_buffer;
	mode = (uint32_t)r_ambientVisibilityMode->integer;
	effectiveMode = mode;
	if ( mode >= 3u ) {
#ifdef USE_VULKAN_RTX
		vk_rtx_scene_prepare();
		rtReady = av.rtaoPipe && vk.rayQueryAvailable && vk_rtx_scene_ready();
#endif
		if ( !rtReady ) {
			effectiveMode = 2u;
			if ( !av.fallbackLogged ) {
				ri.Printf( PRINT_WARNING, "[AV] RTAO requested but ray query/TLAS is unavailable; using GTAO (legacy remains available as mode 1)\n" );
				av.fallbackLogged = qtrue;
			}
		}
	}
	if ( glConfig.stencilBits > 0 ) depthAspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	vk_end_render_pass();
	record_depth_image_layout_transition( cmd, depthAspect, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		0, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	depthView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	normalView = vk.deferred_gbuffer_normal_view ? vk.deferred_gbuffer_normal_view : tr.whiteImage->view;
	classView = vk.visibility_buffer_class_view ? vk.visibility_buffer_class_view : vk.deferred_class_stub_view;
	motionView = vk.motion_vector_view ? vk.motion_vector_view : tr.whiteImage->view;
	AV_FillView( invView, projInfo, &normalsAreWorld );
	if ( av.queryPool && qvkCmdResetQueryPool )
		qvkCmdResetQueryPool( cmd, av.queryPool, vk.cmd_index * AV_QUERY_COUNT, AV_QUERY_COUNT );
	AV_WriteTimestamp( cmd, AV_QUERY_RAW_BEGIN );

	/* Every statically declared sampled descriptor must name an image in the
	 * advertised layout even when its debug/Hybrid branch is dynamically unused. */
	AV_Transition( cmd, &av.gtao, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	AV_Transition( cmd, &av.gtaoAux, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	AV_Transition( cmd, &av.reference, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	AV_Transition( cmd, &av.referenceAux, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );

	/* GTAO production/fallback and hybrid current-frame assistance. */
	if ( effectiveMode == 2u || effectiveMode == 4u ) {
		AV_Transition( cmd, &av.gtao, VK_IMAGE_LAYOUT_GENERAL );
		AV_Transition( cmd, &av.gtaoAux, VK_IMAGE_LAYOUT_GENERAL );
		AV_ImageWrite( &writes[0], &infos[0], av.gtaoSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nearest, depthView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL );
		AV_ImageWrite( &writes[1], &infos[1], av.gtaoSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nearest, normalView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		AV_ImageWrite( &writes[2], &infos[2], av.gtaoSet, 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_NULL_HANDLE, av.gtao.view, VK_IMAGE_LAYOUT_GENERAL );
		AV_ImageWrite( &writes[3], &infos[3], av.gtaoSet, 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_NULL_HANDLE, av.gtaoAux.view, VK_IMAGE_LAYOUT_GENERAL );
		qvkUpdateDescriptorSets( vk.device, 4, writes, 0, NULL );
		Com_Memset( &gtaoPush, 0, sizeof( gtaoPush ) );
		gtaoPush.ef[0] = av.traceWidth; gtaoPush.ef[1] = av.traceHeight; gtaoPush.ef[2] = av.frame; gtaoPush.ef[3] = normalsAreWorld;
		Com_Memcpy( gtaoPush.proj, projInfo, sizeof( projInfo ) ); Com_Memcpy( gtaoPush.inv, invView, sizeof( invView ) );
		gtaoPush.p0[0] = r_rtaoRadius->value; gtaoPush.p0[1] = r_gtaoThickness->value;
		gtaoPush.p0[2] = 0.65f; gtaoPush.p0[3] = 24.0f;
		gtaoPush.p1[0] = (float)r_gtaoSlices->integer; gtaoPush.p1[1] = (float)r_gtaoSteps->integer;
		qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, av.gtaoPipe );
		qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, av.gtaoPL, 0, 1, &av.gtaoSet, 0, NULL );
		qvkCmdPushConstants( cmd, av.gtaoPL, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( gtaoPush ), &gtaoPush );
		qvkCmdDispatch( cmd, ( av.traceWidth + 7u ) / 8u, ( av.traceHeight + 7u ) / 8u, 1 );
		AV_Transition( cmd, &av.gtao, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		AV_Transition( cmd, &av.gtaoAux, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	}

	/* Production RTAO, Reference AO, or both for error visualization. */
	needReference = rtReady && ( mode == 5u || ( r_rtaoDebug->integer >= 15 && r_rtaoDebug->integer <= 16 ) );
	if ( rtReady && effectiveMode >= 3u ) {
		av_image_t *out = mode == 5u ? &av.reference : &av.raw;
		av_image_t *outAux = mode == 5u ? &av.referenceAux : &av.rawAux;
		uint32_t outW = mode == 5u ? av.width : av.traceWidth;
		uint32_t outH = mode == 5u ? av.height : av.traceHeight;
		AV_Transition( cmd, out, VK_IMAGE_LAYOUT_GENERAL ); AV_Transition( cmd, outAux, VK_IMAGE_LAYOUT_GENERAL );
		AV_ImageWrite( &writes[0], &infos[0], av.rtaoSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nearest, depthView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL );
		AV_ImageWrite( &writes[1], &infos[1], av.rtaoSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nearest, normalView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		AV_ImageWrite( &writes[2], &infos[2], av.rtaoSet, 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_NULL_HANDLE, out->view, VK_IMAGE_LAYOUT_GENERAL );
		AV_ImageWrite( &writes[3], &infos[3], av.rtaoSet, 4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_NULL_HANDLE, outAux->view, VK_IMAGE_LAYOUT_GENERAL );
		qvkUpdateDescriptorSets( vk.device, 4, writes, 0, NULL ); vk_rtx_bind_tlas_descriptor( av.rtaoSet );
		Com_Memset( &rtaoPush, 0, sizeof( rtaoPush ) );
		rtaoPush.ef[0] = outW; rtaoPush.ef[1] = outH; rtaoPush.ef[2] = av.frame; rtaoPush.ef[3] = normalsAreWorld;
		Com_Memcpy( rtaoPush.proj, projInfo, sizeof( projInfo ) ); Com_Memcpy( rtaoPush.inv, invView, sizeof( invView ) );
		rtaoPush.p0[0] = r_rtaoRadius->value;
		rtaoPush.p0[1] = r_rtaoContactVisibility->integer ? r_rtaoMinRadius->value : r_rtaoRadius->value;
		rtaoPush.p0[2] = r_rtaoRayBias->value; rtaoPush.p0[3] = r_rtaoNormalBias->value;
		rtaoPush.p1[0] = mode == 5u ? (uint32_t)r_referenceAORays->integer : (uint32_t)r_rtaoRaysPerPixel->integer;
		rtaoPush.p1[1] = mode == 5u ? (uint32_t)r_referenceAOSeed->integer : 0u; rtaoPush.p1[2] = mode == 5u;
		qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, av.rtaoPipe );
		qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, av.rtaoPL, 0, 1, &av.rtaoSet, 0, NULL );
		qvkCmdPushConstants( cmd, av.rtaoPL, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( rtaoPush ), &rtaoPush );
		qvkCmdDispatch( cmd, ( outW + 7u ) / 8u, ( outH + 7u ) / 8u, 1 );
		AV_Transition( cmd, out, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ); AV_Transition( cmd, outAux, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	}
	if ( needReference && mode != 5u ) {
		/* Deterministic comparison pass; deliberately no temporal/filtering. */
		AV_Transition( cmd, &av.reference, VK_IMAGE_LAYOUT_GENERAL ); AV_Transition( cmd, &av.referenceAux, VK_IMAGE_LAYOUT_GENERAL );
		infos[2].imageView = av.reference.view; infos[2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		infos[3].imageView = av.referenceAux.view; infos[3].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		qvkUpdateDescriptorSets( vk.device, 2, writes + 2, 0, NULL );
		rtaoPush.ef[0] = av.width; rtaoPush.ef[1] = av.height;
		rtaoPush.p1[0] = (uint32_t)r_referenceAORays->integer; rtaoPush.p1[1] = (uint32_t)r_referenceAOSeed->integer; rtaoPush.p1[2] = 1u;
		qvkCmdPushConstants( cmd, av.rtaoPL, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( rtaoPush ), &rtaoPush );
		qvkCmdDispatch( cmd, ( av.width + 7u ) / 8u, ( av.height + 7u ) / 8u, 1 );
		AV_Transition( cmd, &av.reference, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ); AV_Transition( cmd, &av.referenceAux, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	}
	AV_WriteTimestamp( cmd, AV_QUERY_RAW_END );

	if ( mode == 5u ) {
		current = &av.reference; currentAux = &av.referenceAux; finalImage = &av.reference;
		/* Preserve the fixed timestamp topology when temporal/filtering are
		 * intentionally bypassed by Reference AO. */
		AV_WriteTimestamp( cmd, AV_QUERY_TEMPORAL_END );
		AV_WriteTimestamp( cmd, AV_QUERY_FILTER_END );
	}
	else {
		current = effectiveMode == 2u ? &av.gtao : &av.raw;
		currentAux = effectiveMode == 2u ? &av.gtaoAux : &av.rawAux;
		readIndex = av.historyIndex; writeIndex = readIndex ^ 1u;
		AV_Transition( cmd, &av.history[readIndex], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		AV_Transition( cmd, &av.historyGeo[readIndex], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		AV_Transition( cmd, &av.historyAux[readIndex], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		AV_Transition( cmd, &av.history[writeIndex], VK_IMAGE_LAYOUT_GENERAL );
		AV_Transition( cmd, &av.historyGeo[writeIndex], VK_IMAGE_LAYOUT_GENERAL );
		AV_Transition( cmd, &av.historyAux[writeIndex], VK_IMAGE_LAYOUT_GENERAL );
		{
			VkImageView sampled[9] = { current->view, currentAux->view, av.gtao.view, depthView, normalView,
				motionView, av.history[readIndex].view, av.historyGeo[readIndex].view, av.historyAux[readIndex].view };
			VkImageLayout layouts[9] = { VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
			uint32_t i;
			for ( i = 0; i < 9; ++i ) AV_ImageWrite( &writes[i], &infos[i], av.temporalSet, i,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, linear, sampled[i], layouts[i] );
			AV_ImageWrite( &writes[9], &infos[9], av.temporalSet, 9, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_NULL_HANDLE, av.history[writeIndex].view, VK_IMAGE_LAYOUT_GENERAL );
			AV_ImageWrite( &writes[10], &infos[10], av.temporalSet, 10, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_NULL_HANDLE, av.historyGeo[writeIndex].view, VK_IMAGE_LAYOUT_GENERAL );
			AV_ImageWrite( &writes[11], &infos[11], av.temporalSet, 11, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_NULL_HANDLE, av.historyAux[writeIndex].view, VK_IMAGE_LAYOUT_GENERAL );
			qvkUpdateDescriptorSets( vk.device, 12, writes, 0, NULL );
		}
		Com_Memset( &temporalPush, 0, sizeof( temporalPush ) );
		temporalPush.ef[0] = av.traceWidth; temporalPush.ef[1] = av.traceHeight;
		temporalPush.ef[2] = ( !av.historyValid || !r_rtaoTemporal->integer ) ? 1u : 0u; temporalPush.ef[3] = normalsAreWorld;
		Com_Memcpy( temporalPush.inv, invView, sizeof( invView ) );
		temporalPush.p0[0] = r_rtaoTemporal->integer ? 0.88f : 0.0f; temporalPush.p0[1] = 0.0025f;
		temporalPush.p0[2] = 0.85f; temporalPush.p0[3] = 0.75f;
		temporalPush.p1[0] = 0.18f; temporalPush.p1[1] = (float)r_rtaoMaxHistory->integer;
		temporalPush.p1[2] = vk.motion_vector_view ? 1.0f : 0.0f; temporalPush.p1[3] = effectiveMode == 4u ? 1.0f : 0.0f;
		qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, av.temporalPipe );
		qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, av.temporalPL, 0, 1, &av.temporalSet, 0, NULL );
		qvkCmdPushConstants( cmd, av.temporalPL, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( temporalPush ), &temporalPush );
		qvkCmdDispatch( cmd, ( av.traceWidth + 7u ) / 8u, ( av.traceHeight + 7u ) / 8u, 1 );
		AV_Transition( cmd, &av.history[writeIndex], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		AV_Transition( cmd, &av.historyGeo[writeIndex], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		AV_Transition( cmd, &av.historyAux[writeIndex], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		av.historyIndex = writeIndex; av.historyValid = qtrue;
		AV_WriteTimestamp( cmd, AV_QUERY_TEMPORAL_END );

		AV_Transition( cmd, &av.filtered, VK_IMAGE_LAYOUT_GENERAL );
		AV_ImageWrite( &writes[0], &infos[0], av.filterSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, linear, av.history[writeIndex].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		AV_ImageWrite( &writes[1], &infos[1], av.filterSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nearest, av.historyAux[writeIndex].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		AV_ImageWrite( &writes[2], &infos[2], av.filterSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nearest, depthView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL );
		AV_ImageWrite( &writes[3], &infos[3], av.filterSet, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nearest, normalView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		AV_ImageWrite( &writes[4], &infos[4], av.filterSet, 4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_NULL_HANDLE, av.filtered.view, VK_IMAGE_LAYOUT_GENERAL );
		qvkUpdateDescriptorSets( vk.device, 5, writes, 0, NULL );
		Com_Memset( &filterPush, 0, sizeof( filterPush ) );
		filterPush.ef[0] = av.traceWidth; filterPush.ef[1] = av.traceHeight; filterPush.ef[2] = normalsAreWorld;
		filterPush.ef[3] = r_rtaoSpatialFilter->integer ? 1u : 0u; Com_Memcpy( filterPush.inv, invView, sizeof( invView ) );
		filterPush.p[0] = 600.0f; filterPush.p[1] = 24.0f; filterPush.p[2] = 20.0f; filterPush.p[3] = 1.0f;
		qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, av.filterPipe );
		qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, av.filterPL, 0, 1, &av.filterSet, 0, NULL );
		qvkCmdPushConstants( cmd, av.filterPL, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( filterPush ), &filterPush );
		qvkCmdDispatch( cmd, ( av.traceWidth + 7u ) / 8u, ( av.traceHeight + 7u ) / 8u, 1 );
		AV_Transition( cmd, &av.filtered, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		AV_WriteTimestamp( cmd, AV_QUERY_FILTER_END );
		finalImage = &av.filtered;
		currentAux = &av.historyAux[writeIndex];
	}

	/* Composite into HDR color for deferred lighting (mode 1/3) and Forward+
	 * sidecar G-buffer (mode 2). Main-world only — weapon/portal/mirror already returned.
	 * Use active G-buffer resources (not fill_wanted): fill_wanted is the capture gate;
	 * by the time we composite, capture for this view has already succeeded. */
	if ( classView &&
		vk.color_format == VK_FORMAT_R16G16B16A16_SFLOAT &&
		( vk_deferred_lighting_active() ||
		  ( r_renderMode && r_renderMode->integer == 2 && vk_deferred_gbuffer_active() ) ) ) {
		AV_ImageWrite( &writes[0], &infos[0], av.compositeSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, linear, finalImage->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		AV_ImageWrite( &writes[1], &infos[1], av.compositeSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nearest, currentAux->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		AV_ImageWrite( &writes[2], &infos[2], av.compositeSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, linear, av.gtao.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		AV_ImageWrite( &writes[3], &infos[3], av.compositeSet, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, linear, av.reference.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		AV_ImageWrite( &writes[4], &infos[4], av.compositeSet, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, linear, av.referenceAux.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		AV_ImageWrite( &writes[5], &infos[5], av.compositeSet, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nearest, classView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		AV_ImageWrite( &writes[6], &infos[6], av.compositeSet, 6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_NULL_HANDLE, vk.color_image_view, VK_IMAGE_LAYOUT_GENERAL );
		AV_ImageWrite( &writes[7], &infos[7], av.compositeSet, 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nearest, depthView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL );
		record_image_layout_transition( cmd, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
		Com_Memset( &metricInfo, 0, sizeof( metricInfo ) );
		metricInfo.buffer = av.metricBuffer;
		metricInfo.offset = (VkDeviceSize)vk.cmd_index * sizeof( uint32_t ) * 4u;
		metricInfo.range = sizeof( uint32_t ) * 4u;
		Com_Memset( &writes[8], 0, sizeof( writes[8] ) );
		writes[8].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[8].dstSet = av.compositeSet; writes[8].dstBinding = 8;
		writes[8].descriptorCount = 1; writes[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[8].pBufferInfo = &metricInfo;
		qvkUpdateDescriptorSets( vk.device, 9, writes, 0, NULL );
		qvkCmdFillBuffer( cmd, av.metricBuffer, metricInfo.offset, metricInfo.range, 0u );
		{
			VkBufferMemoryBarrier metricBarrier;
			Com_Memset( &metricBarrier, 0, sizeof( metricBarrier ) );
			metricBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			metricBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			metricBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			metricBarrier.buffer = av.metricBuffer; metricBarrier.offset = metricInfo.offset; metricBarrier.size = metricInfo.range;
			qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0, 0, NULL, 1, &metricBarrier, 0, NULL );
		}
		Com_Memset( &compositePush, 0, sizeof( compositePush ) );
		compositePush.em[0] = av.width; compositePush.em[1] = av.height; compositePush.em[2] = r_rtaoDebug->integer;
		compositePush.em[3] = needReference ? 1u : 0u; compositePush.p[0] = r_ambientVisibilityStrength->value;
		compositePush.p[1] = (float)effectiveMode; compositePush.p[2] = rtReady ? 1.0f : 0.0f;
		qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, av.compositePipe );
		qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, av.compositePL, 0, 1, &av.compositeSet, 0, NULL );
		qvkCmdPushConstants( cmd, av.compositePL, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( compositePush ), &compositePush );
		gx = ( av.width + 7u ) / 8u; gy = ( av.height + 7u ) / 8u; qvkCmdDispatch( cmd, gx, gy, 1 );
		{
			VkBufferMemoryBarrier metricBarrier;
			Com_Memset( &metricBarrier, 0, sizeof( metricBarrier ) );
			metricBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			metricBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			metricBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
			metricBarrier.buffer = av.metricBuffer; metricBarrier.offset = metricInfo.offset; metricBarrier.size = metricInfo.range;
			qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
				0, 0, NULL, 1, &metricBarrier, 0, NULL );
		}
		record_image_layout_transition( cmd, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0 );
	}
	AV_WriteTimestamp( cmd, AV_QUERY_COMPOSITE_END );
	AV_Barrier( cmd );
	av.appliedThisFrame = qtrue; av.frame++;
}
