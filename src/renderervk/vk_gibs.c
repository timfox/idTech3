/*
=============================================================================
GIBS - Global Illumination Based on Surfels
Implementation based on SIGGRAPH 2021 paper
=============================================================================
*/

#include "tr_local.h"
#include "tr_math_optimized.h"
#include "vk.h"
#include "vk_gibs.h"

#ifdef USE_VULKAN_RAY_TRACING

// Forward declarations
extern orientationr_t backEnd;
extern trGlobals_t tr;

// CVars
cvar_t *r_gibs;
cvar_t *r_gibs_surfelRadius;
cvar_t *r_gibs_maxSurfels;
cvar_t *r_gibs_updateRate;
cvar_t *r_gibs_intensity;
cvar_t *r_gibs_samples;

// Surfel data structure matching GPU layout (std430)
typedef struct {
	float position[3];
	float normal[3];
	float radius;
	float irradiance[3];
	float confidence;
	uint32_t age;
	uint32_t flags;
} SurfelGPU;

// Uniform buffer for GIBS compute shaders
typedef struct {
	mat4_t viewInverse;
	mat4_t projInverse;
	vec3_t cameraPos;
	float time;
	uint32_t surfelCount;
	uint32_t frameIndex;
	float surfelRadius;
	float maxRayDistance;
	uint32_t samplesPerSurfel;
	float intensity;
	uint32_t updateRate;
	VkDeviceAddress tlasAddress;
} GIBSUniformBuffer;

static GIBSUniformBuffer gibsUniformData;

/*
=============================================================================
GIBS Initialization
=============================================================================
*/
void vk_gibs_init( void )
{
	if ( !vk.rt.initialized ) {
		ri.Printf( PRINT_WARNING, "GIBS: Ray tracing not initialized, cannot enable GIBS\n" );
		return;
	}
	
	if ( vk.gibs.initialized ) {
		return;
	}
	
	ri.Printf( PRINT_ALL, "Initializing GIBS (Global Illumination Based on Surfels)...\n" );
	
	// Initialize surfel buffer
	vk.gibs.surfelCapacity = r_gibs_maxSurfels ? r_gibs_maxSurfels->integer : GIBS_MAX_SURFELS;
	if ( vk.gibs.surfelCapacity > GIBS_MAX_SURFELS ) {
		vk.gibs.surfelCapacity = GIBS_MAX_SURFELS;
	}
	
	vk.gibs.surfelCount = 0;
	vk.gibs.frameCounter = 0;
	vk.gibs.updateFrameOffset = 0;
	vk.gibs.activeSurfelCount = 0;
	vk.gibs.updatedSurfelCount = 0;
	
	// Create surfel storage buffer
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = vk.gibs.surfelCapacity * sizeof( SurfelGPU );
	bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	
	VK_CHECK( qvkCreateBuffer( vk.device, &bufferInfo, NULL, &vk.gibs.surfelBuffer ) );
	
	VkMemoryRequirements memReqs;
	qvkGetBufferMemoryRequirements( vk.device, vk.gibs.surfelBuffer, &memReqs );
	
	uint32_t memoryType = find_memory_type( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	
	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = memoryType;
	
	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.gibs.surfelBufferMemory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.gibs.surfelBuffer, vk.gibs.surfelBufferMemory, 0 ) );
	
	// Get device address
	if ( qvkGetBufferDeviceAddress ) {
		VkBufferDeviceAddressInfo addrInfo = {};
		addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		addrInfo.buffer = vk.gibs.surfelBuffer;
		vk.gibs.surfelBufferAddress = qvkGetBufferDeviceAddress( vk.device, &addrInfo );
	}
	
	// Create indirect dispatch buffer
	bufferInfo.size = sizeof( VkDispatchIndirectCommand );
	bufferInfo.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	
	VK_CHECK( qvkCreateBuffer( vk.device, &bufferInfo, NULL, &vk.gibs.surfelIndirectBuffer ) );
	
	qvkGetBufferMemoryRequirements( vk.device, vk.gibs.surfelIndirectBuffer, &memReqs );
	allocInfo.allocationSize = memReqs.size;
	memoryType = find_memory_type( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT );
	
	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.gibs.surfelIndirectBufferMemory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.gibs.surfelIndirectBuffer, vk.gibs.surfelIndirectBufferMemory, 0 ) );
	
	ri.Printf( PRINT_ALL, "GIBS: Initialized with capacity for %u surfels\n", vk.gibs.surfelCapacity );
	
	// Create uniform buffer for GIBS
	VkBufferCreateInfo uniformBufferInfo = {};
	uniformBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	uniformBufferInfo.size = sizeof( GIBSUniformBuffer );
	uniformBufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	uniformBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	
	// Note: Uniform buffer will be created when pipelines are created
	// For now, just mark as initialized
	
	vk.gibs.initialized = qtrue;
}

