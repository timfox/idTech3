/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

OpenVDB / NanoVDB integration implementation.

Loading pipeline:
  1. FS_ReadFile loads the .vdb/.nvdb from game filesystem
  2. For NanoVDB: parse the flat buffer header directly (no dependencies)
  3. NanoVDB is the supported on-disk format
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
#include "vk_nanovdb_decode.h"
#include "vk.h"
#include "vk_cmd.h"
#include <string.h>
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
	float       *majorantData;
	int         majorantDimX;
	int         majorantDimY;
	int         majorantDimZ;
	qboolean    majorantOnGPU;
	VkImage     gpuMajorantImage;
	VkDeviceMemory gpuMajorantMemory;
	VkImageView gpuMajorantView;
} vdbGrid_t;

static vdbGrid_t grids[VDB_MAX_GRIDS];
static int numGrids = 0;
static vdbHandle_t boundFogDensityHandle = VDB_INVALID_HANDLE;
static cvar_t *r_vdb;
static cvar_t *r_vdbMajorantBrick;
static qboolean vdb_console_cmds_registered = qfalse;

#define VALID_GRID(h) ((h) >= 0 && (h) < numGrids && grids[(h)].active)

static void VDB_Cmd_RebuildMajorant_f( void );

static qboolean VDB_RebuildMajorantForGrid( vdbGrid_t *grid, qboolean logResult );

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
	ri.Printf( PRINT_ALL, "VDB: handle %d on GPU\n", h );
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
	ri.Cmd_AddCommand( "vdb_rebuild_majorant", VDB_Cmd_RebuildMajorant_f );
	vdb_console_cmds_registered = qtrue;
	ri.Printf( PRINT_DEVELOPER, "VDB: console commands vdb_load, vdb_upload, vdb_bind_fog, vdb_list, vdb_rebuild_majorant\n" );
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
	ri.Cmd_RemoveCommand( "vdb_rebuild_majorant" );
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

	if ( grid->gpuMajorantView != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, grid->gpuMajorantView, NULL );
		grid->gpuMajorantView = VK_NULL_HANDLE;
	}
	if ( grid->gpuMajorantImage != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, grid->gpuMajorantImage, NULL );
		grid->gpuMajorantImage = VK_NULL_HANDLE;
	}
	if ( grid->gpuMajorantMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, grid->gpuMajorantMemory, NULL );
		grid->gpuMajorantMemory = VK_NULL_HANDLE;
	}
	grid->majorantOnGPU = qfalse;
}

static void VDB_FreeMajorantCpu( vdbGrid_t *grid )
{
	if ( !grid ) {
		return;
	}
	if ( grid->majorantData ) {
		ri.Free( grid->majorantData );
		grid->majorantData = NULL;
	}
	grid->majorantDimX = 0;
	grid->majorantDimY = 0;
	grid->majorantDimZ = 0;
}

/*
===============
VDB_BuildMajorantGrid
OpenVDB-style macrocell majorants (arXiv:2211.09997 §5.1.3): per-brick max density
for Woodcock/delta-tracking segment bounds.
===============
*/
static qboolean VDB_BuildMajorantGrid( vdbGrid_t *grid )
{
	int brick;
	int mx, my, mz;
	int bx, by, bz;
	int ix, iy, iz;
	float maxVal;

	if ( !grid || !grid->data ) {
		return qfalse;
	}

	brick = r_vdbMajorantBrick ? (int)r_vdbMajorantBrick->value : 8;
	if ( brick < 2 ) {
		brick = 2;
	}
	if ( brick > 32 ) {
		brick = 32;
	}

	mx = ( grid->info.dimX + brick - 1 ) / brick;
	my = ( grid->info.dimY + brick - 1 ) / brick;
	mz = ( grid->info.dimZ + brick - 1 ) / brick;
	if ( mx < 1 ) {
		mx = 1;
	}
	if ( my < 1 ) {
		my = 1;
	}
	if ( mz < 1 ) {
		mz = 1;
	}
	if ( mx > 64 ) {
		mx = 64;
	}
	if ( my > 64 ) {
		my = 64;
	}
	if ( mz > 64 ) {
		mz = 64;
	}

	VDB_FreeMajorantCpu( grid );
	grid->majorantData = (float *)ri.Malloc( mx * my * mz * sizeof( float ) );
	if ( !grid->majorantData ) {
		return qfalse;
	}
	Com_Memset( grid->majorantData, 0, mx * my * mz * sizeof( float ) );
	grid->majorantDimX = mx;
	grid->majorantDimY = my;
	grid->majorantDimZ = mz;

	for ( bz = 0; bz < mz; bz++ ) {
		for ( by = 0; by < my; by++ ) {
			for ( bx = 0; bx < mx; bx++ ) {
				maxVal = 0.0f;
				for ( iz = bz * brick; iz < ( bz + 1 ) * brick && iz < grid->info.dimZ; iz++ ) {
					for ( iy = by * brick; iy < ( by + 1 ) * brick && iy < grid->info.dimY; iy++ ) {
						for ( ix = bx * brick; ix < ( bx + 1 ) * brick && ix < grid->info.dimX; ix++ ) {
							const float v = grid->data[iz * grid->info.dimX * grid->info.dimY + iy * grid->info.dimX + ix];
							if ( v > maxVal ) {
								maxVal = v;
							}
						}
					}
				}
				grid->majorantData[bz * mx * my + by * mx + bx] = maxVal;
			}
		}
	}

	ri.Printf( PRINT_ALL, "VDB: majorant grid %dx%dx%d (brick %d) for '%s'\n",
		mx, my, mz, brick, grid->info.name );
	return qtrue;
}

