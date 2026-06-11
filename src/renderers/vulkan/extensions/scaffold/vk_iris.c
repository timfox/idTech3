/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Iris Core — digital pathology WSI tile rendering scaffold.
Landvater & Balis, J Pathol Inform 16 (2025) 100414.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_iris.h"
#include "vk_util.h"
#include "vk_cmd.h"
#include "vk_view_state.h"
#include "vk_staging.h"
#include "vk_image_layout.h"
#include "iris/iris_io.h"

#define IRIS_TILE_SIZE          256u
#define IRIS_ATLAS_TILES_X      16u
#define IRIS_ATLAS_TILES_Y      16u
#define IRIS_DEFAULT_W          1024u
#define IRIS_DEFAULT_H          768u
#define IRIS_TILE_STATE_FREE    0u
#define IRIS_TILE_STATE_LR      1u
#define IRIS_TILE_STATE_HR      2u

typedef struct {
	uint32_t width;
	uint32_t height;
	int32_t pan_x;
	int32_t pan_y;
	uint32_t pans;
	uint32_t tiles_buffered;
	uint32_t spd_dispatches;
	qboolean atlas_general;
	qboolean mip_general;
	qboolean scope_general;
	qboolean ready;
} irisState_t;

static irisState_t iris;

static cvar_t *r_iris;
static cvar_t *r_iris_width;
static cvar_t *r_iris_height;
static cvar_t *r_iris_decoder;
static cvar_t *r_iris_sharpen;
static cvar_t *r_iris_bilinear;
static cvar_t *r_iris_overlay;
static cvar_t *r_iris_overlay_alpha;
static cvar_t *r_iris_overlay_scale;
static cvar_t *r_iris_overlay_mode;
static cvar_t *r_iris_debug;

typedef struct {
	VkDescriptorSetLayout layout;
	VkPipelineLayout pipeline_layout;
	VkPipeline pipeline;
	VkDescriptorPool pool;
	VkDescriptorSet descriptor;
	qboolean ready;
} irisPipeline_t;

static irisPipeline_t iris_clear;
static irisPipeline_t iris_spd;
static irisPipeline_t iris_compose;
static irisPipeline_t iris_overlay;

static void Iris_DestroyGpu( void );
static void Iris_EnsurePipelines( void );
static qboolean Iris_EnsureResources( void );
static qboolean Iris_SeedAtlas( void );
static qboolean Iris_UploadAtlasHost( const uint8_t *rgba, uint32_t atlas_w, uint32_t atlas_h );
static qboolean Iris_DownloadAtlasHost( uint8_t *rgba, uint32_t atlas_w, uint32_t atlas_h );
static qboolean Iris_UploadTileStateHost( const uint32_t *state, uint32_t count );
static qboolean Iris_DownloadTileStateHost( uint32_t *state, uint32_t count );
static qboolean Iris_StageTransferAtlas( const uint8_t *hostRgba, uint8_t *hostOut, qboolean upload );

typedef struct {
	int viewport[4];
	int panTiles[2];
	int atlasTilesX;
	int atlasTilesY;
	int layerHr;
	int mipBilinear;
} irisComposePush_t;

static void Iris_DestroyPipeline( irisPipeline_t *p )
{
	if ( !p ) {
		return;
	}
	if ( p->pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, p->pipeline, NULL );
	}
	if ( p->pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, p->pipeline_layout, NULL );
	}
	if ( p->layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, p->layout, NULL );
	}
	if ( p->pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, p->pool, NULL );
	}
	Com_Memset( p, 0, sizeof( *p ) );
}

static qboolean Iris_CreateComputePipeline( irisPipeline_t *p, VkShaderModule module,
	const VkDescriptorSetLayoutBinding *bindings, uint32_t bindingCount, uint32_t pushSize )
{
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;
	VkDescriptorPoolCreateInfo pci;
	VkDescriptorSetAllocateInfo ai;
	uint32_t bufCount = 0;
	uint32_t imgCount = 0;
	uint32_t poolCount = 0;
	uint32_t i;
	VkDescriptorPoolSize poolSizes[2];

	if ( !p || module == VK_NULL_HANDLE || p->ready ) {
		return qfalse;
	}

	Com_Memset( &layout_ci, 0, sizeof( layout_ci ) );
	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.bindingCount = bindingCount;
	layout_ci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &p->layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = pushSize;

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &p->layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &p->pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = module;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = p->pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &p->pipeline ) );

	for ( i = 0; i < bindingCount; i++ ) {
		if ( bindings[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ) {
			bufCount += bindings[i].descriptorCount;
		} else if ( bindings[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ) {
			imgCount += bindings[i].descriptorCount;
		}
	}
	if ( bufCount > 0 ) {
		poolSizes[poolCount].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		poolSizes[poolCount].descriptorCount = bufCount;
		poolCount++;
	}
	if ( imgCount > 0 ) {
		poolSizes[poolCount].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		poolSizes[poolCount].descriptorCount = imgCount;
		poolCount++;
	}

	Com_Memset( &pci, 0, sizeof( pci ) );
	pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pci.maxSets = 1;
	pci.poolSizeCount = poolCount;
	pci.pPoolSizes = poolSizes;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &pci, NULL, &p->pool ) );

	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	ai.descriptorPool = p->pool;
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &p->layout;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &ai, &p->descriptor ) );

	p->ready = qtrue;
	return qtrue;
}

static qboolean Iris_FindMemoryType( uint32_t typeBits, VkMemoryPropertyFlags props, uint32_t *outIndex )
{
	VkPhysicalDeviceMemoryProperties memProps;
	uint32_t i;

	qvkGetPhysicalDeviceMemoryProperties( vk.physical_device, &memProps );
	for ( i = 0; i < memProps.memoryTypeCount; i++ ) {
		if ( ( typeBits & ( 1u << i ) ) &&
			( memProps.memoryTypes[i].propertyFlags & props ) == props ) {
			*outIndex = i;
			return qtrue;
		}
	}
	return qfalse;
}