/*
=============================================================================
GIBS Shutdown
=============================================================================
*/
void vk_gibs_shutdown( void )
{
	if ( !vk.gibs.initialized ) {
		return;
	}
	
	if ( vk.gibs.surfelBuffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.gibs.surfelBuffer, NULL );
		vk.gibs.surfelBuffer = VK_NULL_HANDLE;
	}
	
	if ( vk.gibs.surfelBufferMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.gibs.surfelBufferMemory, NULL );
		vk.gibs.surfelBufferMemory = VK_NULL_HANDLE;
	}
	
	if ( vk.gibs.surfelIndirectBuffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.gibs.surfelIndirectBuffer, NULL );
		vk.gibs.surfelIndirectBuffer = VK_NULL_HANDLE;
	}
	
	if ( vk.gibs.surfelIndirectBufferMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.gibs.surfelIndirectBufferMemory, NULL );
		vk.gibs.surfelIndirectBufferMemory = VK_NULL_HANDLE;
	}
	
	// Destroy pipelines
	if ( vk.gibs.updatePipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.gibs.updatePipeline, NULL );
		vk.gibs.updatePipeline = VK_NULL_HANDLE;
	}
	
	if ( vk.gibs.spawnPipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.gibs.spawnPipeline, NULL );
		vk.gibs.spawnPipeline = VK_NULL_HANDLE;
	}
	
	// Destroy pipeline layouts
	if ( vk.gibs.updatePipelineLayout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.gibs.updatePipelineLayout, NULL );
		vk.gibs.updatePipelineLayout = VK_NULL_HANDLE;
	}
	
	if ( vk.gibs.spawnPipelineLayout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.gibs.spawnPipelineLayout, NULL );
		vk.gibs.spawnPipelineLayout = VK_NULL_HANDLE;
	}
	
	// Destroy descriptor set layouts
	if ( vk.gibs.updateDescriptorSetLayout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.gibs.updateDescriptorSetLayout, NULL );
		vk.gibs.updateDescriptorSetLayout = VK_NULL_HANDLE;
	}
	
	if ( vk.gibs.spawnDescriptorSetLayout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.gibs.spawnDescriptorSetLayout, NULL );
		vk.gibs.spawnDescriptorSetLayout = VK_NULL_HANDLE;
	}
	
	// Note: Descriptor sets are managed by the descriptor pool and don't need explicit destruction
	
	vk.gibs.initialized = qfalse;
	ri.Printf( PRINT_ALL, "GIBS: Shutdown complete\n" );
}

