/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vulkan Forward+ scaffolding: GPU light SSBO, tile index SSBO, compute tile
cull, optional PBR fragment debug overlay (r_forwardPlusDebug). See
docs/RENDERER_2026_ARCHITECTURE_PASS.md.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_forward_plus.h"
#include "vk_cluster_contract.h"
#include "vk_image_layout.h"
#include "vk_util.h"
#include "vk_view_state.h"
#include "vk_reactive_mask.h"
#include "vk_object_id.h"
#include "vk_pass_registry.h"
#include "vk_ltc.h"
#include "vk_scene_platform.h"
#include "vk_photometric.h"
#include "vk_hiz.h"
#include "vk_vshadow.h"

#define VK_FP_RECORD_STRIDE (sizeof(float) * 16) /* 4 x vec4 per light */
#define VK_FP_HEADER_BYTES (sizeof(float) * 8) /* 2 x vec4: count/meta + tile grid / viewport */
#define VK_FP_TILE_DIM 16u
/* Max flat 2D tiles before Z expansion; total clusters = flat * z_slices (capped). */
#define VK_FP_MAX_TILES (256u * 256u)
#define VK_FP_MAX_Z_SLICES 16u
#define VK_FP_MAX_CLUSTERS (VK_FP_MAX_TILES * 8u)
/* Legacy fixed-slot stride; compact uses header+index pool (see CLUSTER_LIST_META_UINTS). */
#define VK_FP_MIN_PER_TILE 4u
#define VK_FP_MAX_PER_TILE 8u
#define VK_FP_MAX_COMPACT_PER_CLUSTER 32u
#define VK_FP_CLUSTER_LIST_META_UINTS 4u
#define VK_FP_DEFAULT_INDEX_CAP (256u * 1024u)
#define VK_FP_MAX_INDEX_CAP (16u * 1024u * 1024u)
#define VK_FP_PARAM_BYTES 256u
#define VK_FP_DUMMY_LIGHT_FLOATS 32u
#define VK_FP_DUMMY_TILE_UINTS 32u

/* M2 cvars — registered in vk_cluster_register_cvars(). */
cvar_t *r_clusterCompactLists;
cvar_t *r_clusterZFar;
cvar_t *r_clusterMaxIndices;
cvar_t *r_clusterMaxLightsPerCluster;
cvar_t *r_clusterOverflowPolicy;
cvar_t *r_clusterForceBuildFailure;
cvar_t *r_clusterForceOverflow;
cvar_t *r_clusterForceStaleGeneration;
cvar_t *r_clusterInspect;
static cvar_t *r_clusterTransparentPrepass;

typedef struct {
	uint32_t frameGeneration;
	uint32_t submissions;
	uint32_t wboitSubmissions;
	uint32_t forwardSubmissions;
	uint32_t candidates;
	uint32_t accepted;
	uint32_t additiveCandidates;
	uint32_t additiveAccepted;
} vkClusterTransparentFrame_t;
static vkClusterTransparentFrame_t s_transparentFrame;
static int s_transparentFrameNumber = -1;
extern cvar_t *r_renderMode;

static uint32_t vk_fp_active_z_slices( void )
{
	uint32_t z = 1u;

	if ( r_forwardPlusZSlices && r_forwardPlusZSlices->integer > 1 ) {
		z = (uint32_t)r_forwardPlusZSlices->integer;
	}
	if ( z > VK_FP_MAX_Z_SLICES ) {
		z = VK_FP_MAX_Z_SLICES;
	}
	if ( z < 1u ) {
		z = 1u;
	}
	return z;
}

void vk_cluster_transparent_begin_frame( void )
{
	/* RB_DrawSurfs may run more than once for portals/stereo. Keep one
	 * ownership ledger for the whole renderer frame instead of losing the
	 * first view's transparent submissions. */
	if ( s_transparentFrameNumber == tr.frameCount ) {
		return;
	}
	Com_Memset( &s_transparentFrame, 0, sizeof( s_transparentFrame ) );
	s_transparentFrameNumber = tr.frameCount;
	s_transparentFrame.frameGeneration = vk.forward_plus.cluster_list_generation;
}

void vk_cluster_transparent_note_submission( const char *owner )
{
	s_transparentFrame.submissions++;
	if ( owner && !Q_stricmp( owner, "wboit" ) ) {
		s_transparentFrame.wboitSubmissions++;
	} else {
		s_transparentFrame.forwardSubmissions++;
	}
}

void vk_cluster_transparent_note_candidate( qboolean additive )
{
	s_transparentFrame.candidates++;
	if ( additive ) {
		s_transparentFrame.additiveCandidates++;
	}
}

void vk_cluster_transparent_note_accepted( qboolean additive )
{
	s_transparentFrame.accepted++;
	if ( additive ) {
		s_transparentFrame.additiveAccepted++;
	}
}

void vk_cluster_transparent_print_status( void )
{
	ri.Printf( PRINT_ALL,
		"  transparentSubmission: frameGen=%u submissions=%u forward=%u wboit=%u "
		"candidates=%u accepted=%u additive=%u/%u activeMask=%s\n",
		s_transparentFrame.frameGeneration,
		s_transparentFrame.submissions,
		s_transparentFrame.forwardSubmissions,
		s_transparentFrame.wboitSubmissions,
		s_transparentFrame.candidates,
		s_transparentFrame.accepted,
		s_transparentFrame.additiveCandidates,
		s_transparentFrame.additiveAccepted,
		r_clusterTransparentPrepass && r_clusterTransparentPrepass->integer
			? "requested_not_wired" : "conservative_shared_grid" );
}

static qboolean vk_fp_want_compact_lists( void )
{
	int mode;

	if ( vk.forward_plus.fallback_legacy ) {
		return qfalse;
	}
	if ( r_clusterForceBuildFailure && r_clusterForceBuildFailure->integer ) {
		return qfalse;
	}
	if ( r_clusterCompactLists && r_clusterCompactLists->integer == 0 ) {
		return qfalse;
	}
	/* Default: compact on for clustered modes (renderMode 3 or zSlices>1); mode 2 may stay legacy. */
	if ( r_clusterCompactLists && r_clusterCompactLists->integer > 0 ) {
		return qtrue;
	}
	mode = r_renderMode ? r_renderMode->integer : 2;
	if ( mode == 3 || ( r_forwardPlusZSlices && r_forwardPlusZSlices->integer > 1 ) ) {
		return qtrue;
	}
	return qfalse;
}

static uint32_t vk_fp_index_capacity( uint32_t total_clusters )
{
	uint32_t cap;
	uint32_t min_cap;
	uint32_t max_lights = ( r_clusterMaxLightsPerCluster &&
		r_clusterMaxLightsPerCluster->integer > 0 ) ?
		(uint32_t)r_clusterMaxLightsPerCluster->integer : VK_FP_MAX_COMPACT_PER_CLUSTER;
	/* Unified clustered records several depth/pre-opaque and post-opaque culls
	 * per frame. Keep four-pass headroom in the auto pool; explicit
	 * r_clusterMaxIndices remains available for memory-constrained targets. */
	uint64_t auto_cap = (uint64_t)total_clusters * (uint64_t)max_lights * 4u;

	if ( auto_cap > VK_FP_MAX_INDEX_CAP ) {
		auto_cap = VK_FP_MAX_INDEX_CAP;
	}
	cap = auto_cap > 0u ? (uint32_t)auto_cap : VK_FP_DEFAULT_INDEX_CAP;

	if ( r_clusterMaxIndices && r_clusterMaxIndices->integer > 0 ) {
		cap = (uint32_t)r_clusterMaxIndices->integer;
	}
	min_cap = total_clusters * 4u;
	if ( cap < min_cap ) {
		cap = min_cap;
	}
	if ( cap > VK_FP_MAX_INDEX_CAP ) {
		cap = VK_FP_MAX_INDEX_CAP;
	}
	return cap;
}

static uint32_t vk_fp_compact_max_per_cluster( void )
{
	uint32_t v = VK_FP_MAX_COMPACT_PER_CLUSTER;

	if ( r_clusterMaxLightsPerCluster ) {
		v = (uint32_t)r_clusterMaxLightsPerCluster->integer;
	}
	if ( v < 1u ) {
		v = 1u;
	}
	if ( v > VK_FP_MAX_COMPACT_PER_CLUSTER ) {
		v = VK_FP_MAX_COMPACT_PER_CLUSTER;
	}
	return v;
}

static void vk_fp_compute_tile_grid( uint32_t *tiles_x, uint32_t *tiles_y, uint32_t *z_slices_out,
	uint32_t *total_clusters, VkDeviceSize *tile_bytes )
{
	uint32_t vw = vk_get_render_target_width();
	uint32_t vh = vk_get_render_target_height();
	uint32_t flat;
	uint32_t z_slices = vk_fp_active_z_slices();
	qboolean compact = vk_fp_want_compact_lists();

	if ( vw < 16u ) {
		vw = 1280u;
	}
	if ( vh < 16u ) {
		vh = 720u;
	}
	*tiles_x = ( vw + VK_FP_TILE_DIM - 1u ) / VK_FP_TILE_DIM;
	*tiles_y = ( vh + VK_FP_TILE_DIM - 1u ) / VK_FP_TILE_DIM;
	flat = *tiles_x * *tiles_y;
	if ( flat > VK_FP_MAX_TILES ) {
		flat = VK_FP_MAX_TILES;
		*tiles_y = flat / *tiles_x;
		if ( *tiles_y < 1u ) {
			*tiles_y = 1u;
		}
		flat = *tiles_x * *tiles_y;
	}
	while ( z_slices > 1u && (uint64_t)flat * (uint64_t)z_slices > (uint64_t)VK_FP_MAX_CLUSTERS ) {
		z_slices--;
	}
	*z_slices_out = z_slices;
	*total_clusters = flat * z_slices;
	vk.forward_plus.compact_lists = compact;
	if ( compact ) {
		uint32_t idx_cap = vk_fp_index_capacity( *total_clusters );
		vk.forward_plus.index_capacity = idx_cap;
		*tile_bytes = (VkDeviceSize)(
			VK_FP_CLUSTER_LIST_META_UINTS +
			( *total_clusters * 2u ) +
			idx_cap ) * sizeof( uint32_t );
	} else {
		vk.forward_plus.index_capacity = 0u;
		*tile_bytes = (VkDeviceSize)*total_clusters * (VkDeviceSize)VK_FP_MAX_PER_TILE * sizeof( uint32_t );
	}
}

static void *vk_fp_tile_mapped = NULL;

/* Compact-list metadata is written by the cull dispatch, not by the CPU.
 * Console inspection is an explicit diagnostic operation, so synchronize the
 * queue there before reading the mapped header. Never do this in the render
 * loop: a premature read reports zero occupancy and would stall every frame. */
static void vk_fp_sync_cluster_stats( void )
{
	VkMappedMemoryRange range;

	if ( !vk.forward_plus.compact_lists || vk_fp_tile_mapped == NULL ||
		vk.forward_plus.tile_memory == VK_NULL_HANDLE ) {
		return;
	}
	if ( vk.queue != VK_NULL_HANDLE && qvkQueueWaitIdle != NULL ) {
		qvkQueueWaitIdle( vk.queue );
	}
	if ( qvkInvalidateMappedMemoryRanges != NULL ) {
		Com_Memset( &range, 0, sizeof( range ) );
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.memory = vk.forward_plus.tile_memory;
		range.offset = 0;
		range.size = VK_WHOLE_SIZE;
		qvkInvalidateMappedMemoryRanges( vk.device, 1, &range );
	}
	{
		const uint32_t *meta = (const uint32_t *)vk_fp_tile_mapped;
		vk.forward_plus.last_index_used = meta[0];
		vk.forward_plus.last_overflow_count = meta[1];
	}
}

