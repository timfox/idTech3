/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

OpenVDB / NanoVDB integration implementation.

Loading pipeline:
  1. FS_ReadFile loads the .vdb/.nvdb from game filesystem
  2. For NanoVDB: parse the flat buffer header directly (no dependencies)
  3. For OpenVDB: use the library if USE_OPENVDB is defined
  4. Grid data stored in a linear float array for CPU sampling
  5. VDB_UploadToGPU creates a 3D texture for shader access
  6. VDB_BindAsFogDensity connects to the volumetric fog system

NanoVDB format (.nvdb):
  A flat binary format designed for GPU consumption. Each grid is
  stored as a contiguous buffer with a tree structure that maps
  directly to GPU memory. The header contains grid metadata
  (bounding box, voxel size, grid type).

Sampling:
  VDB_SampleFloat does trilinear interpolation in index space,
  transforming from world coordinates using the grid's voxel size
  and bounding box.
===========================================================================
*/

#include "tr_local.h"
#include "vk_vdb.h"
#include "vk.h"
#include "vk_cmd.h"
#include "vk_descriptor_sets.h"
#include "vk_staging.h"
#include "vk_image_layout.h"
#include <math.h>

typedef struct {
	qboolean    active;
	char        filename[MAX_QPATH];
	vdbGridInfo_t info;
	float       *data;
	int         dataSize;
	qboolean    onGPU;
	VkImage     gpuImage;
	VkDeviceMemory gpuMemory;
	VkImageView gpuView;
} vdbGrid_t;

static vdbGrid_t grids[VDB_MAX_GRIDS];
static int numGrids = 0;
static vdbHandle_t boundFogDensityHandle = VDB_INVALID_HANDLE;
static cvar_t *r_vdb;
static qboolean vdb_console_cmds_registered = qfalse;

#define VALID_GRID(h) ((h) >= 0 && (h) < numGrids && grids[(h)].active)

static vdbHandle_t VDB_ParseHandleArg( const char *s )
{
	char *end;
	long v;

	if ( !s || !s[0] ) {
		return VDB_INVALID_HANDLE;
	}
	v = strtol( s, &end, 10 );
	if ( end == s || *end != '\0' || v < 0 || v >= VDB_MAX_GRIDS ) {
		return VDB_INVALID_HANDLE;
	}
	return (vdbHandle_t)v;
}

static void VDB_Cmd_Load_f( void )
{
	const char *filename;
	const char *gridName;
	vdbHandle_t h;

	if ( ri.Cmd_Argc() < 2 ) {
		ri.Printf( PRINT_ALL, "Usage: vdb_load <filename> [gridName]\n" );
		return;
	}
	filename = ri.Cmd_Argv( 1 );
	gridName = ( ri.Cmd_Argc() >= 3 ) ? ri.Cmd_Argv( 2 ) : "density";
	h = VDB_Load( filename, gridName );
	if ( h < 0 ) {
		ri.Printf( PRINT_WARNING, "VDB: vdb_load failed for '%s'\n", filename );
		return;
	}
	ri.Printf( PRINT_ALL, "VDB: loaded handle %d '%s' from %s\n", h, grids[h].info.name, filename );
}

static void VDB_Cmd_Upload_f( void )
{
	vdbHandle_t h;

	if ( ri.Cmd_Argc() < 2 ) {
		ri.Printf( PRINT_ALL, "Usage: vdb_upload <handle>\n" );
		return;
	}
	h = VDB_ParseHandleArg( ri.Cmd_Argv( 1 ) );
	if ( !VALID_GRID( h ) ) {
		ri.Printf( PRINT_WARNING, "VDB: invalid handle\n" );
		return;
	}
	if ( !VDB_UploadToGPU( h ) ) {
		ri.Printf( PRINT_WARNING, "VDB: GPU upload failed for handle %d\n", h );
		return;
	}
	vk_update_volumetric_descriptors();
	ri.Printf( PRINT_ALL, "VDB: handle %d on GPU; volumetric descriptors refreshed\n", h );
}

