/*
=============================================================================
GPU-Driven Rendering Pipeline Implementation
=============================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_gpu_culling.h"

#ifdef USE_VULKAN

// CVars (extern declarations - defined in tr_init.c)
extern cvar_t *r_gpuCulling;
extern cvar_t *r_gpuInstancing;
extern cvar_t *r_cullDistance;

static gpu_instance_t *gpuInstances;
static uint32_t gpuInstanceWriteIndex = 0;

/*
=============================================================================
GPU Culling Initialization
=============================================================================
*/
void vk_gpu_culling_init( void )
{
	if ( vk.gpuCulling.initialized ) {
		return;
	}
	
	ri.Printf( PRINT_ALL, "Initializing GPU-driven culling system...\n" );
	
	// Initialize instance buffer
	vk.gpuCulling.instanceCapacity = MAX_INSTANCE_COUNT;
	vk.gpuCulling.instanceCount = 0;
	
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = vk.gpuCulling.instanceCapacity * sizeof( gpu_instance_t );
	bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	
	VK_CHECK( qvkCreateBuffer( vk.device, &bufferInfo, NULL, &vk.gpuCulling.instanceBuffer ) );
	
	VkMemoryRequirements memReqs;
	qvkGetBufferMemoryRequirements( vk.device, vk.gpuCulling.instanceBuffer, &memReqs );
	
	uint32_t memoryType = find_memory_type( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT );
	
	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = memoryType;
	
	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.gpuCulling.instanceBufferMemory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.gpuCulling.instanceBuffer, vk.gpuCulling.instanceBufferMemory, 0 ) );
	
	// Get device address
	if ( qvkGetBufferDeviceAddress ) {
		VkBufferDeviceAddressInfo addrInfo = {};
		addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		addrInfo.buffer = vk.gpuCulling.instanceBuffer;
		vk.gpuCulling.instanceBufferAddress = qvkGetBufferDeviceAddress( vk.device, &addrInfo );
	}
	
	// Create indirect draw command buffer
	bufferInfo.size = MAX_DRAW_COUNT * sizeof( VkDrawIndexedIndirectCommand );
	bufferInfo.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	
	VK_CHECK( qvkCreateBuffer( vk.device, &bufferInfo, NULL, &vk.gpuCulling.drawCommandBuffer ) );
	
	qvkGetBufferMemoryRequirements( vk.device, vk.gpuCulling.drawCommandBuffer, &memReqs );
	allocInfo.allocationSize = memReqs.size;
	memoryType = find_memory_type( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT );
	
	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.gpuCulling.drawCommandBufferMemory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.gpuCulling.drawCommandBuffer, vk.gpuCulling.drawCommandBufferMemory, 0 ) );
	
	// Create cull data buffer
	bufferInfo.size = sizeof( gpu_cull_data_t );
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	
	VK_CHECK( qvkCreateBuffer( vk.device, &bufferInfo, NULL, &vk.gpuCulling.cullDataBuffer ) );
	
	qvkGetBufferMemoryRequirements( vk.device, vk.gpuCulling.cullDataBuffer, &memReqs );
	allocInfo.allocationSize = memReqs.size;
	memoryType = find_memory_type( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	
	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.gpuCulling.cullDataBufferMemory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.gpuCulling.cullDataBuffer, vk.gpuCulling.cullDataBufferMemory, 0 ) );
	
	// Allocate CPU-side instance array
	gpuInstances = (gpu_instance_t *)ri.Malloc( vk.gpuCulling.instanceCapacity * sizeof( gpu_instance_t ) );
	
	ri.Printf( PRINT_ALL, "GPU culling: Initialized with capacity for %u instances\n", vk.gpuCulling.instanceCapacity );
	
	vk.gpuCulling.initialized = qtrue;
}

