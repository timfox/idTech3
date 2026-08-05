/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Virtual texture (chocolate): dense atlas fallback or sparse VkImage residency
with CPU/GPU feedback-driven page binds. See docs/VIRTUAL_TEXTURE.md.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_vt.h"
#include "vk_sparse.h"
#include "vk_texture_image.h"
#include "vk_util.h"
#include "vk_cmd.h"
#include "../common/tr_image_loaders.h"
#include "vk_vt_feedback_spirv.inc"

#define VT_PAGE_SIZE_DEFAULT 128
#define VT_DENSE_PAGES_X     8
#define VT_DENSE_PAGES_Y     8
#define VT_DENSE_MAX_PAGES   ( VT_DENSE_PAGES_X * VT_DENSE_PAGES_Y )
#define VT_DENSE_W           ( VT_PAGE_SIZE_DEFAULT * VT_DENSE_PAGES_X )
#define VT_DENSE_H           ( VT_PAGE_SIZE_DEFAULT * VT_DENSE_PAGES_Y )
#define VT_MAX_VIRTUAL       64
#define VT_FEEDBACK_WORDS    ( ( VT_MAX_VIRTUAL * VT_MAX_VIRTUAL + 31 ) / 32 )
#define VT_MAX_UV_SAMPLES    256

typedef struct {
	float u;
	float v;
} vtUvSample_t;

typedef struct {
	uint32_t virtualPages;
	uint32_t sampleCount;
	uint32_t wordCount;
	uint32_t _pad;
} vtFeedbackPush_t;

typedef struct {
	qboolean used;
	int      virtualId;
	int      pageX;
	int      pageY;
	int      lruTick;
	int      sparseSlot;
	char     name[MAX_QPATH];
} vtPageSlot_t;

static cvar_t *r_vt;
static cvar_t *r_vtDebug;
static cvar_t *r_vtSample;
static cvar_t *r_vtSparse;
static cvar_t *r_vtFeedback;
static cvar_t *r_vtVirtualPages;
static cvar_t *r_vtPhysicalPages;

static image_t *s_atlas;
static qhandle_t s_atlasShader;
static vtPageSlot_t s_slots[VK_SPARSE_MAX_PAGES];
static int s_slotCapacity;
static int s_nextSlot;
static qboolean s_cmds;
static int s_pageHits;
static int s_pageMisses;
static int s_realLoads;
static int s_procLoads;
static int s_binds;
static int s_unbinds;
static int s_feedbackHits;
static int s_feedbackMisses;
static uint32_t s_zoneGatedFrames;
static int s_lruTick;
static qboolean s_useSparse;
static int s_pageSize;
static int s_virtualPages;
static int s_physicalPages;
static int s_atlasW;
static int s_atlasH;
static worldZoneResidency_t s_worldZones[REF_WORLD_ZONE_MAX];
static int s_worldZoneCount;

static qboolean VT_HasResidentTextureZone( void )
{
	int i;
	if ( s_worldZoneCount <= 0 ) return qtrue; /* legacy scenes have no snapshot */
	for ( i = 0; i < s_worldZoneCount; i++ ) {
		if ( s_worldZones[i].resident && ( s_worldZones[i].residencyMask & REF_WORLD_ZONE_RESIDENCY_TEXTURE ) ) return qtrue;
	}
	return qfalse;
}

static vkSparseImage_t s_sparseImg;
static vkSparsePool_t s_sparsePool;

/* Host-visible feedback bitfield (virtual page requests) + GPU compute stamp. */
static VkBuffer s_feedbackBuffer;
static VkDeviceMemory s_feedbackMemory;
static uint32_t *s_feedbackMapped;
static uint32_t s_feedbackBits[VT_FEEDBACK_WORDS];
static uint32_t s_feedbackWordCount;

static VkBuffer s_uvBuffer;
static VkDeviceMemory s_uvMemory;
static vtUvSample_t *s_uvMapped;
static int s_uvSampleCount;
static int s_gpuDispatches;
static int s_gpuSamples;

static VkShaderModule s_feedbackCs;
static VkDescriptorSetLayout s_feedbackSetLayout;
static VkPipelineLayout s_feedbackPipeLayout;
static VkPipeline s_feedbackPipeline;
static VkDescriptorPool s_feedbackDescPool;
static VkDescriptorSet s_feedbackDescSet;
static qboolean s_feedbackGpuReady;

static void VT_Feedback_Destroy( void );
static qboolean VT_Feedback_Create( void );
static void VT_Feedback_DestroyPipeline( void );
static qboolean VT_Feedback_CreatePipeline( void );
static void VT_Feedback_DispatchGpu( void );
static void VT_EnsureResident( int virtualId, const char *nameHint );

static int VT_VirtualFromXY( int pageX, int pageY )
{
	return pageY * s_virtualPages + pageX;
}

static void VT_XYFromVirtual( int virtualId, int *pageX, int *pageY )
{
	if ( s_virtualPages < 1 ) {
		*pageX = *pageY = 0;
		return;
	}
	*pageX = virtualId % s_virtualPages;
	*pageY = virtualId / s_virtualPages;
}

