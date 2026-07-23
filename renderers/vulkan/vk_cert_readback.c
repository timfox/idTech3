/*
===========================================================================
Phase 2.6A — certification GPU readback.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_cert_readback.h"
#include "vk_oit_contract.h"
#include "vk_oit_weight_contract.h"
#include "vk_hdr_resolve_contract.h"
#include "vk_image_layout.h"
#include "vk_cmd.h"
#include "vk_util.h"

#include <stdlib.h>
#include <string.h>

#define CERT_RB_POOL 4
#define CERT_RB_MAX_RGBA ( 4096u * 4096u * 4u )

typedef struct {
	VkBuffer buffer;
	VkDeviceMemory memory;
	void *mapped;
	VkDeviceSize size;
	qboolean inUse;
} certRbSlot_t;

static qboolean s_cmds;
static certRbSlot_t s_pool[CERT_RB_POOL];
static certReadbackCapture_t s_last;
static float *s_rgbaScratch;
static uint32_t s_rgbaCapacity;
static cvar_t *r_certReadbackBlocking;

typedef struct {
	VkBuffer buffer;
	VkDeviceMemory memory;
	void *mapped;
	VkDeviceSize size;
	VkFormat format;
	uint32_t width;
	uint32_t height;
	uint32_t bpp;
	float *rgba;
	uint32_t rgbaCap;
} certSnapBuf_t;

typedef struct {
	qboolean pending;
	qboolean ready;
	uint64_t frameNumber;
	uint32_t generation;
	certSnapBuf_t buf[4]; /* fog, accum, reveal, resolved */
} certOitSnapSlot_t;

static certOitSnapSlot_t s_snap[NUM_COMMAND_BUFFERS];
static certOitSnapshot_t s_lastOit;

float vk_cert_half_to_float( uint16_t h )
{
	uint32_t sign = ( h >> 15 ) & 1u;
	uint32_t exp = ( h >> 10 ) & 0x1fu;
	uint32_t mant = h & 0x3ffu;
	uint32_t f;
	float out;

	if ( exp == 0 ) {
		if ( mant == 0 ) {
			f = sign << 31;
		} else {
			exp = 1;
			while ( ( mant & 0x400 ) == 0 ) {
				mant <<= 1;
				exp--;
			}
			mant &= 0x3ff;
			exp = 127 - 15 + exp;
			f = ( sign << 31 ) | ( exp << 23 ) | ( mant << 13 );
		}
	} else if ( exp == 31 ) {
		f = ( sign << 31 ) | 0x7f800000u | ( mant << 13 );
	} else {
		f = ( sign << 31 ) | ( ( exp + 127 - 15 ) << 23 ) | ( mant << 13 );
	}
	memcpy( &out, &f, sizeof( out ) );
	return out;
}

const char *vk_cert_readback_resource_name( certReadbackResource_t r )
{
	switch ( r ) {
	case CERT_RB_FOG_SCENE: return "fog_scene";
	case CERT_RB_SCENE_DEPTH: return "SceneDepth";
	case CERT_RB_OIT_ACCUM: return "OITAccum";
	case CERT_RB_OIT_REVEALAGE: return "OITRevealage";
	case CERT_RB_OIT_ADDITIVE: return "OITAdditive";
	case CERT_RB_RESOLVED_WBOIT: return "ResolvedWboitHDR";
	case CERT_RB_SORTED_REFERENCE: return "SortedReferenceHDR";
	case CERT_RB_BLOOM_SOURCE: return "BloomSourceHDR";
	case CERT_RB_TONEMAP_INPUT: return "ToneMapInputHDR";
	case CERT_RB_FINAL_DISPLAY: return "FinalDisplay";
	default: return "?";
	}
}

static qboolean CERT_EnsureScratch( uint32_t floats )
{
	if ( floats > CERT_RB_MAX_RGBA ) {
		return qfalse;
	}
	if ( s_rgbaCapacity >= floats && s_rgbaScratch ) {
		return qtrue;
	}
	if ( s_rgbaScratch ) {
		free( s_rgbaScratch );
		s_rgbaScratch = NULL;
		s_rgbaCapacity = 0;
	}
	s_rgbaScratch = (float *)malloc( (size_t)floats * sizeof( float ) );
	if ( !s_rgbaScratch ) {
		return qfalse;
	}
	s_rgbaCapacity = floats;
	return qtrue;
}

