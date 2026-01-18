/*
===============================================================================
Meshlet System Implementation

Generates and manages meshlets for modern GPU geometry pipelines.
===============================================================================
*/

#include "meshlet.h"
#include "qcommon.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//===============================================================================
// Internal Data Structures
//===============================================================================

typedef struct {
    uint32_t vertexIndex;
    uint32_t triangleIndex;
} meshlet_entry_t;

typedef struct {
    meshlet_vertex_t* vertices;
    uint32_t* indices;
    uint32_t vertexCount;
    uint32_t triangleCount;
} mesh_data_t;

//===============================================================================
// Utility Functions
//===============================================================================

static float vec3_distance(const vec3_t a, const vec3_t b) {
    vec3_t diff;
    VectorSubtract(a, b, diff);
    return VectorLength(diff);
}


static float vec3_dot(const vec3_t a, const vec3_t b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

//===============================================================================
// Bounding Sphere Calculation (Ritter's Algorithm)
//===============================================================================

void Meshlet_CalculateBoundingSphere(const meshlet_vertex_t* vertices,
                                    uint32_t vertexCount,
                                    vec3_t center,
                                    float* radius) {
    if (vertexCount == 0) {
        VectorClear(center);
        *radius = 0.0f;
        return;
    }

    // Start with first two points
    VectorCopy(vertices[0].position, center);
    *radius = 0.0f;

    if (vertexCount == 1) {
        return;
    }

    // Find point farthest from center
    float maxDist = vec3_distance(vertices[1].position, center);
    uint32_t farthest = 1;

    for (uint32_t i = 2; i < vertexCount; ++i) {
        float dist = vec3_distance(vertices[i].position, center);
        if (dist > maxDist) {
            maxDist = dist;
            farthest = i;
        }
    }

    // Set center to farthest point
    VectorCopy(vertices[farthest].position, center);
    *radius = 0.0f;

    // Find point farthest from new center
    maxDist = 0.0f;
    farthest = 0;

    for (uint32_t i = 0; i < vertexCount; ++i) {
        float dist = vec3_distance(vertices[i].position, center);
        if (dist > maxDist) {
            maxDist = dist;
            farthest = i;
        }
    }

    // Update radius
    *radius = maxDist;

    // Ritter's algorithm: check all points and expand sphere as needed
    for (uint32_t i = 0; i < vertexCount; ++i) {
        float dist = vec3_distance(vertices[i].position, center);
        if (dist > *radius) {
            // Point is outside sphere, expand it
            float delta = (dist - *radius) * 0.5f;
            vec3_t direction;
            VectorSubtract(vertices[i].position, center, direction);
            VectorNormalize(direction);

            vec3_t offset;
            VectorScale(direction, delta, offset);
            VectorAdd(center, offset, center);
            *radius += delta;
        }
    }
}

//===============================================================================
// Meshlet Generation
//===============================================================================

static qboolean meshlet_can_add_triangle(const meshlet_t* meshlet,
                                        const mesh_data_t* mesh,
                                        uint32_t triangleIndex,
                                        const meshlet_gen_params_t* params) {
    if (meshlet->vertexCount + 3 > params->maxVertices) {
        return qfalse;
    }

    if (meshlet->indexCount + 3 > params->maxTriangles * 3) {
        return qfalse;
    }

    // Check if adding this triangle would exceed the bounding sphere
    if (meshlet->vertexCount > 0) {
        // Calculate bounding sphere including new triangle vertices
        meshlet_vertex_t testVertices[MESHLET_MAX_VERTICES + 3];
        memcpy(testVertices, &mesh->vertices[meshlet->vertexOffset], meshlet->vertexCount * sizeof(meshlet_vertex_t));

        const uint32_t* triIndices = &mesh->indices[triangleIndex * 3];
        for (uint32_t i = 0; i < 3; ++i) {
            testVertices[meshlet->vertexCount + i] = mesh->vertices[triIndices[i]];
        }

        vec3_t center;
        float radius;
        Meshlet_CalculateBoundingSphere(testVertices, meshlet->vertexCount + 3, center, &radius);

        if (radius > params->maxMeshletRadius) {
            return qfalse;
        }
    }

    return qtrue;
}

static void meshlet_add_triangle(meshlet_t* meshlet,
                                mesh_data_t* mesh,
                                uint32_t triangleIndex,
                                uint32_t* vertexMap,
                                uint32_t* vertexCount) {
    const uint32_t* triIndices = &mesh->indices[triangleIndex * 3];

    // Add vertices to meshlet if not already present
    for (uint32_t i = 0; i < 3; ++i) {
        uint32_t globalIndex = triIndices[i];

        // Check if vertex is already in meshlet
        qboolean found = qfalse;

        for (uint32_t j = 0; j < *vertexCount; ++j) {
            if (vertexMap[j] == globalIndex) {
                found = qtrue;
                break;
            }
        }

        if (!found) {
            // Add new vertex
            vertexMap[*vertexCount] = globalIndex;
            (*vertexCount)++;
        }

        // Add index
        meshlet->vertexCount = *vertexCount;
    }

    meshlet->indexCount += 3;
}

meshlet_data_t* Meshlet_Generate(const meshlet_vertex_t* vertices,
                                uint32_t vertexCount,
                                const uint32_t* indices,
                                uint32_t triangleCount,
                                const meshlet_gen_params_t* params) {
    if (!vertices || !indices || vertexCount == 0 || triangleCount == 0) {
        return NULL;
    }

    mesh_data_t mesh = {
        .vertices = (meshlet_vertex_t*)vertices,
        .indices = (uint32_t*)indices,
        .vertexCount = vertexCount,
        .triangleCount = triangleCount
    };

    // Allocate meshlet data
    meshlet_data_t* data = (meshlet_data_t*)malloc(sizeof(meshlet_data_t));
    if (!data) {
        return NULL;
    }

    memset(data, 0, sizeof(meshlet_data_t));

    // Estimate number of meshlets needed
    uint32_t estimatedMeshlets = triangleCount / (params->maxTriangles / 2) + 1;
    data->meshlets = (meshlet_t*)malloc(estimatedMeshlets * sizeof(meshlet_t));
    if (!data->meshlets) {
        free(data);
        return NULL;
    }

    // Generate meshlets using a simple greedy algorithm
    uint32_t currentTriangle = 0;
    uint32_t meshletIndex = 0;

    while (currentTriangle < triangleCount) {
        if (meshletIndex >= estimatedMeshlets) {
            // Reallocate more space
            estimatedMeshlets *= 2;
            meshlet_t* newMeshlets = (meshlet_t*)realloc(data->meshlets, estimatedMeshlets * sizeof(meshlet_t));
            if (!newMeshlets) {
                Meshlet_FreeData(data);
                return NULL;
            }
            data->meshlets = newMeshlets;
        }

        meshlet_t* meshlet = &data->meshlets[meshletIndex];
        memset(meshlet, 0, sizeof(meshlet_t));

        meshlet->vertexOffset = data->totalVertices;
        meshlet->indexOffset = data->totalIndices;
        meshlet->lodLevel = 0;

        uint32_t vertexMap[MESHLET_MAX_VERTICES];
        uint32_t localVertexCount = 0;

        // Try to add triangles to this meshlet
        qboolean addedTriangle = qfalse;

        for (uint32_t i = currentTriangle; i < triangleCount && !addedTriangle; ++i) {
            if (meshlet_can_add_triangle(meshlet, &mesh, i, params)) {
                meshlet_add_triangle(meshlet, &mesh, i, vertexMap, &localVertexCount);
                currentTriangle = i + 1;
                addedTriangle = qtrue;
            }
        }

        if (!addedTriangle) {
            // Could not add any more triangles, start a new meshlet
            currentTriangle++;
        }

        // Calculate bounding sphere for this meshlet
        if (meshlet->vertexCount > 0) {
            meshlet_vertex_t meshletVertices[MESHLET_MAX_VERTICES];
            for (uint32_t i = 0; i < meshlet->vertexCount; ++i) {
                meshletVertices[i] = mesh.vertices[vertexMap[i]];
            }

            Meshlet_CalculateBoundingSphere(meshletVertices, meshlet->vertexCount,
                                           meshlet->center, &meshlet->radius);

            data->totalVertices += meshlet->vertexCount;
            data->totalIndices += meshlet->indexCount;
            meshletIndex++;
        }
    }

    data->meshletCount = meshletIndex;

    // Optimize for GPU consumption
    if (!Meshlet_OptimizeForGPU(data)) {
        Com_Printf("Warning: Failed to optimize meshlet data for GPU\n");
    }

    Com_Printf("Generated %u meshlets from %u triangles (%u vertices total)\n",
               data->meshletCount, triangleCount, data->totalVertices);

    return data;
}

void Meshlet_FreeData(meshlet_data_t* data) {
    if (!data) return;

    if (data->meshlets) {
        free(data->meshlets);
    }

    free(data);
}

qboolean Meshlet_OptimizeForGPU(meshlet_data_t* data) {
    if (!data || !data->meshlets) return qfalse;

    // Sort meshlets by size (larger first) for better GPU utilization
    // This is a simple bubble sort for demonstration
    for (uint32_t i = 0; i < data->meshletCount - 1; ++i) {
        for (uint32_t j = 0; j < data->meshletCount - i - 1; ++j) {
            if (data->meshlets[j].vertexCount < data->meshlets[j + 1].vertexCount) {
                meshlet_t temp = data->meshlets[j];
                data->meshlets[j] = data->meshlets[j + 1];
                data->meshlets[j + 1] = temp;
            }
        }
    }

    return qtrue;
}

void Meshlet_GetDefaultParams(meshlet_gen_params_t* params) {
    if (!params) return;

    params->maxMeshletRadius = 1000.0f;  // 1000 units max radius
    params->maxVertices = MESHLET_MAX_VERTICES;
    params->maxTriangles = MESHLET_MAX_TRIANGLES;
    params->lodDistanceMultiplier = 0.001f;  // Distance scaling for LOD
    params->generateLODs = qtrue;
}

//===============================================================================
// Rendering API (Stub Implementation)
//===============================================================================

void* Meshlet_UploadToGPU(const meshlet_data_t* data) {
    if (!data || data->meshletCount == 0) {
        return NULL;
    }

    // Create a GPU resource handle structure
    typedef struct {
        VkBuffer meshletBuffer;
        VkDeviceMemory meshletMemory;
        VkBuffer vertexBuffer;
        VkDeviceMemory vertexMemory;
        VkBuffer indexBuffer;
        VkDeviceMemory indexMemory;
        uint32_t meshletCount;
        uint32_t totalVertices;
        uint32_t totalIndices;
    } meshlet_gpu_resources_t;

    meshlet_gpu_resources_t* resources = (meshlet_gpu_resources_t*)malloc(sizeof(meshlet_gpu_resources_t));
    if (!resources) {
        return NULL;
    }

    memset(resources, 0, sizeof(meshlet_gpu_resources_t));
    resources->meshletCount = data->meshletCount;
    resources->totalVertices = data->totalVertices;
    resources->totalIndices = data->totalIndices;

    // Get Vulkan device (this would need to be passed in or accessed via renderer interface)
    // For now, this is a placeholder implementation
    Com_DPrintf("Meshlet_UploadToGPU: Creating GPU resources for %u meshlets (%u vertices, %u indices)\n",
               data->meshletCount, data->totalVertices, data->totalIndices);

    // TODO: Implement actual Vulkan buffer creation
    // This would require access to the Vulkan renderer context
    // For now, we just allocate the structure and return it

    return resources;
}

void Meshlet_FreeGPUResources(void* gpuHandle) {
    if (!gpuHandle) return;

    typedef struct {
        VkBuffer meshletBuffer;
        VkDeviceMemory meshletMemory;
        VkBuffer vertexBuffer;
        VkDeviceMemory vertexMemory;
        VkBuffer indexBuffer;
        VkDeviceMemory indexMemory;
        uint32_t meshletCount;
        uint32_t totalVertices;
        uint32_t totalIndices;
    } meshlet_gpu_resources_t;

    meshlet_gpu_resources_t* resources = (meshlet_gpu_resources_t*)gpuHandle;

    Com_DPrintf("Meshlet_FreeGPUResources: Freeing GPU resources for %u meshlets\n",
               resources->meshletCount);

    // TODO: Implement actual Vulkan resource cleanup
    // This would destroy buffers and free device memory

    free(resources);
}

void Meshlet_Render(void* gpuHandle,
                   const float viewProjectionMatrix[16],
                   const vec3_t cameraPosition,
                   float lodMultiplier) {
    if (!gpuHandle) return;

    typedef struct {
        VkBuffer meshletBuffer;
        VkDeviceMemory meshletMemory;
        VkBuffer vertexBuffer;
        VkDeviceMemory vertexMemory;
        VkBuffer indexBuffer;
        VkDeviceMemory indexMemory;
        uint32_t meshletCount;
        uint32_t totalVertices;
        uint32_t totalIndices;
    } meshlet_gpu_resources_t;

    meshlet_gpu_resources_t* resources = (meshlet_gpu_resources_t*)gpuHandle;

    Com_DPrintf("Meshlet_Render: Rendering %u meshlets with LOD multiplier %.3f\n",
               resources->meshletCount, lodMultiplier);

    // TODO: Implement actual mesh shader rendering
    // This would involve:
    // 1. Binding the mesh shader pipeline
    // 2. Updating descriptor sets with meshlet data
    // 3. Setting push constants (matrices, camera position, LOD params)
    // 4. Dispatching mesh shader work groups
    // 5. Handling frustum culling and LOD selection in the task shader

    // Placeholder: log that rendering would happen
    Com_DPrintf("Meshlet rendering would dispatch %u mesh shader work groups\n",
               (resources->meshletCount + 31) / 32); // Assuming 32 meshlets per work group
}

//===============================================================================
// Visibility and LOD Functions
//===============================================================================

qboolean Meshlet_IsVisible(const meshlet_t* meshlet,
                          const vec4_t frustumPlanes[6]) {
    if (!meshlet) return qfalse;

    // Test bounding sphere against all 6 frustum planes
    for (int i = 0; i < 6; ++i) {
        float dist = vec3_dot(meshlet->center, frustumPlanes[i]) + frustumPlanes[i][3];
        if (dist + meshlet->radius < 0.0f) {
            return qfalse;  // Completely outside this plane
        }
    }

    return qtrue;  // Visible
}

uint32_t Meshlet_CalculateLOD(const vec3_t center,
                              const vec3_t cameraPosition,
                              float lodMultiplier) {
    float distance = vec3_distance(center, cameraPosition);
    float screenSize = 1.0f / (distance * lodMultiplier);

    // Simple LOD calculation based on screen size
    if (screenSize > 0.8f) return 0;  // High detail
    if (screenSize > 0.4f) return 1;  // Medium detail
    if (screenSize > 0.2f) return 2;  // Low detail
    return 3;  // Very low detail
}