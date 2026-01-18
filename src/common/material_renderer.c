/*
===============================================================================
Material Renderer Integration Implementation

Implements the bridge between layered materials and the renderer backend.
===============================================================================
*/

#include "material_renderer.h"
#include "qcommon.h"
#include <stdlib.h>

//===============================================================================
// Internal Data Structures
//===============================================================================

typedef struct {
    materialInstance_t* instance;
    int shaderIndex;
    qboolean bound;
    float lastBindTime;
} material_renderer_handle_t;

typedef struct {
    qboolean initialized;
    material_renderer_handle_t* materials;
    int materialCount;
    int maxMaterials;

    // Global rendering state
    vec3_t cameraPosition;
    vec3_t lightDirection;
    float lightIntensity;
    vec3_t ambientColor;
    float currentTime;
} material_renderer_state_t;

static material_renderer_state_t rendererState = {0};

//===============================================================================
// Renderer Backend Interface
//===============================================================================

// These would normally be provided by the renderer backend
// For now, we'll use stub implementations

static int Renderer_RegisterShader(const char* shaderName) {
    // Stub: would register shader with renderer
    Com_Printf("MaterialRenderer: Registering shader '%s'\n", shaderName);
    return 0; // Return shader index
}

static void Renderer_BindShader(int shaderIndex) {
    // Stub: would bind shader for rendering
    Com_DPrintf("MaterialRenderer: Binding shader %d\n", shaderIndex);
}

static void Renderer_SetUniformMatrix4fv(int location, const float* matrix) {
    // Stub: would set matrix uniform
    Com_DPrintf("MaterialRenderer: Setting matrix uniform at location %d\n", location);
}

static void Renderer_SetUniform3fv(int location, const vec3_t vector) {
    // Stub: would set vector uniform
    Com_DPrintf("MaterialRenderer: Setting vector uniform at location %d\n", location);
}

static void Renderer_SetUniform1f(int location, float value) {
    // Stub: would set float uniform
    Com_DPrintf("MaterialRenderer: Setting float uniform at location %d: %f\n", location, value);
}

static void Renderer_RenderGeometry(int vertexCount, int indexCount,
                                  void* vertexBuffer, void* indexBuffer) {
    // Stub: would render geometry
    Com_DPrintf("MaterialRenderer: Rendering %d vertices, %d indices\n", vertexCount, indexCount);
}

static void* Renderer_CreateProceduralTexture(const float* data, int width, int height, int channels) {
    // Stub: would upload texture to GPU
    Com_Printf("MaterialRenderer: Creating procedural texture %dx%d (%d channels)\n",
               width, height, channels);
    return (void*)1; // Return dummy handle
}

static void Renderer_FreeTexture(void* textureHandle) {
    // Stub: would free GPU texture
    Com_DPrintf("MaterialRenderer: Freeing texture handle %p\n", textureHandle);
}

//===============================================================================
// Material Renderer Implementation
//===============================================================================

qboolean MaterialRenderer_Init(void) {
    if (rendererState.initialized) {
        return qtrue;
    }

    memset(&rendererState, 0, sizeof(rendererState));
    rendererState.maxMaterials = 1024;
    rendererState.materials = (material_renderer_handle_t*)malloc(
        sizeof(material_renderer_handle_t) * rendererState.maxMaterials);

    if (!rendererState.materials) {
        return qfalse;
    }

    memset(rendererState.materials, 0,
           sizeof(material_renderer_handle_t) * rendererState.maxMaterials);

    rendererState.initialized = qtrue;
    Com_Printf("MaterialRenderer: Initialized with capacity for %d materials\n",
               rendererState.maxMaterials);

    return qtrue;
}

void MaterialRenderer_Shutdown(void) {
    if (!rendererState.initialized) {
        return;
    }

    // Free all materials
    for (int i = 0; i < rendererState.materialCount; ++i) {
        if (rendererState.materials[i].instance) {
            MaterialInstance_Free(rendererState.materials[i].instance);
        }
    }

    if (rendererState.materials) {
        free(rendererState.materials);
    }

    memset(&rendererState, 0, sizeof(rendererState));
    Com_Printf("MaterialRenderer: Shutdown complete\n");
}