static qboolean CERT_PoolAlloc( VkDeviceSize size, certRbSlot_t **outSlot )
{
	int i;
	for ( i = 0; i < CERT_RB_POOL; i++ ) {
		if ( !s_pool[i].inUse && s_pool[i].size >= size && s_pool[i].buffer != VK_NULL_HANDLE ) {
			s_pool[i].inUse = qtrue;
			*outSlot = &s_pool[i];
			return qtrue;
		}
	}
	for ( i = 0; i < CERT_RB_POOL; i++ ) {
		VkBufferCreateInfo bci;
		VkMemoryRequirements req;
		VkMemoryAllocateInfo mai;
		certRbSlot_t *s = &s_pool[i];
		if ( s->inUse ) {
			continue;
		}
		if ( s->buffer != VK_NULL_HANDLE ) {
			qvkUnmapMemory( vk.device, s->memory );
			qvkDestroyBuffer( vk.device, s->buffer, NULL );
			qvkFreeMemory( vk.device, s->memory, NULL );
			Com_Memset( s, 0, sizeof( *s ) );
		}
		Com_Memset( &bci, 0, sizeof( bci ) );
		bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bci.size = size;
		bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		if ( qvkCreateBuffer( vk.device, &bci, NULL, &s->buffer ) != VK_SUCCESS ) {
			return qfalse;
		}
		qvkGetBufferMemoryRequirements( vk.device, s->buffer, &req );
		Com_Memset( &mai, 0, sizeof( mai ) );
		mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		mai.allocationSize = req.size;
		mai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		if ( qvkAllocateMemory( vk.device, &mai, NULL, &s->memory ) != VK_SUCCESS ) {
			qvkDestroyBuffer( vk.device, s->buffer, NULL );
			s->buffer = VK_NULL_HANDLE;
			return qfalse;
		}
		qvkBindBufferMemory( vk.device, s->buffer, s->memory, 0 );
		qvkMapMemory( vk.device, s->memory, 0, size, 0, &s->mapped );
		s->size = size;
		s->inUse = qtrue;
		*outSlot = s;
		return qtrue;
	}
	return qfalse;
}

static void CERT_PoolRelease( certRbSlot_t *s )
{
	if ( s ) {
		s->inUse = qfalse;
	}
}

qboolean vk_cert_readback_decode_to_rgba( VkFormat format, uint32_t width, uint32_t height,
	uint32_t rowPitchBytes, const void *src, float *dstRgba )
{
	uint32_t y, x;
	const byte *base = (const byte *)src;
	if ( !src || !dstRgba || width == 0 || height == 0 ) {
		return qfalse;
	}
	for ( y = 0; y < height; y++ ) {
		const byte *row = base + (size_t)y * rowPitchBytes;
		for ( x = 0; x < width; x++ ) {
			float *d = dstRgba + ( (size_t)y * width + x ) * 4;
			switch ( format ) {
			case VK_FORMAT_R16_SFLOAT: {
				const uint16_t *p = (const uint16_t *)( row + x * 2 );
				d[0] = vk_cert_half_to_float( p[0] );
				d[1] = d[2] = 0.0f;
				d[3] = 1.0f;
				break;
			}
			case VK_FORMAT_R16G16B16A16_SFLOAT: {
				const uint16_t *p = (const uint16_t *)( row + x * 8 );
				d[0] = vk_cert_half_to_float( p[0] );
				d[1] = vk_cert_half_to_float( p[1] );
				d[2] = vk_cert_half_to_float( p[2] );
				d[3] = vk_cert_half_to_float( p[3] );
				break;
			}
			case VK_FORMAT_R32G32B32A32_SFLOAT: {
				const float *p = (const float *)( row + x * 16 );
				d[0] = p[0]; d[1] = p[1]; d[2] = p[2]; d[3] = p[3];
				break;
			}
			case VK_FORMAT_R8G8B8A8_UNORM:
			case VK_FORMAT_B8G8R8A8_UNORM:
			case VK_FORMAT_R8G8B8A8_SRGB:
			case VK_FORMAT_B8G8R8A8_SRGB: {
				const byte *p = row + x * 4;
				if ( format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_B8G8R8A8_SRGB ) {
					d[0] = p[2] / 255.0f; d[1] = p[1] / 255.0f; d[2] = p[0] / 255.0f;
				} else {
					d[0] = p[0] / 255.0f; d[1] = p[1] / 255.0f; d[2] = p[2] / 255.0f;
				}
				d[3] = p[3] / 255.0f;
				break;
			}
			default:
				return qfalse;
			}
		}
	}
	return qtrue;
}