static void VT_Status_f( void )
{
	int used = 0;
	int i;

	for ( i = 0; i < s_slotCapacity; i++ ) {
		if ( s_slots[i].used ) {
			used++;
		}
	}
	ri.Printf( PRINT_ALL,
		"[VK][VT] active=%d sparse=%d atlas=%dx%d page=%d virtual=%d physical=%d slots=%d/%d\n"
		"  hits=%d misses=%d loads real=%d proc=%d binds=%d unbinds=%d\n"
		"  feedback=%d gpu=%d dispatches=%d gpuSamples=%d fbHits=%d fbMisses=%d\n"
		"  debug=%d sample=%d shader=%d gran=%ux%u zones=%d\n",
		R_VT_Active() ? 1 : 0,
		s_useSparse ? 1 : 0,
		s_atlasW, s_atlasH, s_pageSize,
		s_virtualPages, s_physicalPages,
		used, s_slotCapacity, s_pageHits, s_pageMisses,
		s_realLoads, s_procLoads, s_binds, s_unbinds,
		( r_vtFeedback && r_vtFeedback->integer ) ? 1 : 0,
		s_feedbackGpuReady ? 1 : 0,
		s_gpuDispatches, s_gpuSamples,
		s_feedbackHits, s_feedbackMisses,
		( r_vtDebug && r_vtDebug->integer ) ? 1 : 0,
		( r_vtSample && r_vtSample->integer ) ? 1 : 0,
		s_atlasShader,
		s_useSparse ? s_sparseImg.granW : (uint32_t)s_pageSize,
		s_useSparse ? s_sparseImg.granH : (uint32_t)s_pageSize, s_worldZoneCount );
	ri.Printf( PRINT_ALL, "  zone-gated feedback frames=%u\n", s_zoneGatedFrames );
	for ( i = 0; i < s_slotCapacity; i++ ) {
		if ( s_slots[i].used && s_slots[i].name[0] ) {
			ri.Printf( PRINT_ALL, "  slot %d: vid=%d xy=%d,%d %s\n",
				i, s_slots[i].virtualId, s_slots[i].pageX, s_slots[i].pageY, s_slots[i].name );
		}
	}
}

static void VT_UnbindAllSparse( void )
{
	int i;

	if ( !s_useSparse ) {
		return;
	}
	for ( i = 0; i < s_slotCapacity; i++ ) {
		if ( s_slots[i].used && s_slots[i].sparseSlot >= 0 ) {
			if ( vk_sparse_unbind_page( &s_sparsePool, s_slots[i].sparseSlot ) ) {
				s_unbinds++;
			}
			s_slots[i].sparseSlot = -1;
			s_slots[i].used = qfalse;
		}
	}
}

static void VT_Flush_f( void )
{
	VT_UnbindAllSparse();
	Com_Memset( s_slots, 0, sizeof( s_slots ) );
	s_nextSlot = 0;
	s_pageHits = s_pageMisses = 0;
	Com_Memset( s_feedbackBits, 0, sizeof( s_feedbackBits ) );
	if ( s_feedbackMapped ) {
		Com_Memset( s_feedbackMapped, 0, s_feedbackWordCount * sizeof( uint32_t ) );
	}
	ri.Printf( PRINT_ALL, "[VK][VT] page table flushed%s\n", s_useSparse ? " (sparse unbound)" : "" );
}

static int VT_FindLRUSlot( void )
{
	int i;
	int best = -1;
	int bestTick = 0x7fffffff;

	for ( i = 0; i < s_slotCapacity; i++ ) {
		if ( !s_slots[i].used ) {
			return i;
		}
		if ( s_slots[i].lruTick < bestTick ) {
			bestTick = s_slots[i].lruTick;
			best = i;
		}
	}
	return best;
}

static void VT_LoadProceduralInto( int virtualId, const char *name )
{
	byte solid[VT_PAGE_SIZE_DEFAULT * VT_PAGE_SIZE_DEFAULT * 4];
	int i;
	int page;

	for ( i = 0; i < VT_PAGE_SIZE_DEFAULT * VT_PAGE_SIZE_DEFAULT; i++ ) {
		solid[i * 4 + 0] = (byte)( ( i * 37 + virtualId * 13 ) & 255 );
		solid[i * 4 + 1] = (byte)( ( i * 17 + virtualId * 7 ) & 255 );
		solid[i * 4 + 2] = 180;
		solid[i * 4 + 3] = 255;
	}
	/* Force LoadPageRGBA to use this virtual id. */
	s_nextSlot = virtualId;
	page = R_VT_LoadPageRGBA( solid, VT_PAGE_SIZE_DEFAULT, VT_PAGE_SIZE_DEFAULT, name ? name : "*proc" );
	(void)page;
	s_procLoads++;
}

