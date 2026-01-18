/*
===============================================================================
Layered Material System Implementation

Implementation of the modern layered material system with procedural generation.
===============================================================================
*/

#include "material_layer.h"
#include "qcommon.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

//===============================================================================
// Forward Declarations
//===============================================================================

static float Procedural_Noise(float x, float y);
static float Procedural_PerlinNoise(float x, float y);
static float Procedural_SimplexNoise(float x, float y);
static float Procedural_VoronoiNoise(float x, float y, float* featurePointX, float* featurePointY);

//===============================================================================
// Material Management
//===============================================================================

layeredMaterial_t* Material_Create(const char* name) {
    layeredMaterial_t* material = (layeredMaterial_t*)malloc(sizeof(layeredMaterial_t));
    if (!material) {
        return NULL;
    }

    memset(material, 0, sizeof(layeredMaterial_t));
    Q_strncpyz(material->name, name, sizeof(material->name));
    material->version = 1;

    // Set defaults
    material->doubleSided = qfalse;
    material->translucent = qfalse;
    material->cullMode = CT_FRONT_SIDED;
    material->depthWrite = qtrue;
    material->depthTest = qtrue;
    material->usePBR = qtrue;
    material->useIBL = qtrue;

    return material;
}

void Material_Free(layeredMaterial_t* material) {
    if (!material) return;
    free(material);
}

int Material_AddLayer(layeredMaterial_t* material, const char* name) {
    if (!material || material->numLayers >= MAX_MATERIAL_LAYERS) {
        return -1;
    }

    int layerIndex = material->numLayers++;
    materialLayer_t* layer = &material->layers[layerIndex];

    memset(layer, 0, sizeof(materialLayer_t));
    Q_strncpyz(layer->name, name, sizeof(layer->name));
    layer->enabled = qtrue;
    layer->blendMode = BLEND_OPAQUE;
    layer->opacity = 1.0f;
    layer->uvScale[0] = layer->uvScale[1] = 1.0f;
    layer->metallic = 0.0f;
    layer->roughness = 0.5f;
    layer->baseColor[0] = layer->baseColor[1] = layer->baseColor[2] = 1.0f;

    return layerIndex;
}

qboolean Material_RemoveLayer(layeredMaterial_t* material, int layerIndex) {
    if (!material || layerIndex < 0 || layerIndex >= material->numLayers) {
        return qfalse;
    }

    // Shift remaining layers down
    for (int i = layerIndex; i < material->numLayers - 1; ++i) {
        material->layers[i] = material->layers[i + 1];
    }

    material->numLayers--;
    return qtrue;
}

//===============================================================================
// Procedural Texture Generation
//===============================================================================

// Simple hash function for noise generation
static uint32_t hash(uint32_t x, uint32_t y, uint32_t seed) {
    uint32_t h = seed;
    h ^= x;
    h *= 0x9e3779b9;
    h ^= y;
    h *= 0x9e3779b9;
    h ^= h >> 16;
    return h;
}

// Smooth interpolation
static float smoothstep(float t) {
    return t * t * (3.0f - 2.0f * t);
}

// Linear interpolation
static float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

// Gradient noise function
static float gradientNoise(float x, float y, uint32_t seed) {
    int ix = (int)floorf(x);
    int iy = (int)floorf(y);

    float fx = x - ix;
    float fy = y - iy;

    // Corner gradients
    uint32_t h00 = hash(ix, iy, seed);
    uint32_t h10 = hash(ix + 1, iy, seed);
    uint32_t h01 = hash(ix, iy + 1, seed);
    uint32_t h11 = hash(ix + 1, iy + 1, seed);

    // Convert to gradients (-1 to 1)
    float g00 = (h00 % 256) / 127.5f - 1.0f;
    float g10 = (h10 % 256) / 127.5f - 1.0f;
    float g01 = (h01 % 256) / 127.5f - 1.0f;
    float g11 = (h11 % 256) / 127.5f - 1.0f;

    // Dot products
    float d00 = fx * g00 + fy * g00;
    float d10 = (fx - 1.0f) * g10 + fy * g10;
    float d01 = fx * g01 + (fy - 1.0f) * g01;
    float d11 = (fx - 1.0f) * g11 + (fy - 1.0f) * g11;

    // Bilinear interpolation
    float sx = smoothstep(fx);
    float sy = smoothstep(fy);

    float a = lerp(d00, d10, sx);
    float b = lerp(d01, d11, sx);

    return lerp(a, b, sy);
}

