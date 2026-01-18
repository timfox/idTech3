/*
===============================================================================
Simple Layered Materials Test

Tests core layered material functionality without engine dependencies.
===============================================================================
*/

#include "../src/common/material_layer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Simple implementations for missing functions
void Q_strncpyz(char* dest, const char* src, int destsize) {
    strncpy(dest, src, destsize - 1);
    dest[destsize - 1] = '\0';
}

float Com_Clamp(float min, float max, float value) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

void MyVectorSet(vec3_t v, float x, float y, float z) {
    v[0] = x; v[1] = y; v[2] = z;
}

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
    MyVectorSet(layer->baseColor, 0.8f, 0.6f, 0.4f); // Warm brown color
    layer->metallic = 0.1f;
    layer->roughness = 0.8f;
    layer->diffuseProcedural = PROC_NOISE_PERLIN;

    // Validate material
    if (!Material_Validate(material)) {
        printf("FAILED: Material validation failed\n");
        Material_Free(material);
        return;
    }

    printf("SUCCESS: Created material with %d layers\n", material->numLayers);
    printf("  - Base layer: PBR material with procedural diffuse\n");

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
        const int width = 32, height = 32, channels = 1;
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
    MyVectorSet(layer->baseColor, 1.0f, 1.0f, 1.0f);

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
    param.type = PARAM_FLOAT;
    param.value.f = 0.3f;
    MaterialInstance_SetParameter(instance2, "roughness", &param);

    // Verify parameter overrides
    materialParameter_t result;

    if (MaterialInstance_GetParameter(instance1, "metallic", &result) &&
        fabs(result.value.f - 0.9f) < 0.001f) {
        printf("SUCCESS: Instance 1 metallic override (%.1f)\n", result.value.f);
    } else {
        printf("FAILED: Instance 1 metallic override\n");
    }

    if (MaterialInstance_GetParameter(instance2, "roughness", &result) &&
        fabs(result.value.f - 0.3f) < 0.001f) {
        printf("SUCCESS: Instance 2 roughness override (%.1f)\n", result.value.f);
    } else {
        printf("FAILED: Instance 2 roughness override\n");
    }

    MaterialInstance_Free(instance1);
    MaterialInstance_Free(instance2);
    Material_Free(baseMaterial);
}

//===============================================================================
// Main Test Function
//===============================================================================

int main(int argc, char* argv[]) {
    printf("Simple Layered Materials Test\n");
    printf("=============================\n\n");

    // Run individual tests
    Test_BasicMaterialCreation();
    Test_ProceduralTextures();
    Test_MaterialInstancing();

    printf("\n=============================\n");
    printf("Test suite completed\n");

    return 0;
}