static void VT_Load_f( void )
{
	char name[MAX_QPATH];
	const char *ext;
	byte *pic = NULL;
	int w = 0, h = 0;
	int page;

	if ( !R_VT_Active() ) {
		ri.Printf( PRINT_ALL, "[VK][VT] r_vt 0 — enable and vid_restart\n" );
		return;
	}
	if ( ri.Cmd_Argc() < 2 ) {
		ri.Printf( PRINT_ALL, "usage: vt_load <imagepath>\n" );
		return;
	}
	Q_strncpyz( name, ri.Cmd_Argv( 1 ), sizeof( name ) );
	ext = COM_GetExtension( name );

	if ( ext && ( !Q_stricmp( ext, "png" ) || !Q_stricmp( ext, "PNG" ) ) ) {
		R_LoadPNG( name, &pic, &w, &h );
	} else if ( ext && ( !Q_stricmp( ext, "tga" ) || !Q_stricmp( ext, "TGA" ) ) ) {
		R_LoadTGA( name, &pic, &w, &h );
	} else if ( ext && ( !Q_stricmp( ext, "jpg" ) || !Q_stricmp( ext, "jpeg" ) ||
		!Q_stricmp( ext, "JPG" ) || !Q_stricmp( ext, "JPEG" ) ) ) {
		R_LoadJPG( name, &pic, &w, &h );
	} else {
		R_LoadPNG( name, &pic, &w, &h );
		if ( !pic ) {
			R_LoadTGA( name, &pic, &w, &h );
		}
	}

	if ( !pic || w < 1 || h < 1 ) {
		VT_LoadProceduralInto( s_nextSlot % ( s_virtualPages * s_virtualPages ), name );
		return;
	}

	page = R_VT_LoadPageRGBA( pic, w, h, name );
	ri.Free( pic );
	s_realLoads++;
	ri.Printf( PRINT_ALL, "[VK][VT] vt_load page=%d name=%s (%dx%d)\n", page, name, w, h );
}

static void VT_Feedback_DestroyPipeline( void )
{
	s_feedbackGpuReady = qfalse;
	if ( s_feedbackPipeline ) {
		qvkDestroyPipeline( vk.device, s_feedbackPipeline, NULL );
		s_feedbackPipeline = VK_NULL_HANDLE;
	}
	if ( s_feedbackPipeLayout ) {
		qvkDestroyPipelineLayout( vk.device, s_feedbackPipeLayout, NULL );
		s_feedbackPipeLayout = VK_NULL_HANDLE;
	}
	if ( s_feedbackSetLayout ) {
		qvkDestroyDescriptorSetLayout( vk.device, s_feedbackSetLayout, NULL );
		s_feedbackSetLayout = VK_NULL_HANDLE;
	}
	if ( s_feedbackDescPool ) {
		qvkDestroyDescriptorPool( vk.device, s_feedbackDescPool, NULL );
		s_feedbackDescPool = VK_NULL_HANDLE;
		s_feedbackDescSet = VK_NULL_HANDLE;
	}
	if ( s_feedbackCs ) {
		qvkDestroyShaderModule( vk.device, s_feedbackCs, NULL );
		s_feedbackCs = VK_NULL_HANDLE;
	}
}