float Procedural_GetValue(float x, float y, const proceduralParams_t* params) {
    if (!params) return 0.0f;

    // Apply offset and scale
    x = (x + params->offset[0]) * params->scale;
    y = (y + params->offset[1]) * params->scale;

    float value = 0.0f;
    float amplitude = params->amplitude;
    float frequency = params->frequency;

    switch (params->type) {
        case PROC_NOISE_PERLIN:
            for (int i = 0; i < params->octaves; ++i) {
                value += gradientNoise(x * frequency, y * frequency, params->seed + i) * amplitude;
                amplitude *= params->persistence;
                frequency *= params->lacunarity;
            }
            // Normalize to 0-1 range
            value = (value + 1.0f) * 0.5f;
            break;

        case PROC_NOISE_SIMPLEX:
            // Simplified simplex noise (using perlin as fallback for now)
            value = Procedural_PerlinNoise(x, y);
            break;

        case PROC_NOISE_VORONOI: {
            float fx, fy;
            value = Procedural_VoronoiNoise(x, y, &fx, &fy);
            break;
        }

        case PROC_CHECKERBOARD: {
            int ix = (int)floorf(x * 10.0f);
            int iy = (int)floorf(y * 10.0f);
            value = ((ix + iy) % 2) ? 1.0f : 0.0f;
            break;
        }

        case PROC_BRICK: {
            float bx = x * 8.0f;
            float by = y * 4.0f;
            int ix = (int)floorf(bx);
            int iy = (int)floorf(by);
            float fx = bx - ix;
            float fy = by - iy;

            // Offset every other row
            if ((iy % 2) == 1) {
                fx += 0.5f;
                if (fx > 1.0f) {
                    fx -= 1.0f;
                    ix++;
                }
            }

            // Create mortar lines
            float mortarWidth = 0.05f;
            qboolean inMortarX = (fx < mortarWidth) || (fx > 1.0f - mortarWidth);
            qboolean inMortarY = (fy < mortarWidth) || (fy > 1.0f - mortarWidth);

            value = (inMortarX || inMortarY) ? 0.3f : 1.0f;
            break;
        }

        case PROC_GRADIENT: {
            // Radial gradient from center
            float dx = x - 0.5f;
            float dy = y - 0.5f;
            float dist = sqrtf(dx * dx + dy * dy) * 2.0f;
            value = Com_Clamp(0.0f, 1.0f, 1.0f - dist);
            break;
        }

        case PROC_WAVE: {
            float wave = sinf(x * params->frequency * 2.0f * M_PI) *
                        cosf(y * params->frequency * 2.0f * M_PI);
            value = (wave + 1.0f) * 0.5f;
            break;
        }

        case PROC_FRACTAL:
            // Fractal noise (multiple octaves of perlin)
            for (int i = 0; i < params->octaves; ++i) {
                value += gradientNoise(x * frequency, y * frequency, params->seed + i) * amplitude;
                amplitude *= params->persistence;
                frequency *= params->lacunarity;
            }
            value = (value + 1.0f) * 0.5f;
            break;

        case PROC_MARBLE: {
            float noise = Procedural_GetValue(x, y, params);
            value = sinf(x * 10.0f + noise * 5.0f) * 0.5f + 0.5f;
            break;
        }

        case PROC_WOOD: {
            float dist = sqrtf(x * x + y * y) * 10.0f;
            float noise = Procedural_GetValue(x, y, params);
            value = sinf(dist + noise * 2.0f) * 0.5f + 0.5f;
            break;
        }

        case PROC_CLOUDS: {
            proceduralParams_t cloudParams = *params;
            cloudParams.type = PROC_FRACTAL;
            cloudParams.octaves = 6;
            cloudParams.frequency = 0.01f;
            cloudParams.persistence = 0.5f;
            cloudParams.lacunarity = 2.0f;
            value = Procedural_GetValue(x, y, &cloudParams);
            break;
        }

        default:
            value = 0.5f; // Default gray
            break;
    }

    return Com_Clamp(0.0f, 1.0f, value);
}