static qboolean CERT_ResolveImage( certReadbackResource_t resource, VkImage *outImage,
	VkFormat *outFormat, uint32_t *outW, uint32_t *outH, uint32_t *outGen )
{
	*outImage = VK_NULL_HANDLE;
	*outGen = 0;
	*outW = glConfig.vidWidth > 0 ? (uint32_t)glConfig.vidWidth : 0;
	*outH = glConfig.vidHeight > 0 ? (uint32_t)glConfig.vidHeight : 0;
	switch ( resource ) {
	case CERT_RB_FOG_SCENE:
		*outImage = vk.fog_scene_image;
		*outFormat = vk.color_format;
		*outGen = vk_hdr_resolve_fog_scene_generation();
		return ( *outImage != VK_NULL_HANDLE );
	case CERT_RB_SCENE_DEPTH:
		*outImage = vk.depth_image;
		*outFormat = vk.depth_format;
		*outGen = vk_hdr_resolve_depth_generation();
		return qfalse; /* depth copy path TBD — formats often D32; mark unsupported for RGBA decode */
	case CERT_RB_OIT_ACCUM:
		*outImage = vk.oit_accum_image;
		*outFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
		*outW = vk.oitExtentWidth ? vk.oitExtentWidth : *outW;
		*outH = vk.oitExtentHeight ? vk.oitExtentHeight : *outH;
		*outGen = vk.oitAttachmentGeneration;
		return ( *outImage != VK_NULL_HANDLE );
	case CERT_RB_OIT_REVEALAGE:
		*outImage = vk.oit_reveal_image;
		*outFormat = VK_FORMAT_R16_SFLOAT;
		*outW = vk.oitExtentWidth ? vk.oitExtentWidth : *outW;
		*outH = vk.oitExtentHeight ? vk.oitExtentHeight : *outH;
		*outGen = vk.oitAttachmentGeneration;
		return ( *outImage != VK_NULL_HANDLE );
	case CERT_RB_OIT_ADDITIVE:
		/* Additive composites into HDR color; no separate attachment yet. */
		*outImage = vk.color_image;
		*outFormat = vk.color_format;
		*outGen = vk_hdr_resolve_scene_hdr_generation();
		return ( *outImage != VK_NULL_HANDLE );
	case CERT_RB_RESOLVED_WBOIT:
	case CERT_RB_BLOOM_SOURCE:
	case CERT_RB_TONEMAP_INPUT:
		*outImage = vk.color_image;
		*outFormat = vk.color_format;
		*outGen = vk_hdr_resolve_scene_hdr_generation();
		return ( *outImage != VK_NULL_HANDLE );
	case CERT_RB_SORTED_REFERENCE:
		return qfalse; /* lab-owned; filled by CPU reference path */
	case CERT_RB_FINAL_DISPLAY:
		*outImage = vk.capture.image ? vk.capture.image : vk.color_image;
		*outFormat = vk.capture.image ? vk.capture_format : vk.color_format;
		*outGen = vk_hdr_resolve_scene_hdr_generation();
		return ( *outImage != VK_NULL_HANDLE );
	default:
		return qfalse;
	}
}

