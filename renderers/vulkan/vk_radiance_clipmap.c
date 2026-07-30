/*
===========================================================================
Raster Ultra 1.13 — Dynamic Radiance Cache GI + Emissive Transport.
Camera-centered multi-level clipmap; raster-only. Ownership: docs/RASTER_ULTRA_1.13.md.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_radiance_clipmap.h"
#include "vk_util.h"
#include "vk_image_layout.h"
#include "vk_pass_registry.h"
#include "vk_cmd.h"
#include "vk_raster_ultra.h"
#include "vk_deferred_gbuffer.h"
#include "vk_view_state.h"
#include "vk_day_night.h"

#include <math.h>

#define RC_MAX_LEVELS           4
#define RC_DEFAULT_LEVELS       3
#define RC_GRID                 24
#define RC_MAX_CELLS_PER_LEVEL  ( RC_GRID * RC_GRID * RC_GRID )
#define RC_MAX_CELLS            ( RC_MAX_LEVELS * RC_MAX_CELLS_PER_LEVEL )
#define RC_MAX_EMISSIVES        64
#define RC_CELL_BYTES           96

typedef struct {
	vec3_t origin;
	float confidence;      /* 0..1 validity*geometry*visibility */
	vec3_t L0;
	float age;
	vec3_t L1x;
	float occupancy;       /* 1 = solid / blocked */
	vec3_t L1y;
	float variance;
	vec3_t L1z;
	float lightingRev;
	vec3_t normal;
	float dirty;
	uint32_t sceneRev;
	uint32_t lastUpdateFrame;
	float leakRisk;
	float injectEnergy;
} rcCell_t;

/* GPU mirror: 6x vec4 = 96 bytes. */
typedef struct {
	float posConf[4];
	float L0_age[4];
	float L1x_occ[4];
	float L1y_var[4];
	float L1z_rev[4];
	float nrmDirty[4];
} rcCellGpu_t;

typedef struct {
	vec3_t origin;
	vec3_t color;
	float intensity;
	float radius;
	float importance;
	qboolean analyticOwns; /* linked dlight owns injection */
} rcEmissive_t;

typedef struct {
	VkImage image;
	VkDeviceMemory memory;
	VkImageView view;
	VkImageLayout layout;
	uint32_t width, height;
} rcImage_t;

typedef struct {
	qboolean ready;
	qboolean cellsReady;
	char mapName[MAX_QPATH];

	int levels;
	int grid;
	float baseCellSize;
	vec3_t camCenter;
	vec3_t levelOrigin[RC_MAX_LEVELS]; /* min corner of clipmap level */
	int scrollCell[RC_MAX_LEVELS][3];

	rcCell_t *cells; /* levels * grid^3 */
	int cellCount;

	rcEmissive_t emissives[RC_MAX_EMISSIVES];
	int emissiveCount;

	uint32_t lightingRevision;
	uint32_t sceneRevision;
	uint32_t frameIndex;
	uint32_t weatherStamp;
	float lastSunZ;

	/* Metrics */
	int cellsUpdated;
	int cellsInvalidated;
	int cellsReused;
	int cellsInjected;
	int propIters;
	double lastCpuMs;
	float energyInjected;
	float energyPropagated;

	/* GPU */
	VkBuffer cellBuffer;
	VkDeviceMemory cellMemory;
	void *cellMapped;
	VkDeviceSize cellBufferSize;

	rcImage_t cacheIrr;
	rcImage_t cacheMeta;

	VkShaderModule sampleCS;
	VkDescriptorSetLayout sampleLayout;
	VkPipelineLayout samplePL;
	VkPipeline samplePipe;
	VkDescriptorPool descriptorPool;
	VkDescriptorSet sampleSet;

	uint32_t width, height;
} rcState_t;

static rcState_t rc;

static cvar_t *r_radianceCache;
static cvar_t *r_radianceCacheLevels;
static cvar_t *r_radianceCacheGrid;
static cvar_t *r_radianceCacheCellSize;
static cvar_t *r_radianceCacheBudget;
static cvar_t *r_radianceCachePropIters;
static cvar_t *r_radianceCacheStrength;
static cvar_t *r_radianceCacheInjectScale;
static cvar_t *r_radianceCacheEmissive;
static cvar_t *r_radianceCacheEmissiveScale;
static cvar_t *r_radianceCacheSkyScale;
static cvar_t *r_radianceCacheDecay;
static cvar_t *r_radianceCacheDebug;
static cvar_t *r_radianceCacheQuality; /* 0=off path, 1=low(use probes), 2=med, 3=high, 4=ultra */

#include "vk_raster_gi_spirv.inc"

/* ---- helpers ---- */

static void RC_RegisterCvars( void )
{
	if ( r_radianceCache ) {
		return;
	}
	r_radianceCache = ri.Cvar_Get( "r_radianceCache", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_radianceCache, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_radianceCache,
		"Raster Ultra 1.13 clipmapped radiance cache (latched, raster-only).\n"
		" 0 - off (default; certified boot unchanged)\n"
		" 1 - enable camera-centered multi-level cache\n"
		"Enable via: exec modern_raster_ultra.cfg; vid_restart\n"
		"Or: exec vulkan_overlay_raster_ultra_1_13_radiance_cache.cfg" );
	ri.Cvar_SetGroup( r_radianceCache, CVG_RENDERER );

	r_radianceCacheLevels = ri.Cvar_Get( "r_radianceCacheLevels", "3", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_radianceCacheLevels, "1", "4", CV_INTEGER );
	r_radianceCacheGrid = ri.Cvar_Get( "r_radianceCacheGrid", "24", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_radianceCacheGrid, "8", "32", CV_INTEGER );
	r_radianceCacheCellSize = ri.Cvar_Get( "r_radianceCacheCellSize", "96", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_radianceCacheBudget = ri.Cvar_Get( "r_radianceCacheBudget", "512", CVAR_ARCHIVE_ND );
	r_radianceCachePropIters = ri.Cvar_Get( "r_radianceCachePropIters", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_radianceCachePropIters, "0", "2", CV_INTEGER );
	r_radianceCacheStrength = ri.Cvar_Get( "r_radianceCacheStrength", "1", CVAR_ARCHIVE_ND );
	r_radianceCacheInjectScale = ri.Cvar_Get( "r_radianceCacheInjectScale", "0.35", CVAR_ARCHIVE_ND );
	r_radianceCacheEmissive = ri.Cvar_Get( "r_radianceCacheEmissive", "1", CVAR_ARCHIVE_ND );
	r_radianceCacheEmissiveScale = ri.Cvar_Get( "r_radianceCacheEmissiveScale", "0.5", CVAR_ARCHIVE_ND );
	r_radianceCacheSkyScale = ri.Cvar_Get( "r_radianceCacheSkyScale", "0.02", CVAR_ARCHIVE_ND );
	r_radianceCacheDecay = ri.Cvar_Get( "r_radianceCacheDecay", "0.92", CVAR_ARCHIVE_ND );
	r_radianceCacheDebug = ri.Cvar_Get( "r_radianceCacheDebug", "0", CVAR_ARCHIVE_ND );
	r_radianceCacheQuality = ri.Cvar_Get( "r_radianceCacheQuality", "3", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_radianceCacheQuality,
		"Radiance cache quality tier:\n"
		" 0 - disabled path\n"
		" 1 - low (prefer probes; cache sample muted)\n"
		" 2 - medium (coarse, reduced budget)\n"
		" 3 - high (default Ultra)\n"
		" 4 - ultra (more prop + emissive)" );
}

static void RC_DestroyImage( rcImage_t *img )
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

static qboolean RC_CreateImage( rcImage_t *img, uint32_t width, uint32_t height, const char *name )
{
	VkImageCreateInfo ici;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo mai;
	VkImageViewCreateInfo vci;
	VkResult res;

	RC_DestroyImage( img );
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
	ici.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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
		RC_DestroyImage( img );
		return qfalse;
	}
	qvkBindImageMemory( vk.device, img->image, img->memory, 0 );
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
		RC_DestroyImage( img );
		return qfalse;
	}
	img->width = width;
	img->height = height;
	img->layout = VK_IMAGE_LAYOUT_UNDEFINED;
	SET_OBJECT_NAME( img->image, name, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	return qtrue;
}