static void vk_fp_destroy_tile_buffer_only( void )
{
	if ( vk_fp_tile_mapped != NULL && vk.forward_plus.tile_memory != VK_NULL_HANDLE ) {
		qvkUnmapMemory( vk.device, vk.forward_plus.tile_memory );
		vk_fp_tile_mapped = NULL;
	}
	if ( vk.forward_plus.tile_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.forward_plus.tile_buffer, NULL );
		vk.forward_plus.tile_buffer = VK_NULL_HANDLE;
	}
	if ( vk.forward_plus.tile_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.forward_plus.tile_memory, NULL );
		vk.forward_plus.tile_memory = VK_NULL_HANDLE;
	}
	vk.forward_plus.tile_capacity_tiles = 0u;
}

static void vk_fp_update_compute_descriptor_tile_binding( void )
{
	VkDescriptorBufferInfo info;
	VkWriteDescriptorSet write;

	if ( vk.forward_plus.descriptor == VK_NULL_HANDLE || vk.forward_plus.tile_buffer == VK_NULL_HANDLE ) {
		return;
	}

	info.buffer = vk.forward_plus.tile_buffer;
	info.offset = 0;
	info.range = VK_WHOLE_SIZE;

	Com_Memset( &write, 0, sizeof( write ) );
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = vk.forward_plus.descriptor;
	write.dstBinding = 1;
	write.dstArrayElement = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	write.pBufferInfo = &info;

	qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
}

/* Recreate tile SSBO when render resolution changes (r_renderScale / FBO) without vid_restart. */
static void vk_fp_ensure_tile_for_render_resolution( void )
{
	VkBufferCreateInfo bci;
	VkMemoryRequirements mr;
	VkMemoryAllocateInfo mai;
	uint32_t mem_type;
	uint32_t tiles_x, tiles_y, z_slices, total_clusters;
	VkDeviceSize tile_bytes;
	qboolean changed;
	VkBuffer new_tile = VK_NULL_HANDLE;
	VkDeviceMemory new_mem = VK_NULL_HANDLE;
	VkResult res;

	if ( !r_forwardPlus || !r_forwardPlus->integer ) {
		return;
	}
	if ( vk.forward_plus.tile_pipeline == VK_NULL_HANDLE || vk.forward_plus.buffer == VK_NULL_HANDLE ) {
		return;
	}
	if ( !vk.device || vk.device_lost ) {
		return;
	}

	{
		qboolean prev_compact = vk.forward_plus.compact_lists;
		uint32_t prev_idx = vk.forward_plus.index_capacity;

		vk_fp_compute_tile_grid( &tiles_x, &tiles_y, &z_slices, &total_clusters, &tile_bytes );

		changed = ( tiles_x != vk.forward_plus.tiles_x || tiles_y != vk.forward_plus.tiles_y ||
			z_slices != vk.forward_plus.z_slices ||
			total_clusters != vk.forward_plus.tile_capacity_tiles ||
			prev_compact != vk.forward_plus.compact_lists ||
			prev_idx != vk.forward_plus.index_capacity ||
			vk.forward_plus.tile_buffer == VK_NULL_HANDLE );
	}

	if ( !changed ) {
		return;
	}

	bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.pNext = NULL;
	bci.flags = 0;
	bci.size = tile_bytes;
	bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bci.queueFamilyIndexCount = 0;
	bci.pQueueFamilyIndices = NULL;
	res = qvkCreateBuffer( vk.device, &bci, NULL, &new_tile );
	if ( res != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[VK][Forward+] tile buffer create failed (%d); keeping previous tile SSBO\n" S_COLOR_WHITE, (int)res );
		return;
	}
	qvkGetBufferMemoryRequirements( vk.device, new_tile, &mr );
	mem_type = vk_find_memory_type( vk.physical_device, mr.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.pNext = NULL;
	mai.allocationSize = mr.size;
	mai.memoryTypeIndex = mem_type;
	res = qvkAllocateMemory( vk.device, &mai, NULL, &new_mem );
	if ( res != VK_SUCCESS ) {
		qvkDestroyBuffer( vk.device, new_tile, NULL );
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[VK][Forward+] tile buffer memory alloc failed (%d); keeping previous tile SSBO\n" S_COLOR_WHITE, (int)res );
		return;
	}
	res = qvkBindBufferMemory( vk.device, new_tile, new_mem, 0 );
	if ( res != VK_SUCCESS ) {
		qvkFreeMemory( vk.device, new_mem, NULL );
		qvkDestroyBuffer( vk.device, new_tile, NULL );
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[VK][Forward+] tile buffer bind failed (%d); keeping previous tile SSBO\n" S_COLOR_WHITE, (int)res );
		return;
	}
	SET_OBJECT_NAME( new_tile, "forward+ tile indices", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );

	vk_fp_destroy_tile_buffer_only();

	vk.forward_plus.tile_buffer = new_tile;
	vk.forward_plus.tile_memory = new_mem;
	vk.forward_plus.tiles_x = tiles_x;
	vk.forward_plus.tiles_y = tiles_y;
	vk.forward_plus.z_slices = z_slices;
	vk.forward_plus.tile_capacity_tiles = total_clusters;
	if ( qvkMapMemory( vk.device, new_mem, 0, VK_WHOLE_SIZE, 0, &vk_fp_tile_mapped ) != VK_SUCCESS ) {
		vk_fp_tile_mapped = NULL;
	}

	vk_fp_update_compute_descriptor_tile_binding();
	vk_forward_plus_init_graphics_descriptors();

	ri.Printf( PRINT_DEVELOPER, "[VK][Forward+] cluster grid resized to %ux%ux%u (%u clusters, compact=%d)\n",
		(unsigned)tiles_x, (unsigned)tiles_y, (unsigned)z_slices, (unsigned)total_clusters,
		vk.forward_plus.compact_lists ? 1 : 0 );
}

typedef struct {
	uint32_t tile_grid[2];
	uint32_t total_tiles; /* flat tiles * z_slices */
	uint32_t num_lights;
	uint32_t max_per_tile;
	uint32_t luminance_sort;
	uint32_t distance_sort;
	uint32_t depth_cull;
	uint32_t hi_z; /* r_forwardPlusHiZ: expanded same-frame depth probes for large lights */
	uint32_t hiz_pyramid;
	uint32_t hiz_levels;
	uint32_t z_slices;
	uint32_t z_slice_mode; /* 0=linear view depth, 1=log */
	float z_near;
	float z_far;
	uint32_t compact_lists;
	uint32_t index_capacity;
	uint32_t overflow_policy;
	uint32_t force_overflow;
} vk_fp_push_t;

static VkDescriptorSet vk_fp_graphics_descriptor = VK_NULL_HANDLE;
static VkDescriptorPool vk_fp_graphics_descriptor_pool = VK_NULL_HANDLE;
static VkBuffer vk_fp_dummy_light_buf = VK_NULL_HANDLE;
static VkBuffer vk_fp_dummy_tile_buf = VK_NULL_HANDLE;
static VkBuffer vk_fp_dummy_param_buf = VK_NULL_HANDLE;
static VkDeviceMemory vk_fp_dummy_light_mem = VK_NULL_HANDLE;
static VkDeviceMemory vk_fp_dummy_tile_mem = VK_NULL_HANDLE;
static VkDeviceMemory vk_fp_dummy_param_mem = VK_NULL_HANDLE;

static void vk_fp_destroy_dummy_buffers( void )
{
	if ( vk_fp_dummy_light_buf != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk_fp_dummy_light_buf, NULL );
		vk_fp_dummy_light_buf = VK_NULL_HANDLE;
	}
	if ( vk_fp_dummy_tile_buf != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk_fp_dummy_tile_buf, NULL );
		vk_fp_dummy_tile_buf = VK_NULL_HANDLE;
	}
	if ( vk_fp_dummy_param_buf != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk_fp_dummy_param_buf, NULL );
		vk_fp_dummy_param_buf = VK_NULL_HANDLE;
	}
	if ( vk_fp_dummy_light_mem != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk_fp_dummy_light_mem, NULL );
		vk_fp_dummy_light_mem = VK_NULL_HANDLE;
	}
	if ( vk_fp_dummy_tile_mem != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk_fp_dummy_tile_mem, NULL );
		vk_fp_dummy_tile_mem = VK_NULL_HANDLE;
	}
	if ( vk_fp_dummy_param_mem != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk_fp_dummy_param_mem, NULL );
		vk_fp_dummy_param_mem = VK_NULL_HANDLE;
	}
}

static void vk_fp_alloc_dummy_ssbo( VkBuffer *buf, VkDeviceMemory *mem, VkDeviceSize size )
{
	VkBufferCreateInfo bci;
	VkMemoryRequirements mr;
	VkMemoryAllocateInfo mai;
	uint32_t mem_type;
	byte *ptr;

	bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.pNext = NULL;
	bci.flags = 0;
	bci.size = size;
	bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bci.queueFamilyIndexCount = 0;
	bci.pQueueFamilyIndices = NULL;
	VK_CHECK( qvkCreateBuffer( vk.device, &bci, NULL, buf ) );
	qvkGetBufferMemoryRequirements( vk.device, *buf, &mr );
	mem_type = vk_find_memory_type( vk.physical_device, mr.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.pNext = NULL;
	mai.allocationSize = mr.size;
	mai.memoryTypeIndex = mem_type;
	VK_CHECK( qvkAllocateMemory( vk.device, &mai, NULL, mem ) );
	VK_CHECK( qvkMapMemory( vk.device, *mem, 0, VK_WHOLE_SIZE, 0, (void **)&ptr ) );
	Com_Memset( ptr, 0, (size_t)mr.size );
	VK_CHECK( qvkBindBufferMemory( vk.device, *buf, *mem, 0 ) );
}

static void vk_fp_create_dummy_buffers( void )
{
	if ( vk_fp_dummy_light_buf != VK_NULL_HANDLE ) {
		return;
	}

	vk_fp_alloc_dummy_ssbo( &vk_fp_dummy_light_buf, &vk_fp_dummy_light_mem,
		(VkDeviceSize)VK_FP_DUMMY_LIGHT_FLOATS * sizeof( float ) );
	vk_fp_alloc_dummy_ssbo( &vk_fp_dummy_tile_buf, &vk_fp_dummy_tile_mem,
		(VkDeviceSize)VK_FP_DUMMY_TILE_UINTS * sizeof( uint32_t ) );
	vk_fp_alloc_dummy_ssbo( &vk_fp_dummy_param_buf, &vk_fp_dummy_param_mem, (VkDeviceSize)VK_FP_PARAM_BYTES );
}

static void vk_fp_write_graphics_descriptor( VkBuffer light_buf, VkBuffer tile_buf, VkBuffer param_buf )
{
	VkDescriptorBufferInfo infos[3];
	VkWriteDescriptorSet writes[3];

	if ( vk_fp_graphics_descriptor == VK_NULL_HANDLE || light_buf == VK_NULL_HANDLE ||
		tile_buf == VK_NULL_HANDLE || param_buf == VK_NULL_HANDLE ) {
		return;
	}

	infos[0].buffer = light_buf;
	infos[0].offset = 0;
	infos[0].range = VK_WHOLE_SIZE;
	infos[1].buffer = tile_buf;
	infos[1].offset = 0;
	infos[1].range = VK_WHOLE_SIZE;
	infos[2].buffer = param_buf;
	infos[2].offset = 0;
	infos[2].range = VK_WHOLE_SIZE;

	Com_Memset( writes, 0, sizeof( writes ) );
	for ( int i = 0; i < 3; i++ ) {
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].dstSet = vk_fp_graphics_descriptor;
		writes[i].dstBinding = (uint32_t)i;
		writes[i].descriptorCount = 1;
		writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[i].pBufferInfo = &infos[i];
	}

	qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );
}

