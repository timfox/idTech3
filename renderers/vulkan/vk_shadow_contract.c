#include "tr_local.h"
#include "vk.h"
#include "vk_shadow_contract.h"
#include "vk_util.h"

#include <assert.h>

#ifdef USE_VULKAN

static cvar_t *r_shadowConsumerDebug;
static GpuShadowRecord s_records[VK_SHADOW_CONTRACT_MAX_RECORDS];
static uint32_t s_generation;
static uint32_t s_allocCount;
static qboolean s_cmdsRegistered;
static qboolean s_deferredConsumerNoted;
static qboolean s_forwardPlusConsumerNoted;

static VkBuffer s_ssbo;
static VkDeviceMemory s_ssboMem;
static void *s_ssboMapped;
static uint32_t s_ssboUploads;
static qboolean s_ssboLogged;
static qboolean s_oitShadowLayoutLogged;

typedef struct {
	uint32_t type;
	uint32_t textureIndex;
	uint32_t layerOrPage;
	uint32_t flags;
	float    worldToShadow[16];
	float    atlasScaleBias[4];
	float    depthBiasParams[4];
	float    filterParams[4];
	uint32_t slot;
	uint32_t cascade;
	uint32_t generation;
	uint32_t extentW;
	uint32_t extentH;
	uint32_t _pad0;
	uint32_t _pad1;
	uint32_t _pad2;
} GpuShadowGpuRecord;

/* 16 + 64 + 48 + 32 = 160 bytes — std430 vec4-friendly. */
static_assert( sizeof( GpuShadowGpuRecord ) == 160, "GpuShadowGpuRecord size" );
static_assert( sizeof( GpuShadowGpuRecord ) % 16 == 0, "GpuShadowGpuRecord align" );

static void VK_ShadowContract_DestroySSBO( void )
{
	if ( s_ssboMapped && s_ssboMem != VK_NULL_HANDLE ) {
		qvkUnmapMemory( vk.device, s_ssboMem );
		s_ssboMapped = NULL;
	}
	if ( s_ssbo != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, s_ssbo, NULL );
		s_ssbo = VK_NULL_HANDLE;
	}
	if ( s_ssboMem != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, s_ssboMem, NULL );
		s_ssboMem = VK_NULL_HANDLE;
	}
}

static qboolean VK_ShadowContract_EnsureSSBO( void )
{
	VkBufferCreateInfo bci;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo mai;
	VkDeviceSize size;

	if ( s_ssboMapped ) {
		return qtrue;
	}
	if ( vk.device == VK_NULL_HANDLE ) {
		return qfalse;
	}

	size = sizeof( GpuShadowGpuRecord ) * VK_SHADOW_CONTRACT_MAX_RECORDS;
	Com_Memset( &bci, 0, sizeof( bci ) );
	bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.size = size;
	bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if ( qvkCreateBuffer( vk.device, &bci, NULL, &s_ssbo ) != VK_SUCCESS ) {
		return qfalse;
	}
	qvkGetBufferMemoryRequirements( vk.device, s_ssbo, &req );
	Com_Memset( &mai, 0, sizeof( mai ) );
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.allocationSize = req.size;
	mai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	if ( qvkAllocateMemory( vk.device, &mai, NULL, &s_ssboMem ) != VK_SUCCESS ) {
		VK_ShadowContract_DestroySSBO();
		return qfalse;
	}
	qvkBindBufferMemory( vk.device, s_ssbo, s_ssboMem, 0 );
	if ( qvkMapMemory( vk.device, s_ssboMem, 0, size, 0, &s_ssboMapped ) != VK_SUCCESS ) {
		VK_ShadowContract_DestroySSBO();
		return qfalse;
	}
	if ( !s_ssboLogged ) {
		ri.Printf( PRINT_ALL, "[VK][shadow_contract] GpuShadowRecord SSBO ready (%u slots, %zu B)\n",
			VK_SHADOW_CONTRACT_MAX_RECORDS, (size_t)size );
		s_ssboLogged = qtrue;
	}
	return qtrue;
}