qboolean vk_cert_readback_capture( certReadbackResource_t resource, certReadbackCapture_t *out )
{
	VkImage image;
	VkFormat format = VK_FORMAT_UNDEFINED;
	uint32_t w = 0, h = 0, gen = 0;
	certRbSlot_t *slot = NULL;
	VkDeviceSize bufSize;
	uint32_t bpp;
	VkCommandBuffer cmd;
	VkBufferImageCopy region;
	const oitContract_t *oit = vk_oit_contract_wboit();
	const oitWeightContract_t *wt = vk_oit_weight_contract_get();
	const hdrResolveContract_t *hr = vk_hdr_resolve_contract_get();

	Com_Memset( &s_last, 0, sizeof( s_last ) );
	if ( out ) {
		Com_Memset( out, 0, sizeof( *out ) );
	}
	if ( vk.device_lost || !vk.device ) {
		return qfalse;
	}
	if ( !CERT_ResolveImage( resource, &image, &format, &w, &h, &gen ) || w == 0 || h == 0 ) {
		ri.Printf( PRINT_DEVELOPER, "cert_readback: resource %s unavailable\n",
			vk_cert_readback_resource_name( resource ) );
		return qfalse;
	}
	switch ( format ) {
	case VK_FORMAT_R16_SFLOAT: bpp = 2; break;
	case VK_FORMAT_R16G16B16A16_SFLOAT: bpp = 8; break;
	case VK_FORMAT_R32G32B32A32_SFLOAT: bpp = 16; break;
	case VK_FORMAT_R8G8B8A8_UNORM:
	case VK_FORMAT_B8G8R8A8_UNORM:
	case VK_FORMAT_R8G8B8A8_SRGB:
	case VK_FORMAT_B8G8R8A8_SRGB: bpp = 4; break;
	default:
		ri.Printf( PRINT_WARNING, "cert_readback: unsupported format %u for %s\n",
			(unsigned)format, vk_cert_readback_resource_name( resource ) );
		return qfalse;
	}
	bufSize = (VkDeviceSize)w * h * bpp;
	if ( !CERT_PoolAlloc( bufSize, &slot ) ) {
		return qfalse;
	}
	if ( r_certReadbackBlocking && r_certReadbackBlocking->integer && vk.cmd ) {
		qvkWaitForFences( vk.device, 1, &vk.cmd->rendering_finished_fence, VK_TRUE, (uint64_t)1e12 );
	}
	cmd = vk_begin_command_buffer();
	{
		VkImageMemoryBarrier b;
		Com_Memset( &b, 0, sizeof( b ) );
		b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		b.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
		b.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		b.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		if ( resource == CERT_RB_FOG_SCENE && vk.fog_scene_layout != VK_IMAGE_LAYOUT_UNDEFINED ) {
			b.oldLayout = vk.fog_scene_layout;
		}
		b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		b.image = image;
		b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		b.subresourceRange.levelCount = 1;
		b.subresourceRange.layerCount = 1;
		qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, NULL, 0, NULL, 1, &b );
	}
	Com_Memset( &region, 0, sizeof( region ) );
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageExtent.width = w;
	region.imageExtent.height = h;
	region.imageExtent.depth = 1;
	qvkCmdCopyImageToBuffer( cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, slot->buffer, 1, &region );
	vk_end_command_buffer( cmd, "cert_readback" );
	qvkDeviceWaitIdle( vk.device );

	if ( !CERT_EnsureScratch( w * h * 4 ) ) {
		CERT_PoolRelease( slot );
		return qfalse;
	}
	if ( !vk_cert_readback_decode_to_rgba( format, w, h, w * bpp, slot->mapped, s_rgbaScratch ) ) {
		CERT_PoolRelease( slot );
		return qfalse;
	}
	CERT_PoolRelease( slot );

	s_last.resource = resource;
	s_last.format = format;
	s_last.width = w;
	s_last.height = h;
	s_last.rowPitchBytes = w * bpp;
	s_last.frameNumber = vk.oitFrameNumber ? vk.oitFrameNumber : (uint64_t)tr.frameCount;
	s_last.generation = gen;
	s_last.pipelineStage = (uint32_t)VK_PIPELINE_STAGE_TRANSFER_BIT;
	Q_strncpyz( s_last.colorSpace, "SCENE_LINEAR_HDR", sizeof( s_last.colorSpace ) );
	s_last.preExposed = qfalse;
	s_last.oitContractHash = oit ? oit->contractHash : 0;
	s_last.weightContractHash = wt ? wt->contractHash : 0;
	s_last.resolveContractHash = hr ? hr->contractHash : 0;
	s_last.rgba = s_rgbaScratch;
	s_last.pixelCount = w * h;
	s_last.valid = qtrue;
	if ( out ) {
		*out = s_last;
	}
	ri.Printf( PRINT_ALL,
		"cert_readback_capture: %s %ux%u fmt=%u gen=%u frame=%llu\n",
		vk_cert_readback_resource_name( resource ), w, h, (unsigned)format, gen,
		(unsigned long long)s_last.frameNumber );
	return qtrue;
}