qboolean Procedural_GenerateTexture(int width, int height, int channels,
                                   const proceduralParams_t* params,
                                   float* output) {
    if (!params || !output || width <= 0 || height <= 0 || channels < 1 || channels > 4) {
        return qfalse;
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float u = (float)x / (float)(width - 1);
            float v = (float)y / (float)(height - 1);

            float value = Procedural_GetValue(u, v, params);

            int index = (y * width + x) * channels;
            for (int c = 0; c < channels; ++c) {
                output[index + c] = value;
            }
        }
    }

    return qtrue;
}

//===============================================================================
// Helper Functions
//===============================================================================

static float Procedural_Noise(float x, float y) {
    // Simple value noise
    int ix = (int)floorf(x);
    int iy = (int)floorf(y);

    uint32_t h = hash((uint32_t)ix, (uint32_t)iy, 0);
    return (h % 256) / 255.0f;
}

static float Procedural_PerlinNoise(float x, float y) {
    return gradientNoise(x, y, 0);
}

static float Procedural_SimplexNoise(float x, float y) {
    // Simplified simplex noise implementation
    // This is a basic approximation - a full implementation would be more complex
    return Procedural_PerlinNoise(x, y);
}

static float Procedural_VoronoiNoise(float x, float y, float* featurePointX, float* featurePointY) {
    int ix = (int)floorf(x);
    int iy = (int)floorf(y);

    float minDist = FLT_MAX;
    float closestX = 0.0f, closestY = 0.0f;

    // Check neighboring cells
    for (int oy = -1; oy <= 1; ++oy) {
        for (int ox = -1; ox <= 1; ++ox) {
            int cx = ix + ox;
            int cy = iy + oy;

            // Get feature point for this cell
            uint32_t h = hash((uint32_t)cx, (uint32_t)cy, 0);
            float fx = cx + (h % 256) / 255.0f;
            float fy = cy + ((h >> 8) % 256) / 255.0f;

            float dx = fx - x;
            float dy = fy - y;
            float dist = dx * dx + dy * dy;

            if (dist < minDist) {
                minDist = dist;
                closestX = fx;
                closestY = fy;
            }
        }
    }

    if (featurePointX) *featurePointX = closestX;
    if (featurePointY) *featurePointY = closestY;

    return sqrtf(minDist);
}

//===============================================================================
// Material Compilation (Stub)
//===============================================================================

qboolean Material_Compile(const layeredMaterial_t* material, char* shaderName) {
    if (!material || !shaderName) return qfalse;

    // For now, just copy the material name
    // A full implementation would generate shader code
    Q_strncpyz(shaderName, material->name, MAX_QPATH);

    return qtrue;
}

//===============================================================================
// Material Instance System
//===============================================================================

materialInstance_t* MaterialInstance_Create(const layeredMaterial_t* baseMaterial) {
    if (!baseMaterial) return NULL;

    materialInstance_t* instance = (materialInstance_t*)malloc(sizeof(materialInstance_t));
    if (!instance) return NULL;

    memset(instance, 0, sizeof(materialInstance_t));
    instance->baseMaterial = baseMaterial;
    instance->compiled = qfalse;
    instance->shaderIndex = -1;

    return instance;
}

void MaterialInstance_Free(materialInstance_t* instance) {
    if (instance) {
        free(instance);
    }
}

qboolean MaterialInstance_SetParameter(materialInstance_t* instance,
                                     const char* paramName,
                                     const materialParameter_t* value) {
    if (!instance || !paramName || !value) return qfalse;

    // Find existing parameter or add new one
    for (int i = 0; i < instance->numOverrides; ++i) {
        if (strcmp(instance->overrideParams[i].name, paramName) == 0) {
            instance->overrideParams[i] = *value;
            instance->compiled = qfalse; // Mark as needing recompilation
            return qtrue;
        }
    }

    if (instance->numOverrides >= MAX_MATERIAL_PARAMETERS) {
        return qfalse;
    }

    instance->overrideParams[instance->numOverrides++] = *value;
    instance->compiled = qfalse;
    return qtrue;
}