void vk_shadow_contract_register( void )
{
	if ( r_shadowConsumerDebug ) {
		return;
	}
	r_shadowConsumerDebug = ri.Cvar_Get( "r_shadowConsumerDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_shadowConsumerDebug, "0", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_shadowConsumerDebug,
		"Shadow consumer debug: 0 off, 1 consumers, 2 cascade, 3 bias/filter" );
	ri.Cvar_SetGroup( r_shadowConsumerDebug, CVG_RENDERER );
	if ( !s_cmdsRegistered ) {
		ri.Cmd_AddCommand( "shadow_status", vk_shadow_contract_status_f );
		s_cmdsRegistered = qtrue;
		ri.Printf( PRINT_ALL, "[VK][shadow_contract] ready: shadow_status, r_shadowConsumerDebug\n" );
	}
}

void vk_shadow_contract_begin_frame( void )
{
	uint32_t i;
	s_generation++;
	s_allocCount = 0;
	s_deferredConsumerNoted = qfalse;
	s_forwardPlusConsumerNoted = qfalse;
	for ( i = 0; i < VK_SHADOW_CONTRACT_MAX_RECORDS; i++ ) {
		s_records[i].allocated = qfalse;
		s_records[i].consumer[0] = '\0';
		s_records[i].generation = s_generation;
	}
}

void vk_shadow_contract_end_frame( void )
{
	vk_shadow_contract_upload_ssbo();
}

GpuShadowRecord *vk_shadow_contract_alloc( uint32_t slot, uint32_t cascade )
{
	GpuShadowRecord *r;
	if ( slot >= VK_SHADOW_CONTRACT_MAX_RECORDS ) {
		return NULL;
	}
	r = &s_records[slot];
	Com_Memset( r, 0, sizeof( *r ) );
	r->slot = slot;
	r->cascade = cascade;
	r->type = VK_SHADOW_TYPE_CSM_CASCADE;
	r->generation = s_generation;
	r->allocated = qtrue;
	r->depthBiasParams[0] = 0.001f;
	r->filterParams[0] = 1.0f;
	r->atlasScaleBias[0] = 1.0f;
	r->atlasScaleBias[1] = 1.0f;
	r->atlasScaleBias[2] = 0.0f;
	r->atlasScaleBias[3] = 0.0f;
	s_allocCount++;
	return r;
}

void vk_shadow_contract_note_consumer( uint32_t slot, const char *consumer )
{
	if ( slot >= VK_SHADOW_CONTRACT_MAX_RECORDS || !consumer ) {
		return;
	}
	/* Dedup status spam per frame, but still stamp every cascade slot. */
	if ( !Q_stricmp( consumer, "deferred" ) ) {
		s_deferredConsumerNoted = qtrue;
	} else if ( !Q_stricmp( consumer, "forward_plus" ) ) {
		s_forwardPlusConsumerNoted = qtrue;
	}
	Q_strncpyz( s_records[slot].consumer, consumer, sizeof( s_records[slot].consumer ) );
}

void vk_shadow_contract_set_transform( uint32_t slot, const float *worldToShadow16 )
{
	if ( slot >= VK_SHADOW_CONTRACT_MAX_RECORDS || !worldToShadow16 ) {
		return;
	}
	Com_Memcpy( s_records[slot].worldToShadow, worldToShadow16, sizeof( float ) * 16u );
}

void vk_shadow_contract_set_extent( uint32_t slot, uint32_t w, uint32_t h )
{
	if ( slot >= VK_SHADOW_CONTRACT_MAX_RECORDS ) {
		return;
	}
	s_records[slot].extentW = w;
	s_records[slot].extentH = h;
}

void vk_shadow_contract_set_atlas( uint32_t slot, float tileScaleX, float tileScaleY,
	float offsetX, float offsetY )
{
	if ( slot >= VK_SHADOW_CONTRACT_MAX_RECORDS ) {
		return;
	}
	s_records[slot].atlasScaleBias[0] = tileScaleX;
	s_records[slot].atlasScaleBias[1] = tileScaleY;
	s_records[slot].atlasScaleBias[2] = offsetX;
	s_records[slot].atlasScaleBias[3] = offsetY;
}

void vk_shadow_contract_set_bias_filter( uint32_t slot, float depthBias, float pcfRadius )
{
	if ( slot >= VK_SHADOW_CONTRACT_MAX_RECORDS ) {
		return;
	}
	s_records[slot].depthBiasParams[0] = depthBias;
	s_records[slot].filterParams[0] = pcfRadius;
}

