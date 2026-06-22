/*
 * Arc Blanc GPU ocean — Tessendorf FFT cascade update on compute queue.
 */
#include "../../tr_local.h"
#include "../../tr_common.h"
#include "../../vk.h"
#include "../../vk_cmd.h"
#include "../../vk_util.h"
#include "vk_arc_blanc_gpu.h"
#include "vk_arc_blanc.h"

#ifdef USE_ARC_BLANC
#include "../../../../world/arc_blanc/arc_blanc.h"
#endif

#define AB_GPU_MAX_N        256u
#define AB_GPU_CASCADE_COUNT 3u
#define AB_GPU_VEL_SAMPLES  8u

typedef struct {
	VkDescriptorSetLayout layout;
	VkPipelineLayout pipeline_layout;
	VkPipeline pipeline;
	VkDescriptorPool pool;
	VkDescriptorSet descriptor;
	qboolean ready;
} abGpuPipeline_t;

typedef struct {
	VkBuffer buf;
	VkDeviceMemory mem;
	void *ptr;
	VkDeviceSize size;
} abGpuBuffer_t;

typedef struct {
	abGpuBuffer_t h0;
	abGpuBuffer_t h0conj;
	abGpuBuffer_t omega;
	abGpuBuffer_t kMag;
	abGpuBuffer_t complexH;
	abGpuBuffer_t complexDx;
	abGpuBuffer_t complexDz;
	abGpuBuffer_t height;
	abGpuBuffer_t dispX;
	abGpuBuffer_t dispZ;
} abGpuCascadeBuf_t;

static abGpuPipeline_t s_htildePipe;
static abGpuPipeline_t s_fftPipe;
static abGpuPipeline_t s_extractPipe;
static abGpuPipeline_t s_combinePipe;
static abGpuPipeline_t s_velocityPipe;
static abGpuPipeline_t s_velocityAccumPipe;

static abGpuCascadeBuf_t s_cascade[AB_GPU_CASCADE_COUNT];
static abGpuBuffer_t s_combinedH;
static abGpuBuffer_t s_combinedDx;
static abGpuBuffer_t s_combinedDz;
static abGpuBuffer_t s_rgba;
static abGpuBuffer_t s_velComplexVx;
static abGpuBuffer_t s_velComplexVy;
static abGpuBuffer_t s_velComplexVz;
static abGpuBuffer_t s_velRealVx;
static abGpuBuffer_t s_velRealVy;
static abGpuBuffer_t s_velRealVz;
static abGpuBuffer_t s_velocitySlice;

static uint32_t s_gridN;
static qboolean s_bufsReady;
static qboolean s_loggedGpu;
static cvar_t *r_arcBlancGpu;

static void ABGpu_DestroyBuffer( abGpuBuffer_t *b )
{
	if ( !b ) {
		return;
	}
	if ( b->ptr ) {
		qvkUnmapMemory( vk.device, b->mem );
		b->ptr = NULL;
	}
	if ( b->buf != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, b->buf, NULL );
		b->buf = VK_NULL_HANDLE;
	}
	if ( b->mem != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, b->mem, NULL );
		b->mem = VK_NULL_HANDLE;
	}
	b->size = 0;
}

static qboolean ABGpu_CreateBuffer( abGpuBuffer_t *b, VkDeviceSize size, VkBufferUsageFlags usage )
{
	VkBufferCreateInfo bci;
	VkMemoryRequirements memReq;
	VkMemoryAllocateInfo mai;
	uint32_t memType;

	if ( !b || size == 0 ) {
		return qfalse;
	}

	ABGpu_DestroyBuffer( b );

	Com_Memset( &bci, 0, sizeof( bci ) );
	bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.size = size;
	bci.usage = usage;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &bci, NULL, &b->buf ) );

	qvkGetBufferMemoryRequirements( vk.device, b->buf, &memReq );
	Com_Memset( &mai, 0, sizeof( mai ) );
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.allocationSize = memReq.size;
	memType = vk_find_memory_type( vk.physical_device, memReq.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	mai.memoryTypeIndex = memType;
	VK_CHECK( qvkAllocateMemory( vk.device, &mai, NULL, &b->mem ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, b->buf, b->mem, 0 ) );
	VK_CHECK( qvkMapMemory( vk.device, b->mem, 0, size, 0, &b->ptr ) );
	b->size = size;
	return qtrue;
}

static void ABGpu_DestroyPipeline( abGpuPipeline_t *p )
{
	if ( !p ) {
		return;
	}
	if ( p->pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, p->pipeline, NULL );
	}
	if ( p->pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, p->pipeline_layout, NULL );
	}
	if ( p->layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, p->layout, NULL );
	}
	if ( p->pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, p->pool, NULL );
	}
	Com_Memset( p, 0, sizeof( *p ) );
}

