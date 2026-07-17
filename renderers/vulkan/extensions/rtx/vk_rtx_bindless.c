/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

D2 Phase A scaffold — RTX bindless texture table + per-primitive material SSBO.
Default off (r_rtxBindless 0). Hit shaders keep SSBO albedo fallback until
descriptor indexing + AS UVs land (see docs/RTX_HIT_SHADER_UV.md).
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_rtx_bindless.h"

#ifdef USE_VULKAN_RTX

typedef struct {
	qboolean		ready;
	qboolean		indexing_supported;
	uint32_t		cap;
	uint32_t		texture_count;
	VkBuffer		prim_buffer;
	VkDeviceMemory	prim_memory;
	uint32_t		prim_capacity;
	uint32_t		world_prim_count;
	uint32_t		entity_prim_count;
	VkBuffer		dummy_ssbo;
	VkDeviceMemory	dummy_ssbo_memory;
} rtx_bindless_t;

static rtx_bindless_t s_bindless;

static uint32_t vk_rtx_bindless_find_memory_type( uint32_t typeBits, VkMemoryPropertyFlags props )
{
	return vk_find_memory_type( vk.physical_device, typeBits, props );
}

static void vk_rtx_bindless_destroy_buffer( VkBuffer *buf, VkDeviceMemory *mem )
{
	if ( buf && *buf != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, *buf, NULL );
		*buf = VK_NULL_HANDLE;
	}
	if ( mem && *mem != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, *mem, NULL );
		*mem = VK_NULL_HANDLE;
	}
}

static qboolean vk_rtx_bindless_alloc_buffer( VkDeviceSize size, VkBufferUsageFlags usage,
	VkMemoryPropertyFlags memProps, VkBuffer *outBuf, VkDeviceMemory *outMem )
{
	VkBufferCreateInfo bi;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo ai;

	Com_Memset( &bi, 0, sizeof( bi ) );
	bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bi.size = size;
	bi.usage = usage;
	if ( qvkCreateBuffer( vk.device, &bi, NULL, outBuf ) != VK_SUCCESS ) {
		return qfalse;
	}
	qvkGetBufferMemoryRequirements( vk.device, *outBuf, &req );
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = vk_rtx_bindless_find_memory_type( req.memoryTypeBits, memProps );
	if ( qvkAllocateMemory( vk.device, &ai, NULL, outMem ) != VK_SUCCESS ) {
		qvkDestroyBuffer( vk.device, *outBuf, NULL );
		*outBuf = VK_NULL_HANDLE;
		return qfalse;
	}
	if ( qvkBindBufferMemory( vk.device, *outBuf, *outMem, 0 ) != VK_SUCCESS ) {
		vk_rtx_bindless_destroy_buffer( outBuf, outMem );
		return qfalse;
	}
	return qtrue;
}

static void vk_rtx_bindless_ensure_dummy_ssbo( void )
{
	RtxPrimMaterial invalid;

	if ( s_bindless.dummy_ssbo != VK_NULL_HANDLE ) {
		return;
	}
	Com_Memset( &invalid, 0, sizeof( invalid ) );
	invalid.textureIndex = RTX_PRIM_MATERIAL_INVALID;
	if ( !vk_rtx_bindless_alloc_buffer( sizeof( invalid ),
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&s_bindless.dummy_ssbo, &s_bindless.dummy_ssbo_memory ) ) {
		return;
	}
	{
		void *ptr = NULL;
		if ( qvkMapMemory( vk.device, s_bindless.dummy_ssbo_memory, 0, sizeof( invalid ), 0, &ptr ) == VK_SUCCESS ) {
			Com_Memcpy( ptr, &invalid, sizeof( invalid ) );
			qvkUnmapMemory( vk.device, s_bindless.dummy_ssbo_memory );
		}
	}
}

