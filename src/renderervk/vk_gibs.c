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
	
	// Destroy uniform buffer
	if ( vk.gibs.uniformBuffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.gibs.uniformBuffer, NULL );
		vk.gibs.uniformBuffer = VK_NULL_HANDLE;
	}
	
	if ( vk.gibs.uniformBufferMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.gibs.uniformBufferMemory, NULL );
		vk.gibs.uniformBufferMemory = VK_NULL_HANDLE;
	}
	
	vk.gibs.initialized = qfalse;
	ri.Printf( PRINT_ALL, "GIBS: Shutdown complete\n" );
}

/*
=============================================================================
GIBS Pipeline Creation
Creates compute pipelines for surfel spawning and updating
Must be called after shaders are loaded (vk_create_shader_modules)
=============================================================================
*/
void vk_gibs_create_pipelines( void )
{
	if ( !vk.gibs.enabled || !vk.gibs.initialized || !vk.rt.initialized ) {
		return;
	}
	
	if ( vk.gibs.updatePipeline != VK_NULL_HANDLE || vk.gibs.spawnPipeline != VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, "GIBS: Pipelines already created\n" );
		return;
	}
	
	ri.Printf( PRINT_ALL, "GIBS: Creating compute pipelines...\n" );
	
	// Load shader modules (shaders must be compiled first via compile.sh)
	// The shader arrays are defined in shader_data.c (included in vk.c)
	extern const uint8_t gibs_spawn_comp_spv[];
	extern const uint8_t gibs_update_comp_spv[];
	
	// Try to load spawn shader module
	if ( vk.modules.gibs_spawn_comp == VK_NULL_HANDLE ) {
		vk.modules.gibs_spawn_comp = SHADER_MODULE( gibs_spawn_comp_spv );
		if ( vk.modules.gibs_spawn_comp != VK_NULL_HANDLE ) {
			SET_OBJECT_NAME( vk.modules.gibs_spawn_comp, "GIBS spawn compute shader module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			ri.Printf( PRINT_DEVELOPER, "GIBS: Loaded spawn compute shader module\n" );
		} else {
			ri.Printf( PRINT_WARNING, "GIBS: Failed to load spawn compute shader (shader may not be compiled)\n" );
		}
	}
	
	// Try to load update shader module
	if ( vk.modules.gibs_update_comp == VK_NULL_HANDLE ) {
		vk.modules.gibs_update_comp = SHADER_MODULE( gibs_update_comp_spv );
		if ( vk.modules.gibs_update_comp != VK_NULL_HANDLE ) {
			SET_OBJECT_NAME( vk.modules.gibs_update_comp, "GIBS update compute shader module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );
			ri.Printf( PRINT_DEVELOPER, "GIBS: Loaded update compute shader module\n" );
		} else {
			ri.Printf( PRINT_WARNING, "GIBS: Failed to load update compute shader (shader may not be compiled)\n" );
		}
	}
	
	// Create uniform buffer
	VkBufferCreateInfo uniformBufferInfo = {};
	uniformBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	uniformBufferInfo.size = sizeof( GIBSUniformBuffer );
	uniformBufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	uniformBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	
	VK_CHECK( qvkCreateBuffer( vk.device, &uniformBufferInfo, NULL, &vk.gibs.uniformBuffer ) );
	
	VkMemoryRequirements memReqs;
	qvkGetBufferMemoryRequirements( vk.device, vk.gibs.uniformBuffer, &memReqs );
	
	uint32_t memoryType = find_memory_type( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	
	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = memoryType;
	
	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.gibs.uniformBufferMemory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.gibs.uniformBuffer, vk.gibs.uniformBufferMemory, 0 ) );
	
	SET_OBJECT_NAME( vk.gibs.uniformBuffer, "GIBS uniform buffer", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );
	SET_OBJECT_NAME( vk.gibs.uniformBufferMemory, "GIBS uniform buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT );
	
	// Create descriptor set layouts
	// Update pipeline: uniform buffer (binding 0), surfel buffer (binding 1), TLAS (binding 2), blue noise texture (binding 3)
	VkDescriptorSetLayoutBinding updateBindings[4] = {};
	updateBindings[0].binding = 0;
	updateBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	updateBindings[0].descriptorCount = 1;
	updateBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	
	updateBindings[1].binding = 1;
	updateBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	updateBindings[1].descriptorCount = 1;
	updateBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	
	updateBindings[2].binding = 2;
	updateBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	updateBindings[2].descriptorCount = 1;
	updateBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	
	updateBindings[3].binding = 3;
	updateBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	updateBindings[3].descriptorCount = 1;
	updateBindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	
	VkDescriptorSetLayoutCreateInfo updateLayoutInfo = {};
	updateLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	updateLayoutInfo.bindingCount = 4;
	updateLayoutInfo.pBindings = updateBindings;
	
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &updateLayoutInfo, NULL, &vk.gibs.updateDescriptorSetLayout ) );
	SET_OBJECT_NAME( vk.gibs.updateDescriptorSetLayout, "GIBS update descriptor set layout", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT );
	
	// Spawn pipeline: uniform buffer (binding 0), surfel buffer (binding 1), depth buffer (binding 2), normal buffer (binding 3), indirect buffer (binding 4)
	VkDescriptorSetLayoutBinding spawnBindings[5] = {};
	spawnBindings[0].binding = 0;
	spawnBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	spawnBindings[0].descriptorCount = 1;
	spawnBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	
	spawnBindings[1].binding = 1;
	spawnBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	spawnBindings[1].descriptorCount = 1;
	spawnBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	
	spawnBindings[2].binding = 2;
	spawnBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	spawnBindings[2].descriptorCount = 1;
	spawnBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	
	spawnBindings[3].binding = 3;
	spawnBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	spawnBindings[3].descriptorCount = 1;
	spawnBindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	
	spawnBindings[4].binding = 4;
	spawnBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	spawnBindings[4].descriptorCount = 1;
	spawnBindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	
	VkDescriptorSetLayoutCreateInfo spawnLayoutInfo = {};
	spawnLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	spawnLayoutInfo.bindingCount = 5;
	spawnLayoutInfo.pBindings = spawnBindings;
	
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &spawnLayoutInfo, NULL, &vk.gibs.spawnDescriptorSetLayout ) );
	SET_OBJECT_NAME( vk.gibs.spawnDescriptorSetLayout, "GIBS spawn descriptor set layout", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT );
	
	// Create pipeline layouts with push constants
	VkPushConstantRange pushConstantRange = {};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = 128; // Enough for push constants
	
	VkPipelineLayoutCreateInfo updatePipelineLayoutInfo = {};
	updatePipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	updatePipelineLayoutInfo.setLayoutCount = 1;
	updatePipelineLayoutInfo.pSetLayouts = &vk.gibs.updateDescriptorSetLayout;
	updatePipelineLayoutInfo.pushConstantRangeCount = 1;
	updatePipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
	
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &updatePipelineLayoutInfo, NULL, &vk.gibs.updatePipelineLayout ) );
	SET_OBJECT_NAME( vk.gibs.updatePipelineLayout, "GIBS update pipeline layout", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );
	
	VkPipelineLayoutCreateInfo spawnPipelineLayoutInfo = {};
	spawnPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	spawnPipelineLayoutInfo.setLayoutCount = 1;
	spawnPipelineLayoutInfo.pSetLayouts = &vk.gibs.spawnDescriptorSetLayout;
	spawnPipelineLayoutInfo.pushConstantRangeCount = 1;
	spawnPipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
	
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &spawnPipelineLayoutInfo, NULL, &vk.gibs.spawnPipelineLayout ) );
	SET_OBJECT_NAME( vk.gibs.spawnPipelineLayout, "GIBS spawn pipeline layout", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT );
	
	// Create compute pipelines
	if ( vk.modules.gibs_update_comp != VK_NULL_HANDLE ) {
		VkComputePipelineCreateInfo computeInfo = {};
		VkPipelineShaderStageCreateInfo shaderStage = {};
		
		shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		shaderStage.module = vk.modules.gibs_update_comp;
		shaderStage.pName = "main";
		
		computeInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		computeInfo.stage = shaderStage;
		computeInfo.layout = vk.gibs.updatePipelineLayout;
		
		VkResult result = qvkCreateComputePipelines( vk.device, vk.pipelineCache, 1, &computeInfo, NULL, &vk.gibs.updatePipeline );
		if ( result == VK_SUCCESS ) {
			SET_OBJECT_NAME( vk.gibs.updatePipeline, "GIBS update compute pipeline", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
			ri.Printf( PRINT_DEVELOPER, "GIBS: Created update compute pipeline\n" );
		} else {
			ri.Printf( PRINT_WARNING, "GIBS: Failed to create update compute pipeline: %d\n", result );
			vk.gibs.updatePipeline = VK_NULL_HANDLE;
		}
	}
	
	if ( vk.modules.gibs_spawn_comp != VK_NULL_HANDLE ) {
		VkComputePipelineCreateInfo computeInfo = {};
		VkPipelineShaderStageCreateInfo shaderStage = {};
		
		shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		shaderStage.module = vk.modules.gibs_spawn_comp;
		shaderStage.pName = "main";
		
		computeInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		computeInfo.stage = shaderStage;
		computeInfo.layout = vk.gibs.spawnPipelineLayout;
		
		VkResult result = qvkCreateComputePipelines( vk.device, vk.pipelineCache, 1, &computeInfo, NULL, &vk.gibs.spawnPipeline );
		if ( result == VK_SUCCESS ) {
			SET_OBJECT_NAME( vk.gibs.spawnPipeline, "GIBS spawn compute pipeline", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );
			ri.Printf( PRINT_DEVELOPER, "GIBS: Created spawn compute pipeline\n" );
		} else {
			ri.Printf( PRINT_WARNING, "GIBS: Failed to create spawn compute pipeline: %d\n", result );
			vk.gibs.spawnPipeline = VK_NULL_HANDLE;
		}
	}
	
	// Allocate descriptor sets
	VkDescriptorSetAllocateInfo descAllocInfo = {};
	descAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descAllocInfo.descriptorPool = vk.descriptor_pool;
	descAllocInfo.descriptorSetCount = 1;
	
	// Allocate update descriptor set
	descAllocInfo.pSetLayouts = &vk.gibs.updateDescriptorSetLayout;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &descAllocInfo, &vk.gibs.updateDescriptorSet ) );
	SET_OBJECT_NAME( vk.gibs.updateDescriptorSet, "GIBS update descriptor set", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
	
	// Allocate spawn descriptor set
	descAllocInfo.pSetLayouts = &vk.gibs.spawnDescriptorSetLayout;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &descAllocInfo, &vk.gibs.spawnDescriptorSet ) );
	SET_OBJECT_NAME( vk.gibs.spawnDescriptorSet, "GIBS spawn descriptor set", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
	
	// Update descriptor sets with buffer/image bindings
	// Update descriptor set: uniform buffer, surfel buffer, TLAS, blue noise texture
	VkWriteDescriptorSet updateWrites[4] = {};
	VkDescriptorBufferInfo updateUniformBufferInfo = {};
	VkDescriptorBufferInfo updateSurfelBufferInfo = {};
	VkWriteDescriptorSetAccelerationStructureKHR updateTlasInfo = {};
	VkDescriptorImageInfo updateBlueNoiseImageInfo = {};
	
	// Uniform buffer
	updateUniformBufferInfo.buffer = vk.gibs.uniformBuffer;
	updateUniformBufferInfo.offset = 0;
	updateUniformBufferInfo.range = sizeof( GIBSUniformBuffer );
	
	updateWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	updateWrites[0].dstSet = vk.gibs.updateDescriptorSet;
	updateWrites[0].dstBinding = 0;
	updateWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	updateWrites[0].descriptorCount = 1;
	updateWrites[0].pBufferInfo = &updateUniformBufferInfo;
	
	// Surfel buffer
	updateSurfelBufferInfo.buffer = vk.gibs.surfelBuffer;
	updateSurfelBufferInfo.offset = 0;
	updateSurfelBufferInfo.range = vk.gibs.surfelCapacity * sizeof( SurfelGPU );
	
	updateWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	updateWrites[1].dstSet = vk.gibs.updateDescriptorSet;
	updateWrites[1].dstBinding = 1;
	updateWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	updateWrites[1].descriptorCount = 1;
	updateWrites[1].pBufferInfo = &updateSurfelBufferInfo;
	
	// TLAS (acceleration structure)
	updateTlasInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	updateTlasInfo.accelerationStructureCount = 1;
	updateTlasInfo.pAccelerationStructures = &vk.rt.tlas;
	
	updateWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	updateWrites[2].pNext = &updateTlasInfo;
	updateWrites[2].dstSet = vk.gibs.updateDescriptorSet;
	updateWrites[2].dstBinding = 2;
	updateWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	updateWrites[2].descriptorCount = 1;
	
	// Blue noise texture (placeholder - will need to be created/loaded)
	// For now, use a null descriptor or default texture
	updateBlueNoiseImageInfo.sampler = vk.samplers.handle[0];
	updateBlueNoiseImageInfo.imageView = VK_NULL_HANDLE; // TODO: Create/load blue noise texture
	updateBlueNoiseImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	
	updateWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	updateWrites[3].dstSet = vk.gibs.updateDescriptorSet;
	updateWrites[3].dstBinding = 3;
	updateWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	updateWrites[3].descriptorCount = 1;
	updateWrites[3].pImageInfo = &updateBlueNoiseImageInfo;
	
	qvkUpdateDescriptorSets( vk.device, 4, updateWrites, 0, NULL );
	
	// Spawn descriptor set: uniform buffer, surfel buffer, depth buffer, normal buffer, indirect buffer
	VkWriteDescriptorSet spawnWrites[5] = {};
	VkDescriptorBufferInfo spawnUniformBufferInfo = {};
	VkDescriptorBufferInfo spawnSurfelBufferInfo = {};
	VkDescriptorBufferInfo spawnIndirectBufferInfo = {};
	VkDescriptorImageInfo spawnDepthImageInfo = {};
	VkDescriptorImageInfo spawnNormalImageInfo = {};
	
	// Uniform buffer
	spawnUniformBufferInfo.buffer = vk.gibs.uniformBuffer;
	spawnUniformBufferInfo.offset = 0;
	spawnUniformBufferInfo.range = sizeof( GIBSUniformBuffer );
	
	spawnWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	spawnWrites[0].dstSet = vk.gibs.spawnDescriptorSet;
	spawnWrites[0].dstBinding = 0;
	spawnWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	spawnWrites[0].descriptorCount = 1;
	spawnWrites[0].pBufferInfo = &spawnUniformBufferInfo;
	
	// Surfel buffer
	spawnSurfelBufferInfo.buffer = vk.gibs.surfelBuffer;
	spawnSurfelBufferInfo.offset = 0;
	spawnSurfelBufferInfo.range = vk.gibs.surfelCapacity * sizeof( SurfelGPU );
	
	spawnWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	spawnWrites[1].dstSet = vk.gibs.spawnDescriptorSet;
	spawnWrites[1].dstBinding = 1;
	spawnWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	spawnWrites[1].descriptorCount = 1;
	spawnWrites[1].pBufferInfo = &spawnSurfelBufferInfo;
	
	// Depth buffer (G-buffer depth texture) - TODO: Use actual depth buffer from G-buffer
	spawnDepthImageInfo.sampler = vk.samplers.handle[0];
	spawnDepthImageInfo.imageView = VK_NULL_HANDLE; // TODO: Use actual depth buffer from G-buffer
	spawnDepthImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	
	spawnWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	spawnWrites[2].dstSet = vk.gibs.spawnDescriptorSet;
	spawnWrites[2].dstBinding = 2;
	spawnWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	spawnWrites[2].descriptorCount = 1;
	spawnWrites[2].pImageInfo = &spawnDepthImageInfo;
	
	// Normal buffer (G-buffer normal texture) - TODO: Use actual normal buffer from G-buffer
	spawnNormalImageInfo.sampler = vk.samplers.handle[0];
	spawnNormalImageInfo.imageView = VK_NULL_HANDLE; // TODO: Use actual normal buffer from G-buffer
	spawnNormalImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	
	spawnWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	spawnWrites[3].dstSet = vk.gibs.spawnDescriptorSet;
	spawnWrites[3].dstBinding = 3;
	spawnWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	spawnWrites[3].descriptorCount = 1;
	spawnWrites[3].pImageInfo = &spawnNormalImageInfo;
	
	// Indirect dispatch buffer
	spawnIndirectBufferInfo.buffer = vk.gibs.surfelIndirectBuffer;
	spawnIndirectBufferInfo.offset = 0;
	spawnIndirectBufferInfo.range = sizeof( VkDispatchIndirectCommand );
	
	spawnWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	spawnWrites[4].dstSet = vk.gibs.spawnDescriptorSet;
	spawnWrites[4].dstBinding = 4;
	spawnWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	spawnWrites[4].descriptorCount = 1;
	spawnWrites[4].pBufferInfo = &spawnIndirectBufferInfo;
	
	qvkUpdateDescriptorSets( vk.device, 5, spawnWrites, 0, NULL );
	
	ri.Printf( PRINT_ALL, "GIBS: Pipeline creation complete\n" );
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