uint32_t vk_shadow_contract_generation( void )
{
	return s_generation;
}

const GpuShadowRecord *vk_shadow_contract_record( uint32_t slot )
{
	if ( slot >= VK_SHADOW_CONTRACT_MAX_RECORDS ) {
		return NULL;
	}
	return &s_records[slot];
}

qboolean vk_shadow_contract_upload_ssbo( void )
{
	GpuShadowGpuRecord *dst;
	uint32_t i;

	if ( !VK_ShadowContract_EnsureSSBO() || !s_ssboMapped ) {
		return qfalse;
	}

	dst = (GpuShadowGpuRecord *)s_ssboMapped;
	Com_Memset( dst, 0, sizeof( GpuShadowGpuRecord ) * VK_SHADOW_CONTRACT_MAX_RECORDS );
	for ( i = 0; i < VK_SHADOW_CONTRACT_MAX_RECORDS; i++ ) {
		const GpuShadowRecord *src = &s_records[i];
		if ( !src->allocated ) {
			continue;
		}
		dst[i].type = src->type;
		dst[i].textureIndex = src->textureIndex;
		dst[i].layerOrPage = src->layerOrPage;
		dst[i].flags = src->flags | ( src->allocated ? 1u : 0u );
		Com_Memcpy( dst[i].worldToShadow, src->worldToShadow, sizeof( dst[i].worldToShadow ) );
		Com_Memcpy( dst[i].atlasScaleBias, src->atlasScaleBias, sizeof( dst[i].atlasScaleBias ) );
		Com_Memcpy( dst[i].depthBiasParams, src->depthBiasParams, sizeof( dst[i].depthBiasParams ) );
		Com_Memcpy( dst[i].filterParams, src->filterParams, sizeof( dst[i].filterParams ) );
		dst[i].slot = src->slot;
		dst[i].cascade = src->cascade;
		dst[i].generation = src->generation;
		dst[i].extentW = src->extentW;
		dst[i].extentH = src->extentH;
	}
	s_ssboUploads++;
	return qtrue;
}

VkBuffer vk_shadow_contract_ssbo( void )
{
	VK_ShadowContract_EnsureSSBO();
	return s_ssbo;
}