/*
=============================================================================
GIBS Update - Called each frame
=============================================================================
*/
void vk_gibs_update( void )
{
	if ( !vk.gibs.enabled || !vk.gibs.initialized || !vk.rt.initialized ) {
		return;
	}
	
	if ( vk.gibs.updatePipeline == VK_NULL_HANDLE ) {
		return; // Pipelines not created yet
	}
	
	vk.gibs.frameCounter++;
	
	// Update surfels every N frames
	uint32_t updateRate = r_gibs_updateRate ? r_gibs_updateRate->integer : GIBS_UPDATE_RATE;
	if ( updateRate == 0 ) {
		updateRate = 1;
	}
	
	if ( ( vk.gibs.frameCounter % updateRate ) == 0 ) {
		// Update uniform buffer with camera data
		extern backEndState_t backEnd;
		
		// Get view inverse matrix (use optimized inversion for better numerical stability)
		if ( backEnd.viewParms.world.modelViewMatrix ) {
			mat4_t viewMatrix;
			Com_Memcpy( viewMatrix, backEnd.viewParms.world.modelViewMatrix, sizeof( mat4_t ) );
			Matrix16InverseOptimized( viewMatrix, gibsUniformData.viewInverse );
		} else {
			Matrix16Identity( gibsUniformData.viewInverse );
		}
		
		// Get projection inverse matrix (projection matrices are usually not affine, use standard inversion)
		if ( backEnd.viewParms.projectionMatrix ) {
			mat4_t projMatrix;
			Com_Memcpy( projMatrix, backEnd.viewParms.projectionMatrix, sizeof( mat4_t ) );
			Matrix16InverseOptimized( projMatrix, gibsUniformData.projInverse );
		} else {
			Matrix16Identity( gibsUniformData.projInverse );
		}
		
		// Get camera position
		if ( backEnd.viewParms.or.origin ) {
			VectorCopy( backEnd.viewParms.or.origin, gibsUniformData.cameraPos );
		} else {
			VectorClear( gibsUniformData.cameraPos );
		}
		
		gibsUniformData.time = tr.refdef.floatTime;
		gibsUniformData.surfelCount = vk.gibs.surfelCount;
		gibsUniformData.frameIndex = vk.gibs.frameCounter;
		gibsUniformData.surfelRadius = r_gibs_surfelRadius ? r_gibs_surfelRadius->value : GIBS_SURFEL_RADIUS;
		gibsUniformData.maxRayDistance = GIBS_MAX_RAY_DISTANCE;
		gibsUniformData.samplesPerSurfel = r_gibs_samples ? r_gibs_samples->integer : GIBS_SAMPLES_PER_SURFEL;
		gibsUniformData.intensity = r_gibs_intensity ? r_gibs_intensity->value : 1.0f;
		gibsUniformData.updateRate = updateRate;
		gibsUniformData.tlasAddress = vk.rt.tlasDeviceAddress;
		
		// Dispatch compute shader to update surfels
		// Calculate how many surfels to update this frame
		uint32_t updateCount = vk.gibs.surfelCount / updateRate;
		if ( updateCount == 0 ) {
			updateCount = 1;
		}
		
		// Bind pipeline and descriptor set
		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.gibs.updatePipeline );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
			vk.gibs.updatePipelineLayout, 0, 1, &vk.gibs.updateDescriptorSet, 0, NULL );
		
		// Push constants
		struct {
			uint32_t updateOffset;
			uint32_t updateCount;
		} pushConstants;
		pushConstants.updateOffset = vk.gibs.updateFrameOffset;
		pushConstants.updateCount = updateCount;
		qvkCmdPushConstants( vk.cmd->command_buffer, vk.gibs.updatePipelineLayout,
			VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( pushConstants ), &pushConstants );
		
		// Dispatch
		uint32_t groupCount = ( updateCount + 63 ) / 64; // 64 threads per group
		qvkCmdDispatch( vk.cmd->command_buffer, groupCount, 1, 1 );
		
		// Update offset for next frame
		vk.gibs.updateFrameOffset = ( vk.gibs.updateFrameOffset + updateCount ) % vk.gibs.surfelCount;
		vk.gibs.updatedSurfelCount = updateCount;
	}
}

/*
=============================================================================
GIBS Surfel Spawning
=============================================================================
*/
void vk_gibs_spawn_surfels( void )
{
	if ( !vk.gibs.enabled || !vk.gibs.initialized || !vk.rt.initialized ) {
		return;
	}
	
	if ( vk.gibs.spawnPipeline == VK_NULL_HANDLE ) {
		return; // Pipeline not created yet
	}
	
	// Only spawn if we have room for more surfels
	if ( vk.gibs.surfelCount >= vk.gibs.surfelCapacity ) {
		return;
	}
	
	// Bind pipeline and descriptor set
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.gibs.spawnPipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.gibs.spawnPipelineLayout, 0, 1, &vk.gibs.spawnDescriptorSet, 0, NULL );
	
	// Push constants
	struct {
		uint32_t outputOffset;
		uint32_t maxSurfels;
		vec2_t resolution;
	} pushConstants;
	pushConstants.outputOffset = vk.gibs.surfelCount;
	pushConstants.maxSurfels = vk.gibs.surfelCapacity;
	pushConstants.resolution[0] = (float)glConfig.vidWidth;
	pushConstants.resolution[1] = (float)glConfig.vidHeight;
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.gibs.spawnPipelineLayout,
		VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( pushConstants ), &pushConstants );
	
	// Dispatch indirect (will be set by shader)
	// For now, use direct dispatch
	uint32_t groupCount = ( vk.gibs.surfelCapacity - vk.gibs.surfelCount + 63 ) / 64;
	qvkCmdDispatch( vk.cmd->command_buffer, groupCount, 1, 1 );
	
	// Update surfel count (simplified - shader should update this atomically)
	// In a full implementation, we'd read back the count from GPU
}

/*
=============================================================================
Check if GIBS is enabled
=============================================================================
*/
qboolean vk_gibs_is_enabled( void )
{
	return vk.gibs.enabled && vk.gibs.initialized && vk.rt.initialized;
}

#endif // USE_VULKAN_RAY_TRACING