void vk_cert_readback_flush( void )
{
	int i;
	qvkDeviceWaitIdle( vk.device );
	for ( i = 0; i < CERT_RB_POOL; i++ ) {
		s_pool[i].inUse = qfalse;
	}
	ri.Printf( PRINT_ALL, "cert_readback_flush: pool released, device idle\n" );
}

const certReadbackCapture_t *vk_cert_readback_last( void )
{
	return s_last.valid ? &s_last : NULL;
}

static void CERT_ReadbackStatus_f( void )
{
	ri.Printf( PRINT_ALL,
		"cert_readback_status:\n"
		"  last=%s valid=%d %ux%u gen=%u frame=%llu space=%s\n"
		"  formats: R16_SFLOAT R16G16B16A16_SFLOAT R32G32B32A32_SFLOAT UNORM8 display\n"
		"  blocking=%d pool=%d\n"
		"  note=never compare captures across different frames/generations\n",
		s_last.valid ? vk_cert_readback_resource_name( s_last.resource ) : "(none)",
		s_last.valid ? 1 : 0, s_last.width, s_last.height, s_last.generation,
		(unsigned long long)s_last.frameNumber,
		s_last.colorSpace[0] ? s_last.colorSpace : "-",
		r_certReadbackBlocking ? r_certReadbackBlocking->integer : 1,
		CERT_RB_POOL );
}

static void CERT_ReadbackCapture_f( void )
{
	const char *name;
	int i;
	if ( ri.Cmd_Argc() < 2 ) {
		ri.Printf( PRINT_ALL, "usage: cert_readback_capture <resource>\n" );
		for ( i = 0; i < (int)CERT_RB_COUNT; i++ ) {
			ri.Printf( PRINT_ALL, "  %s\n", vk_cert_readback_resource_name( (certReadbackResource_t)i ) );
		}
		return;
	}
	name = ri.Cmd_Argv( 1 );
	for ( i = 0; i < (int)CERT_RB_COUNT; i++ ) {
		if ( !Q_stricmp( name, vk_cert_readback_resource_name( (certReadbackResource_t)i ) ) ) {
			vk_cert_readback_capture( (certReadbackResource_t)i, NULL );
			return;
		}
	}
	ri.Printf( PRINT_ALL, "unknown resource '%s'\n", name );
}

static void CERT_ReadbackFlush_f( void )
{
	vk_cert_readback_flush();
}

void vk_cert_readback_shutdown( void )
{
	int i, s;
	for ( i = 0; i < CERT_RB_POOL; i++ ) {
		if ( s_pool[i].buffer != VK_NULL_HANDLE ) {
			qvkUnmapMemory( vk.device, s_pool[i].memory );
			qvkDestroyBuffer( vk.device, s_pool[i].buffer, NULL );
			qvkFreeMemory( vk.device, s_pool[i].memory, NULL );
		}
		Com_Memset( &s_pool[i], 0, sizeof( s_pool[i] ) );
	}
	for ( s = 0; s < NUM_COMMAND_BUFFERS; s++ ) {
		for ( i = 0; i < 4; i++ ) {
			certSnapBuf_t *b = &s_snap[s].buf[i];
			if ( b->buffer != VK_NULL_HANDLE ) {
				qvkUnmapMemory( vk.device, b->memory );
				qvkDestroyBuffer( vk.device, b->buffer, NULL );
				qvkFreeMemory( vk.device, b->memory, NULL );
			}
			free( b->rgba );
			Com_Memset( b, 0, sizeof( *b ) );
		}
		s_snap[s].pending = qfalse;
		s_snap[s].ready = qfalse;
	}
	free( s_rgbaScratch );
	s_rgbaScratch = NULL;
	s_rgbaCapacity = 0;
	Com_Memset( &s_last, 0, sizeof( s_last ) );
	Com_Memset( &s_lastOit, 0, sizeof( s_lastOit ) );
}