static void RC_Transition( VkCommandBuffer cmd, rcImage_t *img, VkImageLayout newLayout )
{
	if ( !img->image || img->layout == newLayout ) {
		return;
	}
	record_image_layout_transition( cmd, img->image, VK_IMAGE_ASPECT_COLOR_BIT,
		img->layout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_IMAGE_LAYOUT_UNDEFINED : img->layout,
		newLayout, 0, 0 );
	img->layout = newLayout;
}

static VkSampler RC_Sampler( qboolean linear )
{
	Vk_Sampler_Def sd;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = linear ? GL_LINEAR : GL_NEAREST;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;
	return vk_find_sampler( &sd );
}

static VkShaderModule RC_Module( const uint8_t *code, uint32_t size, const char *name )
{
	VkShaderModuleCreateInfo ci;
	VkShaderModule mod = VK_NULL_HANDLE;
	VkResult res;

	Com_Memset( &ci, 0, sizeof( ci ) );
	ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.codeSize = size;
	ci.pCode = (const uint32_t *)code;
	res = qvkCreateShaderModule( vk.device, &ci, NULL, &mod );
	if ( res != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[RCache] CreateShaderModule(%s) failed: %s\n" S_COLOR_WHITE, name, vk_result_string( res ) );
		return VK_NULL_HANDLE;
	}
	SET_OBJECT_NAME( mod, name, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
	return mod;
}

static qboolean RC_CreateLayout( const VkDescriptorType *types, uint32_t count, uint32_t pushBytes,
	VkDescriptorSetLayout *outLayout, VkPipelineLayout *outPL )
{
	VkDescriptorSetLayoutBinding binds[8];
	VkDescriptorSetLayoutCreateInfo lci;
	VkPushConstantRange pcr;
	VkPipelineLayoutCreateInfo pci;
	uint32_t i;
	VkResult res;

	Com_Memset( binds, 0, sizeof( binds ) );
	for ( i = 0; i < count; ++i ) {
		binds[i].binding = i;
		binds[i].descriptorType = types[i];
		binds[i].descriptorCount = 1;
		binds[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	}
	Com_Memset( &lci, 0, sizeof( lci ) );
	lci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	lci.bindingCount = count;
	lci.pBindings = binds;
	res = qvkCreateDescriptorSetLayout( vk.device, &lci, NULL, outLayout );
	if ( res != VK_SUCCESS ) {
		return qfalse;
	}
	Com_Memset( &pcr, 0, sizeof( pcr ) );
	pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pcr.size = pushBytes;
	Com_Memset( &pci, 0, sizeof( pci ) );
	pci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pci.setLayoutCount = 1;
	pci.pSetLayouts = outLayout;
	pci.pushConstantRangeCount = 1;
	pci.pPushConstantRanges = &pcr;
	res = qvkCreatePipelineLayout( vk.device, &pci, NULL, outPL );
	return ( res == VK_SUCCESS ) ? qtrue : qfalse;
}

static VkPipeline RC_CreateComputePipeline( VkShaderModule module, VkPipelineLayout layout, const char *name )
{
	VkComputePipelineCreateInfo ci;
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkResult res;

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
			"[RCache] CreateComputePipelines(%s) failed: %s\n" S_COLOR_WHITE, name, vk_result_string( res ) );
		return VK_NULL_HANDLE;
	}
	SET_OBJECT_NAME( pipeline, name, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
	return pipeline;
}

static void RC_DestroyPipelines( void )
{
#define RC_DESTROY( fn, x ) do { if ( rc.x ) { fn( vk.device, rc.x, NULL ); rc.x = VK_NULL_HANDLE; } } while ( 0 )
	RC_DESTROY( qvkDestroyPipeline, samplePipe );
	RC_DESTROY( qvkDestroyPipelineLayout, samplePL );
	RC_DESTROY( qvkDestroyDescriptorSetLayout, sampleLayout );
	RC_DESTROY( qvkDestroyShaderModule, sampleCS );
	RC_DESTROY( qvkDestroyDescriptorPool, descriptorPool );
#undef RC_DESTROY
	rc.sampleSet = VK_NULL_HANDLE;
}

static qboolean RC_CreatePipelines( void )
{
	static const VkDescriptorType sampleTypes[] = {
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
	};
	VkDescriptorPoolSize sizes[3];
	VkDescriptorPoolCreateInfo poolCI;
	VkDescriptorSetAllocateInfo ai;
	VkResult res;

#ifndef VK_RGI_CLIPMAP_SAMPLE_CS_SPV_SIZE
	ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
		"[RCache] clipmap sample SPIR-V missing — rebuild shaders\n" S_COLOR_WHITE );
	return qfalse;
#else
	rc.sampleCS = RC_Module( vk_rgi_clipmap_sample_cs_spv, VK_RGI_CLIPMAP_SAMPLE_CS_SPV_SIZE, "RCache sample" );
	if ( !rc.sampleCS ) {
		return qfalse;
	}
	/* push: extentMeta(16) + levelsMeta(16) + cam(16) + cellSizes(16) + origins[4](64) + proj(16) + invView(64) + params(16) = 224 */
	if ( !RC_CreateLayout( sampleTypes, ARRAY_LEN( sampleTypes ), 224u, &rc.sampleLayout, &rc.samplePL ) ) {
		return qfalse;
	}
	rc.samplePipe = RC_CreateComputePipeline( rc.sampleCS, rc.samplePL, "RCache sample pipeline" );
	if ( !rc.samplePipe ) {
		return qfalse;
	}
	Com_Memset( sizes, 0, sizeof( sizes ) );
	sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; sizes[0].descriptorCount = 4;
	sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; sizes[1].descriptorCount = 4;
	sizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; sizes[2].descriptorCount = 2;
	Com_Memset( &poolCI, 0, sizeof( poolCI ) );
	poolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCI.maxSets = 1;
	poolCI.poolSizeCount = 3;
	poolCI.pPoolSizes = sizes;
	res = qvkCreateDescriptorPool( vk.device, &poolCI, NULL, &rc.descriptorPool );
	if ( res != VK_SUCCESS ) {
		return qfalse;
	}
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	ai.descriptorPool = rc.descriptorPool;
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &rc.sampleLayout;
	res = qvkAllocateDescriptorSets( vk.device, &ai, &rc.sampleSet );
	return ( res == VK_SUCCESS ) ? qtrue : qfalse;
#endif
}

static void RC_DestroyCellBuffer( void )
{
	if ( rc.cellMapped && rc.cellMemory ) {
		qvkUnmapMemory( vk.device, rc.cellMemory );
		rc.cellMapped = NULL;
	}
	if ( rc.cellBuffer ) {
		qvkDestroyBuffer( vk.device, rc.cellBuffer, NULL );
		rc.cellBuffer = VK_NULL_HANDLE;
	}
	if ( rc.cellMemory ) {
		qvkFreeMemory( vk.device, rc.cellMemory, NULL );
		rc.cellMemory = VK_NULL_HANDLE;
	}
	rc.cellBufferSize = 0;
}

