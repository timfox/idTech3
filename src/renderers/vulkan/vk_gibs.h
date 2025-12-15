/*
=============================================================================
GIBS - Global Illumination Based on Surfels
Based on SIGGRAPH 2021 paper and SurfelGI reference implementation
=============================================================================
*/

#pragma once

#include "tr_local.h"
#include "vk.h"

#ifdef USE_VULKAN_RAY_TRACING

// Surfel data structure (matches GPU layout)
typedef struct {
	vec3_t position;      // World space position
	vec3_t normal;        // Surface normal
	float radius;        // Surfel radius
	vec3_t irradiance;   // Cached indirect irradiance (RGB)
	float confidence;    // Confidence value (0-1)
	uint32_t age;        // Age in frames
	uint32_t flags;      // Surfel flags
} gibs_surfel_t;

// Surfel flags
#define GIBS_SURFEL_ACTIVE     0x01
#define GIBS_SURFEL_VALID      0x02
#define GIBS_SURFEL_STALE      0x04

// GIBS configuration
#define GIBS_MAX_SURFELS       1024 * 1024  // Maximum number of surfels
#define GIBS_SURFEL_RADIUS     0.1f         // Default surfel radius
#define GIBS_UPDATE_RATE       4            // Update every N frames
#define GIBS_MAX_RAY_DISTANCE  10.0f        // Maximum ray distance for GI
#define GIBS_SAMPLES_PER_SURFEL 16          // Number of samples per surfel update

// GIBS system state
typedef struct {
	qboolean enabled;
	qboolean initialized;
	
	// Surfel storage
	gibs_surfel_t *surfels;
	uint32_t surfelCount;
	uint32_t surfelCapacity;
	
	// GPU buffers
	VkBuffer surfelBuffer;
	VkDeviceMemory surfelBufferMemory;
	VkDeviceAddress surfelBufferAddress;
	
	VkBuffer surfelIndirectBuffer;  // For indirect dispatch
	VkDeviceMemory surfelIndirectBufferMemory;
	
	// Uniform buffer for GIBS parameters
	VkBuffer uniformBuffer;
	VkDeviceMemory uniformBufferMemory;
	
	// Compute pipelines
	VkPipeline updatePipeline;
	VkPipelineLayout updatePipelineLayout;
	VkDescriptorSetLayout updateDescriptorSetLayout;
	VkDescriptorSet updateDescriptorSet;
	
	VkPipeline spawnPipeline;
	VkPipelineLayout spawnPipelineLayout;
	VkDescriptorSetLayout spawnDescriptorSetLayout;
	VkDescriptorSet spawnDescriptorSet;
	
	// Frame tracking
	uint32_t frameCounter;
	uint32_t updateFrameOffset;
	
	// Statistics
	uint32_t activeSurfelCount;
	uint32_t updatedSurfelCount;
} gibs_system_t;

// External API
void vk_gibs_init( void );
void vk_gibs_shutdown( void );
void vk_gibs_create_pipelines( void );
void vk_gibs_update( void );
void vk_gibs_spawn_surfels( void );
void vk_gibs_cull_stale_surfels( void );
qboolean vk_gibs_is_enabled( void );

// CVars
extern cvar_t *r_gibs;
extern cvar_t *r_gibs_surfelRadius;
extern cvar_t *r_gibs_maxSurfels;
extern cvar_t *r_gibs_updateRate;
extern cvar_t *r_gibs_intensity;
extern cvar_t *r_gibs_samples;

#endif // USE_VULKAN_RAY_TRACING