void vk_forward_plus_create_set_layout( void )
{
	VkDescriptorSetLayoutBinding binds[10];
	VkDescriptorSetLayoutCreateInfo layout_ci;

#ifdef USE_VK_PBR
	if ( vk.set_layout_forward_plus != VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( binds, 0, sizeof( binds ) );
	binds[0].binding = 0;
	binds[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[0].descriptorCount = 1;
	binds[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	binds[1].binding = 1;
	binds[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[1].descriptorCount = 1;
	binds[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	binds[2].binding = 2;
	binds[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[2].descriptorCount = 1;
	binds[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	binds[3].binding = 3;
	binds[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[3].descriptorCount = 1;
	binds[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[4].binding = 4;
	binds[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[4].descriptorCount = 1;
	binds[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	/* Temporal reactive mask (R8 storage); stamped by gen_frag when transparent/stochastic. */
	binds[5].binding = 5;
	binds[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	binds[5].descriptorCount = 1;
	binds[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	/* LTC mat / amp LUTs for rectangular area lights (fragment + deferred via shared tables). */
	binds[6].binding = 6;
	binds[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[6].descriptorCount = 1;
	binds[6].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	binds[7].binding = 7;
	binds[7].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[7].descriptorCount = 1;
	binds[7].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	/* Dynamic-object identity buffer (R32_UINT storage); stamped by gen_frag via imageAtomicMax. */
	binds[8].binding = 8;
	binds[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	binds[8].descriptorCount = 1;
	binds[8].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	/* Real vk_hiz pyramid for optional Forward+ light-volume depth cull. */
	binds[9].binding = 9;
	binds[9].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[9].descriptorCount = 1;
	binds[9].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.pNext = NULL;
	layout_ci.flags = 0;
	layout_ci.bindingCount = 10;
	layout_ci.pBindings = binds;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.set_layout_forward_plus ) );
	SET_OBJECT_NAME( vk.set_layout_forward_plus, "descriptor set layout - forward+", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT );
#endif
}

void vk_forward_plus_destroy_graphics_layout( void )
{
#ifdef USE_VK_PBR
	if ( vk.set_layout_forward_plus != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.set_layout_forward_plus, NULL );
		vk.set_layout_forward_plus = VK_NULL_HANDLE;
	}
#endif
}

void vk_forward_plus_init_graphics_descriptors( void )
{
#ifdef USE_VK_PBR
	VkDescriptorSetAllocateInfo alloc_ci;

	if ( vk.set_layout_forward_plus == VK_NULL_HANDLE || vk.descriptor_pool == VK_NULL_HANDLE ) {
		return;
	}

	if ( vk_fp_graphics_descriptor_pool != vk.descriptor_pool ) {
		vk_fp_graphics_descriptor = VK_NULL_HANDLE;
		vk_fp_graphics_descriptor_pool = vk.descriptor_pool;
	}

	if ( vk_fp_graphics_descriptor == VK_NULL_HANDLE ) {
		alloc_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_ci.pNext = NULL;
		alloc_ci.descriptorPool = vk.descriptor_pool;
		alloc_ci.descriptorSetCount = 1;
		alloc_ci.pSetLayouts = &vk.set_layout_forward_plus;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc_ci, &vk_fp_graphics_descriptor ) );
	}

	vk_fp_create_dummy_buffers();

	if ( r_forwardPlus && r_forwardPlus->integer && vk.forward_plus.buffer != VK_NULL_HANDLE &&
		vk.forward_plus.tile_buffer != VK_NULL_HANDLE && vk.forward_plus.param_buffer != VK_NULL_HANDLE ) {
		vk_fp_write_graphics_descriptor( vk.forward_plus.buffer, vk.forward_plus.tile_buffer, vk.forward_plus.param_buffer );
	} else {
		vk_fp_write_graphics_descriptor( vk_fp_dummy_light_buf, vk_fp_dummy_tile_buf, vk_fp_dummy_param_buf );
	}
	vk_reactive_mask_update_storage_descriptor();
	vk_object_id_update_storage_descriptor();
	vk_ltc_init();
	vk_ltc_update_forward_plus_descriptors( vk_fp_graphics_descriptor );
#endif
}

static void vk_fp_destroy_buffers( void )
{
	vk_fp_destroy_tile_buffer_only();
	if ( vk.forward_plus.param_buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.forward_plus.param_buffer, NULL );
		vk.forward_plus.param_buffer = VK_NULL_HANDLE;
	}
	if ( vk.forward_plus.param_memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.forward_plus.param_memory, NULL );
		vk.forward_plus.param_memory = VK_NULL_HANDLE;
	}
	vk.forward_plus.param_mapped = NULL;
	vk.forward_plus.param_buffer_size = 0u;
	vk.forward_plus.tile_capacity_tiles = 0u;
	vk.forward_plus.descriptor = VK_NULL_HANDLE;
}

static void vk_fp_destroy_compute_pipeline( void )
{
	if ( vk.forward_plus.tile_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.forward_plus.tile_pipeline, NULL );
		vk.forward_plus.tile_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.forward_plus.pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.forward_plus.pipeline_layout, NULL );
		vk.forward_plus.pipeline_layout = VK_NULL_HANDLE;
	}
}

void vk_forward_plus_destroy_compute_pipeline( void )
{
	vk_fp_destroy_compute_pipeline();
}

void vk_forward_plus_on_descriptor_pool_destroyed( void )
{
	vk_fp_graphics_descriptor = VK_NULL_HANDLE;
	vk_fp_graphics_descriptor_pool = VK_NULL_HANDLE;
	vk.forward_plus.descriptor = VK_NULL_HANDLE;
}

static void vk_fp_destroy_light_buffer( void )
{
	if ( vk.forward_plus.staging != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.forward_plus.staging, NULL );
		vk.forward_plus.staging = VK_NULL_HANDLE;
	}
	if ( vk.forward_plus.staging_memory != VK_NULL_HANDLE ) {
		if ( vk.forward_plus.staging_ptr != NULL ) {
			qvkUnmapMemory( vk.device, vk.forward_plus.staging_memory );
			vk.forward_plus.staging_ptr = NULL;
		}
		qvkFreeMemory( vk.device, vk.forward_plus.staging_memory, NULL );
		vk.forward_plus.staging_memory = VK_NULL_HANDLE;
	}
	if ( vk.forward_plus.buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.forward_plus.buffer, NULL );
		vk.forward_plus.buffer = VK_NULL_HANDLE;
	}
	if ( vk.forward_plus.memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.forward_plus.memory, NULL );
		vk.forward_plus.memory = VK_NULL_HANDLE;
	}
	vk.forward_plus.staging_ptr = NULL;
	vk.forward_plus.last_upload_bytes = 0u;
	vk.forward_plus.capacity_bytes = 0u;
}

void vk_forward_plus_shutdown( void )
{
	vk_cluster_unregister_commands();
	vk_forward_plus_on_descriptor_pool_destroyed();
	vk_fp_destroy_buffers();
	vk_fp_destroy_light_buffer();
	vk.forward_plus.last_packed_count = 0u;
	vk.forward_plus.tiles_x = 0u;
	vk.forward_plus.tiles_y = 0u;
	vk.forward_plus.z_slices = 0u;
	vk_fp_destroy_dummy_buffers();
}

uint32_t vk_forward_plus_get_min_per_tile_cap( void )
{
	return VK_FP_MIN_PER_TILE;
}

uint32_t vk_forward_plus_get_max_per_tile_cap( void )
{
	return VK_FP_MAX_PER_TILE;
}

static uint32_t vk_fp_effective_max_per_tile( void )
{
	int v;
	const int min_t = (int)VK_FP_MIN_PER_TILE;
	const int max_t = (int)VK_FP_MAX_PER_TILE;

	if ( !r_forwardPlusMaxPerTile ) {
		return VK_FP_MAX_PER_TILE;
	}
	v = r_forwardPlusMaxPerTile->integer;
	if ( v < min_t ) {
		v = min_t;
	}
	if ( v > max_t ) {
		v = max_t;
	}
	return (uint32_t)v;
}

static void vk_fp_create_buffers_and_compute( void )
{
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkComputePipelineCreateInfo pipe_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkDescriptorSetAllocateInfo alloc_ci;
	VkDescriptorBufferInfo buf_infos[3];
	VkWriteDescriptorSet writes[3];
	VkBufferCreateInfo bci;
	VkMemoryRequirements mr;
	VkMemoryAllocateInfo mai;
	uint32_t mem_type;
	uint32_t tiles_x, tiles_y, z_slices, total_clusters;
	VkDeviceSize tile_bytes;
	/* Up to VK_FP_MAX_GPU_LIGHTS (MAX_REAL_DLIGHTS); indices 0..31 still participate in tess.dlightBits skip. */
	const uint32_t max_lights = (uint32_t)VK_FP_MAX_GPU_LIGHTS;
	const VkDeviceSize light_buf_size = (VkDeviceSize)VK_FP_HEADER_BYTES + (VkDeviceSize)max_lights * (VkDeviceSize)VK_FP_RECORD_STRIDE;

	vk.forward_plus.max_per_tile = vk_fp_effective_max_per_tile();

	vk_fp_compute_tile_grid( &tiles_x, &tiles_y, &z_slices, &total_clusters, &tile_bytes );

	bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.pNext = NULL;
	bci.flags = 0;
	bci.size = light_buf_size;
	bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bci.queueFamilyIndexCount = 0;
	bci.pQueueFamilyIndices = NULL;
	VK_CHECK( qvkCreateBuffer( vk.device, &bci, NULL, &vk.forward_plus.buffer ) );
	qvkGetBufferMemoryRequirements( vk.device, vk.forward_plus.buffer, &mr );
	mem_type = vk_find_memory_type( vk.physical_device, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.pNext = NULL;
	mai.allocationSize = mr.size;
	mai.memoryTypeIndex = mem_type;
	VK_CHECK( qvkAllocateMemory( vk.device, &mai, NULL, &vk.forward_plus.memory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.forward_plus.buffer, vk.forward_plus.memory, 0 ) );
	vk.forward_plus.capacity_bytes = (uint32_t)light_buf_size;
	SET_OBJECT_NAME( vk.forward_plus.buffer, "forward+ light records (device)", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );

	/* Host staging: CPU pack each frame, copy to device before tile cull (VRAM path for compute + PBR). */
	bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	VK_CHECK( qvkCreateBuffer( vk.device, &bci, NULL, &vk.forward_plus.staging ) );
	qvkGetBufferMemoryRequirements( vk.device, vk.forward_plus.staging, &mr );
	mem_type = vk_find_memory_type( vk.physical_device, mr.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	mai.allocationSize = mr.size;
	mai.memoryTypeIndex = mem_type;
	VK_CHECK( qvkAllocateMemory( vk.device, &mai, NULL, &vk.forward_plus.staging_memory ) );
	VK_CHECK( qvkMapMemory( vk.device, vk.forward_plus.staging_memory, 0, VK_WHOLE_SIZE, 0, &vk.forward_plus.staging_ptr ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.forward_plus.staging, vk.forward_plus.staging_memory, 0 ) );
	Com_Memset( vk.forward_plus.staging_ptr, 0, (size_t)light_buf_size );
	SET_OBJECT_NAME( vk.forward_plus.staging, "forward+ light records (staging)", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	vk.forward_plus.last_upload_bytes = 0u;

	if ( vk.modules.forward_plus_tile_cull_cs == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[VK][Forward+] forward_plus_tile_cull compute shader missing; tile SSBO disabled\n" S_COLOR_WHITE );
		vk_fp_destroy_light_buffer();
		vk.forward_plus.tiles_x = 0u;
		vk.forward_plus.tiles_y = 0u;
		vk.forward_plus.z_slices = 0u;
		vk.forward_plus.tile_capacity_tiles = 0u;
		return;
	}

	bci.size = tile_bytes;
	bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	if ( qvkCreateBuffer( vk.device, &bci, NULL, &vk.forward_plus.tile_buffer ) != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[VK][Forward+] tile buffer create failed; Forward+ init aborted\n" S_COLOR_WHITE );
		vk_fp_destroy_light_buffer();
		vk.forward_plus.tiles_x = 0u;
		vk.forward_plus.tiles_y = 0u;
		vk.forward_plus.z_slices = 0u;
		vk.forward_plus.tile_capacity_tiles = 0u;
		return;
	}
	qvkGetBufferMemoryRequirements( vk.device, vk.forward_plus.tile_buffer, &mr );
	mem_type = vk_find_memory_type( vk.physical_device, mr.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	mai.allocationSize = mr.size;
	mai.memoryTypeIndex = mem_type;
	if ( qvkAllocateMemory( vk.device, &mai, NULL, &vk.forward_plus.tile_memory ) != VK_SUCCESS ) {
		qvkDestroyBuffer( vk.device, vk.forward_plus.tile_buffer, NULL );
		vk.forward_plus.tile_buffer = VK_NULL_HANDLE;
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[VK][Forward+] tile buffer memory alloc failed; Forward+ init aborted\n" S_COLOR_WHITE );
		vk_fp_destroy_light_buffer();
		vk.forward_plus.tiles_x = 0u;
		vk.forward_plus.tiles_y = 0u;
		vk.forward_plus.z_slices = 0u;
		vk.forward_plus.tile_capacity_tiles = 0u;
		return;
	}
	if ( qvkBindBufferMemory( vk.device, vk.forward_plus.tile_buffer, vk.forward_plus.tile_memory, 0 ) != VK_SUCCESS ) {
		qvkFreeMemory( vk.device, vk.forward_plus.tile_memory, NULL );
		vk.forward_plus.tile_memory = VK_NULL_HANDLE;
		qvkDestroyBuffer( vk.device, vk.forward_plus.tile_buffer, NULL );
		vk.forward_plus.tile_buffer = VK_NULL_HANDLE;
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[VK][Forward+] tile buffer bind failed; Forward+ init aborted\n" S_COLOR_WHITE );
		vk_fp_destroy_light_buffer();
		vk.forward_plus.tiles_x = 0u;
		vk.forward_plus.tiles_y = 0u;
		vk.forward_plus.z_slices = 0u;
		vk.forward_plus.tile_capacity_tiles = 0u;
		return;
	}
	SET_OBJECT_NAME( vk.forward_plus.tile_buffer, "forward+ tile indices", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	if ( qvkMapMemory( vk.device, vk.forward_plus.tile_memory, 0, VK_WHOLE_SIZE, 0, &vk_fp_tile_mapped ) != VK_SUCCESS ) {
		vk_fp_tile_mapped = NULL;
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[VK][Forward+] tile buffer map failed; compact meta reset disabled\n" S_COLOR_WHITE );
	}

	bci.size = VK_FP_PARAM_BYTES;
	bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	if ( qvkCreateBuffer( vk.device, &bci, NULL, &vk.forward_plus.param_buffer ) != VK_SUCCESS ) {
		vk_fp_destroy_buffers();
		vk_fp_destroy_light_buffer();
		vk.forward_plus.tiles_x = 0u;
		vk.forward_plus.tiles_y = 0u;
		vk.forward_plus.z_slices = 0u;
		vk.forward_plus.tile_capacity_tiles = 0u;
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[VK][Forward+] param buffer create failed; Forward+ init aborted\n" S_COLOR_WHITE );
		return;
	}
	qvkGetBufferMemoryRequirements( vk.device, vk.forward_plus.param_buffer, &mr );
	mem_type = vk_find_memory_type( vk.physical_device, mr.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	mai.allocationSize = mr.size;
	mai.memoryTypeIndex = mem_type;
	if ( qvkAllocateMemory( vk.device, &mai, NULL, &vk.forward_plus.param_memory ) != VK_SUCCESS ) {
		qvkDestroyBuffer( vk.device, vk.forward_plus.param_buffer, NULL );
		vk.forward_plus.param_buffer = VK_NULL_HANDLE;
		vk_fp_destroy_buffers();
		vk_fp_destroy_light_buffer();
		vk.forward_plus.tiles_x = 0u;
		vk.forward_plus.tiles_y = 0u;
		vk.forward_plus.z_slices = 0u;
		vk.forward_plus.tile_capacity_tiles = 0u;
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[VK][Forward+] param buffer memory alloc failed; Forward+ init aborted\n" S_COLOR_WHITE );
		return;
	}
	if ( qvkMapMemory( vk.device, vk.forward_plus.param_memory, 0, VK_WHOLE_SIZE, 0, &vk.forward_plus.param_mapped ) != VK_SUCCESS ||
		qvkBindBufferMemory( vk.device, vk.forward_plus.param_buffer, vk.forward_plus.param_memory, 0 ) != VK_SUCCESS ) {
		qvkFreeMemory( vk.device, vk.forward_plus.param_memory, NULL );
		vk.forward_plus.param_memory = VK_NULL_HANDLE;
		qvkDestroyBuffer( vk.device, vk.forward_plus.param_buffer, NULL );
		vk.forward_plus.param_buffer = VK_NULL_HANDLE;
		vk_fp_destroy_buffers();
		vk_fp_destroy_light_buffer();
		vk.forward_plus.tiles_x = 0u;
		vk.forward_plus.tiles_y = 0u;
		vk.forward_plus.z_slices = 0u;
		vk.forward_plus.tile_capacity_tiles = 0u;
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[VK][Forward+] param buffer map/bind failed; Forward+ init aborted\n" S_COLOR_WHITE );
		return;
	}
	vk.forward_plus.param_buffer_size = (uint32_t)mr.size;
	Com_Memset( vk.forward_plus.param_mapped, 0, (size_t)mr.size );
	SET_OBJECT_NAME( vk.forward_plus.param_buffer, "forward+ tile cull params", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_fp_push_t );

	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.pNext = NULL;
	pl_ci.flags = 0;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.set_layout_forward_plus;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	if ( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.forward_plus.pipeline_layout ) != VK_SUCCESS ) {
		vk_fp_destroy_buffers();
		vk_fp_destroy_light_buffer();
		vk.forward_plus.tiles_x = 0u;
		vk.forward_plus.tiles_y = 0u;
		vk.forward_plus.z_slices = 0u;
		vk.forward_plus.tile_capacity_tiles = 0u;
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[VK][Forward+] tile cull pipeline layout create failed; Forward+ init aborted\n" S_COLOR_WHITE );
		return;
	}

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.forward_plus_tile_cull_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.forward_plus.pipeline_layout;
	if ( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.forward_plus.tile_pipeline ) != VK_SUCCESS ) {
		vk_fp_destroy_compute_pipeline();
		vk_fp_destroy_buffers();
		vk_fp_destroy_light_buffer();
		vk.forward_plus.tiles_x = 0u;
		vk.forward_plus.tiles_y = 0u;
		vk.forward_plus.z_slices = 0u;
		vk.forward_plus.tile_capacity_tiles = 0u;
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[VK][Forward+] tile cull compute pipeline create failed; Forward+ init aborted\n" S_COLOR_WHITE );
		return;
	}
	SET_OBJECT_NAME( vk.forward_plus.tile_pipeline, "pipeline - forward+ tile cull", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	alloc_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_ci.pNext = NULL;
	alloc_ci.descriptorPool = vk.descriptor_pool;
	alloc_ci.descriptorSetCount = 1;
	alloc_ci.pSetLayouts = &vk.set_layout_forward_plus;
	if ( qvkAllocateDescriptorSets( vk.device, &alloc_ci, &vk.forward_plus.descriptor ) != VK_SUCCESS ) {
		vk_fp_destroy_compute_pipeline();
		vk_fp_destroy_buffers();
		vk_fp_destroy_light_buffer();
		vk.forward_plus.tiles_x = 0u;
		vk.forward_plus.tiles_y = 0u;
		vk.forward_plus.z_slices = 0u;
		vk.forward_plus.tile_capacity_tiles = 0u;
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[VK][Forward+] descriptor set alloc failed (pool full?); Forward+ init aborted\n" S_COLOR_WHITE );
		return;
	}

	Com_Memset( buf_infos, 0, sizeof( buf_infos ) );
	buf_infos[0].buffer = vk.forward_plus.buffer;
	buf_infos[0].offset = 0;
	buf_infos[0].range = VK_WHOLE_SIZE;
	buf_infos[1].buffer = vk.forward_plus.tile_buffer;
	buf_infos[1].offset = 0;
	buf_infos[1].range = VK_WHOLE_SIZE;
	buf_infos[2].buffer = vk.forward_plus.param_buffer;
	buf_infos[2].offset = 0;
	buf_infos[2].range = VK_WHOLE_SIZE;

	Com_Memset( writes, 0, sizeof( writes ) );
	for ( int i = 0; i < 3; i++ ) {
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].dstSet = vk.forward_plus.descriptor;
		writes[i].dstBinding = (uint32_t)i;
		writes[i].dstArrayElement = 0;
		writes[i].descriptorCount = 1;
		writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[i].pBufferInfo = &buf_infos[i];
	}
	qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );
	vk_forward_plus_update_depth_descriptor();

	vk.forward_plus.tiles_x = tiles_x;
	vk.forward_plus.tiles_y = tiles_y;
	vk.forward_plus.z_slices = z_slices;
	vk.forward_plus.tile_capacity_tiles = total_clusters;

	vk_forward_plus_init_graphics_descriptors();

	ri.Printf( PRINT_ALL, "[VK][Forward+] light grid: %ux%u tiles × %u Z-slices (%u clusters), %u bytes list, max %u lights/cluster\n",
		(unsigned)tiles_x, (unsigned)tiles_y, (unsigned)z_slices, (unsigned)total_clusters, (unsigned)tile_bytes,
		(unsigned)vk.forward_plus.max_per_tile );
}

void vk_forward_plus_update_depth_descriptor( void )
{
	VkDescriptorImageInfo image_infos[2];
	VkWriteDescriptorSet writes[2];
	Vk_Sampler_Def depth_sd;
	VkImageView depth_view;
	vkHizPyramidSampleInfo_t hiz_info;

	depth_view = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	if ( vk.forward_plus.descriptor == VK_NULL_HANDLE || depth_view == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &depth_sd, 0, sizeof( depth_sd ) );
	depth_sd.gl_mag_filter = depth_sd.gl_min_filter = GL_NEAREST;
	depth_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	depth_sd.noAnisotropy = qtrue;

	Com_Memset( image_infos, 0, sizeof( image_infos ) );
	image_infos[0].sampler = vk_find_sampler( &depth_sd );
	image_infos[0].imageView = depth_view;
	image_infos[0].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = vk.forward_plus.descriptor;
	writes[0].dstBinding = 3;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &image_infos[0];

	if ( vk_hiz_get_pyramid_sample_info( &hiz_info ) ) {
		image_infos[1].sampler = image_infos[0].sampler;
		image_infos[1].imageView = hiz_info.view;
		image_infos[1].imageLayout = hiz_info.layout;
	} else {
		image_infos[1] = image_infos[0];
	}
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = vk.forward_plus.descriptor;
	writes[1].dstBinding = 9;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].pImageInfo = &image_infos[1];

	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );

#ifdef USE_VK_PBR
	if ( vk_fp_graphics_descriptor != VK_NULL_HANDLE && vk_fp_graphics_descriptor != vk.forward_plus.descriptor ) {
		writes[1].dstSet = vk_fp_graphics_descriptor;
		qvkUpdateDescriptorSets( vk.device, 1, &writes[1], 0, NULL );
	}
#endif
}

void vk_forward_plus_update_sun_shadow_descriptor( void )
{
	VkDescriptorImageInfo shadow_info;
	VkWriteDescriptorSet write;
	VkImageView shadow_view;

#ifdef USE_VK_PBR
	shadow_view = vk.sun_shadow_sample_view;
	if ( vk_fp_graphics_descriptor == VK_NULL_HANDLE || shadow_view == VK_NULL_HANDLE ) {
		shadow_view = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
		if ( vk_fp_graphics_descriptor == VK_NULL_HANDLE || shadow_view == VK_NULL_HANDLE ) {
			return;
		}
	}

	if ( vk.sun_shadow_sampler == VK_NULL_HANDLE ) {
		Vk_Sampler_Def shadow_sd;
		Com_Memset( &shadow_sd, 0, sizeof( shadow_sd ) );
		shadow_sd.gl_mag_filter = shadow_sd.gl_min_filter = GL_NEAREST;
		shadow_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		shadow_sd.noAnisotropy = qtrue;
		vk.sun_shadow_sampler = vk_find_sampler( &shadow_sd );
	}

	Com_Memset( &shadow_info, 0, sizeof( shadow_info ) );
	shadow_info.sampler = vk.sun_shadow_sampler;
	shadow_info.imageView = shadow_view;
	shadow_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	Com_Memset( &write, 0, sizeof( write ) );
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = vk_fp_graphics_descriptor;
	write.dstBinding = 4;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &shadow_info;
	qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );

	if ( vk.forward_plus.descriptor != VK_NULL_HANDLE && vk.forward_plus.descriptor != vk_fp_graphics_descriptor ) {
		write.dstSet = vk.forward_plus.descriptor;
		qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
	}
#endif
}

void vk_forward_plus_init( void )
{
	R_ApplyRenderModeLatch();

	/* CPU ↔ shader contract (Clustered Hybrid M1 schema). */
	{
		_Static_assert( VK_FP_RECORD_STRIDE == 64, "light record must be 4x vec4" );
		_Static_assert( VK_FP_HEADER_BYTES == 32, "light header must be 2x vec4" );
		_Static_assert( VK_FP_TILE_DIM == VK_CLUSTER_TILE_SIZE, "cluster tile size alias must match Forward+" );
		_Static_assert( VK_FP_MAX_PER_TILE == 8u, "tile list stride must match deferred_lighting / gen_frag" );
		_Static_assert( VK_FP_MAX_GPU_LIGHTS == 64, "GPU light cap must match pack path" );
	}

	vk_fp_destroy_compute_pipeline();
	vk_fp_destroy_buffers();
	vk_fp_destroy_light_buffer();

	if ( !r_forwardPlus || !r_forwardPlus->integer ) {
		ri.Printf( PRINT_ALL, "[VK][Forward+] r_forwardPlus=0 (Forward+ scaffolding disabled)\n" );
		vk_forward_plus_init_graphics_descriptors();
		return;
	}

	vk_fp_create_buffers_and_compute();
	vk.forward_plus.cluster_list_generation = 1u;
	vk_cluster_register_commands();

	ri.Printf( PRINT_ALL, "[VK][Forward+] r_forwardPlus=1 device-local light SSBO + staging %u bytes (tile cull + PBR read VRAM)\n",
		(unsigned)vk.forward_plus.capacity_bytes );
	ri.Printf( PRINT_ALL,
		"[VK][cluster] schema: record=%uB header=%uB tile=%ux%u maxPerTile=%u maxLights=%u gen=%u\n",
		(unsigned)VK_FP_RECORD_STRIDE, (unsigned)VK_FP_HEADER_BYTES,
		(unsigned)VK_FP_TILE_DIM, (unsigned)VK_FP_TILE_DIM,
		(unsigned)VK_FP_MAX_PER_TILE, (unsigned)VK_FP_MAX_GPU_LIGHTS,
		vk.forward_plus.cluster_list_generation );
	if ( r_forwardPlusLuminanceSort && r_forwardPlusLuminanceSort->integer ) {
		ri.Printf( PRINT_ALL, "[VK][Forward+] r_forwardPlusLuminanceSort=1 (tile overload picks brightest RGB sum)\n" );
	}
}

void vk_forward_plus_ensure_runtime( void )
{
	if ( !r_forwardPlus || !r_forwardPlus->integer || !vk.device || vk.device_lost ) {
		return;
	}

	if ( vk.forward_plus.buffer == VK_NULL_HANDLE ||
		vk.forward_plus.staging == VK_NULL_HANDLE ||
		vk.forward_plus.tile_buffer == VK_NULL_HANDLE ||
		vk.forward_plus.param_buffer == VK_NULL_HANDLE ||
		vk.forward_plus.param_mapped == NULL ||
		vk.forward_plus.descriptor == VK_NULL_HANDLE ||
		vk.forward_plus.pipeline_layout == VK_NULL_HANDLE ||
		vk.forward_plus.tile_pipeline == VK_NULL_HANDLE ||
		vk.forward_plus.tiles_x == 0u ||
		vk.forward_plus.tiles_y == 0u ||
		vk.forward_plus.tile_capacity_tiles == 0u ) {
		ri.Printf( PRINT_WARNING, "[VK][Forward+] runtime incomplete; rebuilding Forward+ resources in-place\n" );
		vk_forward_plus_init();
		return;
	}

	vk_forward_plus_init_graphics_descriptors();
}

void vk_forward_plus_ensure_render_resolution( void )
{
	vk_forward_plus_ensure_runtime();
	vk_fp_ensure_tile_for_render_resolution();
}

void vk_forward_plus_update_for_refdef( void )
{
	float *base;
	uint32_t n;
	uint32_t src;
	unsigned int i;
	uint32_t max_pack;
	const dlight_t *dl;
	float dbg;
	float cos_outer, cos_inner;
	static uint32_t s_trunc_log_src;

	if ( !r_forwardPlus || !r_forwardPlus->integer ) {
		return;
	}
	vk_forward_plus_ensure_runtime();
	if ( vk.forward_plus.buffer == VK_NULL_HANDLE || vk.forward_plus.staging_ptr == NULL ) {
		return;
	}

	base = (float *)vk.forward_plus.staging_ptr;
	src = backEnd.refdef.num_dlights;
	n = src;
	if ( n > (uint32_t)VK_FP_MAX_GPU_LIGHTS ) {
		n = (uint32_t)VK_FP_MAX_GPU_LIGHTS;
	}

	max_pack = ( vk.forward_plus.capacity_bytes - (uint32_t)VK_FP_HEADER_BYTES ) / (uint32_t)VK_FP_RECORD_STRIDE;
	if ( n > max_pack ) {
		n = max_pack;
	}

	if ( src > n ) {
		if ( src != s_trunc_log_src ) {
			ri.Printf( PRINT_DEVELOPER,
				"[VK][Forward+] refdef has %u dlights; packing %u (Forward+ cap %u; surface dlightBits still %d)\n",
				(unsigned)src, (unsigned)n, (unsigned)VK_FP_MAX_GPU_LIGHTS, MAX_DLIGHTS );
			s_trunc_log_src = src;
		}
	} else if ( src <= (uint32_t)VK_FP_MAX_GPU_LIGHTS ) {
		s_trunc_log_src = 0u;
	}

	vk_linear_dlight_cone_cosines( &cos_outer, &cos_inner );

	dbg = ( r_forwardPlusDebug && r_forwardPlusDebug->value > 0.0f ) ? r_forwardPlusDebug->value : 0.0f;

	/* Header vec4: x=count, y=refdef time (ms), z=max lights per tile (4..8), w=debug overlay scale */
	base[0] = (float)n;
	base[1] = (float)backEnd.refdef.time;
	base[2] = (float)vk.forward_plus.max_per_tile;
	base[3] = dbg;

	/* Tile grid + render target size (FBO / r_renderScale; matches NDC->pixel in tile cull) */
	base[4] = (float)vk.forward_plus.tiles_x;
	base[5] = (float)vk.forward_plus.tiles_y;
	base[6] = (float)vk_get_render_target_width();
	base[7] = (float)vk_get_render_target_height();

	dl = backEnd.refdef.dlights;
	if ( !dl ) {
		n = 0u;
		base[0] = 0.0f;
	}

	/* Clear full pack region so shrinks / authored appends cannot leave stale lights. */
	{
		float *body = base + (uint32_t)( VK_FP_HEADER_BYTES / sizeof( float ) );
		Com_Memset( body, 0, (size_t)max_pack * (size_t)VK_FP_RECORD_STRIDE );
	}

	for ( i = 0; i < n && dl; i++ ) {
		dlight_t packed;
		const dlight_t *L;
		float *rec = base + (uint32_t)( VK_FP_HEADER_BYTES / sizeof( float ) ) + (uint32_t)i * (uint32_t)( VK_FP_RECORD_STRIDE / sizeof( float ) );
		vec3_t dir;
		float len;
		float photoScale;

		Com_Memcpy( &packed, dl + i, sizeof( packed ) );
		/* Live-edit overrides from scene platform (origin/color/radius/visibility/area). */
		if ( vk_scene_platform_active() ) {
			vk_scene_platform_apply_light_override( (uint32_t)i, &packed );
		}
		L = &packed;
		if ( !L->radius || ( L->color[0] <= 0.0f && L->color[1] <= 0.0f && L->color[2] <= 0.0f ) ) {
			Com_Memset( rec, 0, (size_t)VK_FP_RECORD_STRIDE );
			continue;
		}

		rec[0] = L->origin[0];
		rec[1] = L->origin[1];
		rec[2] = L->origin[2];
		rec[3] = L->radius;

		{
			vec3_t scaledColor;
			R_DynamicLightColor( L, scaledColor );
			photoScale = vk_photometric_pack_intensity_scale(
				MAX( MAX( scaledColor[0], scaledColor[1] ), scaledColor[2] ), L->radius );
			rec[4] = MAX( scaledColor[0] * photoScale, 0.0f );
			rec[5] = MAX( scaledColor[1] * photoScale, 0.0f );
			rec[6] = MAX( scaledColor[2] * photoScale, 0.0f );
		}

		if ( L->area ) {
			vec3_t halfU, halfV;
			float diag;
			/* type = 2.0 → rect area (lc.w >= 1.5 in shaders). */
			rec[7] = 2.0f;
			VectorScale( L->areaRight, L->areaHalfWidth, halfU );
			VectorScale( L->areaUp, L->areaHalfHeight, halfV );
			diag = sqrtf( L->areaHalfWidth * L->areaHalfWidth + L->areaHalfHeight * L->areaHalfHeight );
			if ( rec[3] < diag * 2.0f ) {
				rec[3] = diag * 2.0f;
			}
			rec[8] = halfU[0];
			rec[9] = halfU[1];
			rec[10] = halfU[2];
			rec[11] = L->additive ? 1.0f : 0.0f;
			rec[12] = halfV[0];
			rec[13] = halfV[1];
			rec[14] = halfV[2];
			rec[15] = 0.0f;
		} else if ( L->linear ) {
			rec[7] = 1.0f;
			VectorSubtract( L->origin2, L->origin, dir );
			len = VectorNormalize( dir );
			if ( len <= 0.001f ) {
				VectorSet( dir, 0.0f, 0.0f, -1.0f );
			}
			rec[8] = dir[0];
			rec[9] = dir[1];
			rec[10] = dir[2];
			rec[11] = cos_outer;
			rec[12] = cos_inner;
			rec[13] = len;
			rec[14] = L->additive ? 1.0f : 0.0f;
			rec[15] = 0.0f;
		} else {
			rec[7] = 0.0f;
			rec[8] = 0.0f;
			rec[9] = 0.0f;
			rec[10] = 0.0f;
			rec[11] = -1.0f;
			rec[12] = -1.0f;
			rec[13] = L->radius;
			rec[14] = L->additive ? 1.0f : 0.0f;
			rec[15] = 0.0f;
		}
	}

	/* Append scene-authored lights (area fixtures / live-edit spawns) if budget remains. */
	if ( vk_scene_platform_active() ) {
		n = vk_scene_platform_append_authored_lights( base, n, (uint32_t)max_pack );
		base[0] = (float)n;
	}

	vk.forward_plus.last_packed_count = n;
	vk.forward_plus.last_upload_bytes = (uint32_t)VK_FP_HEADER_BYTES + n * (uint32_t)VK_FP_RECORD_STRIDE;
}

void vk_forward_plus_upload_refdef( void )
{
	VkBufferMemoryBarrier b[2];
	VkBufferCopy region;

	if ( !r_forwardPlus || !r_forwardPlus->integer ) {
		return;
	}
	vk_spine_pass_begin( VK_SPINE_PASS_LIGHT_PACK );
	vk_forward_plus_ensure_runtime();
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		vk_spine_pass_end( VK_SPINE_PASS_LIGHT_PACK );
		return;
	}
	if ( vk.forward_plus.buffer == VK_NULL_HANDLE || vk.forward_plus.staging == VK_NULL_HANDLE ||
		vk.forward_plus.staging_ptr == NULL || vk.forward_plus.last_upload_bytes == 0u ) {
		vk_spine_pass_end( VK_SPINE_PASS_LIGHT_PACK );
		return;
	}

	Com_Memset( b, 0, sizeof( b ) );
	b[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	b[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	b[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	b[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b[0].buffer = vk.forward_plus.buffer;
	b[0].offset = 0;
	b[0].size = VK_WHOLE_SIZE;
	b[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	b[1].srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
	b[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	b[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b[1].buffer = vk.forward_plus.staging;
	b[1].offset = 0;
	b[1].size = VK_WHOLE_SIZE;
	qvkCmdPipelineBarrier( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_HOST_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 2, b, 0, NULL );

	region.srcOffset = 0;
	region.dstOffset = 0;
	region.size = (VkDeviceSize)vk.forward_plus.last_upload_bytes;
	qvkCmdCopyBuffer( vk.cmd->command_buffer, vk.forward_plus.staging, vk.forward_plus.buffer, 1, &region );

	Com_Memset( b, 0, sizeof( b[0] ) );
	b[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	b[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	b[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	b[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b[0].buffer = vk.forward_plus.buffer;
	b[0].offset = 0;
	b[0].size = VK_WHOLE_SIZE;
	qvkCmdPipelineBarrier( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 1, b, 0, NULL );
	vk_spine_pass_end( VK_SPINE_PASS_LIGHT_PACK );
}

VkDescriptorSet vk_forward_plus_get_graphics_descriptor_set( void )
{
#ifdef USE_VK_PBR
	return vk_fp_graphics_descriptor;
#else
	return VK_NULL_HANDLE;
#endif
}

static void vk_forward_plus_dispatch_tile_cull_internal( qboolean use_depth_cull )
{
	VkBufferMemoryBarrier barriers[3];
	vk_fp_push_t push;
	float *param_f;
	uint32_t *param_u;
	float clip_from_world[16];
	float proj_vk[16];
	const float *view;
	const float *proj_gl;
	VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	vkHizPyramidSampleInfo_t hiz_info;

	if ( !r_forwardPlus || !r_forwardPlus->integer ) {
		return;
	}
	vk_forward_plus_ensure_runtime();
	if ( vk.forward_plus.tile_pipeline == VK_NULL_HANDLE || vk.forward_plus.descriptor == VK_NULL_HANDLE ) {
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}
	if ( vk.forward_plus.param_mapped == NULL || vk.forward_plus.tile_buffer == VK_NULL_HANDLE ) {
		return;
	}

	if ( use_depth_cull && vk.depth_image != VK_NULL_HANDLE ) {
		depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
		if ( glConfig.stencilBits > 0 ) {
			depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	}

	view = backEnd.viewParms.world.modelViewMatrix;
	proj_gl = backEnd.useFirstPersonProjection ? backEnd.firstPersonProjectionMatrix : backEnd.viewParms.projectionMatrix;
	vk_get_projection_matrix_vk( proj_gl, proj_vk );
	myGlMultMatrix( view, proj_vk, clip_from_world );

	param_f = (float *)vk.forward_plus.param_mapped;
	param_u = (uint32_t *)vk.forward_plus.param_mapped;
	Com_Memcpy( param_f, clip_from_world, sizeof( clip_from_world ) );
	param_u[16] = vk.forward_plus.tiles_x;
	param_u[17] = vk.forward_plus.tiles_y;
	param_u[18] = vk_get_render_target_width();
	param_u[19] = vk_get_render_target_height();
	param_f[20] = backEnd.refdef.vieworg[0];
	param_f[21] = backEnd.refdef.vieworg[1];
	param_f[22] = backEnd.refdef.vieworg[2];
	param_f[23] = 0.0f;
	/* clusterMeta: z_slices, z_mode, compactLists, totalClusters */
	param_u[24] = vk.forward_plus.z_slices > 0u ? vk.forward_plus.z_slices : 1u;
	param_u[25] = ( r_forwardPlusZSliceMode && r_forwardPlusZSliceMode->integer ) ? 1u : 0u;
	param_u[26] = vk.forward_plus.compact_lists ? 1u : 0u;
	param_u[27] = vk.forward_plus.tile_capacity_tiles;
	/* clusterZRange: near, far, zScale, zBias */
	{
		float zn = ( r_znear && r_znear->value > 0.0f ) ? r_znear->value : 4.0f;
		float zf = backEnd.viewParms.zFar;
		float clusterFar = ( r_clusterZFar && r_clusterZFar->value > 0.0f ) ? r_clusterZFar->value : 4096.0f;
		float zScale = 0.0f, zBias = 0.0f;
		if ( zn < 1e-3f ) {
			zn = 4.0f;
		}
		if ( zf <= zn + 1e-3f ) {
			zf = zn + 4000.0f;
		}
		if ( clusterFar < zf ) {
			zf = clusterFar;
		}
		if ( zf <= zn + 1e-3f ) {
			zf = zn + 1.0f;
		}
		Cluster_DeriveLogZScaleBias( zn, zf, param_u[24], &zScale, &zBias );
		vk.forward_plus.cluster_z_near = zn;
		vk.forward_plus.cluster_z_far = zf;
		vk.forward_plus.z_scale = zScale;
		vk.forward_plus.z_bias = zBias;
		param_f[28] = zn;
		param_f[29] = zf;
		param_f[30] = zScale;
		param_f[31] = zBias;
	}
	/* Phase 2.3.2: camera forward for certified positive view-depth (Q3 axis[0]). */
	{
		const float *fwd = backEnd.viewParms.or.axis[0];
		param_f[32] = fwd[0];
		param_f[33] = fwd[1];
		param_f[34] = fwd[2];
		param_f[35] = 1.0f;
	}

	/* Compact: reset atomic index cursor + overflow counter before ClusterFill. */
	if ( vk.forward_plus.compact_lists && vk_fp_tile_mapped != NULL ) {
		uint32_t *meta = (uint32_t *)vk_fp_tile_mapped;
		meta[0] = 0u;
		meta[1] = 0u;
		meta[2] = VK_CLUSTER_FLAG_COMPACT_LISTS |
			( ( r_forwardPlusZSliceMode && r_forwardPlusZSliceMode->integer ) ? VK_CLUSTER_FLAG_LOG_Z : 0u );
		meta[3] = vk.forward_plus.cluster_list_generation;
	}

	Com_Memset( barriers, 0, sizeof( barriers ) );
	barriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barriers[0].buffer = vk.forward_plus.buffer;
	barriers[0].offset = 0;
	barriers[0].size = VK_WHOLE_SIZE;

	barriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barriers[1].srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
	barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barriers[1].buffer = vk.forward_plus.param_buffer;
	barriers[1].offset = 0;
	barriers[1].size = VK_WHOLE_SIZE;

	barriers[2].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	/* The compact cursor/header is reset by the host immediately before the
	 * cull dispatch. Make that write visible to the shader before atomics run;
	 * srcAccess=0 allowed the cursor to accumulate across dispatches/frames. */
	barriers[2].srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
	barriers[2].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barriers[2].buffer = vk.forward_plus.tile_buffer;
	barriers[2].offset = 0;
	barriers[2].size = VK_WHOLE_SIZE;

	qvkCmdPipelineBarrier( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, NULL, 3, barriers, 0, NULL );

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.forward_plus.tile_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.forward_plus.pipeline_layout, 0, 1, &vk.forward_plus.descriptor, 0, NULL );

	push.tile_grid[0] = vk.forward_plus.tiles_x;
	push.tile_grid[1] = vk.forward_plus.tiles_y;
	push.z_slices = vk.forward_plus.z_slices > 0u ? vk.forward_plus.z_slices : 1u;
	push.total_tiles = vk.forward_plus.tile_capacity_tiles;
	if ( push.total_tiles == 0u ) {
		push.total_tiles = vk.forward_plus.tiles_x * vk.forward_plus.tiles_y * push.z_slices;
	}
	push.num_lights = vk.forward_plus.last_packed_count;
	if ( vk.forward_plus.compact_lists ) {
		push.max_per_tile = vk_fp_compact_max_per_cluster();
		vk.forward_plus.max_per_tile = push.max_per_tile;
	} else {
		push.max_per_tile = vk.forward_plus.max_per_tile;
	}
	push.luminance_sort = ( r_forwardPlusLuminanceSort && r_forwardPlusLuminanceSort->integer ) ? 1u : 0u;
	push.distance_sort = ( r_forwardPlusDistanceSort && r_forwardPlusDistanceSort->integer ) ? 1u : 0u;
	push.overflow_policy = ( r_clusterOverflowPolicy ) ? (uint32_t)r_clusterOverflowPolicy->integer : 2u;
	if ( push.overflow_policy > 2u ) {
		push.overflow_policy = 2u;
	}
	/* Policy 2 = importance (luminance); policy 1 = stable light index order (no sort). */
	if ( vk.forward_plus.compact_lists && push.overflow_policy == 2u ) {
		push.luminance_sort = 1u;
		push.distance_sort = 0u;
	} else if ( vk.forward_plus.compact_lists && push.overflow_policy == 1u ) {
		push.luminance_sort = 0u;
		push.distance_sort = 0u;
	}
	if ( push.distance_sort && push.luminance_sort ) {
		push.luminance_sort = 0u;
	}
	push.depth_cull = use_depth_cull ? 1u : 0u;
	push.hi_z = ( use_depth_cull && r_forwardPlusHiZ && r_forwardPlusHiZ->integer ) ? 1u : 0u;
	push.hiz_pyramid = ( push.depth_cull &&
		r_forwardPlusHiZPyramid && r_forwardPlusHiZPyramid->integer &&
		vk_hiz_get_pyramid_sample_info( &hiz_info ) ) ? 1u : 0u;
	push.hiz_levels = push.hiz_pyramid ? hiz_info.levels : 1u;
	push.z_slice_mode = ( r_forwardPlusZSliceMode && r_forwardPlusZSliceMode->integer ) ? 1u : 0u;
	push.compact_lists = vk.forward_plus.compact_lists ? 1u : 0u;
	push.index_capacity = vk.forward_plus.index_capacity;
	push.force_overflow = ( r_clusterForceOverflow && r_clusterForceOverflow->integer ) ? 1u : 0u;
	{
		float zn = vk.forward_plus.cluster_z_near;
		float zf = vk.forward_plus.cluster_z_far;
		if ( zn < 1e-3f ) {
			zn = ( r_znear && r_znear->value > 0.0f ) ? r_znear->value : 4.0f;
		}
		if ( zf <= zn + 1e-3f ) {
			zf = zn + 4000.0f;
		}
		push.z_near = zn;
		push.z_far = zf;
	}

	if ( push.distance_sort ) {
		static qboolean distance_sort_logged;
		if ( !distance_sort_logged ) {
			ri.Printf( PRINT_ALL, "[VK][Forward+] r_forwardPlusDistanceSort=1 (overloaded tiles prefer nearest lights)\n" );
			distance_sort_logged = qtrue;
		}
	}
	if ( push.depth_cull ) {
		static qboolean depth_cull_logged;
		if ( !depth_cull_logged ) {
			ri.Printf( PRINT_ALL,
				"[VK][Forward+] r_forwardPlusDepthCull=1 (depth prepass + tile cull; lightVolumeDepthCull)%s\n",
				push.hiz_pyramid ? "; vk_hiz pyramid sampling enabled" :
					( push.hi_z ? "; r_forwardPlusHiZ=1 expanded same-frame probes" : "" ) );
			depth_cull_logged = qtrue;
		}
	}
	if ( push.z_slices > 1u ) {
		static qboolean z_slice_logged;
		if ( !z_slice_logged ) {
			ri.Printf( PRINT_ALL,
				"[VK][Forward+] Z-clustered light grid: %u slices (%s), near=%.1f far=%.1f\n",
				(unsigned)push.z_slices,
				push.z_slice_mode ? "log" : "linear",
				push.z_near, push.z_far );
			z_slice_logged = qtrue;
		}
	}

	vk_forward_plus_update_depth_descriptor();
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.forward_plus.pipeline_layout,
		VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );

	qvkCmdDispatch( vk.cmd->command_buffer, ( push.total_tiles + 63u ) / 64u, 1, 1 );

	barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_HOST_READ_BIT;
	barriers[0].buffer = vk.forward_plus.tile_buffer;
	qvkCmdPipelineBarrier( vk.cmd->command_buffer,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_HOST_BIT,
		0, 0, NULL, 1, barriers, 0, NULL );

	if ( use_depth_cull && vk.depth_image != VK_NULL_HANDLE ) {
		record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT );
	}
}

void vk_forward_plus_dispatch_tile_cull( void )
{
	qboolean resume_main;

	resume_main = ( vk.inRenderPass && vk.renderPassIndex == RENDER_PASS_MAIN ) ? qtrue : qfalse;
	if ( vk.inRenderPass ) {
		vk_end_render_pass();
	}
	vk_spine_pass_begin( VK_SPINE_PASS_TILE_CONSTRUCT );

	if ( r_clusterForceBuildFailure && r_clusterForceBuildFailure->integer ) {
		if ( !vk.forward_plus.fallback_legacy ) {
			ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
				"[VK][cluster] Cluster build failed — Fallback path selected (legacy fixed-slot 2D)\n" S_COLOR_WHITE );
			vk.forward_plus.fallback_legacy = qtrue;
			vk_fp_ensure_tile_for_render_resolution();
		}
	} else if ( vk.forward_plus.fallback_legacy ) {
		ri.Printf( PRINT_ALL, "[VK][cluster] Force build failure cleared — compact path restored\n" );
		vk.forward_plus.fallback_legacy = qfalse;
		vk_fp_ensure_tile_for_render_resolution();
	}

	vk_forward_plus_dispatch_tile_cull_internal( qfalse );
	if ( vk.forward_plus.tile_buffer != VK_NULL_HANDLE ) {
		if ( !( r_clusterForceStaleGeneration && r_clusterForceStaleGeneration->integer ) ) {
			vk.forward_plus.cluster_list_generation++;
			if ( vk.forward_plus.cluster_list_generation == 0u ) {
				vk.forward_plus.cluster_list_generation = 1u;
			}
		}
	}
	vk_spine_pass_end( VK_SPINE_PASS_TILE_CONSTRUCT );
	if ( resume_main ) {
		vk_resume_current_render_pass();
	}
}

void vk_forward_plus_dispatch_tile_cull_after_opaque( void )
{
	qboolean resume_main;

	resume_main = ( vk.inRenderPass && vk.renderPassIndex == RENDER_PASS_MAIN ) ? qtrue : qfalse;
	if ( vk.inRenderPass ) {
		vk_end_render_pass();
	}
	vk_spine_pass_begin( VK_SPINE_PASS_TILE_CONSTRUCT );
	vk_forward_plus_dispatch_tile_cull_internal( qtrue );
	if ( vk.forward_plus.tile_buffer != VK_NULL_HANDLE ) {
		if ( !( r_clusterForceStaleGeneration && r_clusterForceStaleGeneration->integer ) ) {
			vk.forward_plus.cluster_list_generation++;
			if ( vk.forward_plus.cluster_list_generation == 0u ) {
				vk.forward_plus.cluster_list_generation = 1u;
			}
		}
	}
	vk_spine_pass_end( VK_SPINE_PASS_TILE_CONSTRUCT );
	if ( resume_main ) {
		vk_resume_current_render_pass();
	}
}

uint32_t vk_cluster_list_generation( void )
{
	return vk.forward_plus.cluster_list_generation;
}

void vk_cluster_assert_shared_consumers( const char *consumer )
{
	static uint32_t s_last_logged_gen;
	static VkBuffer s_last_tile;
	static VkBuffer s_last_light;
	const char *who = consumer && consumer[0] ? consumer : "unknown";

	if ( vk.forward_plus.tile_buffer == VK_NULL_HANDLE || vk.forward_plus.buffer == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][cluster] assert (%s): Forward+ light/tile SSBOs missing\n" S_COLOR_WHITE, who );
		return;
	}
	if ( s_last_tile != VK_NULL_HANDLE && s_last_tile != vk.forward_plus.tile_buffer ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][cluster] assert (%s): header/tile buffer handle mismatch\n" S_COLOR_WHITE, who );
	}
	if ( s_last_light != VK_NULL_HANDLE && s_last_light != vk.forward_plus.buffer ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][cluster] assert (%s): light buffer handle mismatch\n" S_COLOR_WHITE, who );
	}
	s_last_tile = vk.forward_plus.tile_buffer;
	s_last_light = vk.forward_plus.buffer;
	if ( vk.forward_plus.cluster_list_generation != s_last_logged_gen ) {
		ri.Printf( PRINT_DEVELOPER,
			"[VK][cluster] shared consumers ok (%s): headers=%p light=%p gen=%u tiles=%ux%ux%u compact=%d idx=%u/%u overflow=%u\n",
			who, (void *)vk.forward_plus.tile_buffer, (void *)vk.forward_plus.buffer,
			vk.forward_plus.cluster_list_generation,
			vk.forward_plus.tiles_x, vk.forward_plus.tiles_y, vk.forward_plus.z_slices,
			vk.forward_plus.compact_lists ? 1 : 0,
			vk.forward_plus.last_index_used, vk.forward_plus.index_capacity,
			vk.forward_plus.last_overflow_count );
		s_last_logged_gen = vk.forward_plus.cluster_list_generation;
	}
}

static void vk_cluster_inspect_f( void )
{
	uint32_t *cells;
	uint32_t tilesX, tilesY, zSlices, clusterCount;
	uint32_t cx, cy, slice, tileId;
	uint32_t k, count, offset;
	float zn, zf, viewZ;

	if ( !vk_fp_tile_mapped || vk.forward_plus.tile_buffer == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_ALL, "cluster_inspect: no tile buffer\n" );
		return;
	}
	vk_fp_sync_cluster_stats();
	tilesX = vk.forward_plus.tiles_x;
	tilesY = vk.forward_plus.tiles_y;
	zSlices = vk.forward_plus.z_slices > 0u ? vk.forward_plus.z_slices : 1u;
	clusterCount = vk.forward_plus.tile_capacity_tiles;
	cx = tilesX / 2u;
	cy = tilesY / 2u;
	zn = vk.forward_plus.cluster_z_near;
	zf = vk.forward_plus.cluster_z_far;
	viewZ = 0.5f * ( zn + zf );
	slice = Cluster_ViewDepthToSlice( viewZ, zSlices,
		( r_forwardPlusZSliceMode && r_forwardPlusZSliceMode->integer ) ? 1u : 0u,
		zn, zf, vk.forward_plus.z_scale, vk.forward_plus.z_bias );
	{
		gpuClusterParams_t p;
		Com_Memset( &p, 0, sizeof( p ) );
		p.clusterCountX = tilesX;
		p.clusterCountY = tilesY;
		p.clusterCountZ = zSlices;
		p.tileSizeX = VK_FP_TILE_DIM;
		p.tileSizeY = VK_FP_TILE_DIM;
		p.zNear = zn;
		p.zFar = zf;
		p.zScale = vk.forward_plus.z_scale;
		p.zBias = vk.forward_plus.z_bias;
		tileId = Cluster_IndexFromPixelAndViewDepth( cx * VK_FP_TILE_DIM + 8u, cy * VK_FP_TILE_DIM + 8u,
			viewZ, &p, ( r_forwardPlusZSliceMode && r_forwardPlusZSliceMode->integer ) ? 1u : 0u );
	}
	cells = (uint32_t *)vk_fp_tile_mapped;
	ri.Printf( PRINT_ALL, "cluster_inspect: tile=(%u,%u) slice=%u id=%u compact=%d gen=%u\n",
		cx, cy, slice, tileId, vk.forward_plus.compact_lists ? 1 : 0,
		vk.forward_plus.cluster_list_generation );
	if ( vk.forward_plus.compact_lists ) {
		offset = cells[VK_FP_CLUSTER_LIST_META_UINTS + tileId * 2u];
		count = cells[VK_FP_CLUSTER_LIST_META_UINTS + tileId * 2u + 1u];
		ri.Printf( PRINT_ALL, "  header offset=%u count=%u\n", offset, count );
		for ( k = 0u; k < count && k < 32u; k++ ) {
			uint32_t base = VK_FP_CLUSTER_LIST_META_UINTS + clusterCount * 2u;
			ri.Printf( PRINT_ALL, "  light[%u]=%u\n", k, cells[base + offset + k] );
		}
	} else {
		uint32_t base = tileId * VK_FP_MAX_PER_TILE;
		for ( k = 0u; k < VK_FP_MAX_PER_TILE; k++ ) {
			if ( cells[base + k] == 0xFFFFFFFFu ) {
				break;
			}
			ri.Printf( PRINT_ALL, "  light[%u]=%u\n", k, cells[base + k] );
		}
	}
}

static void vk_hybrid_compare_status_f( void )
{
	cvar_t *warn = ri.Cvar_Get( "r_hybridCompareWarn", "0.05", CVAR_ARCHIVE_ND | CVAR_CHEAT );
	cvar_t *fail = ri.Cvar_Get( "r_hybridCompareFail", "0.25", CVAR_ARCHIVE_ND | CVAR_CHEAT );
	cvar_t *hc = ri.Cvar_Get( "r_hybridCompare", "0", CVAR_ARCHIVE_ND | CVAR_CHEAT );
	ri.Printf( PRINT_ALL, "hybrid_compare_status: mode=%d warn=%.3f fail=%.3f gen=%u compact=%d overflow=%u\n",
		hc ? hc->integer : 0,
		warn ? warn->value : 0.05f,
		fail ? fail->value : 0.25f,
		vk.forward_plus.cluster_list_generation,
		vk.forward_plus.compact_lists ? 1 : 0,
		vk.forward_plus.last_overflow_count );
}

static void vk_cluster_status_f( void )
{
	uint32_t total = vk.forward_plus.tile_capacity_tiles;
	uint32_t stored = 0u;
	uint32_t observed_max = 0u;
	uint32_t i;
	vkHizPyramidSampleInfo_t hizInfo;
	qboolean hizReady = vk_hiz_get_pyramid_sample_info( &hizInfo );
	qboolean hizDispatch = ( r_forwardPlusDepthCull && r_forwardPlusDepthCull->integer &&
		r_forwardPlusHiZPyramid && r_forwardPlusHiZPyramid->integer && hizReady ) ? qtrue : qfalse;
	vk_fp_sync_cluster_stats();
	if ( vk.forward_plus.compact_lists && vk_fp_tile_mapped != NULL ) {
		const uint32_t *cells = (const uint32_t *)vk_fp_tile_mapped;
		for ( i = 0u; i < total; ++i ) {
			const uint32_t count = cells[VK_FP_CLUSTER_LIST_META_UINTS + i * 2u + 1u];
			stored += count;
			if ( count > observed_max ) {
				observed_max = count;
			}
		}
	}
	ri.Printf( PRINT_ALL,
		"cluster_status: grid=%ux%ux%u tile=%u compact=%d fallback=%d gen=%u\n"
		"  lights=%u alloc=%u/%u stored=%u overflow=%u policy=%d zNear=%.2f zFar=%.2f zScale=%.4f\n"
		"  depthCull=%d probePad=%d pyramidCvar=%d pyramidDispatch=%d hizPyramid=%s %ux%u mips=%u layout=%u\n",
		vk.forward_plus.tiles_x, vk.forward_plus.tiles_y, vk.forward_plus.z_slices,
		VK_FP_TILE_DIM,
		vk.forward_plus.compact_lists ? 1 : 0,
		vk.forward_plus.fallback_legacy ? 1 : 0,
		vk.forward_plus.cluster_list_generation,
		vk.forward_plus.last_packed_count,
		vk.forward_plus.last_index_used, vk.forward_plus.index_capacity, stored,
		vk.forward_plus.last_overflow_count,
		r_clusterOverflowPolicy ? r_clusterOverflowPolicy->integer : 2,
		vk.forward_plus.cluster_z_near, vk.forward_plus.cluster_z_far,
		vk.forward_plus.z_scale,
		r_forwardPlusDepthCull ? r_forwardPlusDepthCull->integer : 0,
		r_forwardPlusHiZ ? r_forwardPlusHiZ->integer : 0,
		r_forwardPlusHiZPyramid ? r_forwardPlusHiZPyramid->integer : 0,
		hizDispatch ? 1 : 0,
		hizReady ? "ready" : ( vk_hiz_active() ? "not-ready" : "off" ),
		hizInfo.width, hizInfo.height, hizInfo.levels, (unsigned)hizInfo.layout );
	ri.Printf( PRINT_ALL,
		"  transparentGrid=shared_cluster_lists prepass=%s msaa=forward_native\n",
			r_clusterTransparentPrepass && r_clusterTransparentPrepass->integer
			? "requested_not_wired" : "not_wired" );
	vk_cluster_transparent_print_status();
	{
		const vkVShadowBudget_t *shadow = vk_vshadow_budget();
		const char *opaqueOwner = ( r_renderMode && r_renderMode->integer == 3 &&
			r_deferredLighting && r_deferredLighting->integer ) ? "deferred" : "forward_plus";
		const char *transparentOwner = ( r_oit && r_oit->integer ) ? "wboit" : "forward_plus";
		ri.Printf( PRINT_ALL,
			"  ownership: opaque=%s transparent=%s generation=%u shadowPages=%u/%u shadowLights=%u/%u shadowDrops=%u\n",
			opaqueOwner, transparentOwner, vk.forward_plus.cluster_list_generation,
			shadow ? shadow->pagesClaimed : 0u, shadow ? shadow->physicalPageBudget : 0u,
			shadow ? shadow->localLightsAccepted : 0u, shadow ? shadow->localLightBudget : 0u,
			shadow ? shadow->budgetDrops : 0u );
	}
	if ( total > 0u && vk.forward_plus.index_capacity > 0u ) {
		float util = (float)vk.forward_plus.last_index_used / (float)vk.forward_plus.index_capacity;
		float avg = (float)stored / (float)total;
		ri.Printf( PRINT_ALL, "  allocationUtilization=%.1f%% avgStoredOcc=%.2f maxStored=%u maxPerCluster=%u\n",
			util * 100.0f, avg, observed_max, vk.forward_plus.max_per_tile );
	}
}

static void vk_cluster_z_test_f( void )
{
	uint32_t Z = vk_fp_active_z_slices();
	float zn = ( r_znear && r_znear->value > 0.0f ) ? r_znear->value : 4.0f;
	float zf = ( r_clusterZFar && r_clusterZFar->value > 0.0f ) ? r_clusterZFar->value : 4096.0f;
	float zScale = 0.0f, zBias = 0.0f;
	uint32_t i;
	uint32_t zMode = ( r_forwardPlusZSliceMode && r_forwardPlusZSliceMode->integer ) ? 1u : 0u;

	if ( zf <= zn + 1e-3f ) {
		zf = zn + 4096.0f;
	}
	Cluster_DeriveLogZScaleBias( zn, zf, Z, &zScale, &zBias );
	ri.Printf( PRINT_ALL, "cluster_z_test: Z=%u mode=%s near=%.3f far=%.3f scale=%.6f bias=%.6f\n",
		Z, zMode ? "log2" : "linear", zn, zf, zScale, zBias );
	for ( i = 0u; i < Z; i++ ) {
		float sn = 0.0f, sf = 0.0f;
		Cluster_SliceDepthRange( i, Z, zMode, zn, zf, zScale, zBias, &sn, &sf );
		ri.Printf( PRINT_ALL, "  slice %u: near=%.3f far=%.3f thickness=%.3f ratio=%.3f\n",
			i, sn, sf, sf - sn, ( sn > 1e-3f ) ? ( sf / sn ) : 0.0f );
	}
}

void vk_cluster_register_commands( void )
{
	r_clusterCompactLists = ri.Cvar_Get( "r_clusterCompactLists", "-1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_clusterCompactLists, "-1", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_clusterCompactLists,
		"Cluster light lists: -1=auto (compact for mode 3 / Z-slices>1), 0=legacy fixed 8-slot, 1=force compact header+index. Latched." );
	r_clusterZFar = ri.Cvar_Get( "r_clusterZFar", "4096", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_clusterZFar, "16", "131072", CV_FLOAT );
	ri.Cvar_SetDescription( r_clusterZFar,
		"Cluster Z far clamp. Effective far = min(r_clusterZFar, camera_zFar), floored above zNear." );
	r_clusterMaxIndices = ri.Cvar_Get( "r_clusterMaxIndices", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_clusterMaxIndices, "0", "16777216", CV_INTEGER );
	ri.Cvar_SetDescription( r_clusterMaxIndices, "Compact cluster light-index pool capacity (uints); 0=auto-size from clusters x max lights, capped at 16M (64 MiB). Latched." );
	r_clusterMaxLightsPerCluster = ri.Cvar_Get( "r_clusterMaxLightsPerCluster", "32", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_clusterMaxLightsPerCluster, "1", "32", CV_INTEGER );
	ri.Cvar_SetDescription( r_clusterMaxLightsPerCluster, "Max lights retained per cluster (compact path)." );
	r_clusterOverflowPolicy = ri.Cvar_Get( "r_clusterOverflowPolicy", "2", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_clusterOverflowPolicy, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_clusterOverflowPolicy,
		"0=diagnostic empty+overflow meta, 1=truncate by light index, 2=importance retention (default)." );
	r_clusterForceBuildFailure = ri.Cvar_Get( "r_clusterForceBuildFailure", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_clusterForceBuildFailure, "0", "1", CV_INTEGER );
	r_clusterForceOverflow = ri.Cvar_Get( "r_clusterForceOverflow", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_clusterForceOverflow, "0", "1", CV_INTEGER );
	r_clusterForceStaleGeneration = ri.Cvar_Get( "r_clusterForceStaleGeneration", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_clusterForceStaleGeneration, "0", "1", CV_INTEGER );
	r_clusterInspect = ri.Cvar_Get( "r_clusterInspect", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_clusterInspect, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_clusterInspect, "Print cluster header/indices under crosshair once when set." );
	r_clusterTransparentPrepass = ri.Cvar_Get( "r_clusterTransparentPrepass", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_clusterTransparentPrepass, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_clusterTransparentPrepass,
		"Olsson transparent active-cluster prepass contract; 1 requests the future all-geometry mark pass. Not wired yet; latched." );

	ri.Cmd_AddCommand( "cluster_status", vk_cluster_status_f );
	ri.Cmd_AddCommand( "cluster_z_test", vk_cluster_z_test_f );
	ri.Cmd_AddCommand( "cluster_inspect", vk_cluster_inspect_f );
	ri.Cmd_AddCommand( "hybrid_compare_status", vk_hybrid_compare_status_f );
	ri.Cmd_AddCommand( "cluster_transparent_status", vk_cluster_status_f );
	ri.Printf( PRINT_ALL, "[VK][cluster] M2 compact lists ready (cluster_status, cluster_z_test, r_clusterZFar)\n" );
}

void vk_cluster_unregister_commands( void )
{
	ri.Cmd_RemoveCommand( "cluster_status" );
	ri.Cmd_RemoveCommand( "cluster_z_test" );
	ri.Cmd_RemoveCommand( "cluster_inspect" );
	ri.Cmd_RemoveCommand( "hybrid_compare_status" );
	ri.Cmd_RemoveCommand( "cluster_transparent_status" );
}

void vk_cluster_dispatch_tile_cull( void )
{
	vk_forward_plus_dispatch_tile_cull();
}

VkBuffer vk_cluster_tile_buffer( void )
{
	return vk.forward_plus.tile_buffer;
}

VkBuffer vk_cluster_light_buffer( void )
{
	return vk.forward_plus.buffer;
}

void vk_forward_plus_refresh_viewport_params( uint32_t width, uint32_t height )
{
	uint32_t *param_u;

	if ( !r_forwardPlus || !r_forwardPlus->integer ) {
		return;
	}
	if ( vk.forward_plus.param_mapped == NULL || width == 0 || height == 0 ) {
		return;
	}
	param_u = (uint32_t *)vk.forward_plus.param_mapped;
	param_u[16] = vk.forward_plus.tiles_x;
	param_u[17] = vk.forward_plus.tiles_y;
	param_u[18] = width;
	param_u[19] = height;
}
