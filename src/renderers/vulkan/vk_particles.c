/*
=============================================================================
Vulkan GPU Particle Systems Implementation

GPU-based particle simulation and rendering for offloading CPU.
=============================================================================
*/

#include "tr_local.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;
#include "vk.h"

#ifdef USE_VULKAN

// Vulkan function pointers
extern PFN_vkCreateBuffer qvkCreateBuffer;
extern PFN_vkDestroyBuffer qvkDestroyBuffer;
extern PFN_vkGetBufferMemoryRequirements qvkGetBufferMemoryRequirements;
extern PFN_vkAllocateMemory qvkAllocateMemory;
extern PFN_vkFreeMemory qvkFreeMemory;
extern PFN_vkBindBufferMemory qvkBindBufferMemory;
extern PFN_vkMapMemory qvkMapMemory;
extern PFN_vkUnmapMemory qvkUnmapMemory;
extern uint32_t find_memory_type(uint32_t memory_type_bits, VkMemoryPropertyFlags properties);

// GPU particle data structure (matches CPU particle structure)
typedef struct {
	vec3_t origin;
	vec3_t velocity;
	vec3_t color;
	float size;
	float life;
	float maxLife;
	uint32_t shaderHandle; // qhandle_t as uint32_t
} gpu_particle_t;

// Particle system structure
typedef struct {
	VkBuffer particleBuffer; // GPU buffer for particle data
	VkDeviceMemory particleMemory;
	VkBuffer indirectDrawBuffer; // Indirect draw buffer
	VkDeviceMemory indirectDrawMemory;
	
	VkPipeline simulationPipeline; // Compute pipeline for simulation
	VkPipeline renderingPipeline; // Graphics pipeline for rendering
	VkDescriptorSetLayout simulationDescriptorLayout;
	VkDescriptorSetLayout renderingDescriptorLayout;
	VkDescriptorSet simulationDescriptorSet;
	VkDescriptorSet renderingDescriptorSet;
	
	uint32_t maxParticles;
	uint32_t activeParticleCount;
	
	qboolean initialized;
} vk_particle_system_t;

static vk_particle_system_t vk_particles;

// Forward declaration of CPU particle structure from tr_scene.c
typedef struct {
	vec3_t origin;
	vec3_t velocity;
	vec3_t color;
	float size;
	float life;
	float maxLife;
	qhandle_t shader;
	qboolean active;
} cpu_particle_t;

