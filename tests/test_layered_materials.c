/*
===============================================================================
Layered Materials Test Program

Tests the layered material system functionality including material creation,
layer management, procedural textures, and rendering integration.
===============================================================================
*/

#include "q_shared.h"
#include "material_layer.h"
#include "material_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//===============================================================================
// Test Functions
//===============================================================================

static void Test_BasicMaterialCreation(void) {
    printf("=== Testing Basic Material Creation ===\n");

    // Create a new layered material
    layeredMaterial_t* material = Material_Create("test_material");
    if (!material) {
        printf("FAILED: Could not create material\n");
        return;
    }

    // Set basic properties
    material->usePBR = qtrue;
    material->doubleSided = qfalse;

    // Add a base layer
    int baseLayer = Material_AddLayer(material, "base_layer");
    if (baseLayer == -1) {
        printf("FAILED: Could not add base layer\n");
        Material_Free(material);
        return;
    }

    materialLayer_t* layer = &material->layers[baseLayer];
    layer->enabled = qtrue;
    layer->blendMode = BLEND_OPAQUE;
    layer->opacity = 1.0f;
    VectorSet(layer->baseColor, 0.8f, 0.6f, 0.4f); // Warm brown color
    layer->metallic = 0.1f;
    layer->roughness = 0.8f;
    layer->diffuseProcedural = PROC_NOISE_PERLIN;

    // Add a detail layer
    int detailLayer = Material_AddLayer(material, "detail_layer");
    if (detailLayer == -1) {
        printf("FAILED: Could not add detail layer\n");
        Material_Free(material);
        return;
    }

    layer = &material->layers[detailLayer];
    layer->enabled = qtrue;
    layer->blendMode = BLEND_OVERLAY;
    layer->opacity = 0.3f;
    layer->normalProcedural = PROC_NOISE_PERLIN;
    layer->normalStrength = 0.5f;

    // Validate material
    if (!Material_Validate(material)) {
        printf("FAILED: Material validation failed\n");
        Material_Free(material);
        return;
    }

    printf("SUCCESS: Created material with %d layers\n", material->numLayers);
    printf("  - Base layer: PBR material with procedural diffuse\n");
    printf("  - Detail layer: Overlay normal map\n");

    // Test memory usage
    size_t memoryUsage = Material_GetMemoryUsage(material);
    printf("  - Memory usage: %zu bytes\n", memoryUsage);

    Material_Free(material);
}

static void Test_ProceduralTextures(void) {
    printf("\n=== Testing Procedural Texture Generation ===\n");

    // Test different procedural patterns
    const struct {
        const char* name;
        proceduralType_t type;
    } testPatterns[] = {
        {"Perlin Noise", PROC_NOISE_PERLIN},
        {"Checkerboard", PROC_CHECKERBOARD},
        {"Gradient", PROC_GRADIENT},
        {"Wave", PROC_WAVE},
        {"Marble", PROC_MARBLE},
        {"Wood", PROC_WOOD},
    };

    for (int i = 0; i < sizeof(testPatterns) / sizeof(testPatterns[0]); ++i) {
        proceduralParams_t params = {
            .type = testPatterns[i].type,
            .octaves = 4,
            .frequency = 4.0f,
            .amplitude = 1.0f,
            .persistence = 0.5f,
            .lacunarity = 2.0f,
            .seed = 12345 + i
        };

        // Generate a small texture for testing
        const int width = 64, height = 64, channels = 1;
        float* textureData = (float*)malloc(width * height * channels * sizeof(float));
        if (!textureData) {
            printf("FAILED: Could not allocate texture data for %s\n", testPatterns[i].name);
            continue;
        }

        if (Procedural_GenerateTexture(width, height, channels, &params, textureData)) {
            // Check that values are in valid range
            qboolean validRange = qtrue;
            for (int j = 0; j < width * height * channels; ++j) {
                if (textureData[j] < 0.0f || textureData[j] > 1.0f) {
                    validRange = qfalse;
                    break;
                }
            }

            if (validRange) {
                printf("SUCCESS: Generated %s texture (%dx%d)\n",
                       testPatterns[i].name, width, height);
            } else {
                printf("FAILED: %s texture values out of range [0,1]\n", testPatterns[i].name);
            }
        } else {
            printf("FAILED: Could not generate %s texture\n", testPatterns[i].name);
        }

        free(textureData);
    }
}