qboolean MaterialInstance_GetParameter(const materialInstance_t* instance,
                                     const char* paramName,
                                     materialParameter_t* value) {
    if (!instance || !paramName || !value) return qfalse;

    // Check overrides first
    for (int i = 0; i < instance->numOverrides; ++i) {
        if (strcmp(instance->overrideParams[i].name, paramName) == 0) {
            *value = instance->overrideParams[i];
            return qtrue;
        }
    }

    // Check base material global parameters
    for (int i = 0; i < instance->baseMaterial->numGlobalParams; ++i) {
        if (strcmp(instance->baseMaterial->globalParams[i].name, paramName) == 0) {
            *value = instance->baseMaterial->globalParams[i];
            return qtrue;
        }
    }

    // Check layer parameters
    for (int layer = 0; layer < instance->baseMaterial->numLayers; ++layer) {
        const materialLayer_t* layerPtr = &instance->baseMaterial->layers[layer];
        for (int i = 0; i < layerPtr->numParameters; ++i) {
            if (strcmp(layerPtr->parameters[i].name, paramName) == 0) {
                *value = layerPtr->parameters[i];
                return qtrue;
            }
        }
    }

    return qfalse;
}

int MaterialInstance_Compile(materialInstance_t* instance) {
    if (!instance) return -1;

    if (instance->compiled) {
        return instance->shaderIndex;
    }

    // For now, just use the base material name
    // A full implementation would create a unique shader
    char shaderName[MAX_QPATH];
    if (!Material_Compile(instance->baseMaterial, shaderName)) {
        return -1;
    }

    // TODO: Register with renderer and get shader index
    instance->shaderIndex = 0; // Placeholder
    instance->compiled = qtrue;

    return instance->shaderIndex;
}

//===============================================================================
// Material Loading/Saving
//===============================================================================