static qboolean CERT_EnsureSnapBuf( certSnapBuf_t *b, VkDeviceSize size, uint32_t rgbaFloats )
{
	if ( b->buffer != VK_NULL_HANDLE && b->size >= size && b->mapped ) {
		if ( b->rgba && b->rgbaCap >= rgbaFloats ) {
			return qtrue;
		}
		free( b->rgba );
		b->rgba = (float *)malloc( (size_t)rgbaFloats * sizeof( float ) );
		if ( !b->rgba ) {
			return qfalse;
		}
		b->rgbaCap = rgbaFloats;
		return qtrue;
	}
	if ( b->buffer != VK_NULL_HANDLE ) {
		qvkUnmapMemory( vk.device, b->memory );
		qvkDestroyBuffer( vk.device, b->buffer, NULL );
		qvkFreeMemory( vk.device, b->memory, NULL );
		Com_Memset( b, 0, sizeof( *b ) );
	}
	{
		VkBufferCreateInfo bci;
		VkMemoryRequirements req;
		VkMemoryAllocateInfo mai;
		Com_Memset( &bci, 0, sizeof( bci ) );
		bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bci.size = size;
		bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		VK_CHECK( qvkCreateBuffer( vk.device, &bci, NULL, &b->buffer ) );
		qvkGetBufferMemoryRequirements( vk.device, b->buffer, &req );
		mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		mai.pNext = NULL;
		mai.allocationSize = req.size;
		mai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		VK_CHECK( qvkAllocateMemory( vk.device, &mai, NULL, &b->memory ) );
		VK_CHECK( qvkBindBufferMemory( vk.device, b->buffer, b->memory, 0 ) );
		VK_CHECK( qvkMapMemory( vk.device, b->memory, 0, VK_WHOLE_SIZE, 0, &b->mapped ) );
		b->size = size;
	}
	free( b->rgba );
	b->rgba = (float *)malloc( (size_t)rgbaFloats * sizeof( float ) );
	if ( !b->rgba ) {
		return qfalse;
	}
	b->rgbaCap = rgbaFloats;
	return qtrue;
}

static void CERT_BarrierColor( VkCommandBuffer cmd, VkImage image, VkImageLayout oldL, VkImageLayout newL )
{
	VkImageMemoryBarrier b;
	Com_Memset( &b, 0, sizeof( b ) );
	b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	b.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	b.dstAccessMask = ( newL == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL )
		? VK_ACCESS_TRANSFER_READ_BIT
		: ( VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_MEMORY_READ_BIT );
	b.oldLayout = oldL;
	b.newLayout = newL;
	b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b.image = image;
	b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	b.subresourceRange.levelCount = 1;
	b.subresourceRange.layerCount = 1;
	qvkCmdPipelineBarrier( cmd,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		( newL == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL )
			? VK_PIPELINE_STAGE_TRANSFER_BIT
			: VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0, 0, NULL, 0, NULL, 1, &b );
}