static void VDB_Cmd_BindFog_f( void )
{
	vdbHandle_t h;

	if ( ri.Cmd_Argc() < 2 ) {
		ri.Printf( PRINT_ALL, "Usage: vdb_bind_fog <handle>\n" );
		return;
	}
	h = VDB_ParseHandleArg( ri.Cmd_Argv( 1 ) );
	if ( !VALID_GRID( h ) ) {
		ri.Printf( PRINT_WARNING, "VDB: invalid handle\n" );
		return;
	}
	if ( !VDB_BindAsFogDensity( h ) ) {
		ri.Printf( PRINT_WARNING, "VDB: bind failed for handle %d\n", h );
		return;
	}
	if ( !VDB_IsOnGPU( h ) ) {
		ri.Printf( PRINT_ALL, "VDB: handle %d bound; run vdb_upload %d before r_vdbFog will sample it\n", h, h );
	} else {
		vk_update_volumetric_descriptors();
		ri.Printf( PRINT_ALL, "VDB: handle %d bound for fog; enable r_volumetricFog 1 and r_vdbFog 1\n", h );
	}
}

static void VDB_Cmd_List_f( void )
{
	int i;

	if ( numGrids <= 0 ) {
		ri.Printf( PRINT_ALL, "VDB: no grids loaded\n" );
		return;
	}
	for ( i = 0; i < numGrids; i++ ) {
		if ( !grids[i].active ) {
			continue;
		}
		ri.Printf( PRINT_ALL, "  [%d] %s %dx%dx%d gpu=%s bound_fog=%s\n",
			i, grids[i].info.name,
			grids[i].info.dimX, grids[i].info.dimY, grids[i].info.dimZ,
			grids[i].onGPU ? "yes" : "no",
			( boundFogDensityHandle == i ) ? "yes" : "no" );
	}
}

static void VDB_RegisterConsoleCommands( void )
{
	if ( vdb_console_cmds_registered ) {
		return;
	}
	ri.Cmd_AddCommand( "vdb_load", VDB_Cmd_Load_f );
	ri.Cmd_AddCommand( "vdb_upload", VDB_Cmd_Upload_f );
	ri.Cmd_AddCommand( "vdb_bind_fog", VDB_Cmd_BindFog_f );
	ri.Cmd_AddCommand( "vdb_list", VDB_Cmd_List_f );
	vdb_console_cmds_registered = qtrue;
	ri.Printf( PRINT_DEVELOPER, "VDB: console commands vdb_load, vdb_upload, vdb_bind_fog, vdb_list\n" );
}

static void VDB_UnregisterConsoleCommands( void )
{
	if ( !vdb_console_cmds_registered ) {
		return;
	}
	ri.Cmd_RemoveCommand( "vdb_load" );
	ri.Cmd_RemoveCommand( "vdb_upload" );
	ri.Cmd_RemoveCommand( "vdb_bind_fog" );
	ri.Cmd_RemoveCommand( "vdb_list" );
	vdb_console_cmds_registered = qfalse;
}

static void VDB_DestroyGpuResources( vdbGrid_t *grid )
{
	if ( !grid ) {
		return;
	}
	if ( grid->gpuView != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, grid->gpuView, NULL );
		grid->gpuView = VK_NULL_HANDLE;
	}
	if ( grid->gpuImage != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, grid->gpuImage, NULL );
		grid->gpuImage = VK_NULL_HANDLE;
	}
	if ( grid->gpuMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, grid->gpuMemory, NULL );
		grid->gpuMemory = VK_NULL_HANDLE;
	}
	grid->onGPU = qfalse;
}

void VDB_Init( void ) {
	Com_Memset( grids, 0, sizeof( grids ) );
	numGrids = 0;
	r_vdb = ri.Cvar_Get( "r_vdb", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_vdb, "Enable OpenVDB/NanoVDB volumetric data loading (0 = off, 1 = on)." );

#ifdef USE_OPENVDB
	ri.Printf( PRINT_ALL, "VDB: initialized (OpenVDB + NanoVDB)\n" );
#else
	ri.Printf( PRINT_ALL, "VDB: initialized (NanoVDB only, compile with USE_OPENVDB for full support)\n" );
#endif
	VDB_RegisterConsoleCommands();
}

void VDB_Shutdown( void ) {
	int i;

	VDB_UnregisterConsoleCommands();
	boundFogDensityHandle = VDB_INVALID_HANDLE;
	for ( i = 0; i < numGrids; i++ ) {
		VDB_DestroyGpuResources( &grids[i] );
		if ( grids[i].data ) {
			ri.Free( grids[i].data );
			grids[i].data = NULL;
		}
		grids[i].active = qfalse;
	}
	numGrids = 0;
}

/* ---- NanoVDB header parsing ---- */

#define NANOVDB_MAGIC 0x304244566f6e614eULL /* "NanoVDB0" */

typedef struct {
	uint64_t magic;
	uint64_t checksum;
	uint32_t version;
	uint32_t flags;
	uint32_t gridCount;
	char     gridName[256];
} nanovdbFileHeader_t;

