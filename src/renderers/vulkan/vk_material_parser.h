/*
=============================================================================
Material File Parser for .mat files
=============================================================================
*/

#pragma once

#include "tr_local.h"

#ifdef USE_VULKAN

#define MAX_MATERIAL_FILES 32
#define MAX_MATERIAL_NAME 256
#define MAX_MATERIAL_ENTRIES 1024
#define MAX_MATERIAL_PROPERTIES 32

// Material property types
typedef enum {
    MATERIAL_PROP_TEXTURE_BASE = 0,
    MATERIAL_PROP_TEXTURE_NORMALS,
    MATERIAL_PROP_TEXTURE_EMISSIVE,
    MATERIAL_PROP_ROUGHNESS_OVERRIDE,
    MATERIAL_PROP_METALLIC,
    MATERIAL_PROP_EMISSIVE_FACTOR,
    MATERIAL_PROP_KIND,
    MATERIAL_PROP_IS_LIGHT,
    MATERIAL_PROP_CORRECT_ALBEDO,
    MATERIAL_PROP_SYNTH_EMISSIVE,
    MATERIAL_PROP_EMISSIVE_THRESHOLD,
    MATERIAL_PROP_BUMP_SCALE,
    MATERIAL_PROP_BASE_FACTOR,
    MATERIAL_PROP_MAX
} materialPropertyType_t;

// Material property value
typedef struct {
    materialPropertyType_t type;
    union {
        char stringValue[MAX_MATERIAL_NAME];
        float floatValue;
        int intValue;
    } value;
} materialProperty_t;

// Material entry (one or more texture names mapped to properties)
typedef struct {
    char textureNames[MAX_MATERIAL_NAME]; // Comma-separated list of texture names
    materialProperty_t properties[MAX_MATERIAL_PROPERTIES];
    int numProperties;
} materialEntry_t;

// Material file
typedef struct {
    char filename[MAX_QPATH];
    materialEntry_t entries[MAX_MATERIAL_ENTRIES];
    int numEntries;
} materialFile_t;

// Global material parser state
typedef struct {
    materialFile_t files[MAX_MATERIAL_FILES];
    int numFiles;
    qboolean initialized;
} materialParser_t;

// Material parser API
void vk_material_parser_init(void);
void vk_material_parser_shutdown(void);
void vk_material_parser_load_files(void);
const materialEntry_t* vk_material_parser_find_entry(const char* textureName);
void vk_material_parser_apply_to_material(const materialEntry_t* entry, const char* materialName, material_params_t* params);

// Shader integration
void vk_material_parser_apply_to_shader_stage(const materialEntry_t* entry, shaderStage_t* stage);

#endif // USE_VULKAN