static qboolean Iris_CreateStorageImage( uint32_t w, uint32_t h, VkFormat fmt, VkImageUsageFlags usage,
	VkMemoryPropertyFlags memProps, VkImage *outImg, VkImageView *outView, VkDeviceMemory *outMem )
{
	VkImageCreateInfo image_desc;
	VkMemoryRequirements mem_req;
	VkMemoryAllocateInfo alloc_info;
	VkImageViewCreateInfo view_desc;
	uint32_t memType;

	Com_Memset( &image_desc, 0, sizeof( image_desc ) );
	image_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_desc.imageType = VK_IMAGE_TYPE_2D;
	image_desc.format = fmt;
	image_desc.extent.width = w;
	image_desc.extent.height = h;
	image_desc.extent.depth = 1;
	image_desc.mipLevels = 1;
	image_desc.arrayLayers = 1;
	image_desc.samples = VK_SAMPLE_COUNT_1_BIT;
	image_desc.tiling = ( memProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ) ?
		VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
	image_desc.usage = usage;
	image_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK( qvkCreateImage( vk.device, &image_desc, NULL, outImg ) );
	qvkGetImageMemoryRequirements( vk.device, *outImg, &mem_req );
	if ( !Iris_FindMemoryType( mem_req.memoryTypeBits, memProps, &memType ) ) {
		return qfalse;
	}
	Com_Memset( &alloc_info, 0, sizeof( alloc_info ) );
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = memType;
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, outMem ) );
	VK_CHECK( qvkBindImageMemory( vk.device, *outImg, *outMem, 0 ) );

	Com_Memset( &view_desc, 0, sizeof( view_desc ) );
	view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_desc.image = *outImg;
	view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_desc.format = fmt;
	view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_desc.subresourceRange.levelCount = 1;
	view_desc.subresourceRange.layerCount = 1;
	VK_CHECK( qvkCreateImageView( vk.device, &view_desc, NULL, outView ) );
	return qtrue;
}

