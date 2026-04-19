/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vulkan Forward+ scaffolding: GPU-visible dynamic light record buffer.
Packs lights from the active refdef each view (no tile lists or shader
consumption yet). See docs/RENDERER_2026_ARCHITECTURE_PASS.md.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_forward_plus.h"
#include "vk_util.h"

#define VK_FP_RECORD_STRIDE (sizeof(float) * 16) /* 4 x vec4 per light */
#define VK_FP_HEADER_BYTES 16u

static void vk_fp_destroy_buffer( void )
{
	if ( vk.forward_plus.buffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.forward_plus.buffer, NULL );
		vk.forward_plus.buffer = VK_NULL_HANDLE;
	}
	if ( vk.forward_plus.memory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.forward_plus.memory, NULL );
		vk.forward_plus.memory = VK_NULL_HANDLE;
	}
	vk.forward_plus.mapped = NULL;
	vk.forward_plus.capacity_bytes = 0u;
}

void vk_forward_plus_shutdown( void )
{
	vk_fp_destroy_buffer();
	vk.forward_plus.last_packed_count = 0u;
}

void vk_forward_plus_init( void )
{
	VkMemoryRequirements memreq;
	VkMemoryAllocateInfo alloc;
	VkBufferCreateInfo desc;
	uint32_t mem_bits;
	uint32_t mem_type;
	const uint32_t max_lights = (uint32_t)MAX_REAL_DLIGHTS;
	const VkDeviceSize buf_size = (VkDeviceSize)VK_FP_HEADER_BYTES + (VkDeviceSize)max_lights * (VkDeviceSize)VK_FP_RECORD_STRIDE;

	vk_fp_destroy_buffer();

	if ( !r_forwardPlus || !r_forwardPlus->integer ) {
		return;
	}

	desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.size = buf_size;
	desc.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;

	VK_CHECK( qvkCreateBuffer( vk.device, &desc, NULL, &vk.forward_plus.buffer ) );
	qvkGetBufferMemoryRequirements( vk.device, vk.forward_plus.buffer, &memreq );

	mem_bits = memreq.memoryTypeBits;
	mem_type = vk_find_memory_type( vk.physical_device, mem_bits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

	alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc.pNext = NULL;
	alloc.allocationSize = memreq.size;
	alloc.memoryTypeIndex = mem_type;

	VK_CHECK( qvkAllocateMemory( vk.device, &alloc, NULL, &vk.forward_plus.memory ) );
	VK_CHECK( qvkMapMemory( vk.device, vk.forward_plus.memory, 0, VK_WHOLE_SIZE, 0, &vk.forward_plus.mapped ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.forward_plus.buffer, vk.forward_plus.memory, 0 ) );

	vk.forward_plus.capacity_bytes = (uint32_t)memreq.size;
	Com_Memset( vk.forward_plus.mapped, 0, (size_t)memreq.size );

	SET_OBJECT_NAME( vk.forward_plus.buffer, "forward+ light records", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	SET_OBJECT_NAME( vk.forward_plus.memory, "forward+ light records memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );

	ri.Printf( PRINT_ALL, "[VK][Forward+] r_forwardPlus=1 GPU light record buffer %u bytes (max %u lights)\n",
		(unsigned)vk.forward_plus.capacity_bytes, (unsigned)max_lights );
}

void vk_forward_plus_update_for_refdef( void )
{
	float *base;
	uint32_t n;
	unsigned int i;
	uint32_t max_pack;
	const dlight_t *dl;

	if ( !r_forwardPlus || !r_forwardPlus->integer ) {
		return;
	}
	if ( vk.forward_plus.buffer == VK_NULL_HANDLE || vk.forward_plus.mapped == NULL ) {
		return;
	}

	base = (float *)vk.forward_plus.mapped;
	n = backEnd.refdef.num_dlights;
	if ( n > (uint32_t)MAX_REAL_DLIGHTS ) {
		n = (uint32_t)MAX_REAL_DLIGHTS;
	}

	max_pack = ( vk.forward_plus.capacity_bytes - VK_FP_HEADER_BYTES ) / (uint32_t)VK_FP_RECORD_STRIDE;
	if ( n > max_pack ) {
		n = max_pack;
	}

	/* Header: vec4(count, refdef_time_ms, frame_index, reserved) */
	base[0] = (float)n;
	base[1] = (float)backEnd.refdef.time;
	base[2] = (float)vk.temporal.frameIndex;
	base[3] = 0.0f;

	dl = backEnd.refdef.dlights;
	if ( !dl ) {
		n = 0u;
		base[0] = 0.0f;
	}

	for ( i = 0; i < n; i++ ) {
		const dlight_t *L = dl + i;
		float *rec = base + 4u + (uint32_t)i * (uint32_t)( VK_FP_RECORD_STRIDE / sizeof( float ) );
		vec3_t dir;
		float len;

		rec[0] = L->origin[0];
		rec[1] = L->origin[1];
		rec[2] = L->origin[2];
		rec[3] = L->radius;

		rec[4] = MAX( L->color[0], 0.0f );
		rec[5] = MAX( L->color[1], 0.0f );
		rec[6] = MAX( L->color[2], 0.0f );
		rec[7] = L->linear ? 1.0f : 0.0f;

		if ( L->linear ) {
			VectorSubtract( L->origin2, L->origin, dir );
			len = VectorNormalize( dir );
			if ( len <= 0.001f ) {
				VectorSet( dir, 0.0f, 0.0f, -1.0f );
			}
			rec[8] = dir[0];
			rec[9] = dir[1];
			rec[10] = dir[2];
			rec[11] = cosf( DEG2RAD( 35.0f ) );
			rec[12] = cosf( DEG2RAD( 20.0f ) );
			rec[13] = len;
			rec[14] = L->additive ? 1.0f : 0.0f;
			rec[15] = 0.0f;
		} else {
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

	vk.forward_plus.last_packed_count = n;
}