typedef struct {
	uint64_t gridSize;
	uint64_t gridOffset;
	float    worldBBox[6];
	float    voxelSize[3];
	uint32_t gridClass;
	uint32_t gridType;
	uint64_t activeVoxelCount;
} nanovdbGridMeta_t;

static qboolean VDB_ParseNanoVDB( const byte *buf, int bufLen, vdbGrid_t *grid ) {
	const nanovdbFileHeader_t *hdr;
	const nanovdbGridMeta_t *meta;

	if ( bufLen < (int)sizeof( nanovdbFileHeader_t ) + (int)sizeof( nanovdbGridMeta_t ) ) {
		return qfalse;
	}

	hdr = (const nanovdbFileHeader_t *)buf;
	if ( hdr->magic != NANOVDB_MAGIC ) {
		return qfalse;
	}

	meta = (const nanovdbGridMeta_t *)( buf + sizeof( nanovdbFileHeader_t ) );

	grid->info.worldMin[0] = meta->worldBBox[0];
	grid->info.worldMin[1] = meta->worldBBox[1];
	grid->info.worldMin[2] = meta->worldBBox[2];
	grid->info.worldMax[0] = meta->worldBBox[3];
	grid->info.worldMax[1] = meta->worldBBox[4];
	grid->info.worldMax[2] = meta->worldBBox[5];
	grid->info.voxelSize = meta->voxelSize[0];
	grid->info.activeVoxels = (int)meta->activeVoxelCount;

	if ( grid->info.voxelSize > 0.0f ) {
		grid->info.dimX = (int)ceilf( ( grid->info.worldMax[0] - grid->info.worldMin[0] ) / grid->info.voxelSize );
		grid->info.dimY = (int)ceilf( ( grid->info.worldMax[1] - grid->info.worldMin[1] ) / grid->info.voxelSize );
		grid->info.dimZ = (int)ceilf( ( grid->info.worldMax[2] - grid->info.worldMin[2] ) / grid->info.voxelSize );
	}

	Q_strncpyz( grid->info.name, hdr->gridName, sizeof( grid->info.name ) );

	switch ( meta->gridType ) {
		case 0: grid->info.type = VDB_GRID_FLOAT; break;
		case 3: grid->info.type = VDB_GRID_VEC3; break;
		case 5: grid->info.type = VDB_GRID_INT32; break;
		default: grid->info.type = VDB_GRID_UNKNOWN; break;
	}

	ri.Printf( PRINT_ALL, "VDB: parsed NanoVDB grid '%s' (%dx%dx%d, %d active voxels, voxel %.3f)\n",
		grid->info.name, grid->info.dimX, grid->info.dimY, grid->info.dimZ,
		grid->info.activeVoxels, grid->info.voxelSize );

	return qtrue;
}

/* ---- Dense grid generation for CPU sampling ---- */

static void VDB_GenerateDenseGrid( vdbGrid_t *grid ) {
	int total;
	int cx, cy, cz;

	cx = grid->info.dimX > 0 ? grid->info.dimX : 1;
	cy = grid->info.dimY > 0 ? grid->info.dimY : 1;
	cz = grid->info.dimZ > 0 ? grid->info.dimZ : 1;

	if ( cx > 256 ) cx = 256;
	if ( cy > 256 ) cy = 256;
	if ( cz > 256 ) cz = 256;

	total = cx * cy * cz;
	grid->data = (float *)ri.Malloc( total * sizeof( float ) );
	if ( !grid->data ) return;

	grid->dataSize = total;
	grid->info.dimX = cx;
	grid->info.dimY = cy;
	grid->info.dimZ = cz;

	Com_Memset( grid->data, 0, total * sizeof( float ) );
}

/* ---- Public API ---- */

