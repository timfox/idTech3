/*
=============================================================================
GPU-Driven Rendering Pipeline
Implements GPU culling, instancing, and indirect drawing
=============================================================================
*/

#pragma once

#include "tr_local.h"
#include "vk.h"

#ifdef USE_VULKAN

// GPU culling and instancing structures
#define MAX_INSTANCE_COUNT (1024 * 1024) // 1M instances
#define MAX_DRAW_COUNT 65536

// Instance data structure (matches GPU layout)
typedef struct {
	mat4_t modelMatrix;
	vec4_t color; // RGBA tint
	uint32_t entityIndex;
	uint32_t flags;
	float lodBias;
	uint32_t materialOverride;
	uint32_t padding;
} gpu_instance_t;

// Draw command structure
typedef struct {
	uint32_t indexCount;
	uint32_t instanceCount;
	uint32_t firstIndex;
	int32_t vertexOffset;
	uint32_t firstInstance;
} gpu_draw_command_t;

// Culling data structure
typedef struct {
	vec4_t frustumPlanes[6]; // Frustum planes for culling
	vec3_t cameraPos;
	float cullDistance;
	vec3_t cameraForward;
	float padding;
	uint32_t instanceCount;
	uint32_t drawCount;
} gpu_cull_data_t;

// GPU culling system state
typedef struct {
	qboolean enabled;
	qboolean initialized;
	
	// Instance buffer
	VkBuffer instanceBuffer;
	VkDeviceMemory instanceBufferMemory;
	VkDeviceAddress instanceBufferAddress;
	uint32_t instanceCount;
	uint32_t instanceCapacity;
	
	// Indirect draw command buffer
	VkBuffer drawCommandBuffer;
	VkDeviceMemory drawCommandBufferMemory;
	uint32_t drawCommandCount;
	
	// Cull data buffer
	VkBuffer cullDataBuffer;
	VkDeviceMemory cullDataBufferMemory;
	
	// Compute pipelines
	VkPipeline cullPipeline;
	VkPipelineLayout cullPipelineLayout;
	VkDescriptorSetLayout cullDescriptorSetLayout;
	VkDescriptorSet cullDescriptorSet;
	
	VkPipeline instancePipeline;
	VkPipelineLayout instancePipelineLayout;
	VkDescriptorSetLayout instanceDescriptorSetLayout;
	VkDescriptorSet instanceDescriptorSet;
	
	// Statistics
	uint32_t culledInstanceCount;
	uint32_t visibleInstanceCount;
} gpu_culling_system_t;

// External API
void vk_gpu_culling_init( void );
void vk_gpu_culling_shutdown( void );
void vk_gpu_culling_begin_frame( void );
void vk_gpu_culling_update( void );
void vk_gpu_culling_add_instance( const mat4_t modelMatrix, uint32_t entityIndex, const vec4_t color );
void vk_gpu_culling_execute_indirect( void );
qboolean vk_gpu_culling_is_enabled( void );

// CVars
extern cvar_t *r_gpuCulling;
extern cvar_t *r_gpuInstancing;
extern cvar_t *r_cullDistance;

#endif // USE_VULKAN