static qboolean VT_Feedback_CreatePipeline( void )
{
	VkShaderModuleCreateInfo smci;
	VkDescriptorSetLayoutBinding binds[2];
	VkDescriptorSetLayoutCreateInfo dslci;
	VkPushConstantRange pcr;
	VkPipelineLayoutCreateInfo plci;
	VkComputePipelineCreateInfo pci;
	VkPipelineShaderStageCreateInfo stage;
	VkDescriptorPoolSize poolSize;
	VkDescriptorPoolCreateInfo dpci;
	VkDescriptorSetAllocateInfo dsai;
	VkDescriptorBufferInfo binfo[2];
	VkWriteDescriptorSet writes[2];

	VT_Feedback_DestroyPipeline();
	if ( !s_feedbackBuffer || !s_uvBuffer || !vk.device ) {
		return qfalse;
	}

	Com_Memset( &smci, 0, sizeof( smci ) );
	smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	smci.codeSize = VT_FEEDBACK_CS_SPV_SIZE;
	smci.pCode = vt_feedback_cs_spv;
	if ( qvkCreateShaderModule( vk.device, &smci, NULL, &s_feedbackCs ) != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, "[VK][VT] feedback CS module create failed\n" );
		return qfalse;
	}

	Com_Memset( binds, 0, sizeof( binds ) );
	binds[0].binding = 0;
	binds[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[0].descriptorCount = 1;
	binds[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binds[1].binding = 1;
	binds[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[1].descriptorCount = 1;
	binds[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	Com_Memset( &dslci, 0, sizeof( dslci ) );
	dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dslci.bindingCount = 2;
	dslci.pBindings = binds;
	if ( qvkCreateDescriptorSetLayout( vk.device, &dslci, NULL, &s_feedbackSetLayout ) != VK_SUCCESS ) {
		VT_Feedback_DestroyPipeline();
		return qfalse;
	}

	Com_Memset( &pcr, 0, sizeof( pcr ) );
	pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pcr.offset = 0;
	pcr.size = sizeof( vtFeedbackPush_t );

	Com_Memset( &plci, 0, sizeof( plci ) );
	plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &s_feedbackSetLayout;
	plci.pushConstantRangeCount = 1;
	plci.pPushConstantRanges = &pcr;
	if ( qvkCreatePipelineLayout( vk.device, &plci, NULL, &s_feedbackPipeLayout ) != VK_SUCCESS ) {
		VT_Feedback_DestroyPipeline();
		return qfalse;
	}

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = s_feedbackCs;
	stage.pName = "main";

	Com_Memset( &pci, 0, sizeof( pci ) );
	pci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pci.stage = stage;
	pci.layout = s_feedbackPipeLayout;
	if ( qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pci, NULL, &s_feedbackPipeline ) != VK_SUCCESS ) {
		VT_Feedback_DestroyPipeline();
		return qfalse;
	}

	Com_Memset( &poolSize, 0, sizeof( poolSize ) );
	poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSize.descriptorCount = 2;
	Com_Memset( &dpci, 0, sizeof( dpci ) );
	dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	dpci.maxSets = 1;
	dpci.poolSizeCount = 1;
	dpci.pPoolSizes = &poolSize;
	if ( qvkCreateDescriptorPool( vk.device, &dpci, NULL, &s_feedbackDescPool ) != VK_SUCCESS ) {
		VT_Feedback_DestroyPipeline();
		return qfalse;
	}

	Com_Memset( &dsai, 0, sizeof( dsai ) );
	dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	dsai.descriptorPool = s_feedbackDescPool;
	dsai.descriptorSetCount = 1;
	dsai.pSetLayouts = &s_feedbackSetLayout;
	if ( qvkAllocateDescriptorSets( vk.device, &dsai, &s_feedbackDescSet ) != VK_SUCCESS ) {
		VT_Feedback_DestroyPipeline();
		return qfalse;
	}

	Com_Memset( binfo, 0, sizeof( binfo ) );
	binfo[0].buffer = s_feedbackBuffer;
	binfo[0].offset = 0;
	binfo[0].range = VK_WHOLE_SIZE;
	binfo[1].buffer = s_uvBuffer;
	binfo[1].offset = 0;
	binfo[1].range = VK_WHOLE_SIZE;

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = s_feedbackDescSet;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[0].pBufferInfo = &binfo[0];
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = s_feedbackDescSet;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[1].pBufferInfo = &binfo[1];
	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );

	s_feedbackGpuReady = qtrue;
	ri.Printf( PRINT_ALL, "[VK][VT] feedback compute pipeline ready (atomicOr page stamps)\n" );
	return qtrue;
}

static qboolean VT_Feedback_Create( void )
{
	VkBufferCreateInfo bci;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo mai;
	VkDeviceSize bytes;
	VkDeviceSize uvBytes;

	VT_Feedback_Destroy();
	s_feedbackWordCount = (uint32_t)( ( s_virtualPages * s_virtualPages + 31 ) / 32 );
	if ( s_feedbackWordCount < 1 ) {
		s_feedbackWordCount = 1;
	}
	if ( s_feedbackWordCount > VT_FEEDBACK_WORDS ) {
		s_feedbackWordCount = VT_FEEDBACK_WORDS;
	}
	bytes = s_feedbackWordCount * sizeof( uint32_t );
	uvBytes = (VkDeviceSize)VT_MAX_UV_SAMPLES * sizeof( vtUvSample_t );

	Com_Memset( &bci, 0, sizeof( bci ) );
	bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.size = bytes;
	bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if ( qvkCreateBuffer( vk.device, &bci, NULL, &s_feedbackBuffer ) != VK_SUCCESS ) {
		return qfalse;
	}
	qvkGetBufferMemoryRequirements( vk.device, s_feedbackBuffer, &req );
	Com_Memset( &mai, 0, sizeof( mai ) );
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.allocationSize = req.size;
	mai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	if ( qvkAllocateMemory( vk.device, &mai, NULL, &s_feedbackMemory ) != VK_SUCCESS ) {
		qvkDestroyBuffer( vk.device, s_feedbackBuffer, NULL );
		s_feedbackBuffer = VK_NULL_HANDLE;
		return qfalse;
	}
	VK_CHECK( qvkBindBufferMemory( vk.device, s_feedbackBuffer, s_feedbackMemory, 0 ) );
	VK_CHECK( qvkMapMemory( vk.device, s_feedbackMemory, 0, bytes, 0, (void **)&s_feedbackMapped ) );
	Com_Memset( s_feedbackMapped, 0, bytes );

	bci.size = uvBytes;
	if ( qvkCreateBuffer( vk.device, &bci, NULL, &s_uvBuffer ) != VK_SUCCESS ) {
		VT_Feedback_Destroy();
		return qfalse;
	}
	qvkGetBufferMemoryRequirements( vk.device, s_uvBuffer, &req );
	mai.allocationSize = req.size;
	mai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	if ( qvkAllocateMemory( vk.device, &mai, NULL, &s_uvMemory ) != VK_SUCCESS ) {
		VT_Feedback_Destroy();
		return qfalse;
	}
	VK_CHECK( qvkBindBufferMemory( vk.device, s_uvBuffer, s_uvMemory, 0 ) );
	VK_CHECK( qvkMapMemory( vk.device, s_uvMemory, 0, uvBytes, 0, (void **)&s_uvMapped ) );
	Com_Memset( s_uvMapped, 0, (size_t)uvBytes );

	Com_Memset( s_feedbackBits, 0, sizeof( s_feedbackBits ) );
	s_uvSampleCount = 0;
	s_gpuDispatches = 0;
	s_gpuSamples = 0;

	(void)VT_Feedback_CreatePipeline();
	return qtrue;
}

static void VT_Feedback_Destroy( void )
{
	VT_Feedback_DestroyPipeline();
	if ( s_uvMapped ) {
		qvkUnmapMemory( vk.device, s_uvMemory );
		s_uvMapped = NULL;
	}
	if ( s_uvMemory ) {
		qvkFreeMemory( vk.device, s_uvMemory, NULL );
		s_uvMemory = VK_NULL_HANDLE;
	}
	if ( s_uvBuffer ) {
		qvkDestroyBuffer( vk.device, s_uvBuffer, NULL );
		s_uvBuffer = VK_NULL_HANDLE;
	}
	if ( s_feedbackMapped ) {
		qvkUnmapMemory( vk.device, s_feedbackMemory );
		s_feedbackMapped = NULL;
	}
	if ( s_feedbackMemory ) {
		qvkFreeMemory( vk.device, s_feedbackMemory, NULL );
		s_feedbackMemory = VK_NULL_HANDLE;
	}
	if ( s_feedbackBuffer ) {
		qvkDestroyBuffer( vk.device, s_feedbackBuffer, NULL );
		s_feedbackBuffer = VK_NULL_HANDLE;
	}
	s_uvSampleCount = 0;
}

static void VT_Feedback_DispatchGpu( void )
{
	VkCommandBuffer cmd;
	VkMemoryBarrier barrier;
	vtFeedbackPush_t push;
	uint32_t groups;

	if ( !s_feedbackGpuReady || !s_uvMapped || s_uvSampleCount < 1 ) {
		return;
	}

	push.virtualPages = (uint32_t)s_virtualPages;
	push.sampleCount = (uint32_t)s_uvSampleCount;
	push.wordCount = s_feedbackWordCount;
	push._pad = 0;

	cmd = vk_begin_command_buffer();
	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s_feedbackPipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s_feedbackPipeLayout,
		0, 1, &s_feedbackDescSet, 0, NULL );
	qvkCmdPushConstants( cmd, s_feedbackPipeLayout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof( push ), &push );
	groups = ( push.sampleCount + 63u ) / 64u;
	qvkCmdDispatch( cmd, groups, 1, 1 );

	Com_Memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	qvkCmdPipelineBarrier( cmd,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_HOST_BIT,
		0, 1, &barrier, 0, NULL, 0, NULL );

	vk_end_command_buffer( cmd, "vt_feedback" );
	s_gpuDispatches++;
	s_gpuSamples += s_uvSampleCount;
}