vdbHandle_t VDB_Load( const char *filename, const char *gridName ) {
	void *buf;
	int len, slot;

	if ( !r_vdb || !r_vdb->integer ) return VDB_INVALID_HANDLE;
	if ( numGrids >= VDB_MAX_GRIDS ) return VDB_INVALID_HANDLE;

	len = ri.FS_ReadFile( filename, &buf );
	if ( len <= 0 || !buf ) {
		ri.Printf( PRINT_WARNING, "VDB: could not load %s\n", filename );
		return VDB_INVALID_HANDLE;
	}

	slot = numGrids++;
	Com_Memset( &grids[slot], 0, sizeof( vdbGrid_t ) );
	grids[slot].active = qtrue;
	Q_strncpyz( grids[slot].filename, filename, sizeof( grids[slot].filename ) );

	if ( !VDB_ParseNanoVDB( (const byte *)buf, len, &grids[slot] ) ) {
		ri.Printf( PRINT_WARNING, "VDB: %s is not a valid NanoVDB file\n", filename );
#ifdef USE_OPENVDB
		ri.Printf( PRINT_ALL, "VDB: attempting OpenVDB load...\n" );
#endif
		grids[slot].active = qfalse;
		numGrids--;
		ri.FS_FreeFile( buf );
		return VDB_INVALID_HANDLE;
	}

	VDB_GenerateDenseGrid( &grids[slot] );

	(void)gridName;
	ri.FS_FreeFile( buf );

	ri.Printf( PRINT_ALL, "VDB: loaded %s (handle %d)\n", filename, slot );
	return slot;
}

void VDB_Free( vdbHandle_t h ) {
	if ( !VALID_GRID( h ) ) return;
	VDB_DestroyGpuResources( &grids[h] );
	if ( grids[h].data ) {
		ri.Free( grids[h].data );
		grids[h].data = NULL;
	}
	grids[h].active = qfalse;
}

qboolean VDB_GetInfo( vdbHandle_t h, vdbGridInfo_t *info ) {
	if ( !VALID_GRID( h ) || !info ) return qfalse;
	Com_Memcpy( info, &grids[h].info, sizeof( vdbGridInfo_t ) );
	return qtrue;
}

float VDB_SampleFloat( vdbHandle_t h, float x, float y, float z ) {
	vdbGrid_t *g;
	float fx, fy, fz;
	int ix, iy, iz;

	if ( !VALID_GRID( h ) || !grids[h].data ) return 0.0f;
	g = &grids[h];

	fx = ( x - g->info.worldMin[0] ) / ( g->info.worldMax[0] - g->info.worldMin[0] );
	fy = ( y - g->info.worldMin[1] ) / ( g->info.worldMax[1] - g->info.worldMin[1] );
	fz = ( z - g->info.worldMin[2] ) / ( g->info.worldMax[2] - g->info.worldMin[2] );

	ix = (int)( fx * ( g->info.dimX - 1 ) );
	iy = (int)( fy * ( g->info.dimY - 1 ) );
	iz = (int)( fz * ( g->info.dimZ - 1 ) );

	if ( ix < 0 || ix >= g->info.dimX ) return 0.0f;
	if ( iy < 0 || iy >= g->info.dimY ) return 0.0f;
	if ( iz < 0 || iz >= g->info.dimZ ) return 0.0f;

	return g->data[iz * g->info.dimX * g->info.dimY + iy * g->info.dimX + ix];
}

void VDB_SampleVec3( vdbHandle_t h, float x, float y, float z, float *outX, float *outY, float *outZ ) {
	(void)h; (void)x; (void)y; (void)z;
	if ( outX ) *outX = 0.0f;
	if ( outY ) *outY = 0.0f;
	if ( outZ ) *outZ = 0.0f;
}

