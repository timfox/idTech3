/*
===============================================================================
Material Renderer Integration

Integration layer between the layered material system and the renderer.
Provides functions to load, compile, and render layered materials.
===============================================================================
*/

#pragma once

#include "material_layer.h"

#ifdef __cplusplus
extern "C" {
#endif

//===============================================================================
// Renderer Integration Functions
//===============================================================================

/**
 * @brief Initialize the material renderer system
 * @return true on success
 */
qboolean MaterialRenderer_Init(void);

/**
 * @brief Shutdown the material renderer system
 */
void MaterialRenderer_Shutdown(void);

/**
 * @brief Load a layered material from file and prepare for rendering
 * @param filename Material file path
 * @return Material instance handle or NULL on failure
 */
void* MaterialRenderer_LoadMaterial(const char* filename);

/**
 * @brief Create a material instance from a base material
 * @param baseMaterial Base layered material
 * @return Material instance handle or NULL on failure
 */
void* MaterialRenderer_CreateInstance(const layeredMaterial_t* baseMaterial);

/**
 * @brief Free a material instance
 * @param materialHandle Material instance handle
 */
void MaterialRenderer_FreeMaterial(void* materialHandle);

/**
 * @brief Set a parameter on a material instance
 * @param materialHandle Material instance handle
 * @param paramName Parameter name
 * @param value Parameter value
 * @return true on success
 */
qboolean MaterialRenderer_SetParameter(void* materialHandle,
                                     const char* paramName,
                                     const materialParameter_t* value);

/**
 * @brief Bind a material for rendering
 * @param materialHandle Material instance handle
 * @param modelMatrix Model transformation matrix
 * @param viewMatrix View transformation matrix
 * @param projectionMatrix Projection transformation matrix
 */
void MaterialRenderer_BindMaterial(void* materialHandle,
                                 const float modelMatrix[16],
                                 const float viewMatrix[16],
                                 const float projectionMatrix[16]);

/**
 * @brief Render geometry with the currently bound material
 * @param vertexCount Number of vertices to render
 * @param indexCount Number of indices to render
 * @param vertexBuffer Vertex buffer handle
 * @param indexBuffer Index buffer handle
 */
void MaterialRenderer_RenderGeometry(int vertexCount, int indexCount,
                                   void* vertexBuffer, void* indexBuffer);

/**
 * @brief Unbind the current material
 */
void MaterialRenderer_UnbindMaterial(void);

/**
 * @brief Update global rendering parameters
 * @param cameraPosition Current camera position
 * @param lightDirection Main light direction
 * @param lightIntensity Main light intensity
 * @param ambientColor Ambient light color
 * @param time Current time for animations
 */
void MaterialRenderer_UpdateGlobalParams(const vec3_t cameraPosition,
                                       const vec3_t lightDirection,
                                       float lightIntensity,
                                       const vec3_t ambientColor,
                                       float time);

//===============================================================================
// Procedural Texture Generation (Renderer Integration)
//===============================================================================

/**
 * @brief Generate a procedural texture and upload to GPU
 * @param params Procedural generation parameters
 * @param width Texture width
 * @param height Texture height
 * @param channels Number of color channels
 * @return GPU texture handle or NULL on failure
 */
void* MaterialRenderer_CreateProceduralTexture(const proceduralParams_t* params,
                                              int width, int height, int channels);

/**
 * @brief Free a procedural texture
 * @param textureHandle GPU texture handle
 */
void MaterialRenderer_FreeProceduralTexture(void* textureHandle);

//===============================================================================
// Material Debugging and Introspection
//===============================================================================

/**
 * @brief Get material memory usage statistics
 * @param materialHandle Material instance handle
 * @return Memory usage in bytes
 */
size_t MaterialRenderer_GetMemoryUsage(void* materialHandle);

/**
 * @brief Get material rendering statistics
 * @param materialHandle Material instance handle
 * @param drawCalls Number of draw calls (output)
 * @param triangles Number of triangles rendered (output)
 * @param textures Number of textures used (output)
 */
void MaterialRenderer_GetStats(void* materialHandle,
                             int* drawCalls, int* triangles, int* textures);

/**
 * @brief Validate material for rendering
 * @param materialHandle Material instance handle
 * @return true if material is valid for rendering
 */
qboolean MaterialRenderer_ValidateMaterial(void* materialHandle);

#ifdef __cplusplus
} // extern "C"
#endif