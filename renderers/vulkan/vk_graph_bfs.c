/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_graph_bfs.h"
#include "vk_util.h"
#include "vk_cmd.h"
#include "../../world/sector_graph.h"

#define GRAPH_BFS_MAX_NODES 4096u

typedef struct {
	VkDescriptorSetLayout layout;
	VkPipelineLayout pipeline_layout;
	VkPipeline pipeline;
	VkDescriptorPool pool;
	VkDescriptorSet descriptor;
	qboolean ready;
} graphBfsPipeline_t;

typedef struct {
	VkBuffer rowPtr;
	VkDeviceMemory rowPtr_mem;
	void *rowPtr_ptr;

	VkBuffer colIdx;
	VkDeviceMemory colIdx_mem;
	void *colIdx_ptr;

	VkBuffer frontierA;
	VkDeviceMemory frontierA_mem;
	void *frontierA_ptr;

	VkBuffer frontierB;
	VkDeviceMemory frontierB_mem;
	void *frontierB_ptr;

	VkBuffer visited;
	VkDeviceMemory visited_mem;
	void *visited_ptr;

	uint32_t capacity;
	qboolean ready;
} graphBfsBuffers_t;

static graphBfsPipeline_t s_pipe;
static graphBfsBuffers_t s_buf;
static qboolean s_registered;

static qboolean GraphBfs_CreateBuffer( VkDeviceSize size, VkBufferUsageFlags usage,
	VkBuffer *outBuf, VkDeviceMemory *outMem, void **outPtr )
{
	VkBufferCreateInfo bci;
	VkMemoryRequirements memReq;
	VkMemoryAllocateInfo mai;
	uint32_t memType;

	if ( !outBuf || !outMem ) {
		return qfalse;
	}

	Com_Memset( &bci, 0, sizeof( bci ) );
	bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.size = size;
	bci.usage = usage;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK( qvkCreateBuffer( vk.device, &bci, NULL, outBuf ) );

	qvkGetBufferMemoryRequirements( vk.device, *outBuf, &memReq );
	Com_Memset( &mai, 0, sizeof( mai ) );
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.allocationSize = memReq.size;
	memType = vk_find_memory_type( vk.physical_device, memReq.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	mai.memoryTypeIndex = memType;
	VK_CHECK( qvkAllocateMemory( vk.device, &mai, NULL, outMem ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, *outBuf, *outMem, 0 ) );

	if ( outPtr ) {
		VK_CHECK( qvkMapMemory( vk.device, *outMem, 0, size, 0, outPtr ) );
	}
	return qtrue;
}

static void GraphBfs_DestroyBuffer( VkBuffer *buf, VkDeviceMemory *mem, void **ptr )
{
	if ( ptr && *ptr ) {
		qvkUnmapMemory( vk.device, *mem );
		*ptr = NULL;
	}
	if ( *buf != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, *buf, NULL );
		*buf = VK_NULL_HANDLE;
	}
	if ( *mem != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, *mem, NULL );
		*mem = VK_NULL_HANDLE;
	}
}

static void GraphBfs_DestroyBuffers( void )
{
	GraphBfs_DestroyBuffer( &s_buf.rowPtr, &s_buf.rowPtr_mem, &s_buf.rowPtr_ptr );
	GraphBfs_DestroyBuffer( &s_buf.colIdx, &s_buf.colIdx_mem, &s_buf.colIdx_ptr );
	GraphBfs_DestroyBuffer( &s_buf.frontierA, &s_buf.frontierA_mem, &s_buf.frontierA_ptr );
	GraphBfs_DestroyBuffer( &s_buf.frontierB, &s_buf.frontierB_mem, &s_buf.frontierB_ptr );
	GraphBfs_DestroyBuffer( &s_buf.visited, &s_buf.visited_mem, &s_buf.visited_ptr );
	s_buf.ready = qfalse;
	s_buf.capacity = 0;
}

static void GraphBfs_DestroyGpu( void )
{
	GraphBfs_DestroyBuffers();

	if ( s_pipe.pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, s_pipe.pipeline, NULL );
	}
	if ( s_pipe.pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, s_pipe.pipeline_layout, NULL );
	}
	if ( s_pipe.layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, s_pipe.layout, NULL );
	}
	if ( s_pipe.pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, s_pipe.pool, NULL );
	}
	Com_Memset( &s_pipe, 0, sizeof( s_pipe ) );
}