void* MaterialRenderer_LoadMaterial(const char* filename) {
    if (!rendererState.initialized) {
        return NULL;
    }

    // Load the layered material
    layeredMaterial_t* baseMaterial = Material_Load(filename);
    if (!baseMaterial) {
        Com_Printf("MaterialRenderer: Failed to load material '%s'\n", filename);
        return NULL;
    }

    // Create instance
    void* instance = MaterialRenderer_CreateInstance(baseMaterial);

    // Free the base material (instance holds a reference)
    Material_Free(baseMaterial);

    return instance;
}

void* MaterialRenderer_CreateInstance(const layeredMaterial_t* baseMaterial) {
    if (!rendererState.initialized || !baseMaterial) {
        return NULL;
    }

    // Find free slot
    int slot = -1;
    for (int i = 0; i < rendererState.maxMaterials; ++i) {
        if (!rendererState.materials[i].instance) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        Com_Printf("MaterialRenderer: No free material slots available\n");
        return NULL;
    }

    // Create material instance
    materialInstance_t* instance = MaterialInstance_Create(baseMaterial);
    if (!instance) {
        Com_Printf("MaterialRenderer: Failed to create material instance\n");
        return NULL;
    }

    // Initialize renderer handle
    rendererState.materials[slot].instance = instance;
    rendererState.materials[slot].shaderIndex = -1;
    rendererState.materials[slot].bound = qfalse;
    rendererState.materials[slot].lastBindTime = 0.0f;

    if (slot >= rendererState.materialCount) {
        rendererState.materialCount = slot + 1;
    }

    Com_DPrintf("MaterialRenderer: Created material instance in slot %d\n", slot);
    return &rendererState.materials[slot];
}

void MaterialRenderer_FreeMaterial(void* materialHandle) {
    if (!materialHandle) return;

    material_renderer_handle_t* handle = (material_renderer_handle_t*)materialHandle;

    if (handle->instance) {
        MaterialInstance_Free(handle->instance);
        handle->instance = NULL;
    }

    handle->shaderIndex = -1;
    handle->bound = qfalse;
}

qboolean MaterialRenderer_SetParameter(void* materialHandle,
                                     const char* paramName,
                                     const materialParameter_t* value) {
    if (!materialHandle || !paramName || !value) return qfalse;

    material_renderer_handle_t* handle = (material_renderer_handle_t*)materialHandle;

    if (!handle->instance) return qfalse;

    qboolean result = MaterialInstance_SetParameter(handle->instance, paramName, value);
    if (result) {
        // Mark shader as needing recompilation
        handle->shaderIndex = -1;
    }

    return result;
}

void MaterialRenderer_BindMaterial(void* materialHandle,
                                 const float modelMatrix[16],
                                 const float viewMatrix[16],
                                 const float projectionMatrix[16]) {
    if (!materialHandle) return;

    material_renderer_handle_t* handle = (material_renderer_handle_t*)materialHandle;

    if (!handle->instance) return;

    // Compile material if needed
    if (handle->shaderIndex == -1) {
        handle->shaderIndex = MaterialInstance_Compile(handle->instance);
        if (handle->shaderIndex == -1) {
            Com_Printf("MaterialRenderer: Failed to compile material\n");
            return;
        }
    }

    // Bind the shader
    Renderer_BindShader(handle->shaderIndex);

    // Set transformation matrices
    Renderer_SetUniformMatrix4fv(0, modelMatrix);      // modelMatrix location
    Renderer_SetUniformMatrix4fv(1, viewMatrix);       // viewMatrix location
    Renderer_SetUniformMatrix4fv(2, projectionMatrix); // projectionMatrix location

    // Set global parameters
    Renderer_SetUniform3fv(3, rendererState.cameraPosition);   // cameraPosition
    Renderer_SetUniform1f(4, rendererState.currentTime);       // time
    Renderer_SetUniform3fv(5, rendererState.lightDirection);   // lightDirection
    Renderer_SetUniform1f(6, rendererState.lightIntensity);    // lightIntensity
    Renderer_SetUniform3fv(7, rendererState.ambientColor);     // ambientColor

    handle->bound = qtrue;
    handle->lastBindTime = rendererState.currentTime;
}

void MaterialRenderer_RenderGeometry(int vertexCount, int indexCount,
                                   void* vertexBuffer, void* indexBuffer) {
    if (vertexCount <= 0 || indexCount <= 0) return;

    Renderer_RenderGeometry(vertexCount, indexCount, vertexBuffer, indexBuffer);
}

