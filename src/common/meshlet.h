/*
===============================================================================
Meshlet System - Modern GPU Geometry Pipeline

Meshlets provide GPU-driven rendering with efficient culling and LOD.
This system generates meshlets from triangle meshes for use with mesh shaders.
===============================================================================
*/

#ifndef __MESHLET_H__
#define __MESHLET_H__

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

//===============================================================================
// Meshlet Data Structures
//===============================================================================

#define MESHLET_MAX_VERTICES     64
#define MESHLET_MAX_TRIANGLES    42  // 42 triangles * 3 = 126 indices max
#define MESHLET_MAX_LOD_LEVELS   4

/**
 * @brief Meshlet data structure for GPU consumption
 */
typedef struct {
    vec3_t center;              // Bounding sphere center
    float radius;               // Bounding sphere radius
    uint32_t vertexOffset;      // Offset into vertex buffer
    uint32_t vertexCount;       // Number of vertices in this meshlet
    uint32_t indexOffset;       // Offset into index buffer
    uint32_t indexCount;        // Number of indices in this meshlet
    uint32_t lodLevel;          // LOD level (0 = highest detail)
} meshlet_t;

/**
 * @brief Vertex data structure
 */
typedef struct {
    vec3_t position;
    vec3_t normal;
    vec2_t texCoord;
    vec4_t tangent;  // xyz = tangent, w = handedness
} meshlet_vertex_t;

/**
 * @brief Meshlet generation parameters
 */
typedef struct {
    float maxMeshletRadius;     // Maximum radius for meshlet bounding sphere
    uint32_t maxVertices;       // Maximum vertices per meshlet
    uint32_t maxTriangles;      // Maximum triangles per meshlet
    float lodDistanceMultiplier; // Distance multiplier for LOD selection
    qboolean generateLODs;      // Whether to generate LOD levels
} meshlet_gen_params_t;

/**
 * @brief Generated meshlet data
 */
typedef struct {
    meshlet_t* meshlets;        // Array of meshlets
    uint32_t meshletCount;      // Number of meshlets
    uint32_t totalVertices;     // Total vertices in all meshlets
    uint32_t totalIndices;      // Total indices in all meshlets
    uint32_t lodCount;          // Number of LOD levels
} meshlet_data_t;

//===============================================================================
// Meshlet Generation API
//===============================================================================

/**
 * @brief Generate meshlets from triangle mesh data
 * @param vertices Array of vertex data
 * @param vertexCount Number of vertices
 * @param indices Array of triangle indices (3 per triangle)
 * @param triangleCount Number of triangles
 * @param params Generation parameters
 * @return Generated meshlet data (must be freed with Meshlet_FreeData)
 */
meshlet_data_t* Meshlet_Generate(const meshlet_vertex_t* vertices,
                                uint32_t vertexCount,
                                const uint32_t* indices,
                                uint32_t triangleCount,
                                const meshlet_gen_params_t* params);

/**
 * @brief Free meshlet data
 * @param data Meshlet data to free
 */
void Meshlet_FreeData(meshlet_data_t* data);

/**
 * @brief Optimize meshlet data for GPU consumption
 * @param data Meshlet data to optimize
 * @return true on success
 */
qboolean Meshlet_OptimizeForGPU(meshlet_data_t* data);

/**
 * @brief Get recommended meshlet generation parameters
 * @param params Output parameter structure
 */
void Meshlet_GetDefaultParams(meshlet_gen_params_t* params);

//===============================================================================
// Meshlet Rendering API
//===============================================================================

/**
 * @brief Prepare meshlet data for rendering
 * @param data Meshlet data
 * @return GPU resource handle
 */
void* Meshlet_UploadToGPU(const meshlet_data_t* data);

/**
 * @brief Free GPU resources for meshlet data
 * @param gpuHandle GPU resource handle from Meshlet_UploadToGPU
 */
void Meshlet_FreeGPUResources(void* gpuHandle);

/**
 * @brief Render meshlets using mesh shaders
 * @param gpuHandle GPU resource handle
 * @param viewProjectionMatrix Current view-projection matrix
 * @param cameraPosition Current camera position
 * @param lodMultiplier LOD distance multiplier
 */
void Meshlet_Render(void* gpuHandle,
                   const float viewProjectionMatrix[16],
                   const vec3_t cameraPosition,
                   float lodMultiplier);

//===============================================================================
// Utility Functions
//===============================================================================

/**
 * @brief Calculate bounding sphere for a set of vertices
 * @param vertices Array of vertices
 * @param vertexCount Number of vertices
 * @param center Output center of bounding sphere
 * @param radius Output radius of bounding sphere
 */
void Meshlet_CalculateBoundingSphere(const meshlet_vertex_t* vertices,
                                    uint32_t vertexCount,
                                    vec3_t center,
                                    float* radius);

/**
 * @brief Check if a meshlet is visible from the current view
 * @param meshlet The meshlet to check
 * @param frustumPlanes Array of 6 frustum planes
 * @return true if visible
 */
qboolean Meshlet_IsVisible(const meshlet_t* meshlet,
                          const vec4_t frustumPlanes[6]);

/**
 * @brief Calculate LOD level for a meshlet based on distance
 * @param center Meshlet center
 * @param cameraPosition Camera position
 * @param lodMultiplier Distance multiplier
 * @return LOD level (0 = highest detail)
 */
uint32_t Meshlet_CalculateLOD(const vec3_t center,
                              const vec3_t cameraPosition,
                              float lodMultiplier);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __MESHLET_H__