static qboolean CERT_RecordOne( VkCommandBuffer cmd, certSnapBuf_t *b, VkImage image, VkFormat format,
	uint32_t w, uint32_t h, VkImageLayout currentLayout )
{
	uint32_t bpp;
	VkBufferImageCopy region;
	switch ( format ) {
	case VK_FORMAT_R16_SFLOAT: bpp = 2; break;
	case VK_FORMAT_R16G16B16A16_SFLOAT: bpp = 8; break;
	case VK_FORMAT_R32G32B32A32_SFLOAT: bpp = 16; break;
	case VK_FORMAT_R8G8B8A8_UNORM:
	case VK_FORMAT_B8G8R8A8_UNORM:
	case VK_FORMAT_R8G8B8A8_SRGB:
	case VK_FORMAT_B8G8R8A8_SRGB: bpp = 4; break;
	default:
		return qfalse;
	}
	if ( !image || w == 0 || h == 0 ) {
		return qfalse;
	}
	if ( !CERT_EnsureSnapBuf( b, (VkDeviceSize)w * h * bpp, w * h * 4 ) ) {
		return qfalse;
	}
	b->format = format;
	b->width = w;
	b->height = h;
	b->bpp = bpp;
	CERT_BarrierColor( cmd, image, currentLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL );
	Com_Memset( &region, 0, sizeof( region ) );
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageExtent.width = w;
	region.imageExtent.height = h;
	region.imageExtent.depth = 1;
	qvkCmdCopyImageToBuffer( cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, b->buffer, 1, &region );
	CERT_BarrierColor( cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, currentLayout );
	return qtrue;
}

qboolean vk_cert_readback_record_oit_snapshot( VkCommandBuffer cmd, int cmdIndex )
{
	certOitSnapSlot_t *slot;
	uint32_t w, h;
	VkImageLayout fogLayout;
	if ( !cmd || cmdIndex < 0 || cmdIndex >= NUM_COMMAND_BUFFERS || vk.device_lost ) {
		return qfalse;
	}
	if ( !vk.fog_scene_image || !vk.oit_accum_image || !vk.oit_reveal_image || !vk.color_image ) {
		return qfalse;
	}
	slot = &s_snap[cmdIndex];
	w = vk.oitExtentWidth ? vk.oitExtentWidth : (uint32_t)glConfig.vidWidth;
	h = vk.oitExtentHeight ? vk.oitExtentHeight : (uint32_t)glConfig.vidHeight;
	fogLayout = ( vk.fog_scene_layout != VK_IMAGE_LAYOUT_UNDEFINED )
		? vk.fog_scene_layout : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	if ( !CERT_RecordOne( cmd, &slot->buf[0], vk.fog_scene_image, vk.color_format, w, h, fogLayout ) ||
		!CERT_RecordOne( cmd, &slot->buf[1], vk.oit_accum_image, VK_FORMAT_R16G16B16A16_SFLOAT, w, h,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) ||
		!CERT_RecordOne( cmd, &slot->buf[2], vk.oit_reveal_image, VK_FORMAT_R16_SFLOAT, w, h,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) ||
		!CERT_RecordOne( cmd, &slot->buf[3], vk.color_image, vk.color_format, w, h,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) ) {
		slot->pending = qfalse;
		ri.Printf( PRINT_WARNING, "cert_readback: failed to record OIT snapshot\n" );
		return qfalse;
	}
	slot->pending = qtrue;
	slot->ready = qfalse;
	slot->frameNumber = vk.oitFrameNumber ? vk.oitFrameNumber : (uint64_t)tr.frameCount;
	slot->generation = vk.oitAttachmentGeneration;
	return qtrue;
}

qboolean vk_cert_readback_oit_snapshot_pending( int cmdIndex )
{
	if ( cmdIndex < 0 || cmdIndex >= NUM_COMMAND_BUFFERS ) {
		return qfalse;
	}
	return s_snap[cmdIndex].pending;
}

