#pragma once

#include "tr_local.h"

// GPU-Accelerated Physics Simulation System
// Uses compute shaders for cloth simulation, particle systems, and other GPU-based physics

// Particle structure for cloth simulation
typedef struct {
    vec4_t pos;      // Position (x, y, z, w)
    vec4_t vel;      // Velocity
    vec4_t uv;       // UV coordinates
    vec4_t normal;   // Normal vector
} vk_physics_particle_t;

// Cloth simulation parameters
typedef struct {
    int gridSizeX;
    int gridSizeY;
    float sizeX;
    float sizeY;
    float particleMass;
    float springStiffness;
    float damping;
    float restDistH;  // Horizontal rest distance
    float restDistV;  // Vertical rest distance
    float restDistD;  // Diagonal rest distance
} vk_cloth_params_t;

// Physics simulation state
typedef struct {
    qboolean enabled;
    qboolean initialized;

    // Compute resources
    VkQueue computeQueue;
    VkCommandPool computeCommandPool;
    VkCommandBuffer computeCommandBuffer;
    VkFence computeFence;
    VkSemaphore computeReadySemaphore;
    VkSemaphore computeCompleteSemaphore;

    // Compute pipeline
    VkPipeline computePipeline;
    VkPipelineLayout computePipelineLayout;
    VkDescriptorSetLayout computeDescriptorSetLayout;
    VkDescriptorSet computeDescriptorSet;

    // Storage buffers (double-buffered for ping-pong)
    VkBuffer storageBuffers[2];  // Input and output buffers
    VkDeviceMemory storageBufferMemory[2];
    void* storageBufferMapped[2];
    int currentBufferIndex;  // 0 or 1

    // Uniform buffer for compute shader parameters
    VkBuffer uniformBuffer;
    VkDeviceMemory uniformBufferMemory;
    void* uniformBufferMapped;

    // Cloth parameters
    vk_cloth_params_t clothParams;

    // Simulation state
    float deltaTime;
    vec3_t gravity;
    vec3_t spherePos;  // Collision sphere position
    float sphereRadius;
    int particleCount;

    // Settings
    qboolean simulateWind;
    qboolean calculateNormals;
    int simulationIterations;
} vk_physics_t;

extern vk_physics_t vk_physics;

// Physics API
void VK_Physics_Init(void);
void VK_Physics_Shutdown(void);
void VK_Physics_CreateCloth(int gridSizeX, int gridSizeY, float sizeX, float sizeY);
void VK_Physics_Update(float deltaTime);
void VK_Physics_SetGravity(const vec3_t gravity);
void VK_Physics_SetCollisionSphere(const vec3_t position, float radius);
void VK_Physics_SetWindEnabled(qboolean enabled);
void VK_Physics_SetSimulationIterations(int iterations);

// Buffer access for rendering
VkBuffer VK_Physics_GetCurrentVertexBuffer(void);
int VK_Physics_GetParticleCount(void);

// Utility functions
qboolean VK_Physics_IsEnabled(void);
void VK_Physics_SetEnabled(qboolean enabled);