void vk_particles_init( void )
{
	if ( !r_particles_gpu || !r_particles_gpu->integer ) {
		return;
	}

	if ( vk.device == (VkDevice)0x20000000 ) {
		// Fake device, skip initialization
		return;
	}

	Com_Memset( &vk_particles, 0, sizeof( vk_particles ) );
	
	vk_particles.maxParticles = r_particles_max ? r_particles_max->integer : 100000;
	
	// Create particle buffer for GPU particle data
	VkDeviceSize particleBufferSize = vk_particles.maxParticles * sizeof( gpu_particle_t );
	
	VkBufferCreateInfo bufferInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.size = particleBufferSize,
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = NULL
	};
	
	VkResult result = qvkCreateBuffer( vk.device, &bufferInfo, NULL, &vk_particles.particleBuffer );
	if ( result != VK_SUCCESS ) {
		ri.Printf( PRINT_ERROR, "vk_particles_init: Failed to create particle buffer\n" );
		return;
	}
	
	// Allocate memory for particle buffer
	VkMemoryRequirements memRequirements;
	qvkGetBufferMemoryRequirements( vk.device, vk_particles.particleBuffer, &memRequirements );
	
	VkMemoryAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = NULL,
		.allocationSize = memRequirements.size,
		.memoryTypeIndex = find_memory_type( memRequirements.memoryTypeBits, 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT )
	};
	
	result = qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk_particles.particleMemory );
	if ( result != VK_SUCCESS ) {
		ri.Printf( PRINT_ERROR, "vk_particles_init: Failed to allocate particle buffer memory\n" );
		qvkDestroyBuffer( vk.device, vk_particles.particleBuffer, NULL );
		vk_particles.particleBuffer = VK_NULL_HANDLE;
		return;
	}
	
	qvkBindBufferMemory( vk.device, vk_particles.particleBuffer, vk_particles.particleMemory, 0 );
	
	// Create indirect draw buffer
	VkDeviceSize indirectBufferSize = sizeof( VkDrawIndirectCommand );
	
	VkBufferCreateInfo indirectBufferInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.size = indirectBufferSize,
		.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = NULL
	};
	
	result = qvkCreateBuffer( vk.device, &indirectBufferInfo, NULL, &vk_particles.indirectDrawBuffer );
	if ( result != VK_SUCCESS ) {
		ri.Printf( PRINT_ERROR, "vk_particles_init: Failed to create indirect draw buffer\n" );
		qvkFreeMemory( vk.device, vk_particles.particleMemory, NULL );
		qvkDestroyBuffer( vk.device, vk_particles.particleBuffer, NULL );
		vk_particles.particleBuffer = VK_NULL_HANDLE;
		vk_particles.particleMemory = VK_NULL_HANDLE;
		return;
	}
	
	qvkGetBufferMemoryRequirements( vk.device, vk_particles.indirectDrawBuffer, &memRequirements );
	
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = find_memory_type( memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	
	result = qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk_particles.indirectDrawMemory );
	if ( result != VK_SUCCESS ) {
		ri.Printf( PRINT_ERROR, "vk_particles_init: Failed to allocate indirect draw buffer memory\n" );
		qvkDestroyBuffer( vk.device, vk_particles.indirectDrawBuffer, NULL );
		qvkFreeMemory( vk.device, vk_particles.particleMemory, NULL );
		qvkDestroyBuffer( vk.device, vk_particles.particleBuffer, NULL );
		vk_particles.particleBuffer = VK_NULL_HANDLE;
		vk_particles.particleMemory = VK_NULL_HANDLE;
		vk_particles.indirectDrawBuffer = VK_NULL_HANDLE;
		return;
	}
	
	qvkBindBufferMemory( vk.device, vk_particles.indirectDrawBuffer, vk_particles.indirectDrawMemory, 0 );
	
	// TODO: Create descriptor set layouts, pipelines, and descriptor sets
	// These require shader compilation and pipeline creation which is more complex
	// For now, buffers are created and ready for data upload
	
	ri.Printf( PRINT_DEVELOPER, "GPU particle system initialized (max particles: %u, buffer size: %lu bytes)\n", 
		vk_particles.maxParticles, (unsigned long)particleBufferSize );
	
	vk_particles.initialized = qtrue;
}

void vk_particles_shutdown( void )
{
	if ( !vk_particles.initialized ) {
		return;
	}
	
	if ( vk.device == (VkDevice)0x20000000 ) {
		// Fake device, skip cleanup
		vk_particles.initialized = qfalse;
		return;
	}
	
	// Cleanup particle buffers
	if ( vk_particles.indirectDrawBuffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk_particles.indirectDrawBuffer, NULL );
		vk_particles.indirectDrawBuffer = VK_NULL_HANDLE;
	}
	
	if ( vk_particles.indirectDrawMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk_particles.indirectDrawMemory, NULL );
		vk_particles.indirectDrawMemory = VK_NULL_HANDLE;
	}
	
	if ( vk_particles.particleBuffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk_particles.particleBuffer, NULL );
		vk_particles.particleBuffer = VK_NULL_HANDLE;
	}
	
	if ( vk_particles.particleMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk_particles.particleMemory, NULL );
		vk_particles.particleMemory = VK_NULL_HANDLE;
	}
	
	// TODO: Cleanup pipelines and descriptor sets when implemented
	// if ( vk_particles.simulationPipeline != VK_NULL_HANDLE ) { ... }
	// if ( vk_particles.renderingPipeline != VK_NULL_HANDLE ) { ... }
	// if ( vk_particles.simulationDescriptorLayout != VK_NULL_HANDLE ) { ... }
	// if ( vk_particles.renderingDescriptorLayout != VK_NULL_HANDLE ) { ... }
	
	vk_particles.initialized = qfalse;
	vk_particles.activeParticleCount = 0;
}

// Simulate particles (call from compute shader)
void vk_particles_simulate( float deltaTime )
{
	if ( !vk_particles.initialized || vk_particles.simulationPipeline == VK_NULL_HANDLE ) {
		return;
	}
	
	// Dispatch compute shader for particle simulation
	// Implementation would:
	// 1. Bind compute pipeline
	// 2. Bind descriptor set with particle buffer
	// 3. Dispatch compute shader
	// 4. Update indirect draw buffer with particle count
	(void)deltaTime; // Unused for now
}