void vk_rtx_bindless_init( void )
{
	VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures;
	VkPhysicalDeviceFeatures2 features2;
	uint32_t cap;

	vk_rtx_bindless_shutdown();

	cap = ( r_rtxBindlessCap && r_rtxBindlessCap->integer > 0 ) ? (uint32_t)r_rtxBindlessCap->integer : 4096u;
	if ( cap > 16384u ) {
		cap = 16384u;
	}
	if ( cap < 1u ) {
		cap = 1u;
	}
	s_bindless.cap = cap;

	Com_Memset( &indexingFeatures, 0, sizeof( indexingFeatures ) );
	indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
	Com_Memset( &features2, 0, sizeof( features2 ) );
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = &indexingFeatures;
	if ( qvkGetPhysicalDeviceFeatures2 ) {
		qvkGetPhysicalDeviceFeatures2( vk.physical_device, &features2 );
		s_bindless.indexing_supported =
			( indexingFeatures.shaderSampledImageArrayNonUniformIndexing
				&& indexingFeatures.descriptorBindingPartiallyBound
				&& indexingFeatures.runtimeDescriptorArray ) ? qtrue : qfalse;
	}

	vk_rtx_bindless_ensure_dummy_ssbo();
	s_bindless.ready = qtrue;
	s_bindless.texture_count = 0u;

	ri.Printf( PRINT_ALL,
		"[VK][RTX] bindless scaffold: r_rtxBindless=%d mode=%d cap=%u indexing=%s (D2 Phase A; hit UV sampling not enabled yet)\n",
		r_rtxBindless ? r_rtxBindless->integer : 0,
		r_rtxBindlessMode ? r_rtxBindlessMode->integer : 0,
		s_bindless.cap,
		s_bindless.indexing_supported ? "yes" : "no" );
}

void vk_rtx_bindless_shutdown( void )
{
	vk_rtx_bindless_destroy_buffer( &s_bindless.prim_buffer, &s_bindless.prim_memory );
	vk_rtx_bindless_destroy_buffer( &s_bindless.dummy_ssbo, &s_bindless.dummy_ssbo_memory );
	Com_Memset( &s_bindless, 0, sizeof( s_bindless ) );
}

qboolean vk_rtx_bindless_active( void )
{
	if ( !s_bindless.ready || !r_rtxBindless || r_rtxBindless->integer <= 0 ) {
		return qfalse;
	}
	if ( r_rtxBindlessMode && r_rtxBindlessMode->integer == 0 ) {
		return qfalse;
	}
	/* Phase A scaffold: master latch alone is not enough until indexing path samples. */
	return qfalse;
}

uint32_t vk_rtx_bindless_texture_count( void )
{
	return s_bindless.texture_count;
}

uint32_t vk_rtx_bindless_cap( void )
{
	return s_bindless.cap ? s_bindless.cap : 4096u;
}

int vk_rtx_bindless_mode( void )
{
	return r_rtxBindlessMode ? r_rtxBindlessMode->integer : 0;
}

qboolean vk_rtx_bindless_indexing_supported( void )
{
	return s_bindless.indexing_supported;
}

void vk_rtx_bindless_sync_prim_materials( uint32_t worldPrimCount, uint32_t entityPrimCount )
{
	uint32_t total;
	VkDeviceSize bytes;
	RtxPrimMaterial *host;
	uint32_t i;

	if ( !s_bindless.ready ) {
		return;
	}

	s_bindless.world_prim_count = worldPrimCount;
	s_bindless.entity_prim_count = entityPrimCount;
	total = worldPrimCount + entityPrimCount;
	if ( total == 0u ) {
		total = 1u;
	}

	if ( total > s_bindless.prim_capacity || s_bindless.prim_buffer == VK_NULL_HANDLE ) {
		vk_rtx_bindless_destroy_buffer( &s_bindless.prim_buffer, &s_bindless.prim_memory );
		bytes = (VkDeviceSize)total * sizeof( RtxPrimMaterial );
		if ( !vk_rtx_bindless_alloc_buffer( bytes,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&s_bindless.prim_buffer, &s_bindless.prim_memory ) ) {
			s_bindless.prim_capacity = 0u;
			return;
		}
		s_bindless.prim_capacity = total;
	}

	bytes = (VkDeviceSize)s_bindless.prim_capacity * sizeof( RtxPrimMaterial );
	if ( qvkMapMemory( vk.device, s_bindless.prim_memory, 0, bytes, 0, (void **)&host ) != VK_SUCCESS ) {
		return;
	}
	for ( i = 0u; i < s_bindless.prim_capacity; i++ ) {
		host[i].textureIndex = RTX_PRIM_MATERIAL_INVALID;
		host[i].uvSet = 0u;
		host[i].flags = 0u;
	}
	qvkUnmapMemory( vk.device, s_bindless.prim_memory );
}

