/*
===============================================================================
Layered Material System - Modern Material Pipeline

Extends the traditional shader system with layered materials and procedural
generation capabilities. Supports multiple material layers with blending,
procedural textures, and advanced material properties.
===============================================================================
*/

#pragma once

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

//===============================================================================
// Basic Types (from renderer)
//===============================================================================

// cullType_t is defined in renderer headers
// This header assumes cullType_t is defined by including renderer headers

// Define cullType_t as an enum to match renderer expectations
typedef enum {
    CT_FRONT_SIDED,
    CT_BACK_SIDED,
    CT_TWO_SIDED
} cullType_t;

//===============================================================================
// Material Layer System
//===============================================================================

#define MAX_MATERIAL_LAYERS       16
#define MAX_MATERIAL_PARAMETERS   32
#define MAX_PROCEDURAL_TEXTURES   8

// Material layer blend modes
typedef enum {
    BLEND_OPAQUE,           // No blending, fully opaque
    BLEND_ALPHA,            // Standard alpha blending
    BLEND_ADDITIVE,         // Additive blending
    BLEND_MULTIPLY,         // Multiply blending
    BLEND_SCREEN,           // Screen blending
    BLEND_OVERLAY,          // Overlay blending
    BLEND_SOFT_LIGHT,       // Soft light blending
    BLEND_HARD_LIGHT,       // Hard light blending
    BLEND_DIFFERENCE,       // Difference blending
    BLEND_EXCLUSION,        // Exclusion blending
    BLEND_COLOR_DODGE,      // Color dodge
    BLEND_COLOR_BURN,       // Color burn
    BLEND_NORMAL_MAP,       // Normal map blending
    BLEND_HEIGHT_MAP,       // Height map blending
    BLEND_MASK,             // Mask layer (cuts out areas)
} materialBlendMode_t;

// Material parameter types
typedef enum {
    PARAM_FLOAT,
    PARAM_VEC2,
    PARAM_VEC3,
    PARAM_VEC4,
    PARAM_COLOR,
    PARAM_TEXTURE,
    PARAM_PROCEDURAL,
} materialParamType_t;

// Procedural texture types
typedef enum {
    PROC_NOISE_PERLIN,
    PROC_NOISE_SIMPLEX,
    PROC_NOISE_VORONOI,
    PROC_CHECKERBOARD,
    PROC_BRICK,
    PROC_GRADIENT,
    PROC_WAVE,
    PROC_FRACTAL,
    PROC_MARBLE,
    PROC_WOOD,
    PROC_CLOUDS,
    PROC_CRACKED,       // Cracked/dried mud effect
    PROC_TURBULENCE,    // Turbulence distortion
    PROC_RIDGED,        // Ridged multifractal
    PROC_TERRAIN,       // Terrain heightmap
    PROC_FIRE,          // Fire/flame effect
    PROC_WATER,         // Water surface
    PROC_SMOKE,         // Smoke/cloud effect
} proceduralType_t;

// Material parameter definition
typedef struct {
    char name[64];
    materialParamType_t type;
    union {
        float f;
        vec2_t v2;
        vec3_t v3;
        vec4_t v4;
        color4ub_t color;
        char texture[MAX_QPATH];
        proceduralType_t procedural;
    } value;
    qboolean isAnimated;    // Parameter can be animated over time
} materialParameter_t;

// Material layer definition
typedef struct {
    char name[64];
    qboolean enabled;

    // Base properties
    materialBlendMode_t blendMode;
    float opacity;
    vec2_t uvOffset;
    vec2_t uvScale;
    float uvRotation;

    // Texture maps
    char diffuseMap[MAX_QPATH];      // Base color
    char normalMap[MAX_QPATH];       // Normal map
    char specularMap[MAX_QPATH];     // Specular/roughness
    char metallicMap[MAX_QPATH];     // Metallic map
    char roughnessMap[MAX_QPATH];    // Roughness map
    char emissiveMap[MAX_QPATH];     // Emissive map
    char heightMap[MAX_QPATH];       // Height/displacement map
    char occlusionMap[MAX_QPATH];    // Ambient occlusion

    // Procedural textures (alternative to texture maps)
    proceduralType_t diffuseProcedural;
    proceduralType_t normalProcedural;
    proceduralType_t specularProcedural;

    // Material properties
    vec3_t baseColor;
    float metallic;
    float roughness;
    float emissiveIntensity;
    float normalStrength;
    float heightScale;

    // Animation properties
    float scrollSpeedU;
    float scrollSpeedV;
    float rotationSpeed;
    float waveFrequency;
    float waveAmplitude;

    // Custom parameters
    materialParameter_t parameters[MAX_MATERIAL_PARAMETERS];
    int numParameters;
} materialLayer_t;

// Complete layered material
typedef struct {
    char name[MAX_QPATH];
    int version;

    // Global material properties
    qboolean doubleSided;
    qboolean translucent;
    cullType_t cullMode;
    qboolean depthWrite;
    qboolean depthTest;

    // Lighting model
    qboolean usePBR;        // Physically-based rendering
    qboolean useIBL;        // Image-based lighting
    qboolean useSSS;        // Sub-surface scattering
    qboolean useAnisotropy; // Anisotropic materials

    // Layers
    materialLayer_t layers[MAX_MATERIAL_LAYERS];
    int numLayers;

    // Global parameters
    materialParameter_t globalParams[MAX_MATERIAL_PARAMETERS];
    int numGlobalParams;

    // Rendering hints
    qboolean needsTangents;
    qboolean needsNormals;
    qboolean needsUV2;
    qboolean needsVertexColor;
} layeredMaterial_t;