static qboolean ABGpu_CreateComputePipeline( abGpuPipeline_t *p, VkShaderModule module,
	const VkDescriptorSetLayoutBinding *bindings, uint32_t bindingCount, uint32_t pushSize )
{
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;
	VkDescriptorPoolCreateInfo pci;
	VkDescriptorSetAllocateInfo ai;
	VkDescriptorPoolSize poolSize;
	uint32_t bufCount = 0;
	uint32_t i;

	if ( !p || module == VK_NULL_HANDLE || p->ready ) {
		return qfalse;
	}

	for ( i = 0; i < bindingCount; i++ ) {
		if ( bindings[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ) {
			bufCount += bindings[i].descriptorCount;
		}
	}

	Com_Memset( &layout_ci, 0, sizeof( layout_ci ) );
	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.bindingCount = bindingCount;
	layout_ci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &p->layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = pushSize;

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &p->layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &p->pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = module;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = p->pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, vk.pipelineCache, 1, &pipe_ci, NULL, &p->pipeline ) );

	Com_Memset( &poolSize, 0, sizeof( poolSize ) );
	poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSize.descriptorCount = bufCount > 0 ? bufCount : 1u;
	Com_Memset( &pci, 0, sizeof( pci ) );
	pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pci.maxSets = 1;
	pci.poolSizeCount = 1;
	pci.pPoolSizes = &poolSize;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &pci, NULL, &p->pool ) );

	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	ai.descriptorPool = p->pool;
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &p->layout;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &ai, &p->descriptor ) );

	p->ready = qtrue;
	return qtrue;
}

static void ABGpu_WriteBufferBinding( VkWriteDescriptorSet *w, VkDescriptorBufferInfo *info,
	VkDescriptorSet set, uint32_t binding, VkBuffer buf )
{
	info->buffer = buf;
	info->offset = 0;
	info->range = VK_WHOLE_SIZE;
	w->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	w->dstSet = set;
	w->dstBinding = binding;
	w->descriptorCount = 1;
	w->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	w->pBufferInfo = info;
}