static void VT_EnsureResident( int virtualId, const char *nameHint )
{
	int slot;
	char name[MAX_QPATH];

	if ( virtualId < 0 || virtualId >= s_virtualPages * s_virtualPages ) {
		return;
	}
	slot = R_VT_Lookup( virtualId );
	if ( slot >= 0 ) {
		s_feedbackHits++;
		return;
	}
	s_feedbackMisses++;
	if ( nameHint && nameHint[0] ) {
		Q_strncpyz( name, nameHint, sizeof( name ) );
	} else {
		Com_sprintf( name, sizeof( name ), "*vt_fb_%d", virtualId );
	}
	VT_LoadProceduralInto( virtualId, name );
}

void R_VT_Init( void )
{
	r_vt = ri.Cvar_Get( "r_vt", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_vt, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_vt,
		"Virtual texture: page atlas + CPU page table; optional sparse VkImage residency. Default 0." );
	ri.Cvar_SetGroup( r_vt, CVG_RENDERER );

	r_vtDebug = ri.Cvar_Get( "r_vtDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vtDebug, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_vtDebug, "Draw VT atlas PiP overlay when r_vt 1 (debug consumer)." );
	ri.Cvar_SetGroup( r_vtDebug, CVG_RENDERER );

	r_vtSample = ri.Cvar_Get( "r_vtSample", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vtSample, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_vtSample,
		"When r_vt 1: use VT atlas shader on bsp_stream brush-top fallback faces (demo sample consumer)." );
	ri.Cvar_SetGroup( r_vtSample, CVG_RENDERER );

	r_vtSparse = ri.Cvar_Get( "r_vtSparse", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_vtSparse, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_vtSparse,
		"When r_vt 1: prefer sparse VkImage residency if GPU supports sparseBinding + sparseResidencyImage2D." );
	ri.Cvar_SetGroup( r_vtSparse, CVG_RENDERER );

	r_vtFeedback = ri.Cvar_Get( "r_vtFeedback", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vtFeedback, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_vtFeedback,
		"When r_vt 1: GPU compute stamps page-request bits (atomicOr) from UV samples, then drains residency." );
	ri.Cvar_SetGroup( r_vtFeedback, CVG_RENDERER );

	r_vtVirtualPages = ri.Cvar_Get( "r_vtVirtualPages", "32", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_vtVirtualPages, "8", "64", CV_INTEGER );
	ri.Cvar_SetDescription( r_vtVirtualPages, "Sparse VT virtual page grid side (N×N). Latched." );
	ri.Cvar_SetGroup( r_vtVirtualPages, CVG_RENDERER );

	r_vtPhysicalPages = ri.Cvar_Get( "r_vtPhysicalPages", "64", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_vtPhysicalPages, "8", "256", CV_INTEGER );
	ri.Cvar_SetDescription( r_vtPhysicalPages, "Max simultaneously bound sparse VT pages (LRU). Latched." );
	ri.Cvar_SetGroup( r_vtPhysicalPages, CVG_RENDERER );

	Com_Memset( s_slots, 0, sizeof( s_slots ) );
	s_nextSlot = 0;
	s_atlas = NULL;
	s_atlasShader = 0;
	s_pageHits = s_pageMisses = 0;
	s_realLoads = s_procLoads = 0;
	s_binds = s_unbinds = 0;
	s_feedbackHits = s_feedbackMisses = 0;
	s_zoneGatedFrames = 0;
	s_lruTick = 1;
	s_useSparse = qfalse;
	s_pageSize = VT_PAGE_SIZE_DEFAULT;
	s_virtualPages = VT_DENSE_PAGES_X;
	s_physicalPages = VT_DENSE_MAX_PAGES;
	s_slotCapacity = VT_DENSE_MAX_PAGES;
	s_atlasW = VT_DENSE_W;
	s_atlasH = VT_DENSE_H;
	Com_Memset( &s_sparseImg, 0, sizeof( s_sparseImg ) );
	Com_Memset( &s_sparsePool, 0, sizeof( s_sparsePool ) );

	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "vt_status", VT_Status_f );
		ri.Cmd_AddCommand( "vt_flush", VT_Flush_f );
		ri.Cmd_AddCommand( "vt_load", VT_Load_f );
		s_cmds = qtrue;
	}

	if ( !r_vt->integer ) {
		return;
	}

	s_useSparse = ( r_vtSparse->integer && vk_sparse_available() ) ? qtrue : qfalse;
	if ( s_useSparse ) {
		s_virtualPages = r_vtVirtualPages->integer;
		if ( s_virtualPages < 8 ) {
			s_virtualPages = 8;
		}
		if ( s_virtualPages > VT_MAX_VIRTUAL ) {
			s_virtualPages = VT_MAX_VIRTUAL;
		}
		s_physicalPages = r_vtPhysicalPages->integer;
		if ( s_physicalPages > VK_SPARSE_MAX_PAGES ) {
			s_physicalPages = VK_SPARSE_MAX_PAGES;
		}
		s_slotCapacity = s_physicalPages;
		s_atlasW = s_virtualPages * VT_PAGE_SIZE_DEFAULT;
		s_atlasH = s_virtualPages * VT_PAGE_SIZE_DEFAULT;

		if ( !vk_sparse_create_image2d( &s_sparseImg, s_atlasW, s_atlasH, VK_FORMAT_R8G8B8A8_UNORM,
				VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, "*vt_atlas" ) ) {
			s_useSparse = qfalse;
			ri.Printf( PRINT_WARNING, "[VK][VT] sparse create failed — dense fallback\n" );
		} else if ( s_sparseImg.granW != (uint32_t)VT_PAGE_SIZE_DEFAULT ||
			s_sparseImg.granH != (uint32_t)VT_PAGE_SIZE_DEFAULT ) {
			/* Require 128² granularity for page alignment with our page size. */
			ri.Printf( PRINT_WARNING, "[VK][VT] sparse granularity %ux%u != %d — dense fallback\n",
				s_sparseImg.granW, s_sparseImg.granH, VT_PAGE_SIZE_DEFAULT );
			vk_sparse_destroy_image( &s_sparseImg, NULL );
			s_useSparse = qfalse;
		} else if ( !vk_sparse_pool_init( &s_sparsePool, &s_sparseImg, s_physicalPages ) ) {
			vk_sparse_destroy_image( &s_sparseImg, NULL );
			s_useSparse = qfalse;
		} else {
			s_pageSize = (int)s_sparseImg.granW;
			s_atlas = R_CreateImageShell( "*vt_atlas", s_atlasW, s_atlasH,
				IMGFLAG_CLAMPTOEDGE | IMGFLAG_NOSCALE | IMGFLAG_SPARSE | IMGFLAG_NO_COMPRESSION,
				VK_FORMAT_R8G8B8A8_UNORM );
			if ( s_atlas ) {
				s_atlas->handle = s_sparseImg.image;
				s_atlas->view = s_sparseImg.view;
				s_atlas->descriptor = s_sparseImg.descriptor;
				s_atlas->uploadWidth = s_atlasW;
				s_atlas->uploadHeight = s_atlasH;
				s_atlasShader = RE_RegisterShaderFromImage( "*vt_atlas", LIGHTMAP_2D, s_atlas, qfalse );
			}
			VT_Feedback_Create();
			ri.Printf( PRINT_ALL, "[VK][VT] sparse=1 virtual=%dx%d physical=%d page=%d gran=%ux%u%s\n",
				s_virtualPages, s_virtualPages, s_physicalPages, s_pageSize,
				s_sparseImg.granW, s_sparseImg.granH,
				s_atlasShader ? " + shader" : "" );
		}
	}

	if ( !s_useSparse ) {
		byte *blank;
		int bytes;

		s_virtualPages = VT_DENSE_PAGES_X;
		s_physicalPages = VT_DENSE_MAX_PAGES;
		s_slotCapacity = VT_DENSE_MAX_PAGES;
		s_pageSize = VT_PAGE_SIZE_DEFAULT;
		s_atlasW = VT_DENSE_W;
		s_atlasH = VT_DENSE_H;
		bytes = s_atlasW * s_atlasH * 4;
		blank = (byte *)ri.Hunk_AllocateTempMemory( bytes );
		if ( blank ) {
			Com_Memset( blank, 32, bytes );
			s_atlas = R_CreateImage( "*vt_atlas", NULL, blank, s_atlasW, s_atlasH, IMGFLAG_CLAMPTOEDGE, 0, 0 );
			ri.Hunk_FreeTempMemory( blank );
		}
		if ( s_atlas ) {
			s_atlasShader = RE_RegisterShaderFromImage( "*vt_atlas", LIGHTMAP_2D, s_atlas, qfalse );
		}
		VT_Feedback_Create();
		ri.Printf( PRINT_ALL, "[VK][VT] sparse=0 fallback=dense atlas %dx%d page %d (%d slots)%s\n",
			s_atlasW, s_atlasH, s_pageSize, s_slotCapacity,
			s_atlasShader ? " + debug shader" : "" );
	}
}

