/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

D2 Phase A.1 — RTX bindless texture table + per-primitive material SSBO.
Pack emits dense textureIndex from diffuse images; hit shaders keep SSBO
albedo until descriptor indexing + AS UVs land (r_rtxBindless sampling off).
See docs/RTX_HIT_SHADER_UV.md.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_rtx_bindless.h"
#include "vk_rtx_material.h"
#include <stdlib.h>

#ifdef USE_VULKAN_RTX

typedef struct {
	qboolean		ready;
	qboolean		indexing_supported;
	uint32_t		cap;
	uint32_t		texture_count;
	uint32_t		valid_prim_count;
	VkBuffer		prim_buffer;
	VkDeviceMemory	prim_memory;
	uint32_t		prim_capacity;
	uint32_t		world_prim_count;
	uint32_t		entity_prim_count;
	uint32_t		entity_base;
	VkBuffer		dummy_ssbo;
	VkDeviceMemory	dummy_ssbo_memory;
	RtxPrimMaterial	*host_mats;
	uint32_t		host_capacity;
	image_t			**images;
	uint32_t		image_capacity;
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

static void vk_rtx_bindless_free_host( void )
{
	if ( s_bindless.host_mats ) {
		free( s_bindless.host_mats );
		s_bindless.host_mats = NULL;
	}
	if ( s_bindless.images ) {
		free( s_bindless.images );
		s_bindless.images = NULL;
	}
	s_bindless.host_capacity = 0u;
	s_bindless.image_capacity = 0u;
	s_bindless.texture_count = 0u;
	s_bindless.valid_prim_count = 0u;
}

void vk_rtx_bindless_reset_texture_table( void )
{
	s_bindless.texture_count = 0u;
}

void vk_rtx_bindless_prepare_capacity( uint32_t totalPrims )
{
	uint32_t need;
	RtxPrimMaterial *nmats;
	image_t **nimgs;

	if ( !s_bindless.ready ) {
		return;
	}
	need = totalPrims ? totalPrims : 1u;
	if ( need <= s_bindless.host_capacity && s_bindless.host_mats ) {
		return;
	}
	/* Heap (not ri.Malloc): TAG_RENDERER FreeAll during map load must not steal these. */
	nmats = (RtxPrimMaterial *)malloc( (size_t)need * sizeof( RtxPrimMaterial ) );
	if ( !nmats ) {
		return;
	}
	if ( s_bindless.host_mats && s_bindless.host_capacity > 0u ) {
		uint32_t copy = s_bindless.host_capacity;
		if ( copy > need ) {
			copy = need;
		}
		Com_Memcpy( nmats, s_bindless.host_mats, copy * sizeof( RtxPrimMaterial ) );
		free( s_bindless.host_mats );
	}
	{
		uint32_t i;
		for ( i = s_bindless.host_capacity; i < need; i++ ) {
			nmats[i].textureIndex = RTX_PRIM_MATERIAL_INVALID;
			nmats[i].uvSet = 0u;
			nmats[i].flags = 0u;
		}
	}
	s_bindless.host_mats = nmats;
	s_bindless.host_capacity = need;

	if ( s_bindless.cap > s_bindless.image_capacity || !s_bindless.images ) {
		uint32_t icap = s_bindless.cap ? s_bindless.cap : 4096u;
		nimgs = (image_t **)malloc( (size_t)icap * sizeof( image_t * ) );
		if ( !nimgs ) {
			return;
		}
		Com_Memset( nimgs, 0, (size_t)icap * sizeof( image_t * ) );
		if ( s_bindless.images && s_bindless.texture_count > 0u ) {
			uint32_t copy = s_bindless.texture_count;
			if ( copy > icap ) {
				copy = icap;
			}
			Com_Memcpy( nimgs, s_bindless.images, copy * sizeof( image_t * ) );
			s_bindless.texture_count = copy;
		}
		if ( s_bindless.images ) {
			free( s_bindless.images );
		}
		s_bindless.images = nimgs;
		s_bindless.image_capacity = icap;
	}
}

void vk_rtx_bindless_clear_prims( uint32_t begin, uint32_t count )
{
	uint32_t i, end;

	if ( !s_bindless.host_mats || count == 0u ) {
		return;
	}
	end = begin + count;
	if ( begin >= s_bindless.host_capacity ) {
		return;
	}
	if ( end > s_bindless.host_capacity ) {
		end = s_bindless.host_capacity;
	}
	for ( i = begin; i < end; i++ ) {
		s_bindless.host_mats[i].textureIndex = RTX_PRIM_MATERIAL_INVALID;
		s_bindless.host_mats[i].uvSet = 0u;
		s_bindless.host_mats[i].flags = 0u;
	}
}

void vk_rtx_bindless_set_entity_base( uint32_t worldPrimCount )
{
	s_bindless.entity_base = worldPrimCount;
}

static uint32_t vk_rtx_bindless_register_image( image_t *img )
{
	uint32_t i;

	if ( !img || !s_bindless.images || s_bindless.image_capacity == 0u ) {
		return RTX_PRIM_MATERIAL_INVALID;
	}
	for ( i = 0u; i < s_bindless.texture_count; i++ ) {
		if ( s_bindless.images[i] == img ) {
			return i;
		}
	}
	if ( s_bindless.texture_count >= s_bindless.image_capacity ) {
		return RTX_PRIM_MATERIAL_INVALID;
	}
	s_bindless.images[s_bindless.texture_count] = img;
	return s_bindless.texture_count++;
}

void vk_rtx_bindless_set_prim_from_image( uint32_t primIndex, image_t *img )
{
	uint32_t slot;

	if ( !s_bindless.ready || !s_bindless.host_mats ) {
		return;
	}
	if ( primIndex >= s_bindless.host_capacity ) {
		vk_rtx_bindless_prepare_capacity( primIndex + 1u );
		if ( !s_bindless.host_mats || primIndex >= s_bindless.host_capacity ) {
			return;
		}
	}
	if ( !img || img == tr.defaultImage || img == tr.whiteImage ) {
		s_bindless.host_mats[primIndex].textureIndex = RTX_PRIM_MATERIAL_INVALID;
		s_bindless.host_mats[primIndex].uvSet = 0u;
		s_bindless.host_mats[primIndex].flags = 0u;
		return;
	}
	slot = vk_rtx_bindless_register_image( img );
	s_bindless.host_mats[primIndex].textureIndex = slot;
	s_bindless.host_mats[primIndex].uvSet = 0u;
	s_bindless.host_mats[primIndex].flags = img->hasThumb ? RTX_PRIM_MATERIAL_FLAG_THUMB : 0u;
}

void vk_rtx_bindless_set_prim_from_shader( uint32_t primIndex, const shader_t *shader )
{
	image_t *img = NULL;

	if ( shader ) {
		img = vk_rtx_material_diffuse_image_bindless( shader );
	}
	vk_rtx_bindless_set_prim_from_image( primIndex, img );
}

void vk_rtx_bindless_set_entity_prim_from_image( uint32_t entityPrimIndex, image_t *img )
{
	vk_rtx_bindless_set_prim_from_image( s_bindless.entity_base + entityPrimIndex, img );
}

void vk_rtx_bindless_set_entity_prim_from_shader( uint32_t entityPrimIndex, const shader_t *shader )
{
	vk_rtx_bindless_set_prim_from_shader( s_bindless.entity_base + entityPrimIndex, shader );
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
	s_bindless.valid_prim_count = 0u;
	s_bindless.entity_base = 0u;

	ri.Printf( PRINT_ALL,
		"[VK][RTX] bindless Phase A.1: r_rtxBindless=%d mode=%d cap=%u indexing=%s (prim textureIndex emit; hit UV sample deferred)\n",
		r_rtxBindless ? r_rtxBindless->integer : 0,
		r_rtxBindlessMode ? r_rtxBindlessMode->integer : 0,
		s_bindless.cap,
		s_bindless.indexing_supported ? "yes" : "no" );
}

void vk_rtx_bindless_shutdown( void )
{
	vk_rtx_bindless_destroy_buffer( &s_bindless.prim_buffer, &s_bindless.prim_memory );
	vk_rtx_bindless_destroy_buffer( &s_bindless.dummy_ssbo, &s_bindless.dummy_ssbo_memory );
	vk_rtx_bindless_free_host();
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
	/* Phase A.1: indices emitted; sampling waits on AS UVs + descriptor array. */
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
	RtxPrimMaterial *mapped;
	uint32_t i, valid;

	if ( !s_bindless.ready ) {
		return;
	}

	s_bindless.world_prim_count = worldPrimCount;
	s_bindless.entity_prim_count = entityPrimCount;
	s_bindless.entity_base = worldPrimCount;
	total = worldPrimCount + entityPrimCount;
	if ( total == 0u ) {
		total = 1u;
	}

	vk_rtx_bindless_prepare_capacity( total );
	if ( !s_bindless.host_mats ) {
		return;
	}

	valid = 0u;
	for ( i = 0u; i < total && i < s_bindless.host_capacity; i++ ) {
		if ( s_bindless.host_mats[i].textureIndex != RTX_PRIM_MATERIAL_INVALID ) {
			valid++;
		}
	}
	s_bindless.valid_prim_count = valid;

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
	if ( qvkMapMemory( vk.device, s_bindless.prim_memory, 0, bytes, 0, (void **)&mapped ) != VK_SUCCESS ) {
		return;
	}
	Com_Memcpy( mapped, s_bindless.host_mats,
		( total < s_bindless.host_capacity ? total : s_bindless.host_capacity ) * sizeof( RtxPrimMaterial ) );
	for ( i = total; i < s_bindless.prim_capacity; i++ ) {
		mapped[i].textureIndex = RTX_PRIM_MATERIAL_INVALID;
		mapped[i].uvSet = 0u;
		mapped[i].flags = 0u;
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
		"[VK][RTX] bindless=textures:%u cap:%u mode:%d active:%d indexing:%d validPrims:%u worldPrims:%u entityPrims:%u\n",
		s_bindless.texture_count,
		vk_rtx_bindless_cap(),
		vk_rtx_bindless_mode(),
		vk_rtx_bindless_active() ? 1 : 0,
		s_bindless.indexing_supported ? 1 : 0,
		s_bindless.valid_prim_count,
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
void vk_rtx_bindless_reset_texture_table( void ) {}
void vk_rtx_bindless_prepare_capacity( uint32_t totalPrims ) { (void)totalPrims; }
void vk_rtx_bindless_clear_prims( uint32_t begin, uint32_t count )
{
	(void)begin;
	(void)count;
}
void vk_rtx_bindless_set_entity_base( uint32_t worldPrimCount ) { (void)worldPrimCount; }
void vk_rtx_bindless_set_prim_from_image( uint32_t primIndex, image_t *img )
{
	(void)primIndex;
	(void)img;
}
void vk_rtx_bindless_set_prim_from_shader( uint32_t primIndex, const shader_t *shader )
{
	(void)primIndex;
	(void)shader;
}
void vk_rtx_bindless_set_entity_prim_from_image( uint32_t entityPrimIndex, image_t *img )
{
	(void)entityPrimIndex;
	(void)img;
}
void vk_rtx_bindless_set_entity_prim_from_shader( uint32_t entityPrimIndex, const shader_t *shader )
{
	(void)entityPrimIndex;
	(void)shader;
}
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