//===============================================================================
// Procedural Texture Generation
//===============================================================================

// Noise generation parameters
typedef struct {
    proceduralType_t type;
    int octaves;
    float frequency;
    float amplitude;
    float lacunarity;
    float persistence;
    vec3_t offset;
    float scale;
    int seed;
} proceduralParams_t;

/**
 * @brief Generate procedural texture data
 * @param width Texture width
 * @param height Texture height
 * @param channels Number of color channels (1-4)
 * @param params Procedural generation parameters
 * @param output Output buffer (must be width * height * channels * sizeof(float))
 * @return true on success
 */
qboolean Procedural_GenerateTexture(int width, int height, int channels,
                                   const proceduralParams_t* params,
                                   float* output);

/**
 * @brief Get procedural texture value at position
 * @param x X coordinate (0.0 to 1.0)
 * @param y Y coordinate (0.0 to 1.0)
 * @param params Procedural parameters
 * @return Value between 0.0 and 1.0
 */
float Procedural_GetValue(float x, float y, const proceduralParams_t* params);

//===============================================================================
// Material Layer Management
//===============================================================================

/**
 * @brief Create a new layered material
 * @param name Material name
 * @return Pointer to new material (must be freed with Material_Free)
 */
layeredMaterial_t* Material_Create(const char* name);

/**
 * @brief Load layered material from file
 * @param filename Material file path
 * @return Loaded material or NULL on failure
 */
layeredMaterial_t* Material_Load(const char* filename);

/**
 * @brief Save layered material to file
 * @param material Material to save
 * @param filename Output file path
 * @return true on success
 */
qboolean Material_Save(const layeredMaterial_t* material, const char* filename);

/**
 * @brief Free layered material
 * @param material Material to free
 */
void Material_Free(layeredMaterial_t* material);

/**
 * @brief Add a layer to a material
 * @param material Target material
 * @param name Layer name
 * @return Layer index or -1 on failure
 */
int Material_AddLayer(layeredMaterial_t* material, const char* name);

/**
 * @brief Remove a layer from a material
 * @param material Target material
 * @param layerIndex Layer index to remove
 * @return true on success
 */
qboolean Material_RemoveLayer(layeredMaterial_t* material, int layerIndex);

/**
 * @brief Set material parameter
 * @param material Target material
 * @param paramName Parameter name
 * @param value Parameter value
 * @return true on success
 */
qboolean Material_SetParameter(layeredMaterial_t* material, const char* paramName,
                              const materialParameter_t* value);

/**
 * @brief Get material parameter
 * @param material Source material
 * @param paramName Parameter name
 * @param value Output parameter value
 * @return true if parameter found
 */
qboolean Material_GetParameter(const layeredMaterial_t* material, const char* paramName,
                              materialParameter_t* value);

/**
 * @brief Compile layered material into renderer-compatible format
 * @param material Source material
 * @param shaderName Output shader name
 * @return true on success
 */
qboolean Material_Compile(const layeredMaterial_t* material, char* shaderName);

//===============================================================================
// Material Instance System
//===============================================================================

// Material instance for runtime use
typedef struct {
    const layeredMaterial_t* baseMaterial;
    materialParameter_t overrideParams[MAX_MATERIAL_PARAMETERS];
    int numOverrides;

    // Runtime state
    qboolean compiled;
    int shaderIndex;
    int lastModifiedTime;
} materialInstance_t;

/**
 * @brief Create material instance
 * @param baseMaterial Base material to instance
 * @return New material instance
 */
materialInstance_t* MaterialInstance_Create(const layeredMaterial_t* baseMaterial);

/**
 * @brief Free material instance
 * @param instance Instance to free
 */
void MaterialInstance_Free(materialInstance_t* instance);

/**
 * @brief Set instance parameter override
 * @param instance Material instance
 * @param paramName Parameter name
 * @param value Parameter value
 * @return true on success
 */
qboolean MaterialInstance_SetParameter(materialInstance_t* instance,
                                     const char* paramName,
                                     const materialParameter_t* value);

/**
 * @brief Get final parameter value (with overrides applied)
 * @param instance Material instance
 * @param paramName Parameter name
 * @param value Output parameter value
 * @return true if parameter found
 */
qboolean MaterialInstance_GetParameter(const materialInstance_t* instance,
                                     const char* paramName,
                                     materialParameter_t* value);

/**
 * @brief Compile material instance for rendering
 * @param instance Material instance
 * @return Shader index for rendering
 */
int MaterialInstance_Compile(materialInstance_t* instance);

//===============================================================================
// Utility Functions
//===============================================================================

/**
 * @brief Validate material data
 * @param material Material to validate
 * @return true if valid
 */
qboolean Material_Validate(const layeredMaterial_t* material);

/**
 * @brief Get material memory usage
 * @param material Material to analyze
 * @return Memory usage in bytes
 */
size_t Material_GetMemoryUsage(const layeredMaterial_t* material);

/**
 * @brief Optimize material for rendering
 * @param material Material to optimize
 * @return true on success
 */
qboolean Material_Optimize(layeredMaterial_t* material);

#ifdef __cplusplus
} // extern "C"
#endif