void R_VT_Shutdown( void )
{
	s_worldZoneCount = 0;
	VT_UnbindAllSparse();
	if ( s_useSparse ) {
		if ( s_atlas ) {
			s_atlas->handle = VK_NULL_HANDLE;
			s_atlas->view = VK_NULL_HANDLE;
			s_atlas->descriptor = VK_NULL_HANDLE;
		}
		vk_sparse_destroy_image( &s_sparseImg, &s_sparsePool );
	}
	VT_Feedback_Destroy();
	s_atlas = NULL;
	s_atlasShader = 0;
	s_useSparse = qfalse;
	Com_Memset( s_slots, 0, sizeof( s_slots ) );
}

void R_VT_SetWorldZoneResidency( const worldZoneResidency_t *zones, int count )
{
	if ( !zones || count <= 0 ) {
		s_worldZoneCount = 0;
		return;
	}
	count = MIN( count, REF_WORLD_ZONE_MAX );
	Com_Memcpy( s_worldZones, zones, (size_t)count * sizeof( s_worldZones[0] ) );
	s_worldZoneCount = count;
}

qboolean R_VT_Active( void )
{
	return ( r_vt && r_vt->integer && s_atlas ) ? qtrue : qfalse;
}

qboolean R_VT_IsSparse( void )
{
	return ( R_VT_Active() && s_useSparse ) ? qtrue : qfalse;
}