void vk_rtx_bindless_bind_textures( VkDescriptorSet set, uint32_t binding )
{
	VkDescriptorImageInfo info;
	VkWriteDescriptorSet write;
	VkImageView view;

	if ( set == VK_NULL_HANDLE ) {
		return;
	}

	view = VK_NULL_HANDLE;
	if ( tr.whiteImage && tr.whiteImage->view != VK_NULL_HANDLE ) {
		view = tr.whiteImage->view;
	} else if ( tr.defaultImage && tr.defaultImage->view != VK_NULL_HANDLE ) {
		view = tr.defaultImage->view;
	}
	if ( view == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &info, 0, sizeof( info ) );
	{
		Vk_Sampler_Def sd;
		Com_Memset( &sd, 0, sizeof( sd ) );
		sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
		sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sd.noAnisotropy = qtrue;
		info.sampler = vk_find_sampler( &sd );
	}
	info.imageView = view;
	info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	Com_Memset( &write, 0, sizeof( write ) );
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = set;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &info;
	qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
}

void vk_rtx_bindless_bind_prim_material( VkDescriptorSet set, uint32_t binding )
{
	VkDescriptorBufferInfo info;
	VkWriteDescriptorSet write;
	VkBuffer buf;
	VkDeviceSize range;

	if ( set == VK_NULL_HANDLE ) {
		return;
	}

	vk_rtx_bindless_ensure_dummy_ssbo();
	buf = ( s_bindless.prim_buffer != VK_NULL_HANDLE ) ? s_bindless.prim_buffer : s_bindless.dummy_ssbo;
	if ( buf == VK_NULL_HANDLE ) {
		return;
	}
	range = ( s_bindless.prim_buffer != VK_NULL_HANDLE && s_bindless.prim_capacity > 0u )
		? ( (VkDeviceSize)s_bindless.prim_capacity * sizeof( RtxPrimMaterial ) )
		: sizeof( RtxPrimMaterial );

	Com_Memset( &info, 0, sizeof( info ) );
	info.buffer = buf;
	info.offset = 0;
	info.range = range;

	Com_Memset( &write, 0, sizeof( write ) );
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = set;
	write.dstBinding = binding;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	write.pBufferInfo = &info;
	qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
}

void vk_rtx_bindless_status_line( void )
{
	ri.Printf( PRINT_ALL,
		"[VK][RTX] bindless=textures:%u cap:%u mode:%d active:%d indexing:%d worldPrims:%u entityPrims:%u\n",
		s_bindless.texture_count,
		vk_rtx_bindless_cap(),
		vk_rtx_bindless_mode(),
		vk_rtx_bindless_active() ? 1 : 0,
		s_bindless.indexing_supported ? 1 : 0,
		s_bindless.world_prim_count,
		s_bindless.entity_prim_count );
}

#else /* !USE_VULKAN_RTX */

void vk_rtx_bindless_init( void ) {}
void vk_rtx_bindless_shutdown( void ) {}
qboolean vk_rtx_bindless_active( void ) { return qfalse; }
uint32_t vk_rtx_bindless_texture_count( void ) { return 0u; }
uint32_t vk_rtx_bindless_cap( void ) { return 0u; }
int vk_rtx_bindless_mode( void ) { return 0; }
qboolean vk_rtx_bindless_indexing_supported( void ) { return qfalse; }
void vk_rtx_bindless_sync_prim_materials( uint32_t worldPrimCount, uint32_t entityPrimCount )
{
	(void)worldPrimCount;
	(void)entityPrimCount;
}
void vk_rtx_bindless_bind_textures( VkDescriptorSet set, uint32_t binding )
{
	(void)set;
	(void)binding;
}
void vk_rtx_bindless_bind_prim_material( VkDescriptorSet set, uint32_t binding )
{
	(void)set;
	(void)binding;
}
void vk_rtx_bindless_status_line( void ) {}

#endif