// Upload CPU particles to GPU buffer
void vk_particles_upload_cpu_particles( const cpu_particle_t *particles, int count )
{
	if ( !vk_particles.initialized || !particles || count <= 0 ) {
		return;
	}
	
	if ( count > (int)vk_particles.maxParticles ) {
		count = vk_particles.maxParticles;
		ri.Printf( PRINT_WARNING, "vk_particles_upload_cpu_particles: particle count exceeds max (%d)\n", count );
	}
	
	// Convert CPU particles to GPU particle format
	gpu_particle_t *gpu_particles = ri.Hunk_AllocateTempMemory( count * sizeof( gpu_particle_t ) );
	if ( !gpu_particles ) {
		ri.Printf( PRINT_ERROR, "vk_particles_upload_cpu_particles: failed to allocate temp memory\n" );
		return;
	}
	
	int activeCount = 0;
	for ( int i = 0; i < count; i++ ) {
		if ( !particles[i].active || particles[i].life <= 0.0f ) {
			continue;
		}
		
		gpu_particle_t *gpu = &gpu_particles[activeCount];
		VectorCopy( particles[i].origin, gpu->origin );
		VectorCopy( particles[i].velocity, gpu->velocity );
		VectorCopy( particles[i].color, gpu->color );
		gpu->size = particles[i].size;
		gpu->life = particles[i].life;
		gpu->maxLife = particles[i].maxLife;
		gpu->shaderHandle = particles[i].shader;
		activeCount++;
	}
	
	if ( activeCount == 0 ) {
		ri.Hunk_FreeTempMemory( gpu_particles );
		vk_particles.activeParticleCount = 0;
		return;
	}
	
	// Upload to GPU buffer
	void *mappedData;
	VkResult result = qvkMapMemory( vk.device, vk_particles.particleMemory, 0, 
		activeCount * sizeof( gpu_particle_t ), 0, &mappedData );
	
	if ( result != VK_SUCCESS ) {
		ri.Printf( PRINT_ERROR, "vk_particles_upload_cpu_particles: Failed to map particle buffer memory\n" );
		ri.Hunk_FreeTempMemory( gpu_particles );
		vk_particles.activeParticleCount = 0;
		return;
	}
	
	// Copy particle data to mapped memory
	Com_Memcpy( mappedData, gpu_particles, activeCount * sizeof( gpu_particle_t ) );
	
	qvkUnmapMemory( vk.device, vk_particles.particleMemory );
	
	// Update indirect draw buffer with particle count
	// For indirect drawing, we need to set up the draw command
	VkDrawIndirectCommand drawCmd = {
		.vertexCount = 6, // 6 vertices per particle quad (2 triangles)
		.instanceCount = activeCount,
		.firstVertex = 0,
		.firstInstance = 0
	};
	
	result = qvkMapMemory( vk.device, vk_particles.indirectDrawMemory, 0,
		sizeof( VkDrawIndirectCommand ), 0, &mappedData );
	
	if ( result == VK_SUCCESS ) {
		Com_Memcpy( mappedData, &drawCmd, sizeof( VkDrawIndirectCommand ) );
		qvkUnmapMemory( vk.device, vk_particles.indirectDrawMemory );
	}
	
	vk_particles.activeParticleCount = activeCount;
	
	ri.Hunk_FreeTempMemory( gpu_particles );
}

// Render particles
void vk_particles_render( void )
{
	if ( !vk_particles.initialized || vk_particles.renderingPipeline == VK_NULL_HANDLE ) {
		return;
	}
	
	if ( vk_particles.activeParticleCount == 0 ) {
		return;
	}
	
	// Render particles using indirect draw
	// TODO: Full implementation would:
	// 1. Bind graphics pipeline
	// 2. Bind descriptor set with particle buffer
	// 3. Bind vertex/index buffers if needed
	// 4. Draw indirect using particle count from buffer
	// 5. Handle shader changes per particle (if needed)
	
	// For now, this is a placeholder
	// The actual rendering would use vkCmdDrawIndirect or similar
}

#endif // USE_VULKAN