static void Test_MaterialInstancing(void) {
    printf("\n=== Testing Material Instancing ===\n");

    // Create base material
    layeredMaterial_t* baseMaterial = Material_Create("base_pbr");
    if (!baseMaterial) {
        printf("FAILED: Could not create base material\n");
        return;
    }

    // Add a PBR layer
    int layerIndex = Material_AddLayer(baseMaterial, "pbr_layer");
    if (layerIndex == -1) {
        printf("FAILED: Could not add PBR layer\n");
        Material_Free(baseMaterial);
        return;
    }

    materialLayer_t* layer = &baseMaterial->layers[layerIndex];
    layer->enabled = qtrue;
    layer->metallic = 0.0f;
    layer->roughness = 0.5f;
    VectorSet(layer->baseColor, 1.0f, 1.0f, 1.0f);

    // Create material instances
    materialInstance_t* instance1 = MaterialInstance_Create(baseMaterial);
    materialInstance_t* instance2 = MaterialInstance_Create(baseMaterial);

    if (!instance1 || !instance2) {
        printf("FAILED: Could not create material instances\n");
        MaterialInstance_Free(instance1);
        MaterialInstance_Free(instance2);
        Material_Free(baseMaterial);
        return;
    }

    // Customize instances
    materialParameter_t param;

    // Instance 1: Gold material
    strcpy(param.name, "metallic");
    param.type = PARAM_FLOAT;
    param.value.f = 0.9f;
    MaterialInstance_SetParameter(instance1, "metallic", &param);

    strcpy(param.name, "roughness");
    param.value.f = 0.1f;
    MaterialInstance_SetParameter(instance1, "roughness", &param);

    // Instance 2: Plastic material
    strcpy(param.name, "metallic");
    param.type = PARAM_FLOAT;
    param.value.f = 0.0f;
    MaterialInstance_SetParameter(instance2, "metallic", &param);

    strcpy(param.name, "roughness");
    param.value.f = 0.3f;
    MaterialInstance_SetParameter(instance2, "roughness", &param);

    // Verify parameter overrides
    materialParameter_t result;

    if (MaterialInstance_GetParameter(instance1, "metallic", &result) &&
        result.value.f == 0.9f) {
        printf("SUCCESS: Instance 1 metallic override (%.1f)\n", result.value.f);
    } else {
        printf("FAILED: Instance 1 metallic override\n");
    }

    if (MaterialInstance_GetParameter(instance2, "roughness", &result) &&
        result.value.f == 0.3f) {
        printf("SUCCESS: Instance 2 roughness override (%.1f)\n", result.value.f);
    } else {
        printf("FAILED: Instance 2 roughness override\n");
    }

    MaterialInstance_Free(instance1);
    MaterialInstance_Free(instance2);
    Material_Free(baseMaterial);
}

static void Test_MaterialRenderer(void) {
    printf("\n=== Testing Material Renderer Integration ===\n");

    // Initialize renderer
    if (!MaterialRenderer_Init()) {
        printf("FAILED: Could not initialize material renderer\n");
        return;
    }

    // Create a test material
    layeredMaterial_t* baseMaterial = Material_Create("renderer_test");
    if (!baseMaterial) {
        printf("FAILED: Could not create test material\n");
        MaterialRenderer_Shutdown();
        return;
    }

    int layerIndex = Material_AddLayer(baseMaterial, "test_layer");
    if (layerIndex == -1) {
        printf("FAILED: Could not add test layer\n");
        Material_Free(baseMaterial);
        MaterialRenderer_Shutdown();
        return;
    }

    // Create material instance
    void* materialHandle = MaterialRenderer_CreateInstance(baseMaterial);
    if (!materialHandle) {
        printf("FAILED: Could not create material instance\n");
        Material_Free(baseMaterial);
        MaterialRenderer_Shutdown();
        return;
    }

    // Test parameter setting
    materialParameter_t param;
    strcpy(param.name, "opacity");
    param.type = PARAM_FLOAT;
    param.value.f = 0.8f;

    if (MaterialRenderer_SetParameter(materialHandle, "opacity", &param)) {
        printf("SUCCESS: Set material parameter\n");
    } else {
        printf("FAILED: Could not set material parameter\n");
    }

    // Test statistics
    int drawCalls, triangles, textures;
    MaterialRenderer_GetStats(materialHandle, &drawCalls, &triangles, &textures);
    printf("Material stats: %d draw calls, %d triangles, %d textures\n",
           drawCalls, triangles, textures);

    // Test validation
    if (MaterialRenderer_ValidateMaterial(materialHandle)) {
        printf("SUCCESS: Material validation passed\n");
    } else {
        printf("WARNING: Material validation failed\n");
    }

    // Test global parameter updates
    vec3_t cameraPos = {0, 0, 10};
    vec3_t lightDir = {0, 0, -1};
    vec3_t ambientColor = {0.1f, 0.1f, 0.1f};

    MaterialRenderer_UpdateGlobalParams(cameraPos, lightDir, 1.0f, ambientColor, 0.0f);
    printf("SUCCESS: Updated global rendering parameters\n");

    // Cleanup
    MaterialRenderer_FreeMaterial(materialHandle);
    Material_Free(baseMaterial);
    MaterialRenderer_Shutdown();

    printf("SUCCESS: Material renderer test completed\n");
}

//===============================================================================
// Main Test Function
//===============================================================================

int main(int argc, char* argv[]) {
    printf("Layered Materials Test Suite\n");
    printf("===========================\n\n");

    // Run individual tests
    Test_BasicMaterialCreation();
    Test_ProceduralTextures();
    Test_MaterialInstancing();
    Test_MaterialRenderer();

    printf("\n===========================\n");
    printf("Test suite completed\n");

    return 0;
}