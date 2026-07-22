/*
===========================================================================
Raster Ultra 1.6 — Hi-Z depth pyramid (conservative occlusion).

Distinct from r_forwardPlusHiZ (Forward+ tile probe padding).
Conservative policy: camera-cut / missing pyramid / large objects / recently
visible instances stay visible (no one-frame disappearance).
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_hiz.h"
#include "vk_util.h"
#include "vk_view_state.h"
#include "vk_pass_registry.h"
#include "vk_image_layout.h"
#include "vk_depth_contract.h"

#include <math.h>

#define VK_HIZ_MAX_MIPS 12

typedef struct {
	VkImage image;
	VkImageView views[VK_HIZ_MAX_MIPS];
	VkImageView viewAll;
	VkDeviceMemory memory;
	uint32_t width, height, levels;
	VkImageLayout layout;
} hizPyramid_t;

typedef struct {
	uint32_t srcExtent[2];
	uint32_t dstExtent[2];
	uint32_t srcLevel;
	uint32_t pad;
} hizPush_t;

typedef struct {
	VkDescriptorSetLayout setLayout;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
	VkDescriptorPool descriptorPool;
	VkDescriptorSet descriptor;
	qboolean ready;
} hizCompute_t;

static cvar_t *r_hiZ;
static cvar_t *r_hiZMinVisibleFrames;
static cvar_t *r_hiZLargeObjectPx;
static cvar_t *r_hiZDebug;
static cvar_t *r_hizDebug;

#define VK_HIZ_HOST_MAX_DIM 64

typedef struct {
	VkBuffer buffer;
	VkDeviceMemory memory;
	float *mapped;
	uint32_t width;
	uint32_t height;
	uint32_t level;
	qboolean pending;
	qboolean valid;
} hizHostCache_t;

static hizPyramid_t s_pyramid;
static hizCompute_t s_compute;
static hizHostCache_t s_host[2];
static int s_hostWrite;
static int s_hostRead;
static qboolean s_cameraCut;
static qboolean s_ready;
static qboolean s_gpuBuilt;
static qboolean s_gpuPipelineLogged;
static uint32_t s_buildCount;
static uint32_t s_gpuBuildCount;
static uint32_t s_tests;
static uint32_t s_rejected;
static uint32_t s_biasKeep;
static uint32_t s_hostSampleRejects;
static qboolean s_cmds;

static qboolean HIZ_DebugEnabled( void )
{
	if ( r_hiZDebug && r_hiZDebug->integer ) {
		return qtrue;
	}
	if ( r_hizDebug && r_hizDebug->integer ) {
		return qtrue;
	}
	return qfalse;
}

static void HIZ_DestroyHostCache( void )
{
	int i;

	for ( i = 0; i < 2; i++ ) {
		if ( s_host[i].mapped && s_host[i].memory != VK_NULL_HANDLE ) {
			qvkUnmapMemory( vk.device, s_host[i].memory );
		}
		if ( s_host[i].buffer != VK_NULL_HANDLE ) {
			qvkDestroyBuffer( vk.device, s_host[i].buffer, NULL );
		}
		if ( s_host[i].memory != VK_NULL_HANDLE ) {
			qvkFreeMemory( vk.device, s_host[i].memory, NULL );
		}
		Com_Memset( &s_host[i], 0, sizeof( s_host[i] ) );
	}
	s_hostWrite = 0;
	s_hostRead = 1;
}

static qboolean HIZ_EnsureHostCache( uint32_t width, uint32_t height, uint32_t level )
{
	VkBufferCreateInfo bci;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo mai;
	VkDeviceSize size;
	int i;

	size = (VkDeviceSize)width * (VkDeviceSize)height * sizeof( float );
	if ( size < sizeof( float ) ) {
		size = sizeof( float );
	}

	for ( i = 0; i < 2; i++ ) {
		if ( s_host[i].mapped && s_host[i].width == width && s_host[i].height == height ) {
			s_host[i].level = level;
			continue;
		}
		if ( s_host[i].mapped && s_host[i].memory != VK_NULL_HANDLE ) {
			qvkUnmapMemory( vk.device, s_host[i].memory );
			s_host[i].mapped = NULL;
		}
		if ( s_host[i].buffer != VK_NULL_HANDLE ) {
			qvkDestroyBuffer( vk.device, s_host[i].buffer, NULL );
			s_host[i].buffer = VK_NULL_HANDLE;
		}
		if ( s_host[i].memory != VK_NULL_HANDLE ) {
			qvkFreeMemory( vk.device, s_host[i].memory, NULL );
			s_host[i].memory = VK_NULL_HANDLE;
		}

		Com_Memset( &bci, 0, sizeof( bci ) );
		bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bci.size = size;
		bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		if ( qvkCreateBuffer( vk.device, &bci, NULL, &s_host[i].buffer ) != VK_SUCCESS ) {
			HIZ_DestroyHostCache();
			return qfalse;
		}
		qvkGetBufferMemoryRequirements( vk.device, s_host[i].buffer, &req );
		Com_Memset( &mai, 0, sizeof( mai ) );
		mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		mai.allocationSize = req.size;
		mai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		if ( qvkAllocateMemory( vk.device, &mai, NULL, &s_host[i].memory ) != VK_SUCCESS ) {
			HIZ_DestroyHostCache();
			return qfalse;
		}
		qvkBindBufferMemory( vk.device, s_host[i].buffer, s_host[i].memory, 0 );
		if ( qvkMapMemory( vk.device, s_host[i].memory, 0, size, 0, (void **)&s_host[i].mapped ) != VK_SUCCESS ) {
			HIZ_DestroyHostCache();
			return qfalse;
		}
		s_host[i].width = width;
		s_host[i].height = height;
		s_host[i].level = level;
		s_host[i].valid = qfalse;
	}
	return qtrue;
}

static uint32_t HIZ_SelectHostMip( uint32_t *outW, uint32_t *outH )
{
	uint32_t level = 0;
	uint32_t w = s_pyramid.width;
	uint32_t h = s_pyramid.height;

	while ( level + 1u < s_pyramid.levels && ( w > VK_HIZ_HOST_MAX_DIM || h > VK_HIZ_HOST_MAX_DIM ) ) {
		level++;
		w = w > 1u ? w / 2u : 1u;
		h = h > 1u ? h / 2u : 1u;
	}
	if ( outW ) {
		*outW = w;
	}
	if ( outH ) {
		*outH = h;
	}
	return level;
}

static void HIZ_ScheduleHostReadback( VkCommandBuffer cmd )
{
	uint32_t level, w, h;
	hizHostCache_t *dst;
	VkBufferImageCopy region;
	VkImageMemoryBarrier imgBarrier;
	VkBufferMemoryBarrier bufBarrier;

	if ( cmd == VK_NULL_HANDLE || s_pyramid.image == VK_NULL_HANDLE || s_pyramid.levels < 1u ) {
		return;
	}

	level = HIZ_SelectHostMip( &w, &h );
	if ( !HIZ_EnsureHostCache( w, h, level ) ) {
		return;
	}

	dst = &s_host[s_hostWrite];

	Com_Memset( &imgBarrier, 0, sizeof( imgBarrier ) );
	imgBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imgBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	imgBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	imgBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imgBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	imgBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imgBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imgBarrier.image = s_pyramid.image;
	imgBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imgBarrier.subresourceRange.baseMipLevel = level;
	imgBarrier.subresourceRange.levelCount = 1;
	imgBarrier.subresourceRange.layerCount = 1;
	qvkCmdPipelineBarrier( cmd,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 0, NULL, 0, NULL, 1, &imgBarrier );

	Com_Memset( &region, 0, sizeof( region ) );
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = level;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset.x = 0;
	region.imageOffset.y = 0;
	region.imageOffset.z = 0;
	region.imageExtent.width = w;
	region.imageExtent.height = h;
	region.imageExtent.depth = 1;
	qvkCmdCopyImageToBuffer( cmd, s_pyramid.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		dst->buffer, 1, &region );

	Com_Memset( &bufBarrier, 0, sizeof( bufBarrier ) );
	bufBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	bufBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	bufBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	bufBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	bufBarrier.buffer = dst->buffer;
	bufBarrier.offset = 0;
	bufBarrier.size = VK_WHOLE_SIZE;
	qvkCmdPipelineBarrier( cmd,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
		0, 0, NULL, 1, &bufBarrier, 0, NULL );

	/* Restore pyramid mip for shader sampling next frame. */
	imgBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	imgBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imgBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	imgBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	qvkCmdPipelineBarrier( cmd,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0, 0, NULL, 0, NULL, 1, &imgBarrier );

	dst->valid = qfalse;
	dst->pending = qtrue;
	dst->level = level;
	dst->width = w;
	dst->height = h;
}