layeredMaterial_t* Material_Load(const char* filename) {
    if (!filename) return NULL;

    // Open the material file
    fileHandle_t file = FS_FOpenFileRead(filename, NULL, qfalse);
    if (!file) {
        Com_DPrintf("Material_Load: Could not open file '%s'\n", filename);
        return NULL;
    }

    // Read the entire file
    char* buffer = NULL;
    int fileLen = FS_ReadFile(filename, (void**)&buffer);
    if (fileLen <= 0 || !buffer) {
        FS_FCloseFile(file);
        Com_DPrintf("Material_Load: Could not read file '%s'\n", filename);
        return NULL;
    }

    FS_FCloseFile(file);

    // Parse the material file (simple key-value format)
    layeredMaterial_t* material = NULL;
    char* line = buffer;
    char* end = buffer + fileLen;
    int currentLayer = -1;

    while (line < end) {
        // Skip whitespace
        while (line < end && (*line == ' ' || *line == '\t' || *line == '\r' || *line == '\n')) line++;
        if (line >= end) break;

        // Skip comments
        if (*line == '#' || *line == '/' || *line == ';') {
            while (line < end && *line != '\n') line++;
            continue;
        }

        // Parse key-value pairs
        char* key = line;
        char* value = NULL;

        // Find the equals sign
        while (line < end && *line != '=' && *line != '\n') line++;
        if (line >= end || *line != '=') {
            line++;
            continue;
        }

        *line = '\0'; // Null terminate key
        value = line + 1;

        // Find end of value
        line = value;
        while (line < end && *line != '\n') line++;
        if (line < end) *line++ = '\0'; // Null terminate value

        // Trim whitespace from key and value
        while (*key == ' ' || *key == '\t') key++;
        char* keyEnd = key + strlen(key) - 1;
        while (keyEnd > key && (*keyEnd == ' ' || *keyEnd == '\t')) *keyEnd-- = '\0';

        while (*value == ' ' || *value == '\t') value++;
        char* valueEnd = value + strlen(value) - 1;
        while (valueEnd > value && (*valueEnd == ' ' || *valueEnd == '\t')) *valueEnd-- = '\0';

        // Parse the key-value pair
        if (strcmp(key, "material") == 0) {
            // Create new material
            material = Material_Create(value);
            if (!material) {
                Com_Printf("Material_Load: Failed to create material '%s'\n", value);
                FS_FreeFile(buffer);
                return NULL;
            }
        } else if (strcmp(key, "version") == 0) {
            if (material) {
                material->version = atoi(value);
            }
        } else if (strcmp(key, "doublesided") == 0) {
            if (material) {
                material->doubleSided = (atoi(value) != 0);
            }
        } else if (strcmp(key, "translucent") == 0) {
            if (material) {
                material->translucent = (atoi(value) != 0);
            }
        } else if (strcmp(key, "cullmode") == 0) {
            if (material) {
                if (strcmp(value, "front") == 0) material->cullMode = CT_FRONT_SIDED;
                else if (strcmp(value, "back") == 0) material->cullMode = CT_BACK_SIDED;
                else if (strcmp(value, "none") == 0) material->cullMode = CT_TWO_SIDED;
            }
        } else if (strcmp(key, "depthwrite") == 0) {
            if (material) {
                material->depthWrite = (atoi(value) != 0);
            }
        } else if (strcmp(key, "depthtest") == 0) {
            if (material) {
                material->depthTest = (atoi(value) != 0);
            }
        } else if (strcmp(key, "pbr") == 0) {
            if (material) {
                material->usePBR = (atoi(value) != 0);
            }
        } else if (strcmp(key, "ibl") == 0) {
            if (material) {
                material->useIBL = (atoi(value) != 0);
            }
        } else if (strcmp(key, "layer") == 0) {
            if (material) {
                currentLayer = Material_AddLayer(material, value);
            }
        } else if (currentLayer >= 0 && material) {
            materialLayer_t* layer = &material->layers[currentLayer];

            if (strcmp(key, "enabled") == 0) {
                layer->enabled = (atoi(value) != 0);
            } else if (strcmp(key, "blendmode") == 0) {
                if (strcmp(value, "opaque") == 0) layer->blendMode = BLEND_OPAQUE;
                else if (strcmp(value, "alpha") == 0) layer->blendMode = BLEND_ALPHA;
                else if (strcmp(value, "additive") == 0) layer->blendMode = BLEND_ADDITIVE;
                else if (strcmp(value, "multiply") == 0) layer->blendMode = BLEND_MULTIPLY;
                else if (strcmp(value, "overlay") == 0) layer->blendMode = BLEND_OVERLAY;
            } else if (strcmp(key, "opacity") == 0) {
                layer->opacity = atof(value);
            } else if (strcmp(key, "uvoffset") == 0) {
                sscanf(value, "%f %f", &layer->uvOffset[0], &layer->uvOffset[1]);
            } else if (strcmp(key, "uvscale") == 0) {
                sscanf(value, "%f %f", &layer->uvScale[0], &layer->uvScale[1]);
            } else if (strcmp(key, "uvrotation") == 0) {
                layer->uvRotation = atof(value);
            } else if (strcmp(key, "basecolor") == 0) {
                sscanf(value, "%f %f %f", &layer->baseColor[0], &layer->baseColor[1], &layer->baseColor[2]);
            } else if (strcmp(key, "metallic") == 0) {
                layer->metallic = atof(value);
            } else if (strcmp(key, "roughness") == 0) {
                layer->roughness = atof(value);
            } else if (strcmp(key, "emissive") == 0) {
                layer->emissiveIntensity = atof(value);
            } else if (strcmp(key, "normalstrength") == 0) {
                layer->normalStrength = atof(value);
            } else if (strcmp(key, "diffusemap") == 0) {
                Q_strncpyz(layer->diffuseMap, value, sizeof(layer->diffuseMap));
            } else if (strcmp(key, "normalmap") == 0) {
                Q_strncpyz(layer->normalMap, value, sizeof(layer->normalMap));
            } else if (strcmp(key, "specularmap") == 0) {
                Q_strncpyz(layer->specularMap, value, sizeof(layer->specularMap));
            } else if (strcmp(key, "metallicmap") == 0) {
                Q_strncpyz(layer->metallicMap, value, sizeof(layer->metallicMap));
            } else if (strcmp(key, "roughnessmap") == 0) {
                Q_strncpyz(layer->roughnessMap, value, sizeof(layer->roughnessMap));
            } else if (strcmp(key, "emissivemap") == 0) {
                Q_strncpyz(layer->emissiveMap, value, sizeof(layer->emissiveMap));
            }
        }
    }

    FS_FreeFile(buffer);

    if (material) {
        Com_DPrintf("Material_Load: Successfully loaded material '%s' with %d layers\n",
                   filename, material->numLayers);
    }

    return material;
}