static qboolean GraphBfs_EnsurePipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[5];
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;
	VkDescriptorPoolCreateInfo pci;
	VkDescriptorSetAllocateInfo ai;
	VkDescriptorPoolSize poolSize;

	if ( s_pipe.ready ) {
		return qtrue;
	}
	if ( vk.device == VK_NULL_HANDLE || vk.modules.graph_bfs_expand_cs == VK_NULL_HANDLE ) {
		return qfalse;
	}

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[3].binding = 3;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[3].descriptorCount = 1;
	bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[4].binding = 4;
	bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[4].descriptorCount = 1;
	bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	Com_Memset( &layout_ci, 0, sizeof( layout_ci ) );
	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.bindingCount = 5;
	layout_ci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &s_pipe.layout ) );

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( uint32_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &s_pipe.layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &s_pipe.pipeline_layout ) );

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.graph_bfs_expand_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = s_pipe.pipeline_layout;
	VK_CHECK( qvkCreateComputePipelines( vk.device, vk.pipelineCache, 1, &pipe_ci, NULL, &s_pipe.pipeline ) );

	Com_Memset( &poolSize, 0, sizeof( poolSize ) );
	poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSize.descriptorCount = 5;
	Com_Memset( &pci, 0, sizeof( pci ) );
	pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pci.maxSets = 1;
	pci.poolSizeCount = 1;
	pci.pPoolSizes = &poolSize;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &pci, NULL, &s_pipe.pool ) );

	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	ai.descriptorPool = s_pipe.pool;
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &s_pipe.layout;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &ai, &s_pipe.descriptor ) );

	s_pipe.ready = qtrue;
	return qtrue;
}

static qboolean GraphBfs_EnsureBuffers( uint32_t nodeCount, uint32_t edgeCount )
{
	VkDeviceSize uSize;
	VkDeviceSize rowSize;
	VkDeviceSize colSize;

	if ( nodeCount > GRAPH_BFS_MAX_NODES ) {
		nodeCount = GRAPH_BFS_MAX_NODES;
	}
	if ( s_buf.ready && s_buf.capacity >= nodeCount ) {
		return qtrue;
	}

	GraphBfs_DestroyBuffers();
	if ( !GraphBfs_EnsurePipeline() ) {
		return qfalse;
	}

	uSize = (VkDeviceSize)nodeCount * sizeof( uint32_t );
	rowSize = (VkDeviceSize)( nodeCount + 1u ) * sizeof( uint32_t );
	colSize = (VkDeviceSize)edgeCount * sizeof( uint32_t );
	if ( colSize < uSize ) {
		colSize = uSize * 8u;
	}

	if ( !GraphBfs_CreateBuffer( rowSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		&s_buf.rowPtr, &s_buf.rowPtr_mem, &s_buf.rowPtr_ptr ) ) {
		return qfalse;
	}
	if ( !GraphBfs_CreateBuffer( colSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		&s_buf.colIdx, &s_buf.colIdx_mem, &s_buf.colIdx_ptr ) ) {
		return qfalse;
	}
	if ( !GraphBfs_CreateBuffer( uSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		&s_buf.frontierA, &s_buf.frontierA_mem, &s_buf.frontierA_ptr ) ) {
		return qfalse;
	}
	if ( !GraphBfs_CreateBuffer( uSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		&s_buf.frontierB, &s_buf.frontierB_mem, &s_buf.frontierB_ptr ) ) {
		return qfalse;
	}
	if ( !GraphBfs_CreateBuffer( uSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		&s_buf.visited, &s_buf.visited_mem, &s_buf.visited_ptr ) ) {
		return qfalse;
	}

	s_buf.capacity = nodeCount;
	s_buf.ready = qtrue;
	return qtrue;
}