/*
=============================================================================
GPU Culling Shutdown
=============================================================================
*/
void vk_gpu_culling_shutdown( void )
{
	if ( !vk.gpuCulling.initialized ) {
		return;
	}
	
	if ( vk.gpuCulling.instanceBuffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.gpuCulling.instanceBuffer, NULL );
		vk.gpuCulling.instanceBuffer = VK_NULL_HANDLE;
	}
	
	if ( vk.gpuCulling.instanceBufferMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.gpuCulling.instanceBufferMemory, NULL );
		vk.gpuCulling.instanceBufferMemory = VK_NULL_HANDLE;
	}
	
	if ( vk.gpuCulling.drawCommandBuffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.gpuCulling.drawCommandBuffer, NULL );
		vk.gpuCulling.drawCommandBuffer = VK_NULL_HANDLE;
	}
	
	if ( vk.gpuCulling.drawCommandBufferMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.gpuCulling.drawCommandBufferMemory, NULL );
		vk.gpuCulling.drawCommandBufferMemory = VK_NULL_HANDLE;
	}
	
	if ( vk.gpuCulling.cullDataBuffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.gpuCulling.cullDataBuffer, NULL );
		vk.gpuCulling.cullDataBuffer = VK_NULL_HANDLE;
	}
	
	if ( vk.gpuCulling.cullDataBufferMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.gpuCulling.cullDataBufferMemory, NULL );
		vk.gpuCulling.cullDataBufferMemory = VK_NULL_HANDLE;
	}
	
	if ( gpuInstances ) {
		ri.Free( gpuInstances );
		gpuInstances = NULL;
	}
	
	vk.gpuCulling.initialized = qfalse;
	ri.Printf( PRINT_ALL, "GPU culling: Shutdown complete\n" );
}

/*
=============================================================================
Add Instance for GPU Culling
=============================================================================
*/
void vk_gpu_culling_add_instance( const mat4_t modelMatrix, uint32_t entityIndex, const vec4_t color )
{
	if ( !vk.gpuCulling.enabled || !vk.gpuCulling.initialized ) {
		return;
	}
	
	if ( gpuInstanceWriteIndex >= vk.gpuCulling.instanceCapacity ) {
		ri.Printf( PRINT_WARNING, "GPU culling: Instance buffer full\n" );
		return;
	}
	
	gpu_instance_t *instance = &gpuInstances[gpuInstanceWriteIndex];
	Matrix16Copy( modelMatrix, instance->modelMatrix );
	Vector4Copy( color, instance->color );
	instance->entityIndex = entityIndex;
	instance->flags = 0;
	instance->lodBias = 0.0f;
	instance->materialOverride = 0;
	
	gpuInstanceWriteIndex++;
	vk.gpuCulling.instanceCount = gpuInstanceWriteIndex;
}

