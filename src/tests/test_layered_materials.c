// Unit tests for layered materials system
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef UNIT_TEST

#include "../common/material_layer.h"

// Test material creation and destruction
void test_material_create_destroy() {
    printf("Testing material creation and destruction...\n");

    layeredMaterial_t* material = Material_Create("test_material");
    assert(material != NULL);
    assert(strcmp(material->name, "test_material") == 0);
    assert(material->version == 1);
    assert(material->numLayers == 0);

    Material_Free(material);
    printf("✓ Material create/destroy test passed\n");
}

// Test layer management
void test_layer_management() {
    printf("Testing layer management...\n");

    layeredMaterial_t* material = Material_Create("layer_test");

    // Add first layer
    int layer1 = Material_AddLayer(material, "base_layer");
    assert(layer1 == 0);
    assert(material->numLayers == 1);
    assert(strcmp(material->layers[0].name, "base_layer") == 0);
    assert(material->layers[0].enabled == qtrue);
    assert(material->layers[0].blendMode == BLEND_OPAQUE);

    // Add second layer
    int layer2 = Material_AddLayer(material, "overlay_layer");
    assert(layer2 == 1);
    assert(material->numLayers == 2);

    // Test layer properties
    materialLayer_t* layer = &material->layers[layer1];
    layer->opacity = 0.8f;
    layer->metallic = 0.2f;
    layer->roughness = 0.7f;
    layer->baseColor[0] = 1.0f;
    layer->baseColor[1] = 0.5f;
    layer->baseColor[2] = 0.0f;

    assert(layer->opacity == 0.8f);
    assert(layer->metallic == 0.2f);
    assert(layer->roughness == 0.7f);

    // Remove first layer
    assert(Material_RemoveLayer(material, layer1) == qtrue);
    assert(material->numLayers == 1);
    assert(strcmp(material->layers[0].name, "overlay_layer") == 0);

    Material_Free(material);
    printf("✓ Layer management test passed\n");
}

// Test procedural texture generation
void test_procedural_generation() {
    printf("Testing procedural texture generation...\n");

    proceduralParams_t params = {
        .type = PROC_CHECKERBOARD,
        .octaves = 1,
        .frequency = 1.0f,
        .amplitude = 1.0f,
        .persistence = 1.0f,
        .lacunarity = 2.0f,
        .offset = {0.0f, 0.0f, 0.0f},
        .scale = 1.0f,
        .seed = 0
    };

    const int width = 64, height = 64;
    float* output = (float*)malloc(width * height * sizeof(float));

    assert(Procedural_GenerateTexture(width, height, 1, &params, output) == qtrue);

    // Check some known values for checkerboard pattern
    assert(output[0] >= 0.0f && output[0] <= 1.0f); // Should be valid range
    assert(output[width * height - 1] >= 0.0f && output[width * height - 1] <= 1.0f);

    // Checkerboard should have alternating 0.0 and 1.0 values
    int blackCount = 0, whiteCount = 0;
    for (int i = 0; i < width * height; i++) {
        if (output[i] < 0.1f) blackCount++;
        else if (output[i] > 0.9f) whiteCount++;
    }

    // Should have roughly equal black and white squares
    assert(abs(blackCount - whiteCount) < width * height / 4);

    free(output);
    printf("✓ Procedural generation test passed\n");
}

// Test material parameter system
void test_material_parameters() {
    printf("Testing material parameters...\n");

    layeredMaterial_t* material = Material_Create("param_test");

    // Add a layer to test parameters on
    int layerIdx = Material_AddLayer(material, "param_layer");
    materialLayer_t* layer = &material->layers[layerIdx];

    // Add a parameter to the layer
    materialParameter_t param = {
        .name = "test_float",
        .type = PARAM_FLOAT,
        .value.f = 3.14f,
        .isAnimated = qfalse
    };

    layer->parameters[layer->numParameters++] = param;

    // Test parameter retrieval
    materialParameter_t retrieved;
    assert(Material_GetParameter(material, "test_float", &retrieved) == qtrue);
    assert(retrieved.type == PARAM_FLOAT);
    assert(fabsf(retrieved.value.f - 3.14f) < 0.001f);

    // Test non-existent parameter
    assert(Material_GetParameter(material, "nonexistent", &retrieved) == qfalse);

    Material_Free(material);
    printf("✓ Material parameters test passed\n");
}

// Test material compilation (basic validation)
void test_material_compilation() {
    printf("Testing material compilation...\n");

    layeredMaterial_t* material = Material_Create("compile_test");
    material->usePBR = qtrue;

    // Add a basic layer
    Material_AddLayer(material, "base");

    char shaderName[MAX_QPATH];
    assert(Material_Compile(material, shaderName) == qtrue);
    assert(strlen(shaderName) > 0);
    assert(strstr(shaderName, "layered/") == shaderName); // Should have layered/ prefix

    Material_Free(material);
    printf("✓ Material compilation test passed\n");
}

// Test material validation
void test_material_validation() {
    printf("Testing material validation...\n");

    layeredMaterial_t* material = Material_Create("validation_test");

    // Valid empty material should pass
    assert(Material_Validate(material) == qtrue);

    // Add a layer and test
    Material_AddLayer(material, "test_layer");
    assert(Material_Validate(material) == qtrue);

    // Test invalid material (NULL)
    assert(Material_Validate(NULL) == qfalse);

    Material_Free(material);
    printf("✓ Material validation test passed\n");
}

// Test material instancing
void test_material_instancing() {
    printf("Testing material instancing...\n");

    layeredMaterial_t* baseMaterial = Material_Create("base_material");
    Material_AddLayer(baseMaterial, "base_layer");

    // Create instance
    materialInstance_t* instance = MaterialInstance_Create(baseMaterial);
    assert(instance != NULL);
    assert(instance->baseMaterial == baseMaterial);
    assert(instance->numOverrides == 0);

    // Set parameter override
    materialParameter_t overrideParam = {
        .name = "roughness",
        .type = PARAM_FLOAT,
        .value.f = 0.9f,
        .isAnimated = qfalse
    };

    assert(MaterialInstance_SetParameter(instance, "roughness", &overrideParam) == qtrue);
    assert(instance->numOverrides == 1);

    // Test parameter retrieval with override
    materialParameter_t retrieved;
    assert(MaterialInstance_GetParameter(instance, "roughness", &retrieved) == qtrue);
    assert(fabsf(retrieved.value.f - 0.9f) < 0.001f);

    MaterialInstance_Free(instance);
    Material_Free(baseMaterial);
    printf("✓ Material instancing test passed\n");
}

// Main test runner
int main() {
    printf("Running Layered Materials System Tests...\n\n");

    test_material_create_destroy();
    test_layer_management();
    test_procedural_generation();
    test_material_parameters();
    test_material_compilation();
    test_material_validation();
    test_material_instancing();

    printf("\n🎉 All Layered Materials tests passed!\n");
    return 0;
}

#else
int main() { return 0; }
#endif