static qboolean GraphBfs_DispatchReach( const sectorGraphGpuQuery_t *query, uint32_t *reachBits, int bitWords )
{
	VkCommandBuffer cmd;
	uint32_t *rowPtr;
	uint32_t *colIdx;
	uint32_t *frontierIn;
	uint32_t *frontierOut;
	uint32_t *visited;
	uint32_t nodeCount;
	uint32_t edgeCount;
	int hop;
	int i;
	VkWriteDescriptorSet writes[5];
	VkDescriptorBufferInfo infos[5];
	uint32_t pushNodeCount;
	uint32_t groups;
	qboolean useA;

	if ( !query || !reachBits || bitWords <= 0 || query->nodeCount <= 0 ) {
		return qfalse;
	}
	if ( vk.device == VK_NULL_HANDLE ) {
		return qfalse;
	}

	nodeCount = (uint32_t)query->nodeCount;
	edgeCount = query->rowPtr ? (uint32_t)query->rowPtr[nodeCount] : 0u;
	if ( !GraphBfs_EnsureBuffers( nodeCount, edgeCount ) ) {
		return qfalse;
	}

	rowPtr = (uint32_t *)s_buf.rowPtr_ptr;
	colIdx = (uint32_t *)s_buf.colIdx_ptr;
	frontierIn = (uint32_t *)s_buf.frontierA_ptr;
	frontierOut = (uint32_t *)s_buf.frontierB_ptr;
	visited = (uint32_t *)s_buf.visited_ptr;

	Com_Memset( rowPtr, 0, (size_t)( nodeCount + 1u ) * sizeof( uint32_t ) );
	Com_Memset( colIdx, 0, (size_t)edgeCount * sizeof( uint32_t ) );
	for ( i = 0; i <= query->nodeCount; i++ ) {
		rowPtr[i] = (uint32_t)query->rowPtr[i];
	}
	for ( i = 0; i < (int)edgeCount; i++ ) {
		colIdx[i] = (uint32_t)query->colIdx[i];
	}

	Com_Memset( frontierIn, 0, (size_t)nodeCount * sizeof( uint32_t ) );
	Com_Memset( frontierOut, 0, (size_t)nodeCount * sizeof( uint32_t ) );
	Com_Memset( visited, 0, (size_t)nodeCount * sizeof( uint32_t ) );
	for ( i = 0; i < query->sourceCount; i++ ) {
		int sid = query->sourceNodeIds[i];
		if ( sid >= 0 && sid < query->nodeCount ) {
			frontierIn[sid] = 1u;
			visited[sid] = 1u;
		}
	}

	Com_Memset( infos, 0, sizeof( infos ) );
	infos[0].buffer = s_buf.rowPtr;
	infos[0].offset = 0;
	infos[0].range = VK_WHOLE_SIZE;
	infos[1].buffer = s_buf.colIdx;
	infos[1].offset = 0;
	infos[1].range = VK_WHOLE_SIZE;
	infos[2].buffer = s_buf.frontierA;
	infos[2].offset = 0;
	infos[2].range = VK_WHOLE_SIZE;
	infos[3].buffer = s_buf.frontierB;
	infos[3].offset = 0;
	infos[3].range = VK_WHOLE_SIZE;
	infos[4].buffer = s_buf.visited;
	infos[4].offset = 0;
	infos[4].range = VK_WHOLE_SIZE;

	Com_Memset( writes, 0, sizeof( writes ) );
	for ( i = 0; i < 5; i++ ) {
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].dstSet = s_pipe.descriptor;
		writes[i].dstBinding = (uint32_t)i;
		writes[i].descriptorCount = 1;
		writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[i].pBufferInfo = &infos[i];
	}
	qvkUpdateDescriptorSets( vk.device, 5, writes, 0, NULL );

	cmd = vk_begin_command_buffer();
	if ( cmd == VK_NULL_HANDLE ) {
		return qfalse;
	}

	pushNodeCount = nodeCount;
	groups = ( nodeCount + 63u ) / 64u;
	useA = qtrue;

	for ( hop = 0; hop < query->maxHops; hop++ ) {
		if ( useA ) {
			infos[2].buffer = s_buf.frontierA;
			infos[3].buffer = s_buf.frontierB;
		} else {
			infos[2].buffer = s_buf.frontierB;
			infos[3].buffer = s_buf.frontierA;
		}
		writes[2].pBufferInfo = &infos[2];
		writes[3].pBufferInfo = &infos[3];
		qvkUpdateDescriptorSets( vk.device, 2, &writes[2], 0, NULL );

		qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s_pipe.pipeline );
		qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, s_pipe.pipeline_layout,
			0, 1, &s_pipe.descriptor, 0, NULL );
		qvkCmdPushConstants( cmd, s_pipe.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
			0, sizeof( pushNodeCount ), &pushNodeCount );
		qvkCmdDispatch( cmd, groups, 1, 1 );

		useA = !useA;
	}

	vk_end_command_buffer( cmd, "GraphBfs_DispatchReach" );

	Com_Memset( reachBits, 0, (size_t)bitWords * sizeof( reachBits[0] ) );
	for ( i = 0; i < query->nodeCount; i++ ) {
		if ( visited[i] ) {
			reachBits[i >> 5] |= ( 1u << ( i & 31 ) );
		}
	}
	return qtrue;
}