/*
 * Promote last frame's write buffer to CPU-readable once the prior submit completed
 * (caller records after frame fence wait).
 */
static void HIZ_PromoteHostReadback( void )
{
	int prevWrite = s_hostWrite;

	s_hostRead = prevWrite;
	s_hostWrite = 1 - prevWrite;
	if ( s_host[s_hostRead].mapped && s_host[s_hostRead].pending ) {
		s_host[s_hostRead].valid = qtrue;
		s_host[s_hostRead].pending = qfalse;
	}
}

static float HIZ_SampleHostMin( int x0, int y0, int x1, int y1 )
{
	const hizHostCache_t *src = &s_host[s_hostRead];
	float zFar = 1.0f;
	int x, y;

	if ( !src->mapped || !src->valid || src->width < 1u || src->height < 1u ) {
		return 0.0f; /* empty / far — keep visible */
	}
	if ( x0 < 0 ) {
		x0 = 0;
	}
	if ( y0 < 0 ) {
		y0 = 0;
	}
	if ( x1 >= (int)src->width ) {
		x1 = (int)src->width - 1;
	}
	if ( y1 >= (int)src->height ) {
		y1 = (int)src->height - 1;
	}
	if ( x1 < x0 || y1 < y0 ) {
		return 0.0f;
	}

	/* reversed-Z: farthest in region = minimum stored depth */
	zFar = 1.0f;
	for ( y = y0; y <= y1; y++ ) {
		for ( x = x0; x <= x1; x++ ) {
			float z = src->mapped[y * (int)src->width + x];
			if ( z < zFar ) {
				zFar = z;
			}
		}
	}
	return zFar;
}