static qboolean ABGpu_EnsurePipelines( void )
{
	VkDescriptorSetLayoutBinding htildeBinds[7];
	VkDescriptorSetLayoutBinding fftBinds[1];
	VkDescriptorSetLayoutBinding extractBinds[2];
	VkDescriptorSetLayoutBinding combineBinds[13];

	if ( s_htildePipe.ready && s_fftPipe.ready && s_extractPipe.ready && s_combinePipe.ready ) {
		return qtrue;
	}
	if ( vk.device == VK_NULL_HANDLE ) {
		return qfalse;
	}

	Com_Memset( htildeBinds, 0, sizeof( htildeBinds ) );
	for ( uint32_t i = 0; i < 7u; i++ ) {
		htildeBinds[i].binding = i;
		htildeBinds[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		htildeBinds[i].descriptorCount = 1;
		htildeBinds[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	}
	htildeBinds[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	if ( !s_htildePipe.ready && vk.modules.arc_blanc_htilde_cs != VK_NULL_HANDLE ) {
		if ( !ABGpu_CreateComputePipeline( &s_htildePipe, vk.modules.arc_blanc_htilde_cs,
			htildeBinds, 7, 16 ) ) {
			return qfalse;
		}
	}

	Com_Memset( fftBinds, 0, sizeof( fftBinds ) );
	fftBinds[0].binding = 0;
	fftBinds[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	fftBinds[0].descriptorCount = 1;
	fftBinds[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	if ( !s_fftPipe.ready && vk.modules.arc_blanc_fft_1d_cs != VK_NULL_HANDLE ) {
		if ( !ABGpu_CreateComputePipeline( &s_fftPipe, vk.modules.arc_blanc_fft_1d_cs,
			fftBinds, 1, 12 ) ) {
			return qfalse;
		}
	}

	Com_Memset( extractBinds, 0, sizeof( extractBinds ) );
	extractBinds[0].binding = 0;
	extractBinds[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	extractBinds[0].descriptorCount = 1;
	extractBinds[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	extractBinds[1].binding = 1;
	extractBinds[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	extractBinds[1].descriptorCount = 1;
	extractBinds[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	if ( !s_extractPipe.ready && vk.modules.arc_blanc_extract_cs != VK_NULL_HANDLE ) {
		if ( !ABGpu_CreateComputePipeline( &s_extractPipe, vk.modules.arc_blanc_extract_cs,
			extractBinds, 2, 8 ) ) {
			return qfalse;
		}
	}

	Com_Memset( combineBinds, 0, sizeof( combineBinds ) );
	for ( uint32_t i = 0; i < 13u; i++ ) {
		combineBinds[i].binding = i;
		combineBinds[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		combineBinds[i].descriptorCount = 1;
		combineBinds[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	}
	if ( !s_combinePipe.ready && vk.modules.arc_blanc_combine_cs != VK_NULL_HANDLE ) {
		if ( !ABGpu_CreateComputePipeline( &s_combinePipe, vk.modules.arc_blanc_combine_cs,
			combineBinds, 13, 16 ) ) {
			return qfalse;
		}
	}

	{
		VkDescriptorSetLayoutBinding velocityBinds[7];
		Com_Memset( velocityBinds, 0, sizeof( velocityBinds ) );
		for ( uint32_t i = 0; i < 7u; i++ ) {
			velocityBinds[i].binding = i;
			velocityBinds[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			velocityBinds[i].descriptorCount = 1;
			velocityBinds[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		}
		if ( !s_velocityPipe.ready && vk.modules.arc_blanc_velocity_cs != VK_NULL_HANDLE ) {
			if ( !ABGpu_CreateComputePipeline( &s_velocityPipe, vk.modules.arc_blanc_velocity_cs,
				velocityBinds, 7, 16 ) ) {
				return qfalse;
			}
		}
	}

	{
		VkDescriptorSetLayoutBinding accumBinds[4];
		Com_Memset( accumBinds, 0, sizeof( accumBinds ) );
		for ( uint32_t i = 0; i < 4u; i++ ) {
			accumBinds[i].binding = i;
			accumBinds[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			accumBinds[i].descriptorCount = 1;
			accumBinds[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		}
		if ( !s_velocityAccumPipe.ready && vk.modules.arc_blanc_velocity_accum_cs != VK_NULL_HANDLE ) {
			if ( !ABGpu_CreateComputePipeline( &s_velocityAccumPipe, vk.modules.arc_blanc_velocity_accum_cs,
				accumBinds, 4, 8 ) ) {
				return qfalse;
			}
		}
	}

	return s_htildePipe.ready && s_fftPipe.ready && s_extractPipe.ready && s_combinePipe.ready;
}

static qboolean ABGpu_EnsureBuffers( uint32_t gridN )
{
	VkDeviceSize complexBytes;
	VkDeviceSize realBytes;
	uint32_t c;

	if ( gridN < 16u || gridN > AB_GPU_MAX_N || ( gridN & ( gridN - 1u ) ) != 0u ) {
		return qfalse;
	}
	if ( s_bufsReady && s_gridN == gridN ) {
		return qtrue;
	}

	if ( s_bufsReady && s_gridN != gridN ) {
		uint32_t ci;
		for ( ci = 0; ci < AB_GPU_CASCADE_COUNT; ci++ ) {
			abGpuCascadeBuf_t *cb = &s_cascade[ci];
			ABGpu_DestroyBuffer( &cb->h0 );
			ABGpu_DestroyBuffer( &cb->h0conj );
			ABGpu_DestroyBuffer( &cb->omega );
			ABGpu_DestroyBuffer( &cb->kMag );
			ABGpu_DestroyBuffer( &cb->complexH );
			ABGpu_DestroyBuffer( &cb->complexDx );
			ABGpu_DestroyBuffer( &cb->complexDz );
			ABGpu_DestroyBuffer( &cb->height );
			ABGpu_DestroyBuffer( &cb->dispX );
			ABGpu_DestroyBuffer( &cb->dispZ );
		}
		ABGpu_DestroyBuffer( &s_combinedH );
		ABGpu_DestroyBuffer( &s_combinedDx );
		ABGpu_DestroyBuffer( &s_combinedDz );
		ABGpu_DestroyBuffer( &s_rgba );
		ABGpu_DestroyBuffer( &s_velComplexVx );
		ABGpu_DestroyBuffer( &s_velComplexVy );
		ABGpu_DestroyBuffer( &s_velComplexVz );
		ABGpu_DestroyBuffer( &s_velRealVx );
		ABGpu_DestroyBuffer( &s_velRealVy );
		ABGpu_DestroyBuffer( &s_velRealVz );
		ABGpu_DestroyBuffer( &s_velocitySlice );
		s_bufsReady = qfalse;
	}

	if ( !ABGpu_EnsurePipelines() ) {
		return qfalse;
	}

	complexBytes = (VkDeviceSize)gridN * (VkDeviceSize)gridN * sizeof( float ) * 2u;
	realBytes = (VkDeviceSize)gridN * (VkDeviceSize)gridN * sizeof( float );

	for ( c = 0; c < AB_GPU_CASCADE_COUNT; c++ ) {
		abGpuCascadeBuf_t *cb = &s_cascade[c];
		if ( !ABGpu_CreateBuffer( &cb->h0, complexBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ) ) {
			return qfalse;
		}
		if ( !ABGpu_CreateBuffer( &cb->h0conj, complexBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ) ) {
			return qfalse;
		}
		if ( !ABGpu_CreateBuffer( &cb->omega, realBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ) ) {
			return qfalse;
		}
		if ( !ABGpu_CreateBuffer( &cb->kMag, realBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ) ) {
			return qfalse;
		}
		if ( !ABGpu_CreateBuffer( &cb->complexH, complexBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ) ) {
			return qfalse;
		}
		if ( !ABGpu_CreateBuffer( &cb->complexDx, complexBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ) ) {
			return qfalse;
		}
		if ( !ABGpu_CreateBuffer( &cb->complexDz, complexBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ) ) {
			return qfalse;
		}
		if ( !ABGpu_CreateBuffer( &cb->height, realBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ) ) {
			return qfalse;
		}
		if ( !ABGpu_CreateBuffer( &cb->dispX, realBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ) ) {
			return qfalse;
		}
		if ( !ABGpu_CreateBuffer( &cb->dispZ, realBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ) ) {
			return qfalse;
		}
	}

	if ( !ABGpu_CreateBuffer( &s_combinedH, realBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ) ) {
		return qfalse;
	}
	if ( !ABGpu_CreateBuffer( &s_combinedDx, realBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ) ) {
		return qfalse;
	}
	if ( !ABGpu_CreateBuffer( &s_combinedDz, realBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ) ) {
		return qfalse;
	}
	if ( !ABGpu_CreateBuffer( &s_rgba, (VkDeviceSize)gridN * gridN * sizeof( uint32_t ),
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ) ) {
		return qfalse;
	}
	if ( !ABGpu_CreateBuffer( &s_velComplexVx, complexBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ) ) {
		return qfalse;
	}
	if ( !ABGpu_CreateBuffer( &s_velComplexVy, complexBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ) ) {
		return qfalse;
	}
	if ( !ABGpu_CreateBuffer( &s_velComplexVz, complexBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ) ) {
		return qfalse;
	}
	if ( !ABGpu_CreateBuffer( &s_velRealVx, realBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ) ) {
		return qfalse;
	}
	if ( !ABGpu_CreateBuffer( &s_velRealVy, realBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ) ) {
		return qfalse;
	}
	if ( !ABGpu_CreateBuffer( &s_velRealVz, realBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ) ) {
		return qfalse;
	}
	if ( !ABGpu_CreateBuffer( &s_velocitySlice, realBytes * 3u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ) ) {
		return qfalse;
	}

	s_gridN = gridN;
	s_bufsReady = qtrue;
	return qtrue;
}

static void ABGpu_DestroyAll( void )
{
	uint32_t c;

	for ( c = 0; c < AB_GPU_CASCADE_COUNT; c++ ) {
		abGpuCascadeBuf_t *cb = &s_cascade[c];
		ABGpu_DestroyBuffer( &cb->h0 );
		ABGpu_DestroyBuffer( &cb->h0conj );
		ABGpu_DestroyBuffer( &cb->omega );
		ABGpu_DestroyBuffer( &cb->kMag );
		ABGpu_DestroyBuffer( &cb->complexH );
		ABGpu_DestroyBuffer( &cb->complexDx );
		ABGpu_DestroyBuffer( &cb->complexDz );
		ABGpu_DestroyBuffer( &cb->height );
		ABGpu_DestroyBuffer( &cb->dispX );
		ABGpu_DestroyBuffer( &cb->dispZ );
	}
	ABGpu_DestroyBuffer( &s_combinedH );
	ABGpu_DestroyBuffer( &s_combinedDx );
	ABGpu_DestroyBuffer( &s_combinedDz );
	ABGpu_DestroyBuffer( &s_rgba );
	ABGpu_DestroyBuffer( &s_velComplexVx );
	ABGpu_DestroyBuffer( &s_velComplexVy );
	ABGpu_DestroyBuffer( &s_velComplexVz );
	ABGpu_DestroyBuffer( &s_velRealVx );
	ABGpu_DestroyBuffer( &s_velRealVy );
	ABGpu_DestroyBuffer( &s_velRealVz );
	ABGpu_DestroyBuffer( &s_velocitySlice );
	s_bufsReady = qfalse;
	s_gridN = 0;

	ABGpu_DestroyPipeline( &s_htildePipe );
	ABGpu_DestroyPipeline( &s_fftPipe );
	ABGpu_DestroyPipeline( &s_extractPipe );
	ABGpu_DestroyPipeline( &s_combinePipe );
	ABGpu_DestroyPipeline( &s_velocityPipe );
	ABGpu_DestroyPipeline( &s_velocityAccumPipe );
}

static void ABGpu_DispatchFft2D( VkCommandBuffer cmd, abGpuBuffer_t *complexBuf, uint32_t gridN )
{
	struct {
		uint32_t gridN;
		uint32_t lineIndex;
		uint32_t colPass;
	} push;
	uint32_t line;
	uint32_t groups = ( gridN + 255u ) / 256u;
	VkWriteDescriptorSet write;
	VkDescriptorBufferInfo info;

	Com_Memset( &write, 0, sizeof( write ) );
	ABGpu_WriteBufferBinding( &write, &info, s_fftPipe.descriptor, 0, complexBuf->buf );

	for ( line = 0; line < gridN; line++ ) {
		push.gridN = gridN;
		push.lineIndex = line;
		push.colPass = 0u;
		qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
		qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s_fftPipe.pipeline );
		qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s_fftPipe.pipeline_layout,
			0, 1, &s_fftPipe.descriptor, 0, NULL );
		qvkCmdPushConstants( cmd, s_fftPipe.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
			0, sizeof( push ), &push );
		qvkCmdDispatch( cmd, groups, 1, 1 );
	}

	for ( line = 0; line < gridN; line++ ) {
		push.gridN = gridN;
		push.lineIndex = line;
		push.colPass = 1u;
		qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
		qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s_fftPipe.pipeline );
		qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s_fftPipe.pipeline_layout,
			0, 1, &s_fftPipe.descriptor, 0, NULL );
		qvkCmdPushConstants( cmd, s_fftPipe.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
			0, sizeof( push ), &push );
		qvkCmdDispatch( cmd, groups, 1, 1 );
	}
}

static void ABGpu_DispatchExtract( VkCommandBuffer cmd, abGpuBuffer_t *src, abGpuBuffer_t *dst,
	uint32_t gridN, uint32_t cascadeIndex )
{
	struct {
		uint32_t gridN;
		uint32_t cascadeIndex;
	} push;
	VkWriteDescriptorSet writes[2];
	VkDescriptorBufferInfo infos[2];
	uint32_t groupsX = ( gridN + 7u ) / 8u;

	push.gridN = gridN;
	push.cascadeIndex = cascadeIndex;

	Com_Memset( writes, 0, sizeof( writes ) );
	Com_Memset( infos, 0, sizeof( infos ) );
	ABGpu_WriteBufferBinding( &writes[0], &infos[0], s_extractPipe.descriptor, 0, src->buf );
	ABGpu_WriteBufferBinding( &writes[1], &infos[1], s_extractPipe.descriptor, 1, dst->buf );
	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s_extractPipe.pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s_extractPipe.pipeline_layout,
		0, 1, &s_extractPipe.descriptor, 0, NULL );
	qvkCmdPushConstants( cmd, s_extractPipe.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof( push ), &push );
	qvkCmdDispatch( cmd, groupsX, groupsX, 1 );
}

static void ABGpu_DispatchHtilde( VkCommandBuffer cmd, abGpuCascadeBuf_t *cb, uint32_t gridN,
	float time, float tileLength )
{
	struct {
		uint32_t gridN;
		float time;
		float tileLength;
		float pad;
	} push;
	VkWriteDescriptorSet writes[7];
	VkDescriptorBufferInfo infos[7];
	uint32_t groupsX = ( gridN + 7u ) / 8u;
	int i;

	push.gridN = gridN;
	push.time = time;
	push.tileLength = tileLength;
	push.pad = 0.0f;

	Com_Memset( writes, 0, sizeof( writes ) );
	Com_Memset( infos, 0, sizeof( infos ) );
	ABGpu_WriteBufferBinding( &writes[0], &infos[0], s_htildePipe.descriptor, 0, cb->h0.buf );
	ABGpu_WriteBufferBinding( &writes[1], &infos[1], s_htildePipe.descriptor, 1, cb->h0conj.buf );
	ABGpu_WriteBufferBinding( &writes[2], &infos[2], s_htildePipe.descriptor, 2, cb->omega.buf );
	ABGpu_WriteBufferBinding( &writes[3], &infos[3], s_htildePipe.descriptor, 3, cb->kMag.buf );
	ABGpu_WriteBufferBinding( &writes[4], &infos[4], s_htildePipe.descriptor, 4, cb->complexH.buf );
	ABGpu_WriteBufferBinding( &writes[5], &infos[5], s_htildePipe.descriptor, 5, cb->complexDx.buf );
	ABGpu_WriteBufferBinding( &writes[6], &infos[6], s_htildePipe.descriptor, 6, cb->complexDz.buf );
	qvkUpdateDescriptorSets( vk.device, 7, writes, 0, NULL );

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s_htildePipe.pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s_htildePipe.pipeline_layout,
		0, 1, &s_htildePipe.descriptor, 0, NULL );
	qvkCmdPushConstants( cmd, s_htildePipe.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof( push ), &push );
	qvkCmdDispatch( cmd, groupsX, groupsX, 1 );
}

static void ABGpu_DispatchCombine( VkCommandBuffer cmd, uint32_t gridN, float minH, float maxH,
	qboolean packRgba )
{
	struct {
		uint32_t gridN;
		float minHeight;
		float maxHeight;
		uint32_t packRgba;
	} push;
	VkWriteDescriptorSet writes[13];
	VkDescriptorBufferInfo infos[13];
	uint32_t groupsX = ( gridN + 7u ) / 8u;

	push.gridN = gridN;
	push.minHeight = minH;
	push.maxHeight = maxH;
	push.packRgba = packRgba ? 1u : 0u;

	Com_Memset( writes, 0, sizeof( writes ) );
	Com_Memset( infos, 0, sizeof( infos ) );
	ABGpu_WriteBufferBinding( &writes[0], &infos[0], s_combinePipe.descriptor, 0, s_cascade[0].height.buf );
	ABGpu_WriteBufferBinding( &writes[1], &infos[1], s_combinePipe.descriptor, 1, s_cascade[1].height.buf );
	ABGpu_WriteBufferBinding( &writes[2], &infos[2], s_combinePipe.descriptor, 2, s_cascade[2].height.buf );
	ABGpu_WriteBufferBinding( &writes[3], &infos[3], s_combinePipe.descriptor, 3, s_cascade[0].dispX.buf );
	ABGpu_WriteBufferBinding( &writes[4], &infos[4], s_combinePipe.descriptor, 4, s_cascade[1].dispX.buf );
	ABGpu_WriteBufferBinding( &writes[5], &infos[5], s_combinePipe.descriptor, 5, s_cascade[2].dispX.buf );
	ABGpu_WriteBufferBinding( &writes[6], &infos[6], s_combinePipe.descriptor, 6, s_cascade[0].dispZ.buf );
	ABGpu_WriteBufferBinding( &writes[7], &infos[7], s_combinePipe.descriptor, 7, s_cascade[1].dispZ.buf );
	ABGpu_WriteBufferBinding( &writes[8], &infos[8], s_combinePipe.descriptor, 8, s_cascade[2].dispZ.buf );
	ABGpu_WriteBufferBinding( &writes[9], &infos[9], s_combinePipe.descriptor, 9, s_combinedH.buf );
	ABGpu_WriteBufferBinding( &writes[10], &infos[10], s_combinePipe.descriptor, 10, s_combinedDx.buf );
	ABGpu_WriteBufferBinding( &writes[11], &infos[11], s_combinePipe.descriptor, 11, s_combinedDz.buf );
	ABGpu_WriteBufferBinding( &writes[12], &infos[12], s_combinePipe.descriptor, 12, s_rgba.buf );
	qvkUpdateDescriptorSets( vk.device, 13, writes, 0, NULL );

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s_combinePipe.pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s_combinePipe.pipeline_layout,
		0, 1, &s_combinePipe.descriptor, 0, NULL );
	qvkCmdPushConstants( cmd, s_combinePipe.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof( push ), &push );
	qvkCmdDispatch( cmd, groupsX, groupsX, 1 );
}

static void ABGpu_DispatchVelocityFreq( VkCommandBuffer cmd, abGpuCascadeBuf_t *cb, uint32_t gridN,
	float time, float tileLength, float depthY )
{
	struct {
		uint32_t gridN;
		float time;
		float tileLength;
		float depthY;
	} push;
	VkWriteDescriptorSet writes[7];
	VkDescriptorBufferInfo infos[7];
	uint32_t groupsX = ( gridN + 7u ) / 8u;

	push.gridN = gridN;
	push.time = time;
	push.tileLength = tileLength;
	push.depthY = depthY;

	Com_Memset( writes, 0, sizeof( writes ) );
	Com_Memset( infos, 0, sizeof( infos ) );
	ABGpu_WriteBufferBinding( &writes[0], &infos[0], s_velocityPipe.descriptor, 0, cb->h0.buf );
	ABGpu_WriteBufferBinding( &writes[1], &infos[1], s_velocityPipe.descriptor, 1, cb->h0conj.buf );
	ABGpu_WriteBufferBinding( &writes[2], &infos[2], s_velocityPipe.descriptor, 2, cb->omega.buf );
	ABGpu_WriteBufferBinding( &writes[3], &infos[3], s_velocityPipe.descriptor, 3, cb->kMag.buf );
	ABGpu_WriteBufferBinding( &writes[4], &infos[4], s_velocityPipe.descriptor, 4, s_velComplexVx.buf );
	ABGpu_WriteBufferBinding( &writes[5], &infos[5], s_velocityPipe.descriptor, 5, s_velComplexVy.buf );
	ABGpu_WriteBufferBinding( &writes[6], &infos[6], s_velocityPipe.descriptor, 6, s_velComplexVz.buf );
	qvkUpdateDescriptorSets( vk.device, 7, writes, 0, NULL );

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s_velocityPipe.pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s_velocityPipe.pipeline_layout,
		0, 1, &s_velocityPipe.descriptor, 0, NULL );
	qvkCmdPushConstants( cmd, s_velocityPipe.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof( push ), &push );
	qvkCmdDispatch( cmd, groupsX, groupsX, 1 );
}

static void ABGpu_DispatchVelocityAccum( VkCommandBuffer cmd, uint32_t gridN, qboolean clearSlice )
{
	struct {
		uint32_t gridN;
		uint32_t clearSlice;
	} push;
	VkWriteDescriptorSet writes[4];
	VkDescriptorBufferInfo infos[4];
	uint32_t groupsX = ( gridN + 7u ) / 8u;

	push.gridN = gridN;
	push.clearSlice = clearSlice ? 1u : 0u;

	Com_Memset( writes, 0, sizeof( writes ) );
	Com_Memset( infos, 0, sizeof( infos ) );
	ABGpu_WriteBufferBinding( &writes[0], &infos[0], s_velocityAccumPipe.descriptor, 0, s_velocitySlice.buf );
	ABGpu_WriteBufferBinding( &writes[1], &infos[1], s_velocityAccumPipe.descriptor, 1, s_velRealVx.buf );
	ABGpu_WriteBufferBinding( &writes[2], &infos[2], s_velocityAccumPipe.descriptor, 2, s_velRealVy.buf );
	ABGpu_WriteBufferBinding( &writes[3], &infos[3], s_velocityAccumPipe.descriptor, 3, s_velRealVz.buf );
	qvkUpdateDescriptorSets( vk.device, 4, writes, 0, NULL );

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s_velocityAccumPipe.pipeline );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s_velocityAccumPipe.pipeline_layout,
		0, 1, &s_velocityAccumPipe.descriptor, 0, NULL );
	qvkCmdPushConstants( cmd, s_velocityAccumPipe.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
		0, sizeof( push ), &push );
	qvkCmdDispatch( cmd, groupsX, groupsX, 1 );
}

static void ABGpu_UpdateVelocitySlices( VkCommandBuffer cmd, const arcBlancGpuParams_t *params, uint32_t gridN )
{
	uint32_t s, c;
	const VkDeviceSize sliceBytes = (VkDeviceSize)gridN * gridN * sizeof( float ) * 3u;

	if ( !params || !s_velocityPipe.ready || !s_velocityAccumPipe.ready ) {
		return;
	}

	for ( s = 0; s < AB_GPU_VEL_SAMPLES; s++ ) {
		ABGpu_DispatchVelocityAccum( cmd, gridN, qtrue );

		for ( c = 0; c < AB_GPU_CASCADE_COUNT; c++ ) {
			abGpuCascadeBuf_t *cb = &s_cascade[c];
			float tileLen = params->tileLength[c] > 0.0f ? params->tileLength[c] : 256.0f;
			float depthY = params->depthSamples[s];

			ABGpu_DispatchVelocityFreq( cmd, cb, gridN, params->time, tileLen, depthY );

			ABGpu_DispatchFft2D( cmd, &s_velComplexVx, gridN );
			ABGpu_DispatchExtract( cmd, &s_velComplexVx, &s_velRealVx, gridN, c );

			ABGpu_DispatchFft2D( cmd, &s_velComplexVy, gridN );
			ABGpu_DispatchExtract( cmd, &s_velComplexVy, &s_velRealVy, gridN, c );

			ABGpu_DispatchFft2D( cmd, &s_velComplexVz, gridN );
			ABGpu_DispatchExtract( cmd, &s_velComplexVz, &s_velRealVz, gridN, c );

			ABGpu_DispatchVelocityAccum( cmd, gridN, qfalse );
		}

		if ( params->outVelocitySlice[s] && s_velocitySlice.ptr ) {
			Com_Memcpy( params->outVelocitySlice[s], s_velocitySlice.ptr, (size_t)sliceBytes );
		}
	}
}

void VK_ArcBlancGpu_Init( void )
{
	r_arcBlancGpu = ri.Cvar_Get( "r_arcBlancGpu", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_arcBlancGpu,
		"Arc Blanc GPU FFT ocean: 0=CPU, 1=GPU compute + readback for physics." );
	s_loggedGpu = qfalse;
}

void VK_ArcBlancGpu_Shutdown( void )
{
	ABGpu_DestroyAll();
	s_loggedGpu = qfalse;
}

qboolean RE_ArcBlancGpuOceanStep( const arcBlancGpuParams_t *params )
{
#ifdef USE_ARC_BLANC
	VkCommandBuffer cmd;
	uint32_t gridN;
	uint32_t c;
	uint32_t n2;
	VkDeviceSize complexBytes;
	VkDeviceSize realBytes;

	if ( !params || !r_arcBlancGpu || r_arcBlancGpu->integer <= 0 ) {
		return qfalse;
	}
	if ( vk.device == VK_NULL_HANDLE ) {
		return qfalse;
	}
	if ( !params->h0[0] || !params->h0conj[0] || !params->omega[0] || !params->kMag[0] ) {
		return qfalse;
	}

	gridN = (uint32_t)params->gridN;
	if ( !ABGpu_EnsureBuffers( gridN ) ) {
		return qfalse;
	}

	if ( !s_loggedGpu ) {
		s_loggedGpu = qtrue;
		ri.Printf( PRINT_ALL, "[arc_blanc] GPU FFT ocean enabled (grid=%u)\n", gridN );
	}

	n2 = gridN * gridN;
	complexBytes = (VkDeviceSize)n2 * sizeof( float ) * 2u;
	realBytes = (VkDeviceSize)n2 * sizeof( float );

	for ( c = 0; c < AB_GPU_CASCADE_COUNT; c++ ) {
		abGpuCascadeBuf_t *cb = &s_cascade[c];
		if ( params->h0[c] ) {
			Com_Memcpy( cb->h0.ptr, params->h0[c], (size_t)complexBytes );
		}
		if ( params->h0conj[c] ) {
			Com_Memcpy( cb->h0conj.ptr, params->h0conj[c], (size_t)complexBytes );
		}
		if ( params->omega[c] ) {
			Com_Memcpy( cb->omega.ptr, params->omega[c], (size_t)realBytes );
		}
		if ( params->kMag[c] ) {
			Com_Memcpy( cb->kMag.ptr, params->kMag[c], (size_t)realBytes );
		}
	}

	cmd = vk_begin_command_buffer();
	if ( cmd == VK_NULL_HANDLE ) {
		return qfalse;
	}

	for ( c = 0; c < AB_GPU_CASCADE_COUNT; c++ ) {
		abGpuCascadeBuf_t *cb = &s_cascade[c];
		float tileLen = params->tileLength[c] > 0.0f ? params->tileLength[c] : 256.0f;

		ABGpu_DispatchHtilde( cmd, cb, gridN, params->time, tileLen );

		ABGpu_DispatchFft2D( cmd, &cb->complexH, gridN );
		ABGpu_DispatchExtract( cmd, &cb->complexH, &cb->height, gridN, c );

		ABGpu_DispatchFft2D( cmd, &cb->complexDx, gridN );
		ABGpu_DispatchExtract( cmd, &cb->complexDx, &cb->dispX, gridN, c );

		ABGpu_DispatchFft2D( cmd, &cb->complexDz, gridN );
		ABGpu_DispatchExtract( cmd, &cb->complexDz, &cb->dispZ, gridN, c );
	}

	ABGpu_DispatchCombine( cmd, gridN, params->minHeight, params->maxHeight,
		params->outRgba != NULL );

	if ( params->updateVelocityGpu && params->outVelocitySlice[0] ) {
		ABGpu_UpdateVelocitySlices( cmd, params, gridN );
	}

	vk_end_command_buffer( cmd, "RE_ArcBlancGpuOceanStep" );

	if ( params->outCombinedHeight && s_combinedH.ptr ) {
		Com_Memcpy( params->outCombinedHeight, s_combinedH.ptr, (size_t)realBytes );
	}
	if ( params->outCombinedDispX && s_combinedDx.ptr ) {
		Com_Memcpy( params->outCombinedDispX, s_combinedDx.ptr, (size_t)realBytes );
	}
	if ( params->outCombinedDispZ && s_combinedDz.ptr ) {
		Com_Memcpy( params->outCombinedDispZ, s_combinedDz.ptr, (size_t)realBytes );
	}
	if ( params->outRgba && s_rgba.ptr ) {
		const uint32_t *src = (const uint32_t *)s_rgba.ptr;
		byte *dst = params->outRgba;
		uint32_t i;
		if ( (int)( n2 * 4u ) <= params->rgbaMaxBytes ) {
			for ( i = 0; i < n2; i++ ) {
				uint32_t p = src[i];
				dst[i * 4 + 0] = (byte)( p & 0xffu );
				dst[i * 4 + 1] = (byte)( ( p >> 8 ) & 0xffu );
				dst[i * 4 + 2] = (byte)( ( p >> 16 ) & 0xffu );
				dst[i * 4 + 3] = (byte)( ( p >> 24 ) & 0xffu );
			}
			if ( params->outWidth ) {
				*params->outWidth = (int)gridN;
			}
			if ( params->outHeight ) {
				*params->outHeight = (int)gridN;
			}
		}
	}

	return qtrue;
#else
	(void)params;
	return qfalse;
#endif
}