static qboolean GraphBfs_GpuReach( const sectorGraphGpuQuery_t *query,
	uint32_t *reachBits, int bitWords )
{
	return GraphBfs_DispatchReach( query, reachBits, bitWords );
}

static void GraphBfs_Cmd_Bench( void )
{
	sectorGraphGpuQuery_t query;
	uint32_t cpuBits[SECTOR_GRAPH_BITWORDS];
	uint32_t gpuBits[SECTOR_GRAPH_BITWORDS];
	int rowPtr[26];
	int colIdx[80];
	int sources[1];
	int x, y, nodeId, e = 0;
	int64_t t0, t1;
	int i;

	if ( !GraphBfs_EnsurePipeline() ) {
		ri.Printf( PRINT_WARNING, "[GraphBfs] pipeline not ready (r_graphCompute 1 + vid_restart)\n" );
		return;
	}

	Com_Memset( rowPtr, 0, sizeof( rowPtr ) );
	nodeId = 0;
	for ( y = 0; y < 5; y++ ) {
		for ( x = 0; x < 5; x++ ) {
			rowPtr[nodeId] = e;
			if ( x + 1 < 5 ) {
				colIdx[e++] = nodeId + 1;
			}
			if ( x > 0 ) {
				colIdx[e++] = nodeId - 1;
			}
			if ( y + 1 < 5 ) {
				colIdx[e++] = nodeId + 5;
			}
			if ( y > 0 ) {
				colIdx[e++] = nodeId - 5;
			}
			nodeId++;
		}
	}
	rowPtr[25] = e;
	sources[0] = 0;

	Com_Memset( &query, 0, sizeof( query ) );
	query.nodeCount = 25;
	query.maxHops = 8;
	query.sourceCount = 1;
	query.sourceNodeIds = sources;
	query.rowPtr = rowPtr;
	query.colIdx = colIdx;

	t0 = ri.Milliseconds();
	for ( i = 0; i < 100; i++ ) {
		Com_Memset( cpuBits, 0, sizeof( cpuBits ) );
		(void)GraphBfs_DispatchReach( &query, cpuBits, SECTOR_GRAPH_BITWORDS );
	}
	t1 = ri.Milliseconds();

	Com_Memset( gpuBits, 0, sizeof( gpuBits ) );
	(void)GraphBfs_DispatchReach( &query, gpuBits, SECTOR_GRAPH_BITWORDS );

	ri.Printf( PRINT_ALL, "[GraphBfs] bench 5x5 grid 100x dispatch ~%lld ms (single ~%lld ms)\n",
		(long long)( t1 - t0 ), (long long)( t1 - t0 ) / 100 );
	ri.Printf( PRINT_ALL, "[GraphBfs] reachable corner (4,4): cpu=%s gpu=%s\n",
		( cpuBits[0] & ( 1u << 0 ) ) ? "src" : "-",
		( gpuBits[24 >> 5] & ( 1u << ( 24 & 31 ) ) ) ? "yes" : "no" );
}

qboolean R_GraphBfs_Active( void )
{
	return s_pipe.ready && s_buf.ready;
}

void R_GraphBfs_Init( void )
{
	if ( !s_registered ) {
		SectorGraph_SetGpuReachFn( GraphBfs_GpuReach );
		s_registered = qtrue;
	}

	ri.Cmd_AddCommand( "graph_bfs_bench", GraphBfs_Cmd_Bench );

	if ( ri.Cvar_VariableIntegerValue( "r_graphCompute" ) ) {
		(void)GraphBfs_EnsurePipeline();
		ri.Printf( PRINT_ALL, "[GraphBfs] compute path registered (graph_bfs_bench)\n" );
	}
}

void R_GraphBfs_Shutdown( void )
{
	ri.Cmd_RemoveCommand( "graph_bfs_bench" );
	if ( s_registered ) {
		SectorGraph_SetGpuReachFn( NULL );
		s_registered = qfalse;
	}
	GraphBfs_DestroyGpu();
}