image_t *R_VT_AtlasImage( void )
{
	return s_atlas;
}

qhandle_t R_VT_AtlasShader( void )
{
	return s_atlasShader;
}

qboolean R_VT_WantSample( void )
{
	return ( R_VT_Active() && r_vtSample && r_vtSample->integer && s_atlasShader &&
		VT_HasResidentTextureZone() ) ? qtrue : qfalse;
}

void R_VT_DebugDraw( void )
{
	float size;

	if ( !R_VT_Active() || !r_vtDebug || !r_vtDebug->integer || !s_atlasShader ) {
		return;
	}
	size = (float)( glConfig.vidWidth > 0 ? glConfig.vidWidth : 800 ) * 0.2f;
	if ( size < 96.0f ) {
		size = 96.0f;
	}
	RE_StretchPic( 8.0f, 8.0f, size, size, 0.0f, 0.0f, 1.0f, 1.0f, s_atlasShader );
}

int R_VT_Lookup( int virtualPage )
{
	int i;
	for ( i = 0; i < s_slotCapacity; i++ ) {
		if ( s_slots[i].used && s_slots[i].virtualId == virtualPage ) {
			s_pageHits++;
			s_slots[i].lruTick = ++s_lruTick;
			return i;
		}
	}
	s_pageMisses++;
	return -1;
}

int R_VT_LoadPageRGBA( const byte *rgba, int width, int height, const char *name )
{
	int slot;
	int px, py;
	int pageX, pageY;
	int virtualId;
	byte page[VT_PAGE_SIZE_DEFAULT * VT_PAGE_SIZE_DEFAULT * 4];
	int y, x;
	int sparseSlot;

	if ( !R_VT_Active() || !rgba ) {
		return -1;
	}

	slot = VT_FindLRUSlot();
	if ( slot < 0 ) {
		return -1;
	}

	if ( s_slots[slot].used && s_useSparse && s_slots[slot].sparseSlot >= 0 ) {
		if ( vk_sparse_unbind_page( &s_sparsePool, s_slots[slot].sparseSlot ) ) {
			s_unbinds++;
		}
		s_slots[slot].sparseSlot = -1;
	}

	virtualId = s_nextSlot % ( s_virtualPages * s_virtualPages );
	s_nextSlot++;
	VT_XYFromVirtual( virtualId, &pageX, &pageY );

	/* If name encodes an explicit virtual id via prior EnsureResident path, slots keep virtualId. */
	Com_Memset( page, 0, sizeof( page ) );
	for ( y = 0; y < s_pageSize && y < height && y < VT_PAGE_SIZE_DEFAULT; y++ ) {
		for ( x = 0; x < s_pageSize && x < width && x < VT_PAGE_SIZE_DEFAULT; x++ ) {
			int dst = ( y * VT_PAGE_SIZE_DEFAULT + x ) * 4;
			int src = ( y * width + x ) * 4;
			page[dst + 0] = rgba[src + 0];
			page[dst + 1] = rgba[src + 1];
			page[dst + 2] = rgba[src + 2];
			page[dst + 3] = rgba[src + 3];
		}
	}

	if ( s_useSparse ) {
		sparseSlot = vk_sparse_bind_page( &s_sparsePool, pageX, pageY, slot );
		if ( sparseSlot < 0 ) {
			ri.Printf( PRINT_WARNING, "[VK][VT] sparse bind failed for page %d,%d\n", pageX, pageY );
			return -1;
		}
		s_binds++;
		vk_sparse_page_pixel_offset( &s_sparseImg, pageX, pageY, &px, &py );
		vk_upload_image_data( s_atlas, px, py, s_pageSize, s_pageSize, 1, page,
			VT_PAGE_SIZE_DEFAULT * VT_PAGE_SIZE_DEFAULT * 4, qtrue );
		s_slots[slot].sparseSlot = sparseSlot;
	} else {
		px = ( slot % VT_DENSE_PAGES_X ) * VT_PAGE_SIZE_DEFAULT;
		py = ( slot / VT_DENSE_PAGES_X ) * VT_PAGE_SIZE_DEFAULT;
		pageX = slot % VT_DENSE_PAGES_X;
		pageY = slot / VT_DENSE_PAGES_X;
		virtualId = slot;
		vk_upload_image_data( s_atlas, px, py, VT_PAGE_SIZE_DEFAULT, VT_PAGE_SIZE_DEFAULT, 1, page,
			VT_PAGE_SIZE_DEFAULT * VT_PAGE_SIZE_DEFAULT * 4, qtrue );
		s_slots[slot].sparseSlot = -1;
	}

	s_slots[slot].used = qtrue;
	s_slots[slot].virtualId = virtualId;
	s_slots[slot].pageX = pageX;
	s_slots[slot].pageY = pageY;
	s_slots[slot].lruTick = ++s_lruTick;
	if ( name ) {
		Q_strncpyz( s_slots[slot].name, name, sizeof( s_slots[slot].name ) );
	} else {
		s_slots[slot].name[0] = '\0';
	}
	return slot;
}