qboolean Material_Save(const layeredMaterial_t* material, const char* filename) {
    if (!material || !filename) return qfalse;

    // Open file for writing
    fileHandle_t file = FS_FOpenFileWrite(filename);
    if (!file) {
        Com_Printf("Material_Save: Could not open file '%s' for writing\n", filename);
        return qfalse;
    }

    // Write material header
    FS_Write("material = ", 11, file);
    FS_Write(material->name, strlen(material->name), file);
    FS_Write("\n", 1, file);

    FS_Write("version = ", 10, file);
    char versionStr[32];
    sprintf(versionStr, "%d", material->version);
    FS_Write(versionStr, strlen(versionStr), file);
    FS_Write("\n", 1, file);

    // Write global properties
    FS_Write("doublesided = ", 14, file);
    FS_Write(material->doubleSided ? "1" : "0", 1, file);
    FS_Write("\n", 1, file);

    FS_Write("translucent = ", 14, file);
    FS_Write(material->translucent ? "1" : "0", 1, file);
    FS_Write("\n", 1, file);

    FS_Write("cullmode = ", 11, file);
    const char* cullModeStr = "front";
    if (material->cullMode == CT_BACK_SIDED) cullModeStr = "back";
    else if (material->cullMode == CT_TWO_SIDED) cullModeStr = "none";
    FS_Write(cullModeStr, strlen(cullModeStr), file);
    FS_Write("\n", 1, file);

    FS_Write("depthwrite = ", 13, file);
    FS_Write(material->depthWrite ? "1" : "0", 1, file);
    FS_Write("\n", 1, file);

    FS_Write("depthtest = ", 12, file);
    FS_Write(material->depthTest ? "1" : "0", 1, file);
    FS_Write("\n", 1, file);

    FS_Write("pbr = ", 6, file);
    FS_Write(material->usePBR ? "1" : "0", 1, file);
    FS_Write("\n", 1, file);

    FS_Write("ibl = ", 6, file);
    FS_Write(material->useIBL ? "1" : "0", 1, file);
    FS_Write("\n\n", 2, file);

    // Write layers
    for (int i = 0; i < material->numLayers; ++i) {
        const materialLayer_t* layer = &material->layers[i];

        FS_Write("layer = ", 8, file);
        FS_Write(layer->name, strlen(layer->name), file);
        FS_Write("\n", 1, file);

        FS_Write("enabled = ", 10, file);
        FS_Write(layer->enabled ? "1" : "0", 1, file);
        FS_Write("\n", 1, file);

        FS_Write("blendmode = ", 12, file);
        const char* blendModeStr = "opaque";
        switch (layer->blendMode) {
            case BLEND_ALPHA: blendModeStr = "alpha"; break;
            case BLEND_ADDITIVE: blendModeStr = "additive"; break;
            case BLEND_MULTIPLY: blendModeStr = "multiply"; break;
            case BLEND_OVERLAY: blendModeStr = "overlay"; break;
            default: break;
        }
        FS_Write(blendModeStr, strlen(blendModeStr), file);
        FS_Write("\n", 1, file);

        char floatStr[64];
        sprintf(floatStr, "opacity = %f\n", layer->opacity);
        FS_Write(floatStr, strlen(floatStr), file);

        sprintf(floatStr, "uvoffset = %f %f\n", layer->uvOffset[0], layer->uvOffset[1]);
        FS_Write(floatStr, strlen(floatStr), file);

        sprintf(floatStr, "uvscale = %f %f\n", layer->uvScale[0], layer->uvScale[1]);
        FS_Write(floatStr, strlen(floatStr), file);

        sprintf(floatStr, "uvrotation = %f\n", layer->uvRotation);
        FS_Write(floatStr, strlen(floatStr), file);

        sprintf(floatStr, "basecolor = %f %f %f\n",
                layer->baseColor[0], layer->baseColor[1], layer->baseColor[2]);
        FS_Write(floatStr, strlen(floatStr), file);

        sprintf(floatStr, "metallic = %f\n", layer->metallic);
        FS_Write(floatStr, strlen(floatStr), file);

        sprintf(floatStr, "roughness = %f\n", layer->roughness);
        FS_Write(floatStr, strlen(floatStr), file);

        sprintf(floatStr, "emissive = %f\n", layer->emissiveIntensity);
        FS_Write(floatStr, strlen(floatStr), file);

        sprintf(floatStr, "normalstrength = %f\n", layer->normalStrength);
        FS_Write(floatStr, strlen(floatStr), file);

        // Write texture maps
        if (layer->diffuseMap[0]) {
            FS_Write("diffusemap = ", 13, file);
            FS_Write(layer->diffuseMap, strlen(layer->diffuseMap), file);
            FS_Write("\n", 1, file);
        }

        if (layer->normalMap[0]) {
            FS_Write("normalmap = ", 12, file);
            FS_Write(layer->normalMap, strlen(layer->normalMap), file);
            FS_Write("\n", 1, file);
        }

        if (layer->specularMap[0]) {
            FS_Write("specularmap = ", 14, file);
            FS_Write(layer->specularMap, strlen(layer->specularMap), file);
            FS_Write("\n", 1, file);
        }

        if (layer->metallicMap[0]) {
            FS_Write("metallicmap = ", 14, file);
            FS_Write(layer->metallicMap, strlen(layer->metallicMap), file);
            FS_Write("\n", 1, file);
        }

        if (layer->roughnessMap[0]) {
            FS_Write("roughnessmap = ", 15, file);
            FS_Write(layer->roughnessMap, strlen(layer->roughnessMap), file);
            FS_Write("\n", 1, file);
        }

        if (layer->emissiveMap[0]) {
            FS_Write("emissivemap = ", 14, file);
            FS_Write(layer->emissiveMap, strlen(layer->emissiveMap), file);
            FS_Write("\n", 1, file);
        }

        FS_Write("\n", 1, file);
    }

    FS_FCloseFile(file);

    Com_DPrintf("Material_Save: Successfully saved material '%s' with %d layers\n",
               filename, material->numLayers);
    return qtrue;
}