static void Iris_EnsurePipelines( void )
{
	VkDescriptorSetLayoutBinding clearBind;
	VkDescriptorSetLayoutBinding spdBinds[2];
	VkDescriptorSetLayoutBinding composeBinds[4];
	VkDescriptorSetLayoutBinding overlayBinds[2];

	if ( iris_clear.ready && iris_overlay.ready ) {
		return;
	}

	Com_Memset( &clearBind, 0, sizeof( clearBind ) );
	clearBind.binding = 0;
	clearBind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	clearBind.descriptorCount = 1;
	clearBind.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	(void)Iris_CreateComputePipeline( &iris_clear, vk.modules.iris_clear_cs, &clearBind, 1, 16 );

	Com_Memset( spdBinds, 0, sizeof( spdBinds ) );
	spdBinds[0].binding = 0;
	spdBinds[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	spdBinds[0].descriptorCount = 1;
	spdBinds[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	spdBinds[1].binding = 1;
	spdBinds[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	spdBinds[1].descriptorCount = 1;
	spdBinds[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	(void)Iris_CreateComputePipeline( &iris_spd, vk.modules.iris_spd_cs, spdBinds, 2, 24 );

	Com_Memset( composeBinds, 0, sizeof( composeBinds ) );
	composeBinds[0].binding = 0;
	composeBinds[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	composeBinds[0].descriptorCount = 1;
	composeBinds[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	composeBinds[1].binding = 1;
	composeBinds[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	composeBinds[1].descriptorCount = 1;
	composeBinds[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	composeBinds[2].binding = 2;
	composeBinds[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	composeBinds[2].descriptorCount = 1;
	composeBinds[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	composeBinds[3].binding = 3;
	composeBinds[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	composeBinds[3].descriptorCount = 1;
	composeBinds[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	(void)Iris_CreateComputePipeline( &iris_compose, vk.modules.iris_compose_cs, composeBinds, 4,
		(uint32_t)sizeof( irisComposePush_t ) );

	Com_Memset( overlayBinds, 0, sizeof( overlayBinds ) );
	overlayBinds[0].binding = 0;
	overlayBinds[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	overlayBinds[0].descriptorCount = 1;
	overlayBinds[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	overlayBinds[1].binding = 1;
	overlayBinds[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	overlayBinds[1].descriptorCount = 1;
	overlayBinds[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	(void)Iris_CreateComputePipeline( &iris_overlay, vk.modules.iris_overlay_cs, overlayBinds, 2, 32 );
}

static qboolean Iris_EnsureResources( void )
{
	uint32_t atlasW;
	uint32_t atlasH;

	if ( iris.ready ) {
		return qtrue;
	}

	atlasW = IRIS_ATLAS_TILES_X * IRIS_TILE_SIZE;
	atlasH = IRIS_ATLAS_TILES_Y * IRIS_TILE_SIZE;

	if ( !Iris_CreateStorageImage( atlasW, atlasH, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&vk.iris.tile_atlas, &vk.iris.tile_atlas_view, &vk.iris.tile_atlas_memory ) ) {
		return qfalse;
	}
	if ( !Iris_CreateStorageImage( atlasW / 2u, atlasH / 2u, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_STORAGE_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&vk.iris.tile_mip, &vk.iris.tile_mip_view, &vk.iris.tile_mip_memory ) ) {
		return qfalse;
	}
	if ( !Iris_CreateStorageImage( IRIS_ATLAS_TILES_X, IRIS_ATLAS_TILES_Y, VK_FORMAT_R32_UINT,
			VK_IMAGE_USAGE_STORAGE_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&vk.iris.tile_state, &vk.iris.tile_state_view, &vk.iris.tile_state_memory ) ) {
		return qfalse;
	}

	iris.width = r_iris_width ? (uint32_t)r_iris_width->integer : IRIS_DEFAULT_W;
	iris.height = r_iris_height ? (uint32_t)r_iris_height->integer : IRIS_DEFAULT_H;
	if ( !Iris_CreateStorageImage( iris.width, iris.height, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&vk.iris.scope_image, &vk.iris.scope_view, &vk.iris.scope_memory ) ) {
		return qfalse;
	}

	iris.ready = Iris_SeedAtlas();
	return iris.ready;
}

static qboolean Iris_SeedAtlas( void )
{
	uint32_t x;
	uint32_t y;
	uint8_t *host;
	VkDeviceSize atlasBytes;
	uint32_t zeroState[IRIS_ATLAS_TILES_X * IRIS_ATLAS_TILES_Y];

	atlasBytes = (VkDeviceSize)IRIS_ATLAS_TILES_X * IRIS_TILE_SIZE *
		IRIS_ATLAS_TILES_Y * IRIS_TILE_SIZE * 4u;
	host = (uint8_t *)ri.Hunk_AllocateTempMemory( (int)atlasBytes );
	if ( !host ) {
		return qfalse;
	}

	for ( y = 0; y < IRIS_ATLAS_TILES_Y * IRIS_TILE_SIZE; y++ ) {
		for ( x = 0; x < IRIS_ATLAS_TILES_X * IRIS_TILE_SIZE; x++ ) {
			uint32_t idx = ( y * IRIS_ATLAS_TILES_X * IRIS_TILE_SIZE + x ) * 4u;
			host[idx + 0] = (uint8_t)( ( x * 17 + y * 31 ) & 255 );
			host[idx + 1] = (uint8_t)( ( x * 29 + y * 13 ) & 255 );
			host[idx + 2] = (uint8_t)( ( x + y ) & 255 );
			host[idx + 3] = 255;
		}
	}

	if ( !Iris_StageTransferAtlas( host, NULL, qtrue ) ) {
		return qfalse;
	}

	Com_Memset( zeroState, 0, sizeof( zeroState ) );
	return Iris_UploadTileStateHost( zeroState, IRIS_ATLAS_TILES_X * IRIS_ATLAS_TILES_Y );
}

static qboolean Iris_StageTransferAtlas( const uint8_t *hostRgba, uint8_t *hostOut, qboolean upload )
{
	const uint32_t aw = IRIS_ATLAS_TILES_X * IRIS_TILE_SIZE;
	const uint32_t ah = IRIS_ATLAS_TILES_Y * IRIS_TILE_SIZE;
	const VkDeviceSize bytes = (VkDeviceSize)aw * ah * 4u;
	VkCommandBuffer cmd;
	VkBufferImageCopy region;

	if ( upload && !hostRgba ) {
		return qfalse;
	}
	if ( !upload && !hostOut ) {
		return qfalse;
	}
	if ( !upload && !iris.atlas_general ) {
		return qfalse;
	}

	vk_alloc_staging_buffer( bytes );
	cmd = vk_begin_command_buffer();
	if ( cmd == VK_NULL_HANDLE ) {
		return qfalse;
	}

	Com_Memset( &region, 0, sizeof( region ) );
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageExtent.width = aw;
	region.imageExtent.height = ah;
	region.imageExtent.depth = 1;

	if ( upload ) {
		VkImageLayout oldLayout = iris.atlas_general ?
			VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED;
		Com_Memcpy( vk.staging_buffer.ptr, hostRgba, (size_t)bytes );
		record_image_layout_transition( cmd, vk.iris.tile_atlas, VK_IMAGE_ASPECT_COLOR_BIT,
			oldLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );
		qvkCmdCopyBufferToImage( cmd, vk.staging_buffer.handle, vk.iris.tile_atlas,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );
		record_image_layout_transition( cmd, vk.iris.tile_atlas, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0 );
		iris.atlas_general = qtrue;
	} else {
		record_image_layout_transition( cmd, vk.iris.tile_atlas, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0 );
		qvkCmdCopyImageToBuffer( cmd, vk.iris.tile_atlas, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			vk.staging_buffer.handle, 1, &region );
		record_image_layout_transition( cmd, vk.iris.tile_atlas, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0 );
	}

	vk_end_command_buffer( cmd, "Iris_StageTransferAtlas" );

	if ( !upload ) {
		Com_Memcpy( hostOut, vk.staging_buffer.ptr, (size_t)bytes );
	}
	return qtrue;
}

static qboolean Iris_UploadAtlasHost( const uint8_t *rgba, uint32_t atlas_w, uint32_t atlas_h )
{
	if ( !rgba || !iris.ready ) {
		return qfalse;
	}
	if ( atlas_w != IRIS_ATLAS_TILES_X * IRIS_TILE_SIZE ||
		atlas_h != IRIS_ATLAS_TILES_Y * IRIS_TILE_SIZE ) {
		return qfalse;
	}
	return Iris_StageTransferAtlas( rgba, NULL, qtrue );
}

static qboolean Iris_DownloadAtlasHost( uint8_t *rgba, uint32_t atlas_w, uint32_t atlas_h )
{
	if ( !rgba || !iris.ready ) {
		return qfalse;
	}
	if ( atlas_w != IRIS_ATLAS_TILES_X * IRIS_TILE_SIZE ||
		atlas_h != IRIS_ATLAS_TILES_Y * IRIS_TILE_SIZE ) {
		return qfalse;
	}
	return Iris_StageTransferAtlas( NULL, rgba, qfalse );
}

static qboolean Iris_UploadTileStateHost( const uint32_t *state, uint32_t count )
{
	void *mapped;
	VkDeviceSize stateBytes;

	if ( !state || !iris.ready || count != IRIS_ATLAS_TILES_X * IRIS_ATLAS_TILES_Y ) {
		return qfalse;
	}

	stateBytes = (VkDeviceSize)count * sizeof( uint32_t );
	if ( qvkMapMemory( vk.device, vk.iris.tile_state_memory, 0, stateBytes, 0, &mapped ) != VK_SUCCESS ) {
		return qfalse;
	}
	Com_Memcpy( mapped, state, (size_t)stateBytes );
	qvkUnmapMemory( vk.device, vk.iris.tile_state_memory );
	return qtrue;
}

static qboolean Iris_DownloadTileStateHost( uint32_t *state, uint32_t count )
{
	void *mapped;
	VkDeviceSize stateBytes;

	if ( !state || !iris.ready || count != IRIS_ATLAS_TILES_X * IRIS_ATLAS_TILES_Y ) {
		return qfalse;
	}

	stateBytes = (VkDeviceSize)count * sizeof( uint32_t );
	if ( qvkMapMemory( vk.device, vk.iris.tile_state_memory, 0, stateBytes, 0, &mapped ) != VK_SUCCESS ) {
		return qfalse;
	}
	Com_Memcpy( state, mapped, (size_t)stateBytes );
	qvkUnmapMemory( vk.device, vk.iris.tile_state_memory );
	return qtrue;
}

static qboolean Iris_LoadAtlasFile( const char *path )
{
	iris_file_header_t hdr;
	const uint8_t *payload;
	const uint8_t *statePayload;
	uint8_t *fileBuf;
	int fileLen;

	if ( !path || !path[0] ) {
		ri.Printf( PRINT_WARNING, "[Iris] Usage: iris_load <path.iris>\n" );
		return qfalse;
	}
	if ( !R_Iris_Active() ) {
		ri.Printf( PRINT_WARNING, "[Iris] Enable r_iris 1 + vid_restart\n" );
		return qfalse;
	}
	if ( !Iris_EnsureResources() ) {
		ri.Printf( PRINT_WARNING, "[Iris] GPU resources not ready\n" );
		return qfalse;
	}

	fileLen = ri.FS_ReadFile( path, (void **)&fileBuf );
	if ( fileLen <= 0 || !fileBuf ) {
		ri.Printf( PRINT_WARNING, "[Iris] iris_load failed for '%s'\n", path );
		return qfalse;
	}
	if ( !Iris_ParseAtlasBuffer( fileBuf, fileLen, &hdr, &payload, &statePayload ) ) {
		ri.Printf( PRINT_WARNING, "[Iris] invalid .iris file '%s' (%s)\n",
			path, Iris_IoErrorString( Iris_IoLastError() ) );
		ri.FS_FreeFile( fileBuf );
		return qfalse;
	}
	if ( hdr.tile_px != IRIS_TILE_SIZE ||
		hdr.tiles_x != IRIS_ATLAS_TILES_X ||
		hdr.tiles_y != IRIS_ATLAS_TILES_Y ) {
		ri.Printf( PRINT_WARNING,
			"[Iris] atlas grid mismatch (file %ux%u tiles @%u, engine %ux%u @%u)\n",
			hdr.tiles_x, hdr.tiles_y, hdr.tile_px,
			IRIS_ATLAS_TILES_X, IRIS_ATLAS_TILES_Y, IRIS_TILE_SIZE );
		ri.FS_FreeFile( fileBuf );
		return qfalse;
	}
	if ( !Iris_UploadAtlasHost( payload, hdr.atlas_w, hdr.atlas_h ) ) {
		ri.Printf( PRINT_WARNING, "[Iris] GPU atlas upload failed\n" );
		ri.FS_FreeFile( fileBuf );
		return qfalse;
	}

	iris.pan_x = 0;
	iris.pan_y = 0;
	iris.pans = 0;
	iris.tiles_buffered = 0;
	iris.spd_dispatches = 0;
	if ( statePayload ) {
		(void)Iris_UploadTileStateHost( (const uint32_t *)statePayload,
			IRIS_ATLAS_TILES_X * IRIS_ATLAS_TILES_Y );
	} else {
		uint32_t zeroState[IRIS_ATLAS_TILES_X * IRIS_ATLAS_TILES_Y];
		Com_Memset( zeroState, 0, sizeof( zeroState ) );
		(void)Iris_UploadTileStateHost( zeroState, IRIS_ATLAS_TILES_X * IRIS_ATLAS_TILES_Y );
	}

	ri.FS_FreeFile( fileBuf );
	ri.Printf( PRINT_ALL, "[Iris] loaded '%s' (%ux%u RGBA8 atlas%s)\n", path, hdr.atlas_w, hdr.atlas_h,
		statePayload ? ", tile state" : "" );
	return qtrue;
}

static qboolean Iris_SaveAtlasFile( const char *path )
{
	uint32_t atlasW;
	uint32_t atlasH;
	VkDeviceSize atlasBytes;
	VkDeviceSize stateBytes;
	uint8_t *rgba;
	uint32_t *stateHost;
	uint8_t *fileBuf;
	int fileLen;
	int fileCap;

	if ( !path || !path[0] ) {
		ri.Printf( PRINT_WARNING, "[Iris] Usage: iris_save <path.iris>\n" );
		return qfalse;
	}
	if ( !R_Iris_Active() || !iris.ready ) {
		ri.Printf( PRINT_WARNING, "[Iris] Enable r_iris 1 + vid_restart; atlas must be ready\n" );
		return qfalse;
	}

	atlasW = IRIS_ATLAS_TILES_X * IRIS_TILE_SIZE;
	atlasH = IRIS_ATLAS_TILES_Y * IRIS_TILE_SIZE;
	atlasBytes = (VkDeviceSize)atlasW * atlasH * 4u;
	stateBytes = (VkDeviceSize)IRIS_ATLAS_TILES_X * IRIS_ATLAS_TILES_Y * sizeof( uint32_t );
	fileCap = (int)( sizeof( iris_file_header_t ) + atlasBytes + stateBytes );

	rgba = (uint8_t *)ri.Hunk_AllocateTempMemory( (int)atlasBytes );
	stateHost = (uint32_t *)ri.Hunk_AllocateTempMemory( (int)stateBytes );
	fileBuf = (uint8_t *)ri.Hunk_AllocateTempMemory( fileCap );
	if ( !rgba || !stateHost || !fileBuf ) {
		ri.Printf( PRINT_WARNING, "[Iris] temp memory allocation failed\n" );
		return qfalse;
	}

	if ( !Iris_DownloadAtlasHost( rgba, atlasW, atlasH ) ) {
		ri.Printf( PRINT_WARNING, "[Iris] GPU atlas readback failed\n" );
		return qfalse;
	}
	if ( !Iris_DownloadTileStateHost( stateHost, IRIS_ATLAS_TILES_X * IRIS_ATLAS_TILES_Y ) ) {
		ri.Printf( PRINT_WARNING, "[Iris] tile state readback failed\n" );
		return qfalse;
	}

	fileLen = Iris_SerializeAtlas( rgba, stateHost, atlasW, atlasH, IRIS_TILE_SIZE,
		IRIS_ATLAS_TILES_X, IRIS_ATLAS_TILES_Y, fileBuf, fileCap );
	if ( fileLen <= 0 ) {
		ri.Printf( PRINT_WARNING, "[Iris] serialize failed (%s)\n", Iris_IoErrorString( Iris_IoLastError() ) );
		return qfalse;
	}

	ri.FS_WriteFile( path, fileBuf, fileLen );
	ri.Printf( PRINT_ALL, "[Iris] saved '%s' (%ux%u RGBA8 + tile state, %d bytes)\n",
		path, atlasW, atlasH, fileLen );
	return qtrue;
}

static void Iris_DestroyGpu( void )
{
	if ( vk.iris.scope_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.iris.scope_view, NULL );
		qvkDestroyImage( vk.device, vk.iris.scope_image, NULL );
		qvkFreeMemory( vk.device, vk.iris.scope_memory, NULL );
	}
	if ( vk.iris.tile_mip_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.iris.tile_mip_view, NULL );
		qvkDestroyImage( vk.device, vk.iris.tile_mip, NULL );
		qvkFreeMemory( vk.device, vk.iris.tile_mip_memory, NULL );
	}
	if ( vk.iris.tile_atlas_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.iris.tile_atlas_view, NULL );
		qvkDestroyImage( vk.device, vk.iris.tile_atlas, NULL );
		qvkFreeMemory( vk.device, vk.iris.tile_atlas_memory, NULL );
	}
	if ( vk.iris.tile_state_view != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, vk.iris.tile_state_view, NULL );
		qvkDestroyImage( vk.device, vk.iris.tile_state, NULL );
		qvkFreeMemory( vk.device, vk.iris.tile_state_memory, NULL );
	}
	Com_Memset( &vk.iris, 0, sizeof( vk.iris ) );
	iris.atlas_general = qfalse;
	iris.mip_general = qfalse;
	iris.scope_general = qfalse;
	iris.ready = qfalse;
}

static void Iris_MarkTilesLR( int32_t panX, int32_t panY )
{
	uint32_t tx;
	uint32_t ty;
	uint32_t tilesX;
	uint32_t tilesY;
	uint32_t *stateHost;
	void *mapped;
	VkDeviceSize stateBytes;
	int i;

	tilesX = ( iris.width + IRIS_TILE_SIZE - 1u ) / IRIS_TILE_SIZE;
	tilesY = ( iris.height + IRIS_TILE_SIZE - 1u ) / IRIS_TILE_SIZE;
	stateBytes = (VkDeviceSize)IRIS_ATLAS_TILES_X * IRIS_ATLAS_TILES_Y * sizeof( uint32_t );

	if ( qvkMapMemory( vk.device, vk.iris.tile_state_memory, 0, stateBytes, 0, &mapped ) != VK_SUCCESS ) {
		return;
	}
	stateHost = (uint32_t *)mapped;

	for ( ty = 0; ty < tilesY; ty++ ) {
		for ( tx = 0; tx < tilesX; tx++ ) {
			int ax = (int)( panX + (int32_t)tx );
			int ay = (int)( panY + (int32_t)ty );
			uint32_t idx;
			if ( ax >= 0 && ay >= 0 && ax < (int)IRIS_ATLAS_TILES_X && ay < (int)IRIS_ATLAS_TILES_Y ) {
				idx = (uint32_t)( ay * IRIS_ATLAS_TILES_X + ax );
				if ( stateHost[idx] == IRIS_TILE_STATE_FREE ) {
					iris.tiles_buffered++;
				}
				stateHost[idx] = IRIS_TILE_STATE_LR;
			}
		}
	}

	/* RTBS microtransaction: promote subset to HR (OpenSlide model buffers fewer tiles). */
	{
		int promoteCount;
		promoteCount = (int)( tilesX * tilesY / 2u + 1u );
		if ( r_iris_decoder && r_iris_decoder->integer ) {
			promoteCount = (int)( tilesX * tilesY / 4u + 1u );
		}
		for ( i = 0; i < promoteCount; i++ ) {
			int ax = (int)( panX + (int32_t)( i % tilesX ) );
			int ay = (int)( panY + (int32_t)( i / tilesX ) );
			if ( ax >= 0 && ay >= 0 && ax < (int)IRIS_ATLAS_TILES_X && ay < (int)IRIS_ATLAS_TILES_Y ) {
				if ( stateHost[ ay * IRIS_ATLAS_TILES_X + ax ] == IRIS_TILE_STATE_LR ) {
					stateHost[ ay * IRIS_ATLAS_TILES_X + ax ] = IRIS_TILE_STATE_HR;
				}
			}
		}
	}
	qvkUnmapMemory( vk.device, vk.iris.tile_state_memory );
}

static void Iris_BarrierImageLayoutGeneral( VkCommandBuffer cmd, VkImage image, qboolean *tracked )
{
	VkImageMemoryBarrier barrier;
	VkImageLayout oldLayout;

	if ( !cmd || !image || !tracked ) {
		return;
	}

	oldLayout = *tracked ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED;

	Com_Memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;
	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, NULL, 0, NULL, 1, &barrier );
	*tracked = qtrue;
}

static uint32_t Iris_DispatchSpdForHrTiles( VkCommandBuffer cmd, qboolean singleTileOnly )
{
	VkDescriptorImageInfo atlasInfo;
	VkDescriptorImageInfo mipInfo;
	VkWriteDescriptorSet writes[2];
	struct { int tileOrigin[4]; int tileSize; float sharpen; } spdPush;
	VkDeviceSize stateBytes;
	void *mapped;
	uint32_t *stateHost;
	uint32_t tilesX;
	uint32_t tilesY;
	uint32_t dispatches;
	uint32_t ty;
	uint32_t tx;

	if ( !iris_spd.ready ) {
		return 0;
	}

	tilesX = ( iris.width + IRIS_TILE_SIZE - 1u ) / IRIS_TILE_SIZE;
	tilesY = ( iris.height + IRIS_TILE_SIZE - 1u ) / IRIS_TILE_SIZE;
	stateBytes = (VkDeviceSize)IRIS_ATLAS_TILES_X * IRIS_ATLAS_TILES_Y * sizeof( uint32_t );

	if ( qvkMapMemory( vk.device, vk.iris.tile_state_memory, 0, stateBytes, 0, &mapped ) != VK_SUCCESS ) {
		return 0;
	}
	stateHost = (uint32_t *)mapped;

	Iris_BarrierImageLayoutGeneral( cmd, vk.iris.tile_atlas, &iris.atlas_general );
	Iris_BarrierImageLayoutGeneral( cmd, vk.iris.tile_mip, &iris.mip_general );

	atlasInfo.imageView = vk.iris.tile_atlas_view;
	atlasInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	atlasInfo.sampler = VK_NULL_HANDLE;
	mipInfo.imageView = vk.iris.tile_mip_view;
	mipInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	mipInfo.sampler = VK_NULL_HANDLE;

	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = iris_spd.descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[0].pImageInfo = &atlasInfo;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = iris_spd.descriptor;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[1].pImageInfo = &mipInfo;
	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, iris_spd.pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, iris_spd.pipeline_layout,
		0, 1, &iris_spd.descriptor, 0, NULL );

	spdPush.tileSize = (int)IRIS_TILE_SIZE;
	spdPush.sharpen = r_iris_sharpen ? r_iris_sharpen->value : 0.35f;
	dispatches = 0;

	for ( ty = 0; ty < tilesY; ty++ ) {
		for ( tx = 0; tx < tilesX; tx++ ) {
			int ax = iris.pan_x + (int32_t)tx;
			int ay = iris.pan_y + (int32_t)ty;

			if ( ax < 0 || ay < 0 || ax >= (int)IRIS_ATLAS_TILES_X || ay >= (int)IRIS_ATLAS_TILES_Y ) {
				continue;
			}
			if ( stateHost[ ay * IRIS_ATLAS_TILES_X + ax ] != IRIS_TILE_STATE_HR ) {
				continue;
			}

			spdPush.tileOrigin[0] = ax * (int)IRIS_TILE_SIZE;
			spdPush.tileOrigin[1] = ay * (int)IRIS_TILE_SIZE;
			spdPush.tileOrigin[2] = 0;
			spdPush.tileOrigin[3] = 0;
			qvkCmdPushConstants( cmd, iris_spd.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
				0, sizeof( spdPush ), &spdPush );
			qvkCmdDispatch( cmd, ( IRIS_TILE_SIZE / 2u + 15u ) / 16u,
				( IRIS_TILE_SIZE / 2u + 15u ) / 16u, 1 );
			dispatches++;
			if ( singleTileOnly ) {
				goto done;
			}
		}
	}

done:
	qvkUnmapMemory( vk.device, vk.iris.tile_state_memory );
	return dispatches;
}

static qboolean Iris_RunPanPass( void )
{
	VkCommandBuffer cmd;
	VkDescriptorImageInfo atlasInfo;
	VkDescriptorImageInfo mipInfo;
	VkDescriptorImageInfo stateInfo;
	VkDescriptorImageInfo scopeInfo;
	VkWriteDescriptorSet writes[4];
	VkImageMemoryBarrier barrier;
	struct { int viewport[4]; } clearPush;
	irisComposePush_t composePush;
	uint32_t spdCount;

	if ( !iris.ready || !iris_clear.ready || !iris_compose.ready ) {
		return qfalse;
	}

	cmd = vk_begin_command_buffer();
	if ( cmd == VK_NULL_HANDLE ) {
		return qfalse;
	}

	Com_Memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.oldLayout = iris.scope_general ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.image = vk.iris.scope_image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;
	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, NULL, 0, NULL, 1, &barrier );
	iris.scope_general = qtrue;

	scopeInfo.imageView = vk.iris.scope_view;
	scopeInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	scopeInfo.sampler = VK_NULL_HANDLE;

	clearPush.viewport[0] = (int)iris.width;
	clearPush.viewport[1] = (int)iris.height;
	clearPush.viewport[2] = 0;
	clearPush.viewport[3] = 0;

	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = iris_clear.descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[0].pImageInfo = &scopeInfo;
	qvkUpdateDescriptorSets( vk.device, 1, writes, 0, NULL );

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, iris_clear.pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, iris_clear.pipeline_layout,
		0, 1, &iris_clear.descriptor, 0, NULL );
	qvkCmdPushConstants( cmd, iris_clear.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof( clearPush ), &clearPush );
	qvkCmdDispatch( cmd, ( iris.width + 15u ) / 16u, ( iris.height + 15u ) / 16u, 1 );

	spdCount = Iris_DispatchSpdForHrTiles( cmd, qfalse );
	iris.spd_dispatches += spdCount;

	atlasInfo.imageView = vk.iris.tile_atlas_view;
	atlasInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	mipInfo.imageView = vk.iris.tile_mip_view;
	mipInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	stateInfo.imageView = vk.iris.tile_state_view;
	stateInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	composePush.viewport[0] = (int)iris.width;
	composePush.viewport[1] = (int)iris.height;
	composePush.viewport[2] = 0;
	composePush.viewport[3] = 0;
	composePush.panTiles[0] = iris.pan_x;
	composePush.panTiles[1] = iris.pan_y;
	composePush.atlasTilesX = (int)IRIS_ATLAS_TILES_X;
	composePush.atlasTilesY = (int)IRIS_ATLAS_TILES_Y;
	composePush.layerHr = 0;
	composePush.mipBilinear = ( r_iris_bilinear && r_iris_bilinear->integer ) ? 1 : 0;

	writes[0].dstSet = iris_compose.descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[0].pImageInfo = &atlasInfo;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = iris_compose.descriptor;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[1].pImageInfo = &stateInfo;
	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = iris_compose.descriptor;
	writes[2].dstBinding = 2;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[2].pImageInfo = &scopeInfo;
	writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[3].dstSet = iris_compose.descriptor;
	writes[3].dstBinding = 3;
	writes[3].descriptorCount = 1;
	writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[3].pImageInfo = &mipInfo;
	qvkUpdateDescriptorSets( vk.device, 4, writes, 0, NULL );

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, iris_compose.pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, iris_compose.pipeline_layout,
		0, 1, &iris_compose.descriptor, 0, NULL );
	qvkCmdPushConstants( cmd, iris_compose.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof( composePush ), &composePush );
	qvkCmdDispatch( cmd, ( iris.width + 15u ) / 16u, ( iris.height + 15u ) / 16u, 1 );

	composePush.layerHr = 1;
	qvkCmdPushConstants( cmd, iris_compose.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof( composePush ), &composePush );
	qvkCmdDispatch( cmd, ( iris.width + 15u ) / 16u, ( iris.height + 15u ) / 16u, 1 );

	vk_end_command_buffer( cmd, "Iris_RunPanPass" );
	return qtrue;
}

static void Iris_Cmd_Status( void )
{
	ri.Printf( PRINT_ALL,
		"[Iris] r_iris=%d overlay=%d mode=%d alpha=%.2f scale=%.2f scope=%ux%u pan=(%d,%d)\n",
		r_iris ? r_iris->integer : 0,
		r_iris_overlay ? r_iris_overlay->integer : 0,
		r_iris_overlay_mode ? r_iris_overlay_mode->integer : 0,
		r_iris_overlay_alpha ? r_iris_overlay_alpha->value : 0.92f,
		r_iris_overlay_scale ? r_iris_overlay_scale->value : 0.4f,
		iris.width, iris.height, iris.pan_x, iris.pan_y );
	ri.Printf( PRINT_ALL,
		"[Iris] pans=%u tiles=%u spd=%u ready=%d decoder=%d sharpen=%.2f bilinear=%d\n",
		iris.pans, iris.tiles_buffered, iris.spd_dispatches, iris.ready ? 1 : 0,
		r_iris_decoder ? r_iris_decoder->integer : 0,
		r_iris_sharpen ? r_iris_sharpen->value : 0.35f,
		r_iris_bilinear ? r_iris_bilinear->integer : 1 );
}

static void Iris_Cmd_Load( void )
{
	const char *path = ( ri.Cmd_Argc() >= 2 ) ? ri.Cmd_Argv( 1 ) : NULL;
	(void)Iris_LoadAtlasFile( path );
}

static void Iris_Cmd_Save( void )
{
	const char *path = ( ri.Cmd_Argc() >= 2 ) ? ri.Cmd_Argv( 1 ) : NULL;
	(void)Iris_SaveAtlasFile( path );
}

static void Iris_Cmd_SpdStep( void )
{
	VkCommandBuffer cmd;
	uint32_t count;

	if ( !R_Iris_Active() ) {
		ri.Printf( PRINT_WARNING, "[Iris] Enable r_iris 1 + vid_restart\n" );
		return;
	}
	if ( !Iris_EnsureResources() ) {
		ri.Printf( PRINT_WARNING, "[Iris] GPU resources not ready\n" );
		return;
	}
	Iris_EnsurePipelines();
	if ( !iris_spd.ready ) {
		ri.Printf( PRINT_WARNING, "[Iris] iris_spd pipeline not ready\n" );
		return;
	}

	cmd = vk_begin_command_buffer();
	if ( cmd == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, "[Iris] command buffer unavailable\n" );
		return;
	}
	count = Iris_DispatchSpdForHrTiles( cmd, qtrue );
	iris.spd_dispatches += count;
	vk_end_command_buffer( cmd, "Iris_Cmd_SpdStep" );

	ri.Printf( PRINT_ALL, "[Iris] SPD step: %u HR tile(s) sharpened (r_iris_sharpen=%.2f)\n",
		count, r_iris_sharpen ? r_iris_sharpen->value : 0.35f );
}

static void Iris_Cmd_Pan( void )
{
	int steps;
	int i;

	if ( !R_Iris_Active() ) {
		ri.Printf( PRINT_WARNING, "[Iris] Enable r_iris 1 + vid_restart\n" );
		return;
	}
	if ( !Iris_EnsureResources() ) {
		ri.Printf( PRINT_WARNING, "[Iris] GPU resources not ready\n" );
		return;
	}
	Iris_EnsurePipelines();

	steps = ( ri.Cmd_Argc() >= 2 ) ? atoi( ri.Cmd_Argv( 1 ) ) : 1;
	for ( i = 0; i < steps; i++ ) {
		iris.pan_x += 3;
		iris.pan_y += 2;
		if ( iris.pan_x > (int)IRIS_ATLAS_TILES_X - 4 ) {
			iris.pan_x = 0;
		}
		if ( iris.pan_y > (int)IRIS_ATLAS_TILES_Y - 4 ) {
			iris.pan_y = 0;
		}
		Iris_MarkTilesLR( iris.pan_x, iris.pan_y );
		if ( !Iris_RunPanPass() ) {
			ri.Printf( PRINT_WARNING, "[Iris] pan pass failed at step %d\n", i );
			break;
		}
		iris.pans++;
	}

	if ( r_iris_debug && r_iris_debug->integer ) {
		ri.Printf( PRINT_ALL, "[Iris] pan complete steps=%d (TeFOV model: iris_teFOV)\n", steps );
	}
}

static void Iris_Cmd_Reset( void )
{
	if ( !R_Iris_Active() ) {
		ri.Printf( PRINT_WARNING, "[Iris] Enable r_iris 1 + vid_restart\n" );
		return;
	}
	if ( !Iris_EnsureResources() ) {
		ri.Printf( PRINT_WARNING, "[Iris] GPU resources not ready\n" );
		return;
	}

	iris.pan_x = 0;
	iris.pan_y = 0;
	iris.pans = 0;
	iris.tiles_buffered = 0;
	iris.spd_dispatches = 0;
	iris.mip_general = qfalse;
	if ( !Iris_SeedAtlas() ) {
		ri.Printf( PRINT_WARNING, "[Iris] reset failed to re-seed atlas\n" );
		return;
	}
	ri.Printf( PRINT_ALL, "[Iris] reset scope pan, tile state, and SPD counters\n" );
}

void R_Iris_Init( void )
{
	r_iris = ri.Cvar_Get( "r_iris", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_iris_width = ri.Cvar_Get( "r_iris_width", "1024", CVAR_ARCHIVE_ND );
	r_iris_height = ri.Cvar_Get( "r_iris_height", "768", CVAR_ARCHIVE_ND );
	r_iris_decoder = ri.Cvar_Get( "r_iris_decoder", "0", CVAR_ARCHIVE_ND );
	r_iris_sharpen = ri.Cvar_Get( "r_iris_sharpen", "0.35", CVAR_ARCHIVE_ND );
	r_iris_bilinear = ri.Cvar_Get( "r_iris_bilinear", "1", CVAR_ARCHIVE_ND );
	r_iris_overlay = ri.Cvar_Get( "r_iris_overlay", "0", CVAR_ARCHIVE_ND );
	r_iris_overlay_alpha = ri.Cvar_Get( "r_iris_overlay_alpha", "0.92", CVAR_ARCHIVE_ND );
	r_iris_overlay_scale = ri.Cvar_Get( "r_iris_overlay_scale", "0.4", CVAR_ARCHIVE_ND );
	r_iris_overlay_mode = ri.Cvar_Get( "r_iris_overlay_mode", "0", CVAR_ARCHIVE_ND );
	r_iris_debug = ri.Cvar_Get( "r_iris_debug", "0", CVAR_ARCHIVE_ND );

	ri.Cvar_CheckRange( r_iris, "0", "1", CV_INTEGER );
	ri.Cvar_CheckRange( r_iris_width, "256", "4096", CV_INTEGER );
	ri.Cvar_CheckRange( r_iris_height, "256", "4096", CV_INTEGER );
	ri.Cvar_CheckRange( r_iris_decoder, "0", "1", CV_INTEGER );
	ri.Cvar_CheckRange( r_iris_overlay, "0", "1", CV_INTEGER );
	ri.Cvar_CheckRange( r_iris_bilinear, "0", "1", CV_INTEGER );
	ri.Cvar_CheckRange( r_iris_overlay_mode, "0", "1", CV_INTEGER );

	ri.Cvar_SetDescription( r_iris,
		"Iris digital pathology WSI renderer scaffold (J Pathol Inform 2025)." );
	ri.Cvar_SetDescription( r_iris_sharpen,
		"SPD Laplacian sharpen scale for HR tile mips." );
	ri.Cvar_SetDescription( r_iris_bilinear,
		"Bilinear upsample of tile_mip during LR compose (0=nearest)." );
	ri.Cvar_SetDescription( r_iris_overlay,
		"Alpha-blend Iris scope onto the main color buffer after the scene pass." );
	ri.Cvar_SetDescription( r_iris_overlay_alpha,
		"Scope overlay blend strength (1 = opaque scope in overlay region)." );
	ri.Cvar_SetDescription( r_iris_overlay_scale,
		"PiP overlay width as a fraction of the framebuffer (mode 0 only)." );
	ri.Cvar_SetDescription( r_iris_overlay_mode,
		"0 = bottom-left PiP, 1 = fullscreen overlay." );
	ri.Cvar_SetDescription( r_iris_decoder,
		"0=Iris Codec RTBS (fast HR promote), 1=OpenSlide model (slower HR promote)." );

	ri.Cmd_AddCommand( "iris_status", Iris_Cmd_Status );
	ri.Cmd_AddCommand( "iris_pan", Iris_Cmd_Pan );
	ri.Cmd_AddCommand( "iris_spd_step", Iris_Cmd_SpdStep );
	ri.Cmd_AddCommand( "iris_load", Iris_Cmd_Load );
	ri.Cmd_AddCommand( "iris_save", Iris_Cmd_Save );
	ri.Cmd_AddCommand( "iris_reset", Iris_Cmd_Reset );

	if ( r_iris->integer ) {
		ri.Printf( PRINT_ALL, "[Iris] Enabled — iris_pan, r_iris_overlay, see docs/IRIS.md\n" );
	}
}

void R_Iris_Shutdown( void )
{
	ri.Cmd_RemoveCommand( "iris_status" );
	ri.Cmd_RemoveCommand( "iris_pan" );
	ri.Cmd_RemoveCommand( "iris_spd_step" );
	ri.Cmd_RemoveCommand( "iris_load" );
	ri.Cmd_RemoveCommand( "iris_save" );
	ri.Cmd_RemoveCommand( "iris_reset" );
	Iris_DestroyPipeline( &iris_clear );
	Iris_DestroyPipeline( &iris_spd );
	Iris_DestroyPipeline( &iris_compose );
	Iris_DestroyPipeline( &iris_overlay );
	Iris_DestroyGpu();
}

qboolean R_Iris_Active( void )
{
	return ( r_iris && r_iris->integer && vk.device != VK_NULL_HANDLE ) ? qtrue : qfalse;
}

qboolean R_Iris_PanNewFOV( void )
{
	if ( !R_Iris_Active() ) {
		return qfalse;
	}
	if ( !Iris_EnsureResources() ) {
		return qfalse;
	}
	Iris_EnsurePipelines();
	iris.pan_x += 3;
	iris.pan_y += 2;
	Iris_MarkTilesLR( iris.pan_x, iris.pan_y );
	iris.pans++;
	return Iris_RunPanPass();
}

qboolean vk_iris_overlay_active( void )
{
	if ( !R_Iris_Active() || !iris.ready || !r_iris_overlay || !r_iris_overlay->integer ||
		!vk.fboActive || vk.color_image == VK_NULL_HANDLE ) {
		return qfalse;
	}
	Iris_EnsurePipelines();
	return iris_overlay.ready ? qtrue : qfalse;
}

void vk_iris_record_overlay( VkCommandBuffer cmd )
{
	VkImageMemoryBarrier barriers[2];
	VkDescriptorImageInfo scopeInfo;
	VkDescriptorImageInfo colorInfo;
	VkWriteDescriptorSet writes[2];
	struct { int dstRect[4]; int scopeSize[2]; float alpha; float pad; } overlayPush;
	VkImageLayout colorOldLayout;
	VkImageLayout colorRestoreLayout;
	uint32_t fbW;
	uint32_t fbH;
	uint32_t dstW;
	uint32_t dstH;
	uint32_t dstX;
	uint32_t dstY;
	float scale;
	float alpha;
	float aspect;

	if ( !vk_iris_overlay_active() || !cmd || vk.iris.scope_image == VK_NULL_HANDLE ) {
		return;
	}
	if ( vk.renderPassIndex != RENDER_PASS_MAIN && vk.renderPassIndex != RENDER_PASS_POST_BLOOM ) {
		return;
	}

	fbW = vk_get_render_target_width();
	fbH = vk_get_render_target_height();
	if ( fbW < 1u || fbH < 1u ) {
		return;
	}

	alpha = r_iris_overlay_alpha ? r_iris_overlay_alpha->value : 0.92f;
	if ( alpha < 0.0f ) {
		alpha = 0.0f;
	}
	if ( alpha > 1.0f ) {
		alpha = 1.0f;
	}

	if ( r_iris_overlay_mode && r_iris_overlay_mode->integer ) {
		dstX = 0;
		dstY = 0;
		dstW = fbW;
		dstH = fbH;
	} else {
		scale = r_iris_overlay_scale ? r_iris_overlay_scale->value : 0.4f;
		if ( scale < 0.1f ) {
			scale = 0.1f;
		}
		if ( scale > 1.0f ) {
			scale = 1.0f;
		}
		aspect = ( iris.height > 0u ) ? ( (float)iris.width / (float)iris.height ) : 1.0f;
		dstW = (uint32_t)( (float)fbW * scale );
		if ( dstW < 64u ) {
			dstW = 64u;
		}
		dstH = (uint32_t)( (float)dstW / aspect );
		if ( dstH < 48u ) {
			dstH = 48u;
		}
		if ( dstH > fbH ) {
			dstH = fbH;
			dstW = (uint32_t)( (float)dstH * aspect );
		}
		dstX = 16u;
		dstY = ( fbH > dstH ) ? ( fbH - dstH - 16u ) : 0u;
	}

	colorRestoreLayout = ( vk.renderPassIndex == RENDER_PASS_POST_BLOOM ) ?
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorOldLayout = colorRestoreLayout;

	Com_Memset( barriers, 0, sizeof( barriers ) );
	barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barriers[0].image = vk.iris.scope_image;
	barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barriers[0].subresourceRange.levelCount = 1;
	barriers[0].subresourceRange.layerCount = 1;

	barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
	barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	barriers[1].oldLayout = colorOldLayout;
	barriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barriers[1].image = vk.color_image;
	barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barriers[1].subresourceRange.levelCount = 1;
	barriers[1].subresourceRange.layerCount = 1;

	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, NULL, 0, NULL, 2, barriers );

	scopeInfo.imageView = vk.iris.scope_view;
	scopeInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	scopeInfo.sampler = VK_NULL_HANDLE;
	colorInfo.imageView = vk.color_image_view;
	colorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	colorInfo.sampler = VK_NULL_HANDLE;

	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = iris_overlay.descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[0].pImageInfo = &scopeInfo;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = iris_overlay.descriptor;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[1].pImageInfo = &colorInfo;
	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );

	overlayPush.dstRect[0] = (int)dstX;
	overlayPush.dstRect[1] = (int)dstY;
	overlayPush.dstRect[2] = (int)dstW;
	overlayPush.dstRect[3] = (int)dstH;
	overlayPush.scopeSize[0] = (int)iris.width;
	overlayPush.scopeSize[1] = (int)iris.height;
	overlayPush.alpha = alpha;
	overlayPush.pad = 0.0f;

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, iris_overlay.pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, iris_overlay.pipeline_layout,
		0, 1, &iris_overlay.descriptor, 0, NULL );
	qvkCmdPushConstants( cmd, iris_overlay.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof( overlayPush ), &overlayPush );
	qvkCmdDispatch( cmd, ( dstW + 15u ) / 16u, ( dstH + 15u ) / 16u, 1 );

	barriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barriers[1].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	barriers[1].newLayout = colorRestoreLayout;
	qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		0, 0, NULL, 0, NULL, 1, &barriers[1] );

	if ( r_iris_debug && r_iris_debug->integer ) {
		ri.Printf( PRINT_DEVELOPER, "[Iris] overlay %ux%u@%u,%u alpha=%.2f mode=%d\n",
			dstW, dstH, dstX, dstY, alpha,
			r_iris_overlay_mode ? r_iris_overlay_mode->integer : 0 );
	}
}