static qboolean RC_EnsureCellBuffer( void )
{
	VkBufferCreateInfo bci;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo mai;
	VkResult res;
	VkDeviceSize size;

	size = (VkDeviceSize)rc.cellCount * sizeof( rcCellGpu_t );
	if ( rc.cellBuffer && rc.cellBufferSize >= size ) {
		return qtrue;
	}
	RC_DestroyCellBuffer();
	Com_Memset( &bci, 0, sizeof( bci ) );
	bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.size = size;
	bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	res = qvkCreateBuffer( vk.device, &bci, NULL, &rc.cellBuffer );
	if ( res != VK_SUCCESS ) {
		return qfalse;
	}
	qvkGetBufferMemoryRequirements( vk.device, rc.cellBuffer, &req );
	Com_Memset( &mai, 0, sizeof( mai ) );
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.allocationSize = req.size;
	mai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	res = qvkAllocateMemory( vk.device, &mai, NULL, &rc.cellMemory );
	if ( res != VK_SUCCESS ) {
		RC_DestroyCellBuffer();
		return qfalse;
	}
	qvkBindBufferMemory( vk.device, rc.cellBuffer, rc.cellMemory, 0 );
	res = qvkMapMemory( vk.device, rc.cellMemory, 0, size, 0, &rc.cellMapped );
	if ( res != VK_SUCCESS ) {
		RC_DestroyCellBuffer();
		return qfalse;
	}
	rc.cellBufferSize = size;
	SET_OBJECT_NAME( rc.cellBuffer, "RCache cell SSBO", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	return qtrue;
}

static qboolean RC_EnsureImages( uint32_t width, uint32_t height )
{
	if ( rc.cacheIrr.image && rc.width == width && rc.height == height ) {
		return qtrue;
	}
	if ( !RC_CreateImage( &rc.cacheIrr, width, height, "RCache irradiance" ) ||
		!RC_CreateImage( &rc.cacheMeta, width, height, "RCache meta" ) ) {
		return qfalse;
	}
	rc.width = width;
	rc.height = height;
	return qtrue;
}

static void RC_FreeCells( void )
{
	if ( rc.cells ) {
		ri.Free( rc.cells );
		rc.cells = NULL;
	}
	rc.cellCount = 0;
	rc.cellsReady = qfalse;
}

static mnode_t *RC_PointInLeaf( const vec3_t p )
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

static float RC_OccupancyAt( const vec3_t p )
{
	/* Conservative multi-sample: any solid neighbor counts as blocked (reduces thin-wall leaks). */
	static const float offs[13][3] = {
		{ 0, 0, 0 },
		{ 4, 0, 0 }, { -4, 0, 0 },
		{ 0, 4, 0 }, { 0, -4, 0 },
		{ 0, 0, 4 }, { 0, 0, -4 },
		{ 12, 0, 0 }, { -12, 0, 0 },
		{ 0, 12, 0 }, { 0, -12, 0 },
		{ 0, 0, 12 }, { 0, 0, -12 }
	};
	int i, solid = 0;
	for ( i = 0; i < 13; ++i ) {
		vec3_t q;
		mnode_t *leaf;
		VectorSet( q, p[0] + offs[i][0], p[1] + offs[i][1], p[2] + offs[i][2] );
		leaf = RC_PointInLeaf( q );
		if ( !leaf || ( leaf->contents & CONTENTS_SOLID ) ) {
			solid++;
		}
	}
	if ( solid >= 3 ) {
		return 1.0f;
	}
	if ( solid >= 1 ) {
		return 0.55f; /* partial — damp inject/propagate */
	}
	return 0.0f;
}

/*
===============
RC_BlockedBetween

Segment solid test between neighboring clipmap cells — blocks thin-wall bleed
when empty cells sit on opposite sides of a thin BSP wall.
===============
*/
static qboolean RC_BlockedBetween( const vec3_t a, const vec3_t b )
{
	static const float fracs[3] = { 0.25f, 0.5f, 0.75f };
	int i;

	for ( i = 0; i < 3; ++i ) {
		vec3_t q;
		mnode_t *leaf;
		q[0] = a[0] + ( b[0] - a[0] ) * fracs[i];
		q[1] = a[1] + ( b[1] - a[1] ) * fracs[i];
		q[2] = a[2] + ( b[2] - a[2] ) * fracs[i];
		leaf = RC_PointInLeaf( q );
		if ( !leaf || ( leaf->contents & CONTENTS_SOLID ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

static int RC_CellIndex( int level, int x, int y, int z )
{
	int g = rc.grid;
	return level * g * g * g + x + y * g + z * g * g;
}

static float RC_LevelCellSize( int level )
{
	return rc.baseCellSize * (float)( 1 << level );
}

static void RC_SnapOrigin( int level, const vec3_t cam, vec3_t outMin )
{
	float cs = RC_LevelCellSize( level );
	float half = 0.5f * (float)rc.grid * cs;
	int i;

	for ( i = 0; i < 3; ++i ) {
		float center = cam[i];
		float snapped = floorf( ( center - half ) / cs ) * cs;
		outMin[i] = snapped;
	}
}

static void RC_InvalidateCell( rcCell_t *c )
{
	VectorClear( c->L0 );
	VectorClear( c->L1x );
	VectorClear( c->L1y );
	VectorClear( c->L1z );
	c->confidence = 0.0f;
	c->age = 0.0f;
	c->variance = 0.0f;
	c->dirty = 1.0f;
	c->leakRisk = 0.0f;
	c->injectEnergy = 0.0f;
	c->lightingRev = (float)rc.lightingRevision;
	c->sceneRev = rc.sceneRevision;
	rc.cellsInvalidated++;
}

static void RC_ScrollLevel( int level, const vec3_t cam )
{
	vec3_t newMin;
	float cs = RC_LevelCellSize( level );
	int dx, dy, dz;
	int x, y, z;
	int g = rc.grid;
	rcCell_t *scratch;

	RC_SnapOrigin( level, cam, newMin );
	dx = (int)floorf( ( newMin[0] - rc.levelOrigin[level][0] ) / cs + 0.5f );
	dy = (int)floorf( ( newMin[1] - rc.levelOrigin[level][1] ) / cs + 0.5f );
	dz = (int)floorf( ( newMin[2] - rc.levelOrigin[level][2] ) / cs + 0.5f );

	if ( dx == 0 && dy == 0 && dz == 0 ) {
		return;
	}

	/* Large jump / teleport: full invalidate. */
	if ( abs( dx ) >= g || abs( dy ) >= g || abs( dz ) >= g ) {
		for ( z = 0; z < g; ++z ) {
			for ( y = 0; y < g; ++y ) {
				for ( x = 0; x < g; ++x ) {
					rcCell_t *c = &rc.cells[RC_CellIndex( level, x, y, z )];
					c->origin[0] = newMin[0] + ( x + 0.5f ) * cs;
					c->origin[1] = newMin[1] + ( y + 0.5f ) * cs;
					c->origin[2] = newMin[2] + ( z + 0.5f ) * cs;
					c->occupancy = RC_OccupancyAt( c->origin );
					RC_InvalidateCell( c );
					c->confidence = ( c->occupancy > 0.5f ) ? 0.0f : 0.15f;
				}
			}
		}
		VectorCopy( newMin, rc.levelOrigin[level] );
		rc.scrollCell[level][0] += dx;
		rc.scrollCell[level][1] += dy;
		rc.scrollCell[level][2] += dz;
		return;
	}

	scratch = (rcCell_t *)ri.Hunk_AllocateTempMemory( sizeof( rcCell_t ) * g * g * g );
	if ( !scratch ) {
		VectorCopy( newMin, rc.levelOrigin[level] );
		return;
	}
	for ( z = 0; z < g; ++z ) {
		for ( y = 0; y < g; ++y ) {
			for ( x = 0; x < g; ++x ) {
				int sx = x + dx;
				int sy = y + dy;
				int sz = z + dz;
				rcCell_t *dst = &scratch[x + y * g + z * g * g];
				if ( sx >= 0 && sx < g && sy >= 0 && sy < g && sz >= 0 && sz < g ) {
					*dst = rc.cells[RC_CellIndex( level, sx, sy, sz )];
					rc.cellsReused++;
				} else {
					Com_Memset( dst, 0, sizeof( *dst ) );
					dst->origin[0] = newMin[0] + ( x + 0.5f ) * cs;
					dst->origin[1] = newMin[1] + ( y + 0.5f ) * cs;
					dst->origin[2] = newMin[2] + ( z + 0.5f ) * cs;
					dst->occupancy = RC_OccupancyAt( dst->origin );
					RC_InvalidateCell( dst );
					dst->confidence = ( dst->occupancy > 0.5f ) ? 0.0f : 0.15f;
				}
				/* Refresh world origin after scroll. */
				dst->origin[0] = newMin[0] + ( x + 0.5f ) * cs;
				dst->origin[1] = newMin[1] + ( y + 0.5f ) * cs;
				dst->origin[2] = newMin[2] + ( z + 0.5f ) * cs;
			}
		}
	}
	Com_Memcpy( &rc.cells[RC_CellIndex( level, 0, 0, 0 )], scratch, sizeof( rcCell_t ) * g * g * g );
	ri.Hunk_FreeTempMemory( scratch );
	VectorCopy( newMin, rc.levelOrigin[level] );
	rc.scrollCell[level][0] += dx;
	rc.scrollCell[level][1] += dy;
	rc.scrollCell[level][2] += dz;
}

static void RC_BumpLightingRevision( const char *reason )
{
	rc.lightingRevision++;
	if ( r_radianceCacheDebug && r_radianceCacheDebug->integer >= 2 ) {
		ri.Printf( PRINT_DEVELOPER, "[RCache] lightingRev=%u (%s)\n",
			rc.lightingRevision, reason ? reason : "" );
	}
}

static void RC_InjectCell( rcCell_t *c, const trRefdef_t *refdef )
{
	vec3_t dyn = { 0, 0, 0 };
	vec3_t L1x = { 0, 0, 0 }, L1y = { 0, 0, 0 }, L1z = { 0, 0, 0 };
	float injectScale = r_radianceCacheInjectScale ? r_radianceCacheInjectScale->value : 0.35f;
	float decay = r_radianceCacheDecay ? r_radianceCacheDecay->value : 0.92f;
	float energy = 0.0f;
	int i;

	if ( c->occupancy > 0.5f ) {
		VectorClear( c->L0 );
		VectorClear( c->L1x );
		VectorClear( c->L1y );
		VectorClear( c->L1z );
		c->confidence = 0.0f;
		c->dirty = 0.0f;
		c->age = 0.0f;
		c->leakRisk = 1.0f;
		return;
	}
	/* Partial occupancy: mute injection to reduce wall bleed. */
	if ( c->occupancy > 0.4f ) {
		injectScale *= 0.25f;
	}

	/* Decay stale radiance when lights change / age. */
	c->L0[0] *= decay;
	c->L0[1] *= decay;
	c->L0[2] *= decay;
	c->L1x[0] *= decay; c->L1x[1] *= decay; c->L1x[2] *= decay;
	c->L1y[0] *= decay; c->L1y[1] *= decay; c->L1y[2] *= decay;
	c->L1z[0] *= decay; c->L1z[1] *= decay; c->L1z[2] *= decay;

	if ( refdef ) {
		for ( i = 0; i < (int)refdef->num_dlights; ++i ) {
			const dlight_t *dl = &refdef->dlights[i];
			vec3_t dir;
			float dist, atten, power, radius;
			VectorSubtract( dl->origin, c->origin, dir );
			dist = VectorNormalize( dir );
			radius = dl->radius > 1.0f ? dl->radius : 1.0f;
			if ( dist > radius * 1.25f ) {
				continue;
			}
			atten = 1.0f - dist / ( radius * 1.25f );
			atten *= atten;
			power = atten * injectScale;
			dyn[0] += dl->color[0] * power;
			dyn[1] += dl->color[1] * power;
			dyn[2] += dl->color[2] * power;
			L1x[0] += dir[0] * dl->color[0] * power * 0.45f;
			L1y[1] += dir[1] * dl->color[1] * power * 0.45f;
			L1z[2] += dir[2] * dl->color[2] * power * 0.45f;
			energy += power * ( dl->color[0] + dl->color[1] + dl->color[2] );
		}
	}

	/* Explicit emissive fixtures (skip if analyticOwns — dlight already injected). */
	if ( r_radianceCacheEmissive && r_radianceCacheEmissive->integer ) {
		float eScale = r_radianceCacheEmissiveScale ? r_radianceCacheEmissiveScale->value : 0.5f;
		for ( i = 0; i < rc.emissiveCount; ++i ) {
			const rcEmissive_t *e = &rc.emissives[i];
			vec3_t dir;
			float dist, atten, power;
			if ( e->analyticOwns ) {
				continue;
			}
			VectorSubtract( e->origin, c->origin, dir );
			dist = VectorNormalize( dir );
			if ( dist > e->radius ) {
				continue;
			}
			atten = 1.0f - dist / ( e->radius > 1.0f ? e->radius : 1.0f );
			atten *= atten * e->importance;
			power = atten * eScale * e->intensity * injectScale;
			dyn[0] += e->color[0] * power;
			dyn[1] += e->color[1] * power;
			dyn[2] += e->color[2] * power;
			L1x[0] += dir[0] * e->color[0] * power * 0.4f;
			L1y[1] += dir[1] * e->color[1] * power * 0.4f;
			L1z[2] += dir[2] * e->color[2] * power * 0.4f;
			energy += power;
		}
	}

	/* Soft sky outdoors only. */
	if ( tr.sunDirection[0] || tr.sunDirection[1] || tr.sunDirection[2] ) {
		float sky = Com_Clamp( 0.0f, 1.0f, tr.sunDirection[2] * 0.5f + 0.5f );
		float skyScale = r_radianceCacheSkyScale ? r_radianceCacheSkyScale->value : 0.02f;
		vec3_t skyAmbient;
		mnode_t *leaf = RC_PointInLeaf( c->origin );
		qboolean interior = qfalse;
		vk_day_night_sky_ambient( skyAmbient );
		if ( leaf && ( leaf->area == -1 || ( leaf->contents & CONTENTS_SOLID ) ) ) {
			interior = qtrue;
		}
		/* Heuristic: clusters with low sky visibility stay dark. */
		if ( !interior && sky > 0.2f ) {
			dyn[0] += skyScale * sky * skyAmbient[0];
			dyn[1] += skyScale * sky * skyAmbient[1];
			dyn[2] += skyScale * sky * skyAmbient[2] * 1.1f;
			L1z[2] += skyScale * sky * 0.3f;
		}
	}

	/* Soft energy clamp — prevent runaway injection. */
	{
		float lum = dyn[0] * 0.2126f + dyn[1] * 0.7152f + dyn[2] * 0.0722f;
		if ( lum > 2.0f ) {
			float s = 2.0f / lum;
			dyn[0] *= s; dyn[1] *= s; dyn[2] *= s;
			L1x[0] *= s; L1y[1] *= s; L1z[2] *= s;
		}
	}

	c->L0[0] = Com_Clamp( 0.0f, 4.0f, c->L0[0] * 0.35f + dyn[0] );
	c->L0[1] = Com_Clamp( 0.0f, 4.0f, c->L0[1] * 0.35f + dyn[1] );
	c->L0[2] = Com_Clamp( 0.0f, 4.0f, c->L0[2] * 0.35f + dyn[2] );
	c->L1x[0] = L1x[0]; c->L1x[1] = L1x[1]; c->L1x[2] = L1x[2];
	c->L1y[0] = L1y[0]; c->L1y[1] = L1y[1]; c->L1y[2] = L1y[2];
	c->L1z[0] = L1z[0]; c->L1z[1] = L1z[1]; c->L1z[2] = L1z[2];
	c->injectEnergy = energy;
	c->confidence = Com_Clamp( 0.05f, 1.0f, 0.35f + energy * 0.15f );
	c->age = 0.0f;
	c->dirty = 0.0f;
	c->lightingRev = (float)rc.lightingRevision;
	c->sceneRev = rc.sceneRevision;
	c->lastUpdateFrame = rc.frameIndex;
	c->leakRisk = c->occupancy;
	c->variance = fabsf( dyn[0] - c->L0[0] ) + fabsf( dyn[1] - c->L0[1] );
	rc.energyInjected += energy;
	rc.cellsInjected++;
}

static int RC_CellPriority( const rcCell_t *c, const vec3_t viewOrg )
{
	float dist = Distance( c->origin, viewOrg );
	int score = (int)dist;
	score += (int)( c->age * 4.0f );
	if ( c->dirty > 0.5f ) {
		score -= 8000;
	}
	if ( c->occupancy > 0.5f ) {
		score += 20000; /* deprioritize solid */
	}
	return score;
}

static void RC_Propagate( int iters )
{
	int level, it, x, y, z, g;
	float albedoTransport = 0.45f; /* conservative diffuse bounce */
	float energyDelta = 0.0f;

	if ( iters < 1 || !rc.cells ) {
		return;
	}
	g = rc.grid;
	for ( it = 0; it < iters; ++it ) {
		for ( level = 0; level < rc.levels; ++level ) {
			for ( z = 1; z < g - 1; ++z ) {
				for ( y = 1; y < g - 1; ++y ) {
					for ( x = 1; x < g - 1; ++x ) {
						rcCell_t *c = &rc.cells[RC_CellIndex( level, x, y, z )];
						rcCell_t *n[6];
						vec3_t sum = { 0, 0, 0 };
						float wsum = 0.0f;
						int k;
						if ( c->occupancy > 0.5f || c->confidence < 0.05f ) {
							continue;
						}
						n[0] = &rc.cells[RC_CellIndex( level, x - 1, y, z )];
						n[1] = &rc.cells[RC_CellIndex( level, x + 1, y, z )];
						n[2] = &rc.cells[RC_CellIndex( level, x, y - 1, z )];
						n[3] = &rc.cells[RC_CellIndex( level, x, y + 1, z )];
						n[4] = &rc.cells[RC_CellIndex( level, x, y, z - 1 )];
						n[5] = &rc.cells[RC_CellIndex( level, x, y, z + 1 )];
						for ( k = 0; k < 6; ++k ) {
							float w;
							if ( n[k]->occupancy > 0.5f || n[k]->confidence < 0.05f ) {
								/* Blocked — raise leak risk if neighbor bright through wall. */
								if ( n[k]->occupancy > 0.5f &&
									( n[k]->L0[0] + n[k]->L0[1] + n[k]->L0[2] ) > 0.1f ) {
									c->leakRisk = Com_Clamp( 0.0f, 1.0f, c->leakRisk + 0.05f );
								}
								continue;
							}
							/* Thin-wall gate: empty cells on both sides still blocked by BSP. */
							if ( RC_BlockedBetween( c->origin, n[k]->origin ) ) {
								c->leakRisk = Com_Clamp( 0.0f, 1.0f, c->leakRisk + 0.2f );
								continue;
							}
							w = n[k]->confidence * ( 1.0f - n[k]->occupancy );
							sum[0] += n[k]->L0[0] * w;
							sum[1] += n[k]->L0[1] * w;
							sum[2] += n[k]->L0[2] * w;
							wsum += w;
						}
						if ( wsum > 1e-4f ) {
							float inv = albedoTransport / wsum;
							float before = c->L0[0] + c->L0[1] + c->L0[2];
							c->L0[0] = Com_Clamp( 0.0f, 4.0f, c->L0[0] * 0.7f + sum[0] * inv );
							c->L0[1] = Com_Clamp( 0.0f, 4.0f, c->L0[1] * 0.7f + sum[1] * inv );
							c->L0[2] = Com_Clamp( 0.0f, 4.0f, c->L0[2] * 0.7f + sum[2] * inv );
							energyDelta += fabsf( ( c->L0[0] + c->L0[1] + c->L0[2] ) - before );
						}
					}
				}
			}
		}
	}
	rc.propIters = iters;
	rc.energyPropagated += energyDelta;
}

static void RC_BudgetUpdate( const trRefdef_t *refdef )
{
	int budget = r_radianceCacheBudget ? r_radianceCacheBudget->integer : 512;
	int quality = r_radianceCacheQuality ? r_radianceCacheQuality->integer : 3;
	int i, updated = 0;
	int *order;
	int n;
	vec3_t viewOrg;
	int propIters;

	if ( !rc.cells || rc.cellCount <= 0 || !refdef ) {
		return;
	}
	if ( quality <= 1 ) {
		budget = budget / 4;
	} else if ( quality == 2 ) {
		budget = budget / 2;
	} else if ( quality >= 4 ) {
		budget = budget + budget / 2;
	}
	if ( budget < 16 ) {
		budget = 16;
	}
	if ( budget > rc.cellCount ) {
		budget = rc.cellCount;
	}

	VectorCopy( refdef->vieworg, viewOrg );
	rc.cellsUpdated = 0;
	rc.cellsInjected = 0;
	rc.energyInjected = 0.0f;

	for ( i = 0; i < rc.cellCount; ++i ) {
		rc.cells[i].age += 1.0f;
	}

	order = (int *)ri.Hunk_AllocateTempMemory( sizeof( int ) * rc.cellCount );
	if ( !order ) {
		for ( i = 0; i < budget; ++i ) {
			RC_InjectCell( &rc.cells[i], refdef );
			updated++;
		}
	} else {
		n = rc.cellCount;
		for ( i = 0; i < n; ++i ) {
			order[i] = i;
		}
		for ( i = 0; i < budget; ++i ) {
			int best = i;
			int j;
			for ( j = i + 1; j < n; ++j ) {
				if ( RC_CellPriority( &rc.cells[order[j]], viewOrg ) <
					RC_CellPriority( &rc.cells[order[best]], viewOrg ) ) {
					best = j;
				}
			}
			if ( best != i ) {
				int tmp = order[i];
				order[i] = order[best];
				order[best] = tmp;
			}
			{
				rcCell_t *cell = &rc.cells[order[i]];
				float occ = RC_OccupancyAt( cell->origin );
				if ( fabsf( occ - cell->occupancy ) > 0.4f ) {
					cell->occupancy = occ;
					RC_InvalidateCell( cell );
					cell->confidence = ( occ > 0.5f ) ? 0.0f : 0.15f;
					rc.sceneRevision++;
				} else {
					cell->occupancy = occ;
				}
				RC_InjectCell( cell, refdef );
			}
			updated++;
		}
		ri.Hunk_FreeTempMemory( order );
	}
	rc.cellsUpdated = updated;

	propIters = r_radianceCachePropIters ? r_radianceCachePropIters->integer : 1;
	if ( quality <= 1 ) {
		propIters = 0;
	} else if ( quality >= 4 && propIters < 2 ) {
		propIters = 2;
	}
	RC_Propagate( propIters );
}

static void RC_UploadCells( void )
{
	rcCellGpu_t *dst;
	int i;

	if ( !rc.cellMapped || !rc.cells ) {
		return;
	}
	dst = (rcCellGpu_t *)rc.cellMapped;
	for ( i = 0; i < rc.cellCount; ++i ) {
		const rcCell_t *c = &rc.cells[i];
		dst[i].posConf[0] = c->origin[0];
		dst[i].posConf[1] = c->origin[1];
		dst[i].posConf[2] = c->origin[2];
		dst[i].posConf[3] = ( c->occupancy > 0.5f ) ? 0.0f : c->confidence;
		dst[i].L0_age[0] = c->L0[0];
		dst[i].L0_age[1] = c->L0[1];
		dst[i].L0_age[2] = c->L0[2];
		dst[i].L0_age[3] = c->age;
		dst[i].L1x_occ[0] = c->L1x[0];
		dst[i].L1x_occ[1] = c->L1x[1];
		dst[i].L1x_occ[2] = c->L1x[2];
		dst[i].L1x_occ[3] = c->occupancy;
		dst[i].L1y_var[0] = c->L1y[0];
		dst[i].L1y_var[1] = c->L1y[1];
		dst[i].L1y_var[2] = c->L1y[2];
		dst[i].L1y_var[3] = c->variance;
		dst[i].L1z_rev[0] = c->L1z[0];
		dst[i].L1z_rev[1] = c->L1z[1];
		dst[i].L1z_rev[2] = c->L1z[2];
		dst[i].L1z_rev[3] = c->lightingRev;
		dst[i].nrmDirty[0] = c->normal[0];
		dst[i].nrmDirty[1] = c->normal[1];
		dst[i].nrmDirty[2] = c->normal[2];
		dst[i].nrmDirty[3] = c->leakRisk;
	}
}

static qboolean RC_AllocateGrid( void )
{
	int level, x, y, z, g;
	float cs;

	RC_FreeCells();
	rc.levels = r_radianceCacheLevels ? r_radianceCacheLevels->integer : RC_DEFAULT_LEVELS;
	if ( rc.levels < 1 ) {
		rc.levels = 1;
	}
	if ( rc.levels > RC_MAX_LEVELS ) {
		rc.levels = RC_MAX_LEVELS;
	}
	rc.grid = r_radianceCacheGrid ? r_radianceCacheGrid->integer : RC_GRID;
	if ( rc.grid < 8 ) {
		rc.grid = 8;
	}
	if ( rc.grid > 32 ) {
		rc.grid = 32;
	}
	rc.baseCellSize = r_radianceCacheCellSize ? r_radianceCacheCellSize->value : 96.0f;
	if ( rc.baseCellSize < 32.0f ) {
		rc.baseCellSize = 32.0f;
	}

	g = rc.grid;
	rc.cellCount = rc.levels * g * g * g;
	rc.cells = (rcCell_t *)ri.Malloc( sizeof( rcCell_t ) * rc.cellCount );
	if ( !rc.cells ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[RCache] cell allocation failed\n" S_COLOR_WHITE );
		return qfalse;
	}
	Com_Memset( rc.cells, 0, sizeof( rcCell_t ) * rc.cellCount );
	VectorClear( rc.camCenter );
	for ( level = 0; level < rc.levels; ++level ) {
		RC_SnapOrigin( level, rc.camCenter, rc.levelOrigin[level] );
		cs = RC_LevelCellSize( level );
		for ( z = 0; z < g; ++z ) {
			for ( y = 0; y < g; ++y ) {
				for ( x = 0; x < g; ++x ) {
					rcCell_t *c = &rc.cells[RC_CellIndex( level, x, y, z )];
					c->origin[0] = rc.levelOrigin[level][0] + ( x + 0.5f ) * cs;
					c->origin[1] = rc.levelOrigin[level][1] + ( y + 0.5f ) * cs;
					c->origin[2] = rc.levelOrigin[level][2] + ( z + 0.5f ) * cs;
					c->occupancy = RC_OccupancyAt( c->origin );
					c->confidence = ( c->occupancy > 0.5f ) ? 0.0f : 0.1f;
					c->dirty = 1.0f;
					c->lightingRev = (float)rc.lightingRevision;
					c->sceneRev = rc.sceneRevision;
				}
			}
		}
	}
	if ( !RC_EnsureCellBuffer() ) {
		RC_FreeCells();
		return qfalse;
	}
	rc.cellsReady = qtrue;
	ri.Printf( PRINT_ALL,
		"[RCache] clipmap ready: levels=%d grid=%d^3 cell=%.0f cells=%d mem≈%zuKB (raster-only)\n",
		rc.levels, rc.grid, rc.baseCellSize, rc.cellCount,
		(size_t)rc.cellCount * sizeof( rcCell_t ) / 1024 );
	return qtrue;
}

static void RC_Status_f( void )
{
	size_t bytes = (size_t)rc.cellCount * sizeof( rcCell_t );
	ri.Printf( PRINT_ALL,
		"radiance_cache: active=%d ready=%d levels=%d grid=%d cellSize=%.0f cells=%d\n"
		"  updated=%d invalidated=%d reused=%d injected=%d propIters=%d\n"
		"  lightingRev=%u sceneRev=%u energyInj=%.3f energyProp=%.3f cpuMs=%.2f mem=%zuKB\n"
		"  strength=%.2f emissives=%d quality=%d debug=%d\n",
		vk_radiance_clipmap_active() ? 1 : 0,
		rc.cellsReady ? 1 : 0,
		rc.levels, rc.grid, rc.baseCellSize, rc.cellCount,
		rc.cellsUpdated, rc.cellsInvalidated, rc.cellsReused, rc.cellsInjected, rc.propIters,
		rc.lightingRevision, rc.sceneRevision, rc.energyInjected, rc.energyPropagated,
		rc.lastCpuMs, bytes / 1024,
		r_radianceCacheStrength ? r_radianceCacheStrength->value : 0.0f,
		rc.emissiveCount,
		r_radianceCacheQuality ? r_radianceCacheQuality->integer : 0,
		r_radianceCacheDebug ? r_radianceCacheDebug->integer : 0 );
}

static void RC_Invalidate_f( void )
{
	vk_radiance_clipmap_invalidate();
	ri.Printf( PRINT_ALL, "radiance_cache: invalidated (lightingRev=%u sceneRev=%u)\n",
		rc.lightingRevision, rc.sceneRevision );
}

static void RC_AddEmissive_f( void )
{
	vec3_t o, c;
	float intensity, radius, importance;

	if ( ri.Cmd_Argc() < 9 ) {
		ri.Printf( PRINT_ALL,
			"Usage: rcache_add_emissive x y z r g b intensity radius [importance]\n" );
		return;
	}
	o[0] = atof( ri.Cmd_Argv( 1 ) );
	o[1] = atof( ri.Cmd_Argv( 2 ) );
	o[2] = atof( ri.Cmd_Argv( 3 ) );
	c[0] = atof( ri.Cmd_Argv( 4 ) );
	c[1] = atof( ri.Cmd_Argv( 5 ) );
	c[2] = atof( ri.Cmd_Argv( 6 ) );
	intensity = atof( ri.Cmd_Argv( 7 ) );
	radius = atof( ri.Cmd_Argv( 8 ) );
	importance = ( ri.Cmd_Argc() > 9 ) ? atof( ri.Cmd_Argv( 9 ) ) : 1.0f;
	vk_radiance_clipmap_register_emissive( o, c, intensity, radius, importance );
	ri.Printf( PRINT_ALL, "radiance_cache: emissive #%d registered\n", rc.emissiveCount );
}

/* ---- public API ---- */

void vk_radiance_clipmap_register_emissive( const vec3_t origin, const vec3_t color,
	float intensity, float radius, float importance )
{
	rcEmissive_t *e;
	if ( rc.emissiveCount >= RC_MAX_EMISSIVES ) {
		return;
	}
	e = &rc.emissives[rc.emissiveCount++];
	VectorCopy( origin, e->origin );
	VectorCopy( color, e->color );
	e->intensity = intensity > 0.0f ? intensity : 1.0f;
	e->radius = radius > 1.0f ? radius : 128.0f;
	e->importance = importance > 0.0f ? importance : 1.0f;
	e->analyticOwns = qfalse;
	RC_BumpLightingRevision( "emissive_register" );
}

void vk_radiance_clipmap_metrics( int *levelsOut, int *cellsOut, int *updatedOut,
	int *invalidatedOut, int *reusedOut, int *injectedOut, size_t *bytesOut )
{
	if ( levelsOut ) *levelsOut = rc.levels;
	if ( cellsOut ) *cellsOut = rc.cellCount;
	if ( updatedOut ) *updatedOut = rc.cellsUpdated;
	if ( invalidatedOut ) *invalidatedOut = rc.cellsInvalidated;
	if ( reusedOut ) *reusedOut = rc.cellsReused;
	if ( injectedOut ) *injectedOut = rc.cellsInjected;
	if ( bytesOut ) *bytesOut = (size_t)rc.cellCount * sizeof( rcCell_t );
}

qboolean vk_radiance_clipmap_active( void )
{
	return ( r_radianceCache && r_radianceCache->integer && rc.ready &&
		r_radianceCacheQuality && r_radianceCacheQuality->integer > 0 ) ? qtrue : qfalse;
}

qboolean vk_radiance_clipmap_ready( void )
{
	return ( vk_radiance_clipmap_active() && rc.cellsReady && rc.cacheIrr.view ) ? qtrue : qfalse;
}

VkImageView vk_radiance_clipmap_irradiance_view( void )
{
	return rc.cacheIrr.view;
}

VkImageView vk_radiance_clipmap_meta_view( void )
{
	return rc.cacheMeta.view;
}

VkImageLayout vk_radiance_clipmap_sample_layout( void )
{
	return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void vk_radiance_clipmap_invalidate( void )
{
	int i;
	rc.sceneRevision++;
	RC_BumpLightingRevision( "invalidate" );
	rc.cellsInvalidated = 0;
	rc.cellsReused = 0;
	for ( i = 0; i < rc.cellCount; ++i ) {
		RC_InvalidateCell( &rc.cells[i] );
		rc.cells[i].occupancy = RC_OccupancyAt( rc.cells[i].origin );
		rc.cells[i].confidence = ( rc.cells[i].occupancy > 0.5f ) ? 0.0f : 0.1f;
	}
	rc.emissiveCount = 0;
}

void vk_radiance_clipmap_on_map_load( void )
{
	if ( tr.world ) {
		Q_strncpyz( rc.mapName, tr.world->baseName, sizeof( rc.mapName ) );
	}
	vk_radiance_clipmap_invalidate();
	if ( vk_radiance_clipmap_active() ) {
		RC_AllocateGrid();
	}
}

void vk_radiance_clipmap_shutdown( void )
{
	if ( ri.Cmd_RemoveCommand ) {
		ri.Cmd_RemoveCommand( "radiance_cache_status" );
		ri.Cmd_RemoveCommand( "rcache_status" );
		ri.Cmd_RemoveCommand( "rcache_invalidate" );
		ri.Cmd_RemoveCommand( "rcache_add_emissive" );
	}
	RC_DestroyPipelines();
	RC_DestroyCellBuffer();
	RC_DestroyImage( &rc.cacheIrr );
	RC_DestroyImage( &rc.cacheMeta );
	RC_FreeCells();
	Com_Memset( &rc, 0, sizeof( rc ) );
}

void vk_radiance_clipmap_init( void )
{
	RC_RegisterCvars();
	if ( rc.ready || !vk.device || !vk.fboActive ) {
		return;
	}
	if ( !r_radianceCache || !r_radianceCache->integer ) {
		return;
	}
	if ( !RC_CreatePipelines() ) {
		RC_DestroyPipelines();
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[RCache] pipeline create failed — radiance cache unavailable\n" S_COLOR_WHITE );
		return;
	}
	if ( ri.Cmd_RemoveCommand ) {
		ri.Cmd_RemoveCommand( "radiance_cache_status" );
		ri.Cmd_RemoveCommand( "rcache_status" );
		ri.Cmd_RemoveCommand( "rcache_invalidate" );
		ri.Cmd_RemoveCommand( "rcache_add_emissive" );
	}
	if ( ri.Cmd_AddCommand ) {
		ri.Cmd_AddCommand( "radiance_cache_status", RC_Status_f );
		ri.Cmd_AddCommand( "rcache_status", RC_Status_f );
		ri.Cmd_AddCommand( "rcache_invalidate", RC_Invalidate_f );
		ri.Cmd_AddCommand( "rcache_add_emissive", RC_AddEmissive_f );
	}
	rc.ready = qtrue;
	ri.Printf( PRINT_ALL,
		"[RCache] Raster Ultra 1.13 radiance clipmap initialized (r_radianceCache=%d; RT unused)\n",
		r_radianceCache->integer );
}

void vk_radiance_clipmap_frame_begin( void )
{
	uint32_t w, h;

	RC_RegisterCvars();
	if ( !rc.ready && vk.device && vk.fboActive && r_radianceCache && r_radianceCache->integer ) {
		vk_radiance_clipmap_init();
	}
	if ( !rc.ready ) {
		return;
	}
	rc.frameIndex++;
	rc.cellsInvalidated = 0;
	rc.cellsReused = 0;
	w = vk_get_render_target_width();
	h = vk_get_render_target_height();
	if ( w && h ) {
		RC_EnsureImages( w, h );
	}

	/* Weather / sun change → lighting revision (stale noon must not persist at night). */
	{
		float sunZ = tr.sunDirection[2];
		uint32_t weather = (uint32_t)ri.Cvar_VariableIntegerValue( "r_weather" );
		weather ^= (uint32_t)( ri.Cvar_VariableIntegerValue( "r_weatherPreset" ) * 17 );
		if ( fabsf( sunZ - rc.lastSunZ ) > 0.05f || weather != rc.weatherStamp ) {
			rc.lastSunZ = sunZ;
			rc.weatherStamp = weather;
			RC_BumpLightingRevision( "sky_weather" );
			{
				int i;
				for ( i = 0; i < rc.cellCount; ++i ) {
					rc.cells[i].dirty = 1.0f;
				}
			}
		}
	}
}

void vk_radiance_clipmap_update( const trRefdef_t *refdef )
{
	int level;
	int t0;

	if ( !vk_radiance_clipmap_active() || !refdef ) {
		return;
	}
	if ( !rc.cellsReady ) {
		if ( !RC_AllocateGrid() ) {
			return;
		}
	}

	t0 = ri.Milliseconds ? ri.Milliseconds() : 0;

	/* Teleport: large camera jump invalidates all levels. */
	{
		float jump = Distance( refdef->vieworg, rc.camCenter );
		float cover = rc.baseCellSize * (float)rc.grid;
		if ( rc.cellsReady && jump > cover * 0.75f ) {
			RC_BumpLightingRevision( "teleport" );
			for ( level = 0; level < rc.levels; ++level ) {
				int x, y, z, g = rc.grid;
				float cs = RC_LevelCellSize( level );
				RC_SnapOrigin( level, refdef->vieworg, rc.levelOrigin[level] );
				for ( z = 0; z < g; ++z ) {
					for ( y = 0; y < g; ++y ) {
						for ( x = 0; x < g; ++x ) {
							rcCell_t *c = &rc.cells[RC_CellIndex( level, x, y, z )];
							c->origin[0] = rc.levelOrigin[level][0] + ( x + 0.5f ) * cs;
							c->origin[1] = rc.levelOrigin[level][1] + ( y + 0.5f ) * cs;
							c->origin[2] = rc.levelOrigin[level][2] + ( z + 0.5f ) * cs;
							c->occupancy = RC_OccupancyAt( c->origin );
							RC_InvalidateCell( c );
							c->confidence = ( c->occupancy > 0.5f ) ? 0.0f : 0.15f;
						}
					}
				}
			}
		}
	}
	VectorCopy( refdef->vieworg, rc.camCenter );
	for ( level = 0; level < rc.levels; ++level ) {
		RC_ScrollLevel( level, refdef->vieworg );
	}

	RC_BudgetUpdate( refdef );
	RC_UploadCells();
	if ( ri.Milliseconds ) {
		rc.lastCpuMs = (double)( ri.Milliseconds() - t0 );
	}

	vk_spine_note_write( VK_SPINE_RES_RADIANCE_CLIPMAP, VK_SPINE_PASS_RASTER_GI, VK_SPINE_ACCESS_STORAGE_WRITE );
}

void vk_radiance_clipmap_dispatch_sample( VkCommandBuffer cmd,
	VkImageView depthView, VkImageView normalView,
	const float invView[16], const float projInfo[4], uint32_t normalsAreWorld,
	uint32_t width, uint32_t height )
{
	VkDescriptorImageInfo infos[5];
	VkWriteDescriptorSet writes[5];
	VkDescriptorBufferInfo bufInfo;
	uint32_t gx, gy;
	int level;

	struct {
		uint32_t extentMeta[4];
		uint32_t levelsMeta[4];
		float cam[4];
		float cellSizes[4];
		float origins[16]; /* 4 levels * xyz + pad */
		float projInfo[4];
		float invView[16];
		float params0[4];
	} push;

	if ( !vk_radiance_clipmap_ready() || !cmd || !rc.samplePipe || !rc.cellBuffer ) {
		return;
	}
	RC_EnsureImages( width, height );

	RC_Transition( cmd, &rc.cacheIrr, VK_IMAGE_LAYOUT_GENERAL );
	RC_Transition( cmd, &rc.cacheMeta, VK_IMAGE_LAYOUT_GENERAL );

	Com_Memset( infos, 0, sizeof( infos ) );
	Com_Memset( writes, 0, sizeof( writes ) );
	infos[0].sampler = RC_Sampler( qfalse );
	infos[0].imageView = depthView;
	infos[0].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = rc.sampleSet;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &infos[0];

	infos[1].sampler = RC_Sampler( qfalse );
	infos[1].imageView = normalView;
	infos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = rc.sampleSet;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].pImageInfo = &infos[1];

	Com_Memset( &bufInfo, 0, sizeof( bufInfo ) );
	bufInfo.buffer = rc.cellBuffer;
	bufInfo.range = VK_WHOLE_SIZE;
	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = rc.sampleSet;
	writes[2].dstBinding = 2;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[2].pBufferInfo = &bufInfo;

	infos[3].imageView = rc.cacheIrr.view;
	infos[3].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[3].dstSet = rc.sampleSet;
	writes[3].dstBinding = 3;
	writes[3].descriptorCount = 1;
	writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[3].pImageInfo = &infos[3];

	infos[4].imageView = rc.cacheMeta.view;
	infos[4].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[4].dstSet = rc.sampleSet;
	writes[4].dstBinding = 4;
	writes[4].descriptorCount = 1;
	writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[4].pImageInfo = &infos[4];

	qvkUpdateDescriptorSets( vk.device, 5, writes, 0, NULL );

	Com_Memset( &push, 0, sizeof( push ) );
	push.extentMeta[0] = width;
	push.extentMeta[1] = height;
	push.extentMeta[2] = (uint32_t)rc.cellCount;
	push.extentMeta[3] = normalsAreWorld;
	push.levelsMeta[0] = (uint32_t)rc.levels;
	push.levelsMeta[1] = (uint32_t)rc.grid;
	push.levelsMeta[2] = rc.lightingRevision;
	push.levelsMeta[3] = (uint32_t)( r_radianceCacheDebug ? r_radianceCacheDebug->integer : 0 );
	push.cam[0] = rc.camCenter[0];
	push.cam[1] = rc.camCenter[1];
	push.cam[2] = rc.camCenter[2];
	for ( level = 0; level < RC_MAX_LEVELS; ++level ) {
		push.cellSizes[level] = ( level < rc.levels ) ? RC_LevelCellSize( level ) : 0.0f;
		push.origins[level * 4 + 0] = rc.levelOrigin[level][0];
		push.origins[level * 4 + 1] = rc.levelOrigin[level][1];
		push.origins[level * 4 + 2] = rc.levelOrigin[level][2];
	}
	Com_Memcpy( push.projInfo, projInfo, sizeof( float ) * 4 );
	Com_Memcpy( push.invView, invView, sizeof( float ) * 16 );
	push.params0[0] = r_radianceCacheStrength ? r_radianceCacheStrength->value : 1.0f;
	push.params0[1] = 0.35f; /* level blend width */
	push.params0[2] = ( r_radianceCacheQuality && r_radianceCacheQuality->integer <= 1 ) ? 0.25f : 1.0f;
	push.params0[3] = 0.02f; /* min confidence */

	vk_spine_note_write( VK_SPINE_RES_RADIANCE_CACHE_IRRADIANCE, VK_SPINE_PASS_RASTER_GI, VK_SPINE_ACCESS_STORAGE_WRITE );
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rc.samplePipe );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rc.samplePL, 0, 1, &rc.sampleSet, 0, NULL );
	qvkCmdPushConstants( cmd, rc.samplePL, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );
	gx = ( width + 7u ) / 8u;
	gy = ( height + 7u ) / 8u;
	qvkCmdDispatch( cmd, gx, gy, 1 );

	RC_Transition( cmd, &rc.cacheIrr, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	RC_Transition( cmd, &rc.cacheMeta, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
}