void MaterialRenderer_UnbindMaterial(void) {
    // Unbind any bound materials
    for (int i = 0; i < rendererState.materialCount; ++i) {
        if (rendererState.materials[i].instance && rendererState.materials[i].bound) {
            rendererState.materials[i].bound = qfalse;
        }
    }
}

void MaterialRenderer_UpdateGlobalParams(const vec3_t cameraPosition,
                                       const vec3_t lightDirection,
                                       float lightIntensity,
                                       const vec3_t ambientColor,
                                       float time) {
    VectorCopy(cameraPosition, rendererState.cameraPosition);
    VectorCopy(lightDirection, rendererState.lightDirection);
    VectorCopy(ambientColor, rendererState.ambientColor);
    rendererState.lightIntensity = lightIntensity;
    rendererState.currentTime = time;
}

//===============================================================================
// Procedural Texture Generation
//===============================================================================

void* MaterialRenderer_CreateProceduralTexture(const proceduralParams_t* params,
                                              int width, int height, int channels) {
    if (!params || width <= 0 || height <= 0 || channels < 1 || channels > 4) {
        return NULL;
    }

    // Allocate texture data
    size_t dataSize = width * height * channels * sizeof(float);
    float* textureData = (float*)malloc(dataSize);
    if (!textureData) {
        return NULL;
    }

    // Generate procedural texture
    if (!Procedural_GenerateTexture(width, height, channels, params, textureData)) {
        free(textureData);
        return NULL;
    }

    // Upload to GPU (this would normally happen in the renderer)
    void* textureHandle = Renderer_CreateProceduralTexture(textureData, width, height, channels);

    // Free CPU data
    free(textureData);

    return textureHandle;
}

void MaterialRenderer_FreeProceduralTexture(void* textureHandle) {
    if (textureHandle) {
        Renderer_FreeTexture(textureHandle);
    }
}

//===============================================================================
// Debugging and Introspection
//===============================================================================

size_t MaterialRenderer_GetMemoryUsage(void* materialHandle) {
    if (!materialHandle) return 0;

    material_renderer_handle_t* handle = (material_renderer_handle_t*)materialHandle;

    if (!handle->instance) return 0;

    // Get base material memory usage
    size_t usage = Material_GetMemoryUsage(handle->instance->baseMaterial);

    // Add instance overhead
    usage += sizeof(materialInstance_t);
    usage += handle->instance->numOverrides * sizeof(materialParameter_t);

    return usage;
}

void MaterialRenderer_GetStats(void* materialHandle,
                             int* drawCalls, int* triangles, int* textures) {
    if (!materialHandle || !drawCalls || !triangles || !textures) return;

    material_renderer_handle_t* handle = (material_renderer_handle_t*)materialHandle;

    if (!handle->instance) {
        *drawCalls = *triangles = *textures = 0;
        return;
    }

    // Stub statistics (would be gathered from actual rendering)
    *drawCalls = 1;  // One draw call per material bind
    *triangles = 0;  // Would need to be tracked during rendering
    *textures = 0;   // Count of textures used by material

    // Count textures in material layers
    const layeredMaterial_t* material = handle->instance->baseMaterial;
    for (int i = 0; i < material->numLayers; ++i) {
        const materialLayer_t* layer = &material->layers[i];
        if (layer->diffuseMap[0]) (*textures)++;
        if (layer->normalMap[0]) (*textures)++;
        if (layer->specularMap[0]) (*textures)++;
        if (layer->metallicMap[0]) (*textures)++;
        if (layer->roughnessMap[0]) (*textures)++;
        if (layer->emissiveMap[0]) (*textures)++;
        if (layer->heightMap[0]) (*textures)++;
        if (layer->occlusionMap[0]) (*textures)++;
    }
}

qboolean MaterialRenderer_ValidateMaterial(void* materialHandle) {
    if (!materialHandle) return qfalse;

    material_renderer_handle_t* handle = (material_renderer_handle_t*)materialHandle;

    if (!handle->instance) return qfalse;

    // Validate base material
    if (!Material_Validate(handle->instance->baseMaterial)) {
        return qfalse;
    }

    // Check if material is compiled
    if (handle->shaderIndex == -1) {
        return qfalse;
    }

    return qtrue;
}