static void VK_ShadowContract_EnsureOitSetLayout( void )
{
	VkDescriptorSetLayoutBinding binds[2];
	VkDescriptorSetLayoutCreateInfo ci;

	if ( vk.set_layout_oit_shadow != VK_NULL_HANDLE ) {
		return;
	}
	Com_Memset( binds, 0, sizeof( binds ) );
	binds[0].binding = 0;
	binds[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binds[0].descriptorCount = 1;
	binds[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	binds[1].binding = 1;
	binds[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binds[1].descriptorCount = 1;
	binds[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	Com_Memset( &ci, 0, sizeof( ci ) );
	ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	ci.bindingCount = 2;
	ci.pBindings = binds;
	if ( qvkCreateDescriptorSetLayout( vk.device, &ci, NULL, &vk.set_layout_oit_shadow ) != VK_SUCCESS ) {
		vk.set_layout_oit_shadow = VK_NULL_HANDLE;
		return;
	}
	SET_OBJECT_NAME( vk.set_layout_oit_shadow, "set layout - oit shadow",
		VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT );
	if ( !s_oitShadowLayoutLogged ) {
		ri.Printf( PRINT_ALL, "[VK][shadow_contract] OIT shadow set layout ready (SSBO+sunAtlas)\n" );
		s_oitShadowLayoutLogged = qtrue;
	}
}

VkDescriptorSetLayout vk_shadow_contract_oit_set_layout( void )
{
	VK_ShadowContract_EnsureOitSetLayout();
	return vk.set_layout_oit_shadow;
}

void vk_shadow_contract_oit_update_descriptors( void )
{
	VkDescriptorSetAllocateInfo alloc;
	VkDescriptorBufferInfo buf;
	VkDescriptorImageInfo img;
	VkWriteDescriptorSet writes[2];
	VkBuffer ssbo;
	VkImageView shadow_view;
	VkSampler shadow_samp;
	VkImageLayout shadow_layout;
	Vk_Sampler_Def sd;
	qboolean haveRealShadow;

	VK_ShadowContract_EnsureOitSetLayout();
	if ( vk.set_layout_oit_shadow == VK_NULL_HANDLE || vk.descriptor_pool == VK_NULL_HANDLE ) {
		return;
	}

	ssbo = vk_shadow_contract_ssbo();
	if ( ssbo == VK_NULL_HANDLE ) {
		return;
	}
	vk_shadow_contract_upload_ssbo();

	if ( vk.oit_shadow_descriptor == VK_NULL_HANDLE ) {
		Com_Memset( &alloc, 0, sizeof( alloc ) );
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.descriptorPool = vk.descriptor_pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = &vk.set_layout_oit_shadow;
		if ( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.oit_shadow_descriptor ) != VK_SUCCESS ) {
			vk.oit_shadow_descriptor = VK_NULL_HANDLE;
			return;
		}
		SET_OBJECT_NAME( vk.oit_shadow_descriptor, "descriptor - oit shadow",
			VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
	}

	haveRealShadow = qfalse;
	shadow_view = vk.sun_shadow_sample_view ? vk.sun_shadow_sample_view : vk.sun_shadow_view;
	shadow_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	if ( shadow_view != VK_NULL_HANDLE ) {
		haveRealShadow = qtrue;
	} else if ( tr.whiteImage && tr.whiteImage->view != VK_NULL_HANDLE ) {
		shadow_view = tr.whiteImage->view;
		shadow_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}
	if ( !shadow_view ) {
		return;
	}

	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;
	shadow_samp = ( haveRealShadow && vk.sun_shadow_sampler )
		? vk.sun_shadow_sampler : vk_find_sampler( &sd );

	Com_Memset( &buf, 0, sizeof( buf ) );
	buf.buffer = ssbo;
	buf.offset = 0;
	buf.range = VK_WHOLE_SIZE;
	Com_Memset( &img, 0, sizeof( img ) );
	img.sampler = shadow_samp;
	img.imageView = shadow_view;
	img.imageLayout = shadow_layout;

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = vk.oit_shadow_descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[0].pBufferInfo = &buf;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = vk.oit_shadow_descriptor;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].pImageInfo = &img;
	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );
}

VkDescriptorSet vk_shadow_contract_oit_descriptor( void )
{
	if ( vk.oit_shadow_descriptor == VK_NULL_HANDLE ) {
		vk_shadow_contract_oit_update_descriptors();
	}
	return vk.oit_shadow_descriptor;
}

void vk_shadow_contract_shutdown( void )
{
	/* Descriptor set freed with descriptor_pool; only clear handle. */
	vk.oit_shadow_descriptor = VK_NULL_HANDLE;
	VK_ShadowContract_DestroySSBO();
}

void vk_shadow_contract_status_f( void )
{
	uint32_t i;
	int dbg = r_shadowConsumerDebug ? r_shadowConsumerDebug->integer : 0;
	ri.Printf( PRINT_ALL, "======== Shadow Contract Status ========\n" );
	ri.Printf( PRINT_ALL, "r_shadowConsumerDebug=%d generation=%u records=%u\n",
		dbg, s_generation, s_allocCount );
	ri.Printf( PRINT_ALL, "ssbo=%s uploads=%u size=%zu B\n",
		s_ssbo != VK_NULL_HANDLE ? "yes" : "no", s_ssboUploads,
		(size_t)( sizeof( GpuShadowGpuRecord ) * VK_SHADOW_CONTRACT_MAX_RECORDS ) );
	ri.Printf( PRINT_ALL, "shared GpuShadowRecord: type/tex/layer/flags + worldToShadow + bias/filter\n" );
	for ( i = 0; i < VK_SHADOW_CONTRACT_MAX_RECORDS; i++ ) {
		const GpuShadowRecord *r = &s_records[i];
		if ( !r->allocated && dbg < 2 ) {
			continue;
		}
		ri.Printf( PRINT_ALL,
			"  slot=%u cascade=%u type=%u tex=%u gen=%u extent=%ux%u consumer=%s\n",
			r->slot, r->cascade, r->type, r->textureIndex, r->generation,
			r->extentW, r->extentH,
			r->consumer[0] ? r->consumer : "(none)" );
	}
}

#endif