/*
=============================================================================
GPU Culling Update - Called each frame
=============================================================================
*/
void vk_gpu_culling_update( void )
{
	if ( !vk.gpuCulling.enabled || !vk.gpuCulling.initialized ) {
		return;
	}
	
	if ( vk.gpuCulling.cullPipeline == VK_NULL_HANDLE ) {
		return; // Pipeline not created yet
	}
	
	// Reset instance count
	gpuInstanceWriteIndex = 0;
	vk.gpuCulling.instanceCount = 0;
	
	// Update cull data with current camera frustum
	gpu_cull_data_t cullData;
	extern backEndState_t backEnd;
	
	// Extract frustum planes from viewParms (6 planes: left, right, top, bottom, near, far)
	if ( backEnd.viewParms.frustum ) {
		for ( int i = 0; i < 6; i++ ) {
			if ( i < 5 && backEnd.viewParms.frustum[i].normal ) {
				// Convert plane to vec4 format (normal + distance)
				cullData.frustumPlanes[i][0] = backEnd.viewParms.frustum[i].normal[0];
				cullData.frustumPlanes[i][1] = backEnd.viewParms.frustum[i].normal[1];
				cullData.frustumPlanes[i][2] = backEnd.viewParms.frustum[i].normal[2];
				cullData.frustumPlanes[i][3] = backEnd.viewParms.frustum[i].dist;
			} else {
				// Far plane (if not provided, use a default)
				cullData.frustumPlanes[i][0] = 0.0f;
				cullData.frustumPlanes[i][1] = 0.0f;
				cullData.frustumPlanes[i][2] = -1.0f;
				cullData.frustumPlanes[i][3] = backEnd.viewParms.zFar > 0.0f ? backEnd.viewParms.zFar : 1000.0f;
			}
		}
	} else {
		// Default frustum if not available
		Com_Memset( cullData.frustumPlanes, 0, sizeof( cullData.frustumPlanes ) );
	}
	
	// Get camera position and forward vector
	if ( backEnd.viewParms.or.origin ) {
		VectorCopy( backEnd.viewParms.or.origin, cullData.cameraPos );
	} else {
		VectorClear( cullData.cameraPos );
	}
	
	if ( backEnd.viewParms.or.axis[0] ) {
		VectorCopy( backEnd.viewParms.or.axis[0], cullData.cameraForward );
	} else {
		cullData.cameraForward[0] = 0.0f;
		cullData.cameraForward[1] = 0.0f;
		cullData.cameraForward[2] = 1.0f;
	}
	
	cullData.cullDistance = r_cullDistance ? r_cullDistance->value : 5000.0f;
	cullData.instanceCount = vk.gpuCulling.instanceCount;
	cullData.drawCount = 0; // Reset draw count for this frame
	
	// Upload cull data to GPU
	void *mapped;
	VK_CHECK( qvkMapMemory( vk.device, vk.gpuCulling.cullDataBufferMemory, 0, sizeof( gpu_cull_data_t ), 0, &mapped ) );
	Com_Memcpy( mapped, &cullData, sizeof( gpu_cull_data_t ) );
	qvkUnmapMemory( vk.device, vk.gpuCulling.cullDataBufferMemory );
	
	// Upload instance data to GPU
	if ( vk.gpuCulling.instanceCount > 0 && gpuInstances ) {
		VK_CHECK( qvkMapMemory( vk.device, vk.gpuCulling.instanceBufferMemory, 0, 
			vk.gpuCulling.instanceCount * sizeof( gpu_instance_t ), 0, &mapped ) );
		Com_Memcpy( mapped, gpuInstances, vk.gpuCulling.instanceCount * sizeof( gpu_instance_t ) );
		qvkUnmapMemory( vk.device, vk.gpuCulling.instanceBufferMemory );
	}
	
	// Dispatch culling compute shader
	if ( vk.gpuCulling.instanceCount > 0 && vk.gpuCulling.cullPipeline != VK_NULL_HANDLE ) {
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.gpuCulling.cullPipeline );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
			vk.gpuCulling.cullPipelineLayout, 0, 1, &vk.gpuCulling.cullDescriptorSet, 0, NULL );
		
		uint32_t workgroupCount = ( vk.gpuCulling.instanceCount + 255 ) / 256; // 256 threads per workgroup
		qvkCmdDispatch( vk.cmd->command_buffer, workgroupCount, 1, 1 );
	}
	
	// Compact visible instances (would be done by instance compaction compute shader)
	// This is handled by the GPU compute shader pipeline
}

/*
=============================================================================
Execute Indirect Draw Commands
=============================================================================
*/
void vk_gpu_culling_execute_indirect( void )
{
	if ( !vk.gpuCulling.enabled || !vk.gpuCulling.initialized ) {
		return;
	}
	
	if ( vk.gpuCulling.drawCommandCount == 0 ) {
		return;
	}
	
	if ( vk.gpuCulling.drawCommandBuffer == VK_NULL_HANDLE ) {
		return;
	}
	
	if ( !qvkCmdDrawIndexedIndirect ) {
		ri.Printf( PRINT_WARNING, "GPU culling: vkCmdDrawIndexedIndirect not available\n" );
		return;
	}
	
	// Ensure descriptor sets are bound
	vk_bind_descriptor_sets();
	
	// Execute indirect draw commands
	// The draw commands were written by the GPU culling compute shader
	// Each command contains: indexCount, instanceCount, firstIndex, vertexOffset, firstInstance
	// Note: This assumes:
	// - A graphics pipeline is already bound
	// - Vertex buffers are bound (including instance buffer if using instancing)
	// - Index buffer is bound
	qvkCmdDrawIndexedIndirect(
		vk.cmd->command_buffer,
		vk.gpuCulling.drawCommandBuffer,
		0, // offset into buffer
		vk.gpuCulling.drawCommandCount, // number of draw commands
		sizeof( VkDrawIndexedIndirectCommand ) // stride between commands
	);
	
	// Update statistics
	vk.gpuCulling.visibleInstanceCount = vk.gpuCulling.drawCommandCount;
}

/*
=============================================================================
Check if GPU Culling is Enabled
=============================================================================
*/
qboolean vk_gpu_culling_is_enabled( void )
{
	return vk.gpuCulling.enabled && vk.gpuCulling.initialized;
}

#endif // USE_VULKAN