static qboolean HIZ_ProjectAabb( const vec3_t mins, const vec3_t maxs,
	float *outMinZ, float *outMaxZ, float *outMinX, float *outMaxX, float *outMinY, float *outMaxY,
	float *outDiagPx )
{
	const float *mv = backEnd.viewParms.world.modelViewMatrix;
	const float *proj = backEnd.viewParms.projectionMatrix;
	int i;
	float minZ = 1.0f, maxZ = 0.0f;
	float minX = 1.0f, maxX = -1.0f, minY = 1.0f, maxY = -1.0f;
	int behind = 0;
	float corners[8][3];

	corners[0][0] = mins[0]; corners[0][1] = mins[1]; corners[0][2] = mins[2];
	corners[1][0] = maxs[0]; corners[1][1] = mins[1]; corners[1][2] = mins[2];
	corners[2][0] = mins[0]; corners[2][1] = maxs[1]; corners[2][2] = mins[2];
	corners[3][0] = maxs[0]; corners[3][1] = maxs[1]; corners[3][2] = mins[2];
	corners[4][0] = mins[0]; corners[4][1] = mins[1]; corners[4][2] = maxs[2];
	corners[5][0] = maxs[0]; corners[5][1] = mins[1]; corners[5][2] = maxs[2];
	corners[6][0] = mins[0]; corners[6][1] = maxs[1]; corners[6][2] = maxs[2];
	corners[7][0] = maxs[0]; corners[7][1] = maxs[1]; corners[7][2] = maxs[2];

	for ( i = 0; i < 8; i++ ) {
		vec4_t eye, clip;
		float invW, ndcX, ndcY, ndcZ;

		R_TransformModelToClip( corners[i], mv, proj, eye, clip );
		if ( clip[3] <= 0.0f ) {
			behind++;
			continue;
		}
		invW = 1.0f / clip[3];
		ndcX = clip[0] * invW;
		ndcY = clip[1] * invW;
		ndcZ = clip[2] * invW;
		if ( ndcX < minX ) {
			minX = ndcX;
		}
		if ( ndcX > maxX ) {
			maxX = ndcX;
		}
		if ( ndcY < minY ) {
			minY = ndcY;
		}
		if ( ndcY > maxY ) {
			maxY = ndcY;
		}
		if ( ndcZ < minZ ) {
			minZ = ndcZ;
		}
		if ( ndcZ > maxZ ) {
			maxZ = ndcZ;
		}
	}

	if ( behind >= 8 ) {
		return qfalse;
	}
	/* Clamp NDC to screen; partial behind-camera expands to full screen (conservative). */
	if ( behind > 0 ) {
		minX = -1.0f;
		maxX = 1.0f;
		minY = -1.0f;
		maxY = 1.0f;
	}
	if ( minX < -1.0f ) {
		minX = -1.0f;
	}
	if ( maxX > 1.0f ) {
		maxX = 1.0f;
	}
	if ( minY < -1.0f ) {
		minY = -1.0f;
	}
	if ( maxY > 1.0f ) {
		maxY = 1.0f;
	}

	*outMinZ = minZ;
	*outMaxZ = maxZ;
	*outMinX = minX;
	*outMaxX = maxX;
	*outMinY = minY;
	*outMaxY = maxY;
	{
		float dx = ( maxX - minX ) * 0.5f * (float)s_host[s_hostRead].width;
		float dy = ( maxY - minY ) * 0.5f * (float)s_host[s_hostRead].height;
		*outDiagPx = (float)sqrt( (double)( dx * dx + dy * dy ) );
	}
	return qtrue;
}

static void HIZ_MipExtent( uint32_t level, uint32_t *outW, uint32_t *outH )
{
	uint32_t w = s_pyramid.width >> level;
	uint32_t h = s_pyramid.height >> level;

	if ( w < 1u ) {
		w = 1u;
	}
	if ( h < 1u ) {
		h = 1u;
	}
	*outW = w;
	*outH = h;
}