void R_VT_Feedback_BeginFrame( void )
{
	if ( !R_VT_Active() || !r_vtFeedback || !r_vtFeedback->integer ) {
		return;
	}
	if ( !VT_HasResidentTextureZone() ) {
		s_zoneGatedFrames++;
		return;
	}
	Com_Memset( s_feedbackBits, 0, sizeof( s_feedbackBits ) );
	if ( s_feedbackMapped ) {
		Com_Memset( s_feedbackMapped, 0, s_feedbackWordCount * sizeof( uint32_t ) );
	}
	s_uvSampleCount = 0;
}

void R_VT_Feedback_RequestPage( int virtualPage )
{
	uint32_t word, bit;

	if ( !R_VT_Active() || !r_vtFeedback || !r_vtFeedback->integer || !VT_HasResidentTextureZone() ) {
		return;
	}
	if ( virtualPage < 0 || virtualPage >= s_virtualPages * s_virtualPages ) {
		return;
	}
	word = (uint32_t)virtualPage >> 5;
	bit = 1u << ( virtualPage & 31 );
	if ( word < VT_FEEDBACK_WORDS ) {
		s_feedbackBits[word] |= bit;
	}
	if ( s_feedbackMapped && word < s_feedbackWordCount ) {
		s_feedbackMapped[word] |= bit;
	}
}

void R_VT_Feedback_RequestUV( float u, float v )
{
	int pageX, pageY;

	if ( !R_VT_Active() || !r_vtFeedback || !r_vtFeedback->integer || !VT_HasResidentTextureZone() || s_virtualPages < 1 ) {
		return;
	}
	if ( u < 0.0f ) {
		u = 0.0f;
	}
	if ( v < 0.0f ) {
		v = 0.0f;
	}
	if ( u >= 1.0f ) {
		u = 0.999f;
	}
	if ( v >= 1.0f ) {
		v = 0.999f;
	}

	/* Queue for GPU compute stamp (preferred path). */
	if ( s_uvMapped && s_uvSampleCount < VT_MAX_UV_SAMPLES ) {
		s_uvMapped[s_uvSampleCount].u = u;
		s_uvMapped[s_uvSampleCount].v = v;
		s_uvSampleCount++;
	} else {
		/* CPU fallback when UV buffer full or GPU path unavailable. */
		pageX = (int)( u * (float)s_virtualPages );
		pageY = (int)( v * (float)s_virtualPages );
		R_VT_Feedback_RequestPage( VT_VirtualFromXY( pageX, pageY ) );
	}
}

void R_VT_Feedback_EndFrame( void )
{
	uint32_t w, b;
	int vid;

	if ( !R_VT_Active() || !r_vtFeedback || !r_vtFeedback->integer || !VT_HasResidentTextureZone() ) {
		return;
	}

	/* GPU path: stamp bits via compute atomicOr, then drain. */
	VT_Feedback_DispatchGpu();

	/* Merge GPU-mapped buffer into CPU bits. */
	if ( s_feedbackMapped ) {
		for ( w = 0; w < s_feedbackWordCount && w < VT_FEEDBACK_WORDS; w++ ) {
			s_feedbackBits[w] |= s_feedbackMapped[w];
		}
	}

	for ( w = 0; w < s_feedbackWordCount && w < VT_FEEDBACK_WORDS; w++ ) {
		uint32_t bits = s_feedbackBits[w];
		if ( !bits ) {
			continue;
		}
		for ( b = 0; b < 32; b++ ) {
			if ( !( bits & ( 1u << b ) ) ) {
				continue;
			}
			vid = (int)( w * 32 + b );
			if ( vid >= s_virtualPages * s_virtualPages ) {
				continue;
			}
			VT_EnsureResident( vid, NULL );
		}
	}
	s_uvSampleCount = 0;
}