static void CERT_FillCaptureFromSnap( certReadbackCapture_t *out, certSnapBuf_t *b,
	certReadbackResource_t res, uint64_t frame, uint32_t gen )
{
	const oitContract_t *oit = vk_oit_contract_wboit();
	const oitWeightContract_t *wt = vk_oit_weight_contract_get();
	const hdrResolveContract_t *hr = vk_hdr_resolve_contract_get();
	Com_Memset( out, 0, sizeof( *out ) );
	out->resource = res;
	out->format = b->format;
	out->width = b->width;
	out->height = b->height;
	out->rowPitchBytes = b->width * b->bpp;
	out->frameNumber = frame;
	out->generation = gen;
	out->pipelineStage = (uint32_t)VK_PIPELINE_STAGE_TRANSFER_BIT;
	Q_strncpyz( out->colorSpace, "SCENE_LINEAR_HDR", sizeof( out->colorSpace ) );
	out->oitContractHash = oit ? oit->contractHash : 0;
	out->weightContractHash = wt ? wt->contractHash : 0;
	out->resolveContractHash = hr ? hr->contractHash : 0;
	out->rgba = b->rgba;
	out->pixelCount = b->width * b->height;
	out->valid = ( b->rgba != NULL ) ? qtrue : qfalse;
}

qboolean vk_cert_readback_finalize_oit_snapshot( int cmdIndex, certOitSnapshot_t *out )
{
	certOitSnapSlot_t *slot;
	int i;
	if ( cmdIndex < 0 || cmdIndex >= NUM_COMMAND_BUFFERS ) {
		return qfalse;
	}
	slot = &s_snap[cmdIndex];
	if ( !slot->pending ) {
		return qfalse;
	}
	for ( i = 0; i < 4; i++ ) {
		certSnapBuf_t *b = &slot->buf[i];
		if ( !b->mapped || !b->rgba ||
			!vk_cert_readback_decode_to_rgba( b->format, b->width, b->height,
				b->width * b->bpp, b->mapped, b->rgba ) ) {
			slot->pending = qfalse;
			ri.Printf( PRINT_WARNING, "cert_readback: OIT snapshot decode failed slot=%d\n", i );
			return qfalse;
		}
	}
	Com_Memset( &s_lastOit, 0, sizeof( s_lastOit ) );
	s_lastOit.valid = qtrue;
	s_lastOit.frameNumber = slot->frameNumber;
	s_lastOit.generation = slot->generation;
	CERT_FillCaptureFromSnap( &s_lastOit.fog, &slot->buf[0], CERT_RB_FOG_SCENE, slot->frameNumber, slot->generation );
	CERT_FillCaptureFromSnap( &s_lastOit.accum, &slot->buf[1], CERT_RB_OIT_ACCUM, slot->frameNumber, slot->generation );
	CERT_FillCaptureFromSnap( &s_lastOit.reveal, &slot->buf[2], CERT_RB_OIT_REVEALAGE, slot->frameNumber, slot->generation );
	CERT_FillCaptureFromSnap( &s_lastOit.resolved, &slot->buf[3], CERT_RB_RESOLVED_WBOIT, slot->frameNumber, slot->generation );
	slot->pending = qfalse;
	slot->ready = qtrue;
	if ( out ) {
		*out = s_lastOit;
	}
	ri.Printf( PRINT_ALL,
		"cert_readback: OIT snapshot ready frame=%llu gen=%u %ux%u\n",
		(unsigned long long)s_lastOit.frameNumber, s_lastOit.generation,
		s_lastOit.fog.width, s_lastOit.fog.height );
	return qtrue;
}

const certOitSnapshot_t *vk_cert_readback_last_oit_snapshot( void )
{
	return s_lastOit.valid ? &s_lastOit : NULL;
}

void vk_cert_readback_register( void )
{
	if ( s_cmds ) {
		return;
	}
	r_certReadbackBlocking = ri.Cvar_Get( "r_certReadbackBlocking", "1", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_certReadbackBlocking, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_certReadbackBlocking,
		"Certification readback waits for GPU idle (required for correct evidence)." );
	ri.Cmd_AddCommand( "cert_readback_status", CERT_ReadbackStatus_f );
	ri.Cmd_AddCommand( "cert_readback_capture", CERT_ReadbackCapture_f );
	ri.Cmd_AddCommand( "cert_readback_flush", CERT_ReadbackFlush_f );
	s_cmds = qtrue;
	ri.Printf( PRINT_ALL,
		"[VK][Cert] readback ready (snapshot+capture; Phase 2.6C deferred OIT)\n" );
}