//===============================================================================
// Utility Functions
//===============================================================================

qboolean Material_Validate(const layeredMaterial_t* material) {
    if (!material) return qfalse;

    if (material->numLayers < 0 || material->numLayers > MAX_MATERIAL_LAYERS) {
        return qfalse;
    }

    // Validate each layer
    for (int i = 0; i < material->numLayers; ++i) {
        const materialLayer_t* layer = &material->layers[i];
        if (layer->numParameters < 0 || layer->numParameters > MAX_MATERIAL_PARAMETERS) {
            return qfalse;
        }
    }

    return qtrue;
}

size_t Material_GetMemoryUsage(const layeredMaterial_t* material) {
    if (!material) return 0;

    size_t usage = sizeof(layeredMaterial_t);

    // Add layer parameter memory
    for (int i = 0; i < material->numLayers; ++i) {
        usage += material->layers[i].numParameters * sizeof(materialParameter_t);
    }

    return usage;
}

qboolean Material_Optimize(layeredMaterial_t* material) {
    if (!material) return qfalse;

    // Remove disabled layers
    for (int i = material->numLayers - 1; i >= 0; --i) {
        if (!material->layers[i].enabled) {
            Material_RemoveLayer(material, i);
        }
    }

    // Sort layers by blend mode for better rendering
    // Simple bubble sort by blend complexity
    for (int i = 0; i < material->numLayers - 1; ++i) {
        for (int j = 0; j < material->numLayers - i - 1; ++j) {
            int complexity1 = material->layers[j].blendMode;
            int complexity2 = material->layers[j + 1].blendMode;
            if (complexity1 > complexity2) {
                materialLayer_t temp = material->layers[j];
                material->layers[j] = material->layers[j + 1];
                material->layers[j + 1] = temp;
            }
        }
    }

    return qtrue;
}