static qboolean VDB_Upload3DTexture( const float *src, int dimX, int dimY, int dimZ,
	VkImage *outImage, VkDeviceMemory *outMemory, VkImageView *outView, const char *debugName )
{
	VkCommandBuffer cmd;
	VkDeviceSize uploadBytes;
	uint32_t width, height, depth;
	VkImageCreateInfo image_desc;
	VkImageViewCreateInfo view_desc;
	VkMemoryRequirements mem_req;
	VkMemoryAllocateInfo alloc_info;
	VkBufferImageCopy region;

	if ( !src || dimX < 1 || dimY < 1 || dimZ < 1 || !vk.device || vk.device_lost ) {
		return qfalse;
	}

	width = (uint32_t)dimX;
	height = (uint32_t)dimY;
	depth = (uint32_t)dimZ;
	uploadBytes = (VkDeviceSize)( dimX * dimY * dimZ ) * sizeof( float );

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

	if ( qvkCreateImage( vk.device, &image_desc, NULL, outImage ) != VK_SUCCESS ) {
		return qfalse;
	}

	qvkGetImageMemoryRequirements( vk.device, *outImage, &mem_req );
	Com_Memset( &alloc_info, 0, sizeof( alloc_info ) );
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( vk.physical_device, mem_req.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	if ( qvkAllocateMemory( vk.device, &alloc_info, NULL, outMemory ) != VK_SUCCESS ||
		qvkBindImageMemory( vk.device, *outImage, *outMemory, 0 ) != VK_SUCCESS ) {
		qvkDestroyImage( vk.device, *outImage, NULL );
		*outImage = VK_NULL_HANDLE;
		return qfalse;
	}

	Com_Memset( &view_desc, 0, sizeof( view_desc ) );
	view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_desc.image = *outImage;
	view_desc.viewType = VK_IMAGE_VIEW_TYPE_3D;
	view_desc.format = VK_FORMAT_R32_SFLOAT;
	view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_desc.subresourceRange.levelCount = 1;
	view_desc.subresourceRange.layerCount = 1;
	if ( qvkCreateImageView( vk.device, &view_desc, NULL, outView ) != VK_SUCCESS ) {
		qvkDestroyImage( vk.device, *outImage, NULL );
		qvkFreeMemory( vk.device, *outMemory, NULL );
		*outImage = VK_NULL_HANDLE;
		*outMemory = VK_NULL_HANDLE;
		return qfalse;
	}

	if ( vk.staging_buffer.size < uploadBytes ) {
		vk_alloc_staging_buffer( uploadBytes );
	}
	if ( !vk.staging_buffer.ptr || vk.staging_buffer.size < uploadBytes ) {
		return qfalse;
	}

	Com_Memcpy( vk.staging_buffer.ptr, src, (size_t)uploadBytes );
	cmd = vk_begin_command_buffer();
	record_image_layout_transition( cmd, *outImage, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );

	Com_Memset( &region, 0, sizeof( region ) );
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageExtent.width = width;
	region.imageExtent.height = height;
	region.imageExtent.depth = depth;

	qvkCmdCopyBufferToImage( cmd, vk.staging_buffer.handle, *outImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );

	record_image_layout_transition( cmd, *outImage, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_SHADER_STAGE_COMPUTE_BIT );

	vk_end_command_buffer( cmd, debugName );
	SET_OBJECT_NAME( *outImage, debugName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
	return qtrue;
}

void VDB_Init( void ) {
	Com_Memset( grids, 0, sizeof( grids ) );
	numGrids = 0;
	r_vdb = ri.Cvar_Get( "r_vdb", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_vdb, "Enable OpenVDB/NanoVDB volumetric data loading (0 = off, 1 = on)." );
	r_vdbMajorantBrick = ri.Cvar_Get( "r_vdbMajorantBrick", "8", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vdbMajorantBrick, "2", "32", CV_INTEGER );
	ri.Cvar_SetDescription( r_vdbMajorantBrick,
		"Brick size for OpenVDB majorant macrocells (delta-tracking segment bounds; arXiv:2211.09997)." );

	ri.Printf( PRINT_ALL, "VDB: initialized (NanoVDB)\n" );
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
		VDB_FreeMajorantCpu( &grids[i] );
		grids[i].active = qfalse;
	}
	numGrids = 0;
}

/* ---- NanoVDB header + dense decode ---- */

#define NVDB_OFF_GRID_NAME   40
#define NVDB_OFF_WORLD_BBOX  560
#define NVDB_OFF_VOXEL_SIZE  608
#define NVDB_OFF_GRID_TYPE   636
#define NVDB_TREE_VOXEL_CNT  56

static uint32_t nvdb_read_u32( const byte *p ) {
	uint32_t v;
	memcpy( &v, p, sizeof( v ) );
	return v;
}

static double nvdb_read_f64( const byte *p ) {
	double v;
	memcpy( &v, p, sizeof( v ) );
	return v;
}

static qboolean VDB_ReadGridInfo( const byte *grid, vdbGrid_t *out ) {
	const byte *tree;
	uint32_t gridType;
	double vx, vy, vz;

	if ( !grid || !out ) {
		return qfalse;
	}

	tree = grid + NANOVDB_GRIDDATA_BYTES;
	Q_strncpyz( out->info.name, (const char *)( grid + NVDB_OFF_GRID_NAME ), sizeof( out->info.name ) );

	out->info.worldMin[0] = (float)nvdb_read_f64( grid + NVDB_OFF_WORLD_BBOX + 0 );
	out->info.worldMin[1] = (float)nvdb_read_f64( grid + NVDB_OFF_WORLD_BBOX + 8 );
	out->info.worldMin[2] = (float)nvdb_read_f64( grid + NVDB_OFF_WORLD_BBOX + 16 );
	out->info.worldMax[0] = (float)nvdb_read_f64( grid + NVDB_OFF_WORLD_BBOX + 24 );
	out->info.worldMax[1] = (float)nvdb_read_f64( grid + NVDB_OFF_WORLD_BBOX + 32 );
	out->info.worldMax[2] = (float)nvdb_read_f64( grid + NVDB_OFF_WORLD_BBOX + 40 );

	vx = nvdb_read_f64( grid + NVDB_OFF_VOXEL_SIZE + 0 );
	vy = nvdb_read_f64( grid + NVDB_OFF_VOXEL_SIZE + 8 );
	vz = nvdb_read_f64( grid + NVDB_OFF_VOXEL_SIZE + 16 );
	out->info.voxelSize = (float)vx;
	(void)vy;
	(void)vz;

	{
		uint64_t vox;
		memcpy( &vox, tree + NVDB_TREE_VOXEL_CNT, sizeof( vox ) );
		out->info.activeVoxels = (int)vox;
	}

	gridType = nvdb_read_u32( grid + NVDB_OFF_GRID_TYPE );
	switch ( gridType ) {
	case 1:
	case 2:
	case 9:
		out->info.type = VDB_GRID_FLOAT;
		break;
	case 6:
		out->info.type = VDB_GRID_VEC3;
		break;
	case 4:
		out->info.type = VDB_GRID_INT32;
		break;
	default:
		out->info.type = VDB_GRID_UNKNOWN;
		break;
	}

	return qtrue;
}

static qboolean VDB_AllocAndDecodeNanoVDB( const byte *buf, int bufLen, const char *gridName, vdbGrid_t *grid ) {
	vdbNanoIndexBBox_t idx;
	int total;
	float maxVal;
	int i;

	if ( !VDB_NanoVDB_GetIndexDims( buf, bufLen, gridName, &idx ) ) {
		return qfalse;
	}

	total = idx.dimX * idx.dimY * idx.dimZ;
	if ( total <= 0 || total > 256 * 256 * 256 ) {
		return qfalse;
	}

	grid->data = (float *)ri.Malloc( total * sizeof( float ) );
	if ( !grid->data ) {
		return qfalse;
	}
	grid->dataSize = total;
	grid->info.dimX = idx.dimX;
	grid->info.dimY = idx.dimY;
	grid->info.dimZ = idx.dimZ;
	Com_Memset( grid->data, 0, (size_t)total * sizeof( float ) );

	if ( !VDB_NanoVDB_DecodeToDense( buf, bufLen, gridName, grid->data, total, &idx ) ) {
		ri.Free( grid->data );
		grid->data = NULL;
		grid->dataSize = 0;
		return qfalse;
	}

	maxVal = 0.0f;
	for ( i = 0; i < total; i++ ) {
		if ( grid->data[i] > maxVal ) {
			maxVal = grid->data[i];
		}
	}

	ri.Printf( PRINT_ALL, "VDB: decoded NanoVDB '%s' %dx%dx%d (%d voxels, peak %.4f)\n",
		grid->info.name, grid->info.dimX, grid->info.dimY, grid->info.dimZ,
		grid->info.activeVoxels, maxVal );
	return qtrue;
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

	{
		const byte *fileBuf = (const byte *)buf;
		const byte *gridPtr = NULL;

		if ( !VDB_NanoVDB_ResolveGrid( fileBuf, len, gridName, &gridPtr ) ) {
			ri.Printf( PRINT_WARNING, "VDB: %s is not a valid NanoVDB file (or unsupported grid type)\n", filename );
			grids[slot].active = qfalse;
			numGrids--;
			ri.FS_FreeFile( buf );
			return VDB_INVALID_HANDLE;
		}

		if ( !VDB_ReadGridInfo( gridPtr, &grids[slot] ) ) {
			ri.Printf( PRINT_WARNING, "VDB: failed to read grid metadata from %s\n", filename );
			grids[slot].active = qfalse;
			numGrids--;
			ri.FS_FreeFile( buf );
			return VDB_INVALID_HANDLE;
		}

		if ( !VDB_AllocAndDecodeNanoVDB( fileBuf, len, gridName, &grids[slot] ) ) {
			ri.Printf( PRINT_WARNING, "VDB: failed to decode voxel data from %s\n", filename );
			grids[slot].active = qfalse;
			numGrids--;
			ri.FS_FreeFile( buf );
			return VDB_INVALID_HANDLE;
		}
	}

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
	VDB_FreeMajorantCpu( &grids[h] );
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

static qboolean VDB_RebuildMajorantForGrid( vdbGrid_t *grid, qboolean logResult )
{
	if ( !grid || !grid->data ) {
		return qfalse;
	}

	if ( grid->gpuMajorantView != VK_NULL_HANDLE ) {
		qvkDestroyImageView( vk.device, grid->gpuMajorantView, NULL );
		grid->gpuMajorantView = VK_NULL_HANDLE;
	}
	if ( grid->gpuMajorantImage != VK_NULL_HANDLE ) {
		qvkDestroyImage( vk.device, grid->gpuMajorantImage, NULL );
		grid->gpuMajorantImage = VK_NULL_HANDLE;
	}
	if ( grid->gpuMajorantMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, grid->gpuMajorantMemory, NULL );
		grid->gpuMajorantMemory = VK_NULL_HANDLE;
	}
	grid->majorantOnGPU = qfalse;

	if ( !VDB_BuildMajorantGrid( grid ) ) {
		if ( logResult ) {
			ri.Printf( PRINT_WARNING, "VDB: majorant rebuild failed for '%s'\n", grid->info.name );
		}
		return qfalse;
	}

	if ( grid->onGPU && vk.device && !vk.device_lost && grid->majorantData ) {
		if ( VDB_Upload3DTexture( grid->majorantData, grid->majorantDimX, grid->majorantDimY, grid->majorantDimZ,
			&grid->gpuMajorantImage, &grid->gpuMajorantMemory, &grid->gpuMajorantView, "VDB majorant 3D" ) ) {
			grid->majorantOnGPU = qtrue;
		} else {
			if ( logResult ) {
				ri.Printf( PRINT_WARNING, "VDB: majorant GPU re-upload failed for '%s'\n", grid->info.name );
			}
			return qfalse;
		}
	}

	if ( logResult ) {
		ri.Printf( PRINT_ALL, "VDB: majorant rebuilt for '%s' (%dx%dx%d bricks)\n",
			grid->info.name, grid->majorantDimX, grid->majorantDimY, grid->majorantDimZ );
	}
	return qtrue;
}

void VDB_FrameUpdate( void )
{
	int i;
	qboolean anyOnGpu = qfalse;
	qboolean refreshed = qfalse;

	if ( !r_vdbMajorantBrick || !r_vdbMajorantBrick->modified ) {
		return;
	}
	r_vdbMajorantBrick->modified = qfalse;

	for ( i = 0; i < numGrids; i++ ) {
		if ( !grids[i].active || !grids[i].data ) {
			continue;
		}
		if ( grids[i].onGPU ) {
			anyOnGpu = qtrue;
		}
		if ( VDB_RebuildMajorantForGrid( &grids[i], qfalse ) ) {
			refreshed = qtrue;
		}
	}

	if ( refreshed && anyOnGpu && vk.device && !vk.device_lost ) {
		vk_update_volumetric_descriptors();
	}
	ri.Printf( PRINT_DEVELOPER, "VDB: r_vdbMajorantBrick changed; majorant bricks refreshed for loaded grids\n" );
}

qboolean VDB_UploadToGPU( vdbHandle_t h ) {
	vdbGrid_t *grid;

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
	if ( grid->dataSize <= 0 ) {
		return qfalse;
	}

	VDB_DestroyGpuResources( grid );
	(void)VDB_BuildMajorantGrid( grid );

	if ( !VDB_Upload3DTexture( grid->data, grid->info.dimX, grid->info.dimY, grid->info.dimZ,
		&grid->gpuImage, &grid->gpuMemory, &grid->gpuView, "VDB density 3D" ) ) {
		VDB_DestroyGpuResources( grid );
		ri.Printf( PRINT_WARNING, "VDB: density GPU upload failed for grid %d\n", h );
		return qfalse;
	}
	grid->onGPU = qtrue;

	if ( grid->majorantData ) {
		if ( VDB_Upload3DTexture( grid->majorantData, grid->majorantDimX, grid->majorantDimY, grid->majorantDimZ,
			&grid->gpuMajorantImage, &grid->gpuMajorantMemory, &grid->gpuMajorantView, "VDB majorant 3D" ) ) {
			grid->majorantOnGPU = qtrue;
		} else {
			ri.Printf( PRINT_WARNING, "VDB: majorant GPU upload failed for grid %d (density-only path)\n", h );
		}
	}

	ri.Printf( PRINT_ALL, "VDB: grid %d '%s' uploaded to GPU (%dx%dx%d density, majorant %s)\n",
		h, grid->info.name, grid->info.dimX, grid->info.dimY, grid->info.dimZ,
		grid->majorantOnGPU ? "yes" : "no" );
	if ( vk.device && !vk.device_lost ) {
		vk_update_volumetric_descriptors();
	}
	return qtrue;
}

static void VDB_Cmd_RebuildMajorant_f( void )
{
	vdbHandle_t h;

	if ( ri.Cmd_Argc() < 2 ) {
		ri.Printf( PRINT_ALL, "Usage: vdb_rebuild_majorant <handle>\n" );
		return;
	}
	h = VDB_ParseHandleArg( ri.Cmd_Argv( 1 ) );
	if ( !VALID_GRID( h ) || !grids[h].data ) {
		ri.Printf( PRINT_WARNING, "VDB: invalid handle or no CPU density\n" );
		return;
	}

	if ( VDB_RebuildMajorantForGrid( &grids[h], qtrue ) && grids[h].onGPU && vk.device && !vk.device_lost ) {
		vk_update_volumetric_descriptors();
	}
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

qboolean VDB_HasMajorantOnGPU( vdbHandle_t h ) {
	if ( !VALID_GRID( h ) ) {
		return qfalse;
	}
	return grids[h].majorantOnGPU && grids[h].gpuMajorantView != VK_NULL_HANDLE;
}

VkImageView VDB_GetGpuMajorantView( vdbHandle_t h ) {
	if ( !VDB_HasMajorantOnGPU( h ) ) {
		return VK_NULL_HANDLE;
	}
	return grids[h].gpuMajorantView;
}

qboolean VDB_BindAsFogDensity( vdbHandle_t h ) {
	if ( !VALID_GRID( h ) ) {
		return qfalse;
	}
	boundFogDensityHandle = h;
	ri.Printf( PRINT_ALL, "VDB: grid %d bound as fog density source\n", h );
	if ( grids[h].onGPU && vk.device && !vk.device_lost ) {
		vk_update_volumetric_descriptors();
	}
	return qtrue;
}

vdbHandle_t VDB_GetBoundFogDensityHandle( void ) {
	return boundFogDensityHandle;
}

int VDB_GetGridCount( void ) {
	return numGrids;
}