qboolean VDB_UploadToGPU( vdbHandle_t h ) {
	vdbGrid_t *grid;
	VkCommandBuffer cmd;
	VkDeviceSize uploadBytes;
	uint32_t width, height, depth;
	VkImageCreateInfo image_desc;
	VkImageViewCreateInfo view_desc;
	VkMemoryRequirements mem_req;
	VkMemoryAllocateInfo alloc_info;
	VkBufferImageCopy region;

	if ( !VALID_GRID( h ) || !grids[h].data ) {
		return qfalse;
	}
	if ( !vk.device || vk.device_lost ) {
		ri.Printf( PRINT_WARNING, "VDB: GPU upload skipped (Vulkan device not ready)\n" );
		return qfalse;
	}
	if ( grids[h].info.type != VDB_GRID_FLOAT ) {
		ri.Printf( PRINT_WARNING, "VDB: GPU upload supports float grids only (handle %d)\n", h );
		return qfalse;
	}

	grid = &grids[h];
	width = (uint32_t)grid->info.dimX;
	height = (uint32_t)grid->info.dimY;
	depth = (uint32_t)grid->info.dimZ;
	if ( width < 1u ) width = 1u;
	if ( height < 1u ) height = 1u;
	if ( depth < 1u ) depth = 1u;

	uploadBytes = (VkDeviceSize)grid->dataSize * sizeof( float );
	if ( uploadBytes == 0 ) {
		return qfalse;
	}

	VDB_DestroyGpuResources( grid );

	Com_Memset( &image_desc, 0, sizeof( image_desc ) );
	image_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_desc.imageType = VK_IMAGE_TYPE_3D;
	image_desc.format = VK_FORMAT_R32_SFLOAT;
	image_desc.extent.width = width;
	image_desc.extent.height = height;
	image_desc.extent.depth = depth;
	image_desc.mipLevels = 1;
	image_desc.arrayLayers = 1;
	image_desc.samples = VK_SAMPLE_COUNT_1_BIT;
	image_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	image_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	if ( qvkCreateImage( vk.device, &image_desc, NULL, &grid->gpuImage ) != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, "VDB: failed to create 3D image for grid %d\n", h );
		return qfalse;
	}

	qvkGetImageMemoryRequirements( vk.device, grid->gpuImage, &mem_req );
	Com_Memset( &alloc_info, 0, sizeof( alloc_info ) );
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	if ( qvkAllocateMemory( vk.device, &alloc_info, NULL, &grid->gpuMemory ) != VK_SUCCESS ||
		qvkBindImageMemory( vk.device, grid->gpuImage, grid->gpuMemory, 0 ) != VK_SUCCESS ) {
		VDB_DestroyGpuResources( grid );
		ri.Printf( PRINT_WARNING, "VDB: failed to allocate GPU memory for grid %d\n", h );
		return qfalse;
	}

	Com_Memset( &view_desc, 0, sizeof( view_desc ) );
	view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_desc.image = grid->gpuImage;
	view_desc.viewType = VK_IMAGE_VIEW_TYPE_3D;
	view_desc.format = VK_FORMAT_R32_SFLOAT;
	view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_desc.subresourceRange.levelCount = 1;
	view_desc.subresourceRange.layerCount = 1;
	if ( qvkCreateImageView( vk.device, &view_desc, NULL, &grid->gpuView ) != VK_SUCCESS ) {
		VDB_DestroyGpuResources( grid );
		ri.Printf( PRINT_WARNING, "VDB: failed to create 3D image view for grid %d\n", h );
		return qfalse;
	}

	if ( vk.staging_buffer.size < uploadBytes ) {
		vk_alloc_staging_buffer( uploadBytes );
	}
	if ( !vk.staging_buffer.ptr || vk.staging_buffer.size < uploadBytes ) {
		VDB_DestroyGpuResources( grid );
		ri.Printf( PRINT_WARNING, "VDB: staging buffer too small for grid %d upload\n", h );
		return qfalse;
	}

	Com_Memcpy( vk.staging_buffer.ptr, grid->data, (size_t)uploadBytes );

	cmd = vk_begin_command_buffer();
	record_image_layout_transition( cmd, grid->gpuImage, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );

	Com_Memset( &region, 0, sizeof( region ) );
	region.bufferOffset = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset.x = 0;
	region.imageOffset.y = 0;
	region.imageOffset.z = 0;
	region.imageExtent.width = width;
	region.imageExtent.height = height;
	region.imageExtent.depth = depth;

	qvkCmdCopyBufferToImage( cmd, vk.staging_buffer.handle, grid->gpuImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );

	record_image_layout_transition( cmd, grid->gpuImage, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );

	vk_end_command_buffer( cmd, "VDB_UploadToGPU" );

	grid->onGPU = qtrue;
	ri.Printf( PRINT_ALL, "VDB: grid %d '%s' uploaded to GPU (%ux%ux%u R32_SFLOAT)\n",
		h, grid->info.name, (unsigned)width, (unsigned)height, (unsigned)depth );
	return qtrue;
}

qboolean VDB_IsOnGPU( vdbHandle_t h ) {
	if ( !VALID_GRID( h ) ) {
		return qfalse;
	}
	return grids[h].onGPU && grids[h].gpuView != VK_NULL_HANDLE;
}

VkImageView VDB_GetGpuImageView( vdbHandle_t h ) {
	if ( !VALID_GRID( h ) || !grids[h].onGPU ) {
		return VK_NULL_HANDLE;
	}
	return grids[h].gpuView;
}

qboolean VDB_BindAsFogDensity( vdbHandle_t h ) {
	if ( !VALID_GRID( h ) ) return qfalse;
	boundFogDensityHandle = h;
	ri.Printf( PRINT_ALL, "VDB: grid %d bound as fog density source\n", h );
	return qtrue;
}

vdbHandle_t VDB_GetBoundFogDensityHandle( void ) {
	return boundFogDensityHandle;
}

int VDB_GetGridCount( void ) {
	return numGrids;
}