static void HIZ_DestroyCompute( void )
{
	if ( s_compute.pipeline ) {
		qvkDestroyPipeline( vk.device, s_compute.pipeline, NULL );
	}
	if ( s_compute.pipelineLayout ) {
		qvkDestroyPipelineLayout( vk.device, s_compute.pipelineLayout, NULL );
	}
	if ( s_compute.setLayout ) {
		qvkDestroyDescriptorSetLayout( vk.device, s_compute.setLayout, NULL );
	}
	if ( s_compute.descriptorPool ) {
		qvkDestroyDescriptorPool( vk.device, s_compute.descriptorPool, NULL );
	}
	Com_Memset( &s_compute, 0, sizeof( s_compute ) );
}

static qboolean HIZ_CreateCompute( void )
{
	VkDescriptorSetLayoutBinding bindings[3];
	VkDescriptorSetLayoutCreateInfo dci;
	VkPushConstantRange pushRange;
	VkPipelineLayoutCreateInfo plci;
	VkComputePipelineCreateInfo pipeCi;
	VkDescriptorPoolSize poolSizes[2];
	VkDescriptorPoolCreateInfo poolCi;
	VkDescriptorSetAllocateInfo allocCi;

	if ( s_compute.ready ) {
		return qtrue;
	}
	if ( vk.device == VK_NULL_HANDLE || vk.modules.hiz_downsample_cs == VK_NULL_HANDLE ) {
		return qfalse;
	}

	HIZ_DestroyCompute();

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	Com_Memset( &dci, 0, sizeof( dci ) );
	dci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dci.bindingCount = 3;
	dci.pBindings = bindings;
	if ( qvkCreateDescriptorSetLayout( vk.device, &dci, NULL, &s_compute.setLayout ) != VK_SUCCESS ) {
		HIZ_DestroyCompute();
		return qfalse;
	}

	Com_Memset( &pushRange, 0, sizeof( pushRange ) );
	pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pushRange.offset = 0;
	pushRange.size = sizeof( hizPush_t );

	Com_Memset( &plci, 0, sizeof( plci ) );
	plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &s_compute.setLayout;
	plci.pushConstantRangeCount = 1;
	plci.pPushConstantRanges = &pushRange;
	if ( qvkCreatePipelineLayout( vk.device, &plci, NULL, &s_compute.pipelineLayout ) != VK_SUCCESS ) {
		HIZ_DestroyCompute();
		return qfalse;
	}

	Com_Memset( &pipeCi, 0, sizeof( pipeCi ) );
	pipeCi.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipeCi.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipeCi.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	pipeCi.stage.module = vk.modules.hiz_downsample_cs;
	pipeCi.stage.pName = "main";
	pipeCi.layout = s_compute.pipelineLayout;
	if ( qvkCreateComputePipelines( vk.device, vk.pipelineCache, 1, &pipeCi, NULL, &s_compute.pipeline ) != VK_SUCCESS ) {
		HIZ_DestroyCompute();
		return qfalse;
	}
	SET_OBJECT_NAME( s_compute.pipeline, "pipeline - hiz downsample", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	Com_Memset( poolSizes, 0, sizeof( poolSizes ) );
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[0].descriptorCount = 1;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSizes[1].descriptorCount = 2;

	Com_Memset( &poolCi, 0, sizeof( poolCi ) );
	poolCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCi.maxSets = 1;
	poolCi.poolSizeCount = 2;
	poolCi.pPoolSizes = poolSizes;
	if ( qvkCreateDescriptorPool( vk.device, &poolCi, NULL, &s_compute.descriptorPool ) != VK_SUCCESS ) {
		HIZ_DestroyCompute();
		return qfalse;
	}

	Com_Memset( &allocCi, 0, sizeof( allocCi ) );
	allocCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocCi.descriptorPool = s_compute.descriptorPool;
	allocCi.descriptorSetCount = 1;
	allocCi.pSetLayouts = &s_compute.setLayout;
	if ( qvkAllocateDescriptorSets( vk.device, &allocCi, &s_compute.descriptor ) != VK_SUCCESS ) {
		HIZ_DestroyCompute();
		return qfalse;
	}

	s_compute.ready = qtrue;
	if ( !s_gpuPipelineLogged ) {
		ri.Printf( PRINT_ALL, "[VK][HiZ] GPU downsample pipeline ready (hiz_downsample_cs)\n" );
		s_gpuPipelineLogged = qtrue;
	}
	return qtrue;
}

static void HIZ_BindMipDescriptors( VkImageView depthView, VkImageView srcView, VkImageView dstView )
{
	VkDescriptorImageInfo depthInfo;
	VkDescriptorImageInfo srcInfo;
	VkDescriptorImageInfo dstInfo;
	VkWriteDescriptorSet writes[3];
	Vk_Sampler_Def depthSd;
	uint32_t writeCount;

	Com_Memset( &depthSd, 0, sizeof( depthSd ) );
	depthSd.gl_mag_filter = depthSd.gl_min_filter = GL_NEAREST;
	depthSd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	depthSd.noAnisotropy = qtrue;

	Com_Memset( &depthInfo, 0, sizeof( depthInfo ) );
	depthInfo.sampler = vk_find_sampler( &depthSd );
	depthInfo.imageView = depthView;
	depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	Com_Memset( &srcInfo, 0, sizeof( srcInfo ) );
	srcInfo.imageView = srcView;
	srcInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	Com_Memset( &dstInfo, 0, sizeof( dstInfo ) );
	dstInfo.imageView = dstView;
	dstInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = s_compute.descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &depthInfo;

	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = s_compute.descriptor;
	writes[1].dstBinding = 2;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[1].pImageInfo = &dstInfo;
	writeCount = 2;

	if ( srcView != VK_NULL_HANDLE ) {
		writes[1].dstBinding = 1;
		writes[1].pImageInfo = &srcInfo;

		writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[2].dstSet = s_compute.descriptor;
		writes[2].dstBinding = 2;
		writes[2].descriptorCount = 1;
		writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[2].pImageInfo = &dstInfo;
		writeCount = 3;
	}

	qvkUpdateDescriptorSets( vk.device, writeCount, writes, 0, NULL );
}

static void HIZ_BarrierMipWriteToRead( VkCommandBuffer cmd, uint32_t mipLevel )
{
	VkImageMemoryBarrier barrier;

	if ( s_pyramid.image == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &barrier, 0, sizeof( barrier ) );
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = s_pyramid.image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = mipLevel;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	qvkCmdPipelineBarrier( cmd,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0, 0, NULL, 0, NULL, 1, &barrier );
}

static qboolean HIZ_DispatchGpuDownsample( VkCommandBuffer cmd )
{
	VkImageView depthView;
	VkImageAspectFlags depthAspect;
	hizPush_t push;
	uint32_t mip;

	if ( !s_compute.ready || s_compute.pipeline == VK_NULL_HANDLE ||
		s_compute.descriptor == VK_NULL_HANDLE || cmd == VK_NULL_HANDLE ) {
		return qfalse;
	}
	if ( vk.depth_image == VK_NULL_HANDLE || s_pyramid.levels < 1u ) {
		return qfalse;
	}

	depthView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	if ( depthView == VK_NULL_HANDLE ) {
		return qfalse;
	}

	depthAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 ) {
		depthAspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	if ( s_pyramid.layout == VK_IMAGE_LAYOUT_UNDEFINED ) {
		record_image_layout_transition( cmd, s_pyramid.image, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
			0, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	} else if ( s_pyramid.layout != VK_IMAGE_LAYOUT_GENERAL ) {
		record_image_layout_transition( cmd, s_pyramid.image, VK_IMAGE_ASPECT_COLOR_BIT,
			s_pyramid.layout, VK_IMAGE_LAYOUT_GENERAL,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	}
	s_pyramid.layout = VK_IMAGE_LAYOUT_GENERAL;

	record_depth_image_layout_transition( cmd, depthAspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s_compute.pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s_compute.pipelineLayout,
		0, 1, &s_compute.descriptor, 0, NULL );

	for ( mip = 0; mip < s_pyramid.levels; mip++ ) {
		uint32_t dstW, dstH, srcW, srcH;
		uint32_t groupsX, groupsY;
		VkImageView srcView;

		HIZ_MipExtent( mip, &dstW, &dstH );
		if ( mip == 0u ) {
			srcW = s_pyramid.width;
			srcH = s_pyramid.height;
			srcView = VK_NULL_HANDLE;
		} else {
			HIZ_MipExtent( mip - 1u, &srcW, &srcH );
			srcView = s_pyramid.views[mip - 1u];
		}

		HIZ_BindMipDescriptors( depthView, srcView, s_pyramid.views[mip] );

		Com_Memset( &push, 0, sizeof( push ) );
		push.srcExtent[0] = srcW;
		push.srcExtent[1] = srcH;
		push.dstExtent[0] = dstW;
		push.dstExtent[1] = dstH;
		push.srcLevel = mip;
		qvkCmdPushConstants( cmd, s_compute.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
			0, sizeof( push ), &push );

		groupsX = ( dstW + 7u ) / 8u;
		groupsY = ( dstH + 7u ) / 8u;
		qvkCmdDispatch( cmd, groupsX, groupsY, 1 );

		if ( mip + 1u < s_pyramid.levels ) {
			HIZ_BarrierMipWriteToRead( cmd, mip );
		}
	}

	record_image_layout_transition( cmd, s_pyramid.image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
	s_pyramid.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	record_depth_image_layout_transition( cmd, depthAspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT );

	HIZ_ScheduleHostReadback( cmd );
	return qtrue;
}

void vk_hiz_register_cvars( void )
{
	if ( r_hiZ ) {
		return;
	}
	r_hiZ = ri.Cvar_Get( "r_hiZ", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_hiZ, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_hiZ,
		"Raster Ultra 1.6 Hi-Z depth pyramid for conservative occlusion (latched).\n"
		"Distinct from r_forwardPlusHiZ (tile probe pad only).\n"
		" 0 off (default / certified boot)\n"
		" 1 build pyramid; cull uses conservative bias + frustum companion" );
	ri.Cvar_SetGroup( r_hiZ, CVG_RENDERER );

	r_hiZMinVisibleFrames = ri.Cvar_Get( "r_hiZMinVisibleFrames", "2", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hiZMinVisibleFrames, "0", "8", CV_INTEGER );
	ri.Cvar_SetDescription( r_hiZMinVisibleFrames,
		"Minimum frames an instance stays visible after becoming visible (anti one-frame pop)." );

	r_hiZLargeObjectPx = ri.Cvar_Get( "r_hiZLargeObjectPx", "256", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hiZLargeObjectPx, "32", "4096", CV_FLOAT );
	ri.Cvar_SetDescription( r_hiZLargeObjectPx,
		"Projected AABB diagonal (px) above which Hi-Z never rejects (large-object handling)." );

	r_hiZDebug = ri.Cvar_Get( "r_hiZDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hiZDebug, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_hiZDebug,
		"Hi-Z debug (alias r_hizDebug): 0 off, 1 pyramid stats, 2 reject heatmap." );
	/* Alias for Foundation Consolidation naming (r_hizDebug). */
	(void)ri.Cvar_Get( "r_hizDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_hiZDebug,
		"Hi-Z debug verbosity (pyramid build / future visualization)." );

	r_hizDebug = ri.Cvar_Get( "r_hizDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hizDebug, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_hizDebug,
		"Alias for r_hiZDebug (alternate naming)." );
}

static void HIZ_DestroyPyramid( void )
{
	uint32_t i;

	if ( s_pyramid.viewAll ) {
		qvkDestroyImageView( vk.device, s_pyramid.viewAll, NULL );
	}
	for ( i = 0; i < VK_HIZ_MAX_MIPS; i++ ) {
		if ( s_pyramid.views[i] ) {
			qvkDestroyImageView( vk.device, s_pyramid.views[i], NULL );
		}
	}
	if ( s_pyramid.image ) {
		qvkDestroyImage( vk.device, s_pyramid.image, NULL );
	}
	if ( s_pyramid.memory ) {
		qvkFreeMemory( vk.device, s_pyramid.memory, NULL );
	}
	Com_Memset( &s_pyramid, 0, sizeof( s_pyramid ) );
	s_ready = qfalse;
	s_gpuBuilt = qfalse;
	HIZ_DestroyHostCache();
}

static qboolean HIZ_CreatePyramid( uint32_t width, uint32_t height )
{
	VkImageCreateInfo ici;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo mai;
	VkImageViewCreateInfo vci;
	uint32_t levels = 1;
	uint32_t w = width;
	uint32_t h = height;
	uint32_t i;

	HIZ_DestroyPyramid();
	if ( width < 1 || height < 1 || vk.device == VK_NULL_HANDLE ) {
		return qfalse;
	}

	while ( ( w > 1 || h > 1 ) && levels < VK_HIZ_MAX_MIPS ) {
		w = w > 1 ? w / 2 : 1;
		h = h > 1 ? h / 2 : 1;
		levels++;
	}

	Com_Memset( &ici, 0, sizeof( ici ) );
	ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ici.imageType = VK_IMAGE_TYPE_2D;
	ici.format = VK_FORMAT_R32_SFLOAT;
	ici.extent.width = width;
	ici.extent.height = height;
	ici.extent.depth = 1;
	ici.mipLevels = levels;
	ici.arrayLayers = 1;
	ici.samples = VK_SAMPLE_COUNT_1_BIT;
	ici.tiling = VK_IMAGE_TILING_OPTIMAL;
	ici.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if ( qvkCreateImage( vk.device, &ici, NULL, &s_pyramid.image ) != VK_SUCCESS ) {
		return qfalse;
	}
	qvkGetImageMemoryRequirements( vk.device, s_pyramid.image, &req );
	Com_Memset( &mai, 0, sizeof( mai ) );
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.allocationSize = req.size;
	mai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	if ( qvkAllocateMemory( vk.device, &mai, NULL, &s_pyramid.memory ) != VK_SUCCESS ) {
		HIZ_DestroyPyramid();
		return qfalse;
	}
	qvkBindImageMemory( vk.device, s_pyramid.image, s_pyramid.memory, 0 );

	for ( i = 0; i < levels; i++ ) {
		Com_Memset( &vci, 0, sizeof( vci ) );
		vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		vci.image = s_pyramid.image;
		vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
		vci.format = VK_FORMAT_R32_SFLOAT;
		vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		vci.subresourceRange.baseMipLevel = i;
		vci.subresourceRange.levelCount = 1;
		vci.subresourceRange.layerCount = 1;
		if ( qvkCreateImageView( vk.device, &vci, NULL, &s_pyramid.views[i] ) != VK_SUCCESS ) {
			HIZ_DestroyPyramid();
			return qfalse;
		}
	}

	Com_Memset( &vci, 0, sizeof( vci ) );
	vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	vci.image = s_pyramid.image;
	vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
	vci.format = VK_FORMAT_R32_SFLOAT;
	vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vci.subresourceRange.baseMipLevel = 0;
	vci.subresourceRange.levelCount = levels;
	vci.subresourceRange.layerCount = 1;
	if ( qvkCreateImageView( vk.device, &vci, NULL, &s_pyramid.viewAll ) != VK_SUCCESS ) {
		HIZ_DestroyPyramid();
		return qfalse;
	}

	s_pyramid.width = width;
	s_pyramid.height = height;
	s_pyramid.levels = levels;
	s_pyramid.layout = VK_IMAGE_LAYOUT_UNDEFINED;
	s_ready = qtrue;
	s_gpuBuilt = qfalse;
	ri.Printf( PRINT_ALL, "[VK][HiZ] pyramid %ux%u levels=%u (Raster Ultra 1.6)\n",
		width, height, levels );
	return qtrue;
}

void vk_hiz_init( void )
{
	vk_hiz_register_cvars();
	s_cameraCut = qtrue;
	s_gpuBuilt = qfalse;
	s_buildCount = s_gpuBuildCount = s_tests = s_rejected = s_biasKeep = 0;
	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "hiz_status", vk_hiz_status_f );
		s_cmds = qtrue;
	}
	if ( r_hiZ && r_hiZ->integer ) {
		HIZ_CreateCompute();
		if ( vk.renderWidth > 0 && vk.renderHeight > 0 ) {
			HIZ_CreatePyramid( vk.renderWidth, vk.renderHeight );
		}
	}
}

void vk_hiz_shutdown( void )
{
	HIZ_DestroyPyramid();
	HIZ_DestroyCompute();
}

void vk_hiz_on_resize( void )
{
	if ( !r_hiZ || !r_hiZ->integer ) {
		HIZ_DestroyPyramid();
		return;
	}
	HIZ_CreatePyramid( vk.renderWidth, vk.renderHeight );
	s_cameraCut = qtrue;
}

void vk_hiz_on_camera_cut( void )
{
	s_cameraCut = qtrue;
	s_gpuBuilt = qfalse;
}

qboolean vk_hiz_active( void )
{
	return ( r_hiZ && r_hiZ->integer ) ? qtrue : qfalse;
}

qboolean vk_hiz_ready( void )
{
	return ( vk_hiz_active() && s_ready && s_gpuBuilt && !s_cameraCut ) ? qtrue : qfalse;
}

void vk_hiz_build( void )
{
	VkCommandBuffer cmd;

	if ( !vk_hiz_active() ) {
		return;
	}
	if ( !s_ready || s_pyramid.width != vk.renderWidth || s_pyramid.height != vk.renderHeight ) {
		HIZ_CreatePyramid( vk.renderWidth, vk.renderHeight );
	}
	if ( !s_ready ) {
		return;
	}

	if ( !s_compute.ready ) {
		HIZ_CreateCompute();
	}

	/* Previous frame's copy is GPU-complete after frame fence; promote for CPU cull. */
	HIZ_PromoteHostReadback();

	s_gpuBuilt = qfalse;
	cmd = ( vk.cmd && vk.cmd->command_buffer ) ? vk.cmd->command_buffer : VK_NULL_HANDLE;
	if ( cmd != VK_NULL_HANDLE && HIZ_DispatchGpuDownsample( cmd ) ) {
		s_gpuBuilt = qtrue;
		s_gpuBuildCount++;
		s_cameraCut = qfalse;
		s_buildCount++;
		vk_depth_contract_note_reader( "hiz_downsample" );
		if ( HIZ_DebugEnabled() ) {
			ri.Printf( PRINT_DEVELOPER, "[VK][HiZ] gpuBuilt=yes mips=%u layout=%u host=%ux%u ready=%s\n",
				s_pyramid.levels, (unsigned)s_pyramid.layout,
				s_host[s_hostRead].width, s_host[s_hostRead].height,
				s_host[s_hostRead].valid ? "yes" : "no" );
		}
	}
}

/*
===============
vk_hiz_aabb_visible

Conservative occlusion: never reject on camera cut / missing pyramid / large
screen projection / recently visible instances. Host samples prior-frame Hi-Z
mip (reversed-Z farthest = min).
===============
*/
qboolean vk_hiz_aabb_visible( const vec3_t mins, const vec3_t maxs,
	qboolean wasVisibleLastFrame, uint32_t visibleAge )
{
	float diagPx;
	float minZ, maxZ, minX, maxX, minY, maxY;
	float occluderFar;
	int minFrames;
	int x0, y0, x1, y1;
	const hizHostCache_t *host;

	s_tests++;

	if ( !vk_hiz_active() || s_cameraCut || !s_ready ) {
		s_biasKeep++;
		return qtrue;
	}

	minFrames = r_hiZMinVisibleFrames ? r_hiZMinVisibleFrames->integer : 2;
	if ( wasVisibleLastFrame || visibleAge < (uint32_t)minFrames ) {
		s_biasKeep++;
		return qtrue;
	}

	host = &s_host[s_hostRead];
	if ( !s_gpuBuilt || !host->valid || !host->mapped || host->width < 1u || host->height < 1u ) {
		s_biasKeep++;
		return qtrue;
	}

	if ( !HIZ_ProjectAabb( mins, maxs, &minZ, &maxZ, &minX, &maxX, &minY, &maxY, &diagPx ) ) {
		s_biasKeep++;
		return qtrue;
	}

	if ( diagPx > ( r_hiZLargeObjectPx ? r_hiZLargeObjectPx->value : 256.0f ) ) {
		s_biasKeep++;
		return qtrue;
	}

	/* NDC [-1,1] → host mip texel range */
	x0 = (int)floor( ( minX * 0.5f + 0.5f ) * (float)host->width );
	x1 = (int)ceil( ( maxX * 0.5f + 0.5f ) * (float)host->width ) - 1;
	y0 = (int)floor( ( minY * 0.5f + 0.5f ) * (float)host->height );
	y1 = (int)ceil( ( maxY * 0.5f + 0.5f ) * (float)host->height ) - 1;

	occluderFar = HIZ_SampleHostMin( x0, y0, x1, y1 );

	/*
	 * reversed-Z: object nearest depth is maxZ (closer → larger). Occluded when
	 * the entire AABB is farther than the farthest occluder in the region:
	 * maxZ (nearest point of object) <= occluderFar (farthest depth in Hi-Z).
	 */
	if ( maxZ <= occluderFar + 1e-4f ) {
		s_rejected++;
		s_hostSampleRejects++;
		return qfalse;
	}

	return qtrue;
}

void vk_hiz_status_f( void )
{
	ri.Printf( PRINT_ALL, "======== Hi-Z (Raster Ultra 1.6) ========\n" );
	ri.Printf( PRINT_ALL, "active     : %s (ready=%s gpuBuilt=%s cameraCut=%s)\n",
		vk_hiz_active() ? "yes" : "no",
		s_ready ? "yes" : "no",
		s_gpuBuilt ? "yes" : "no",
		s_cameraCut ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "pyramid    : %ux%u levels=%u layout=%u\n",
		s_pyramid.width, s_pyramid.height, s_pyramid.levels, (unsigned)s_pyramid.layout );
	ri.Printf( PRINT_ALL, "gpu        : pipeline=%s module=%s downsampleRuns=%u\n",
		s_compute.ready ? "yes" : "no",
		( vk.modules.hiz_downsample_cs != VK_NULL_HANDLE ) ? "yes" : "no",
		s_gpuBuildCount );
	ri.Printf( PRINT_ALL, "hostCache  : %ux%u level=%u valid=%s (1-frame lag)\n",
		s_host[s_hostRead].width, s_host[s_hostRead].height, s_host[s_hostRead].level,
		s_host[s_hostRead].valid ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "builds     : %u\n", s_buildCount );
	ri.Printf( PRINT_ALL, "tests      : %u rejected=%u biasKeep=%u hostRejects=%u\n",
		s_tests, s_rejected, s_biasKeep, s_hostSampleRejects );
	ri.Printf( PRINT_ALL, "debug      : r_hiZDebug=%d r_hizDebug=%d\n",
		r_hiZDebug ? r_hiZDebug->integer : 0,
		r_hizDebug ? r_hizDebug->integer : 0 );
	ri.Printf( PRINT_ALL, "note       : r_forwardPlusHiZ is tile probe pad — not this pyramid\n" );
	ri.Printf( PRINT_ALL, "policy     : minVisibleFrames=%d largeObjectPx=%.0f; reversed-Z min mip\n",
		r_hiZMinVisibleFrames ? r_hiZMinVisibleFrames->integer : 2,
		r_hiZLargeObjectPx ? r_hiZLargeObjectPx->value : 256.0f );
	ri.Printf( PRINT_ALL, "==========================================\n" );
}
