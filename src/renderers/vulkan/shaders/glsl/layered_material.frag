#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

//===============================================================================
// Layered Material Fragment Shader
//
// Modern PBR shader with support for layered materials and procedural textures.
//===============================================================================

//===============================================================================
// Material Layer Data Structures
//===============================================================================

#define MAX_MATERIAL_LAYERS 16
#define MAX_TEXTURES 128

struct MaterialLayer {
    // Base properties
    vec3 baseColor;
    float opacity;

    // PBR properties
    float metallic;
    float roughness;
    float emissiveIntensity;
    float normalStrength;

    // Texture indices (-1 = no texture, -2 = procedural)
    int diffuseTexture;
    int normalTexture;
    int metallicTexture;
    int roughnessTexture;
    int emissiveTexture;
    int occlusionTexture;

    // UV transformation
    vec2 uvOffset;
    vec2 uvScale;
    float uvRotation;

    // Animation
    vec2 scrollSpeed;
    float rotationSpeed;

    // Blend mode
    int blendMode;
};

// Blend modes
#define BLEND_OPAQUE         0
#define BLEND_ALPHA          1
#define BLEND_ADDITIVE       2
#define BLEND_MULTIPLY       3
#define BLEND_SCREEN         4
#define BLEND_OVERLAY        5
#define BLEND_SOFT_LIGHT     6
#define BLEND_HARD_LIGHT     7
#define BLEND_DIFFERENCE     8
#define BLEND_EXCLUSION      9
#define BLEND_COLOR_DODGE    10
#define BLEND_COLOR_BURN     11
#define BLEND_NORMAL_MAP     12
#define BLEND_HEIGHT_MAP     13
#define BLEND_MASK           14

//===============================================================================
// Shader Inputs
//===============================================================================

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;
layout(location = 5) in flat uint inMaterialID;

//===============================================================================
// Shader Outputs
//===============================================================================

layout(location = 0) out vec4 outColor;

//===============================================================================
// Uniform Buffers
//===============================================================================

// Material data
layout(set = 0, binding = 0, scalar) readonly buffer MaterialLayers {
    MaterialLayer layers[];
} materialLayers;

// Global material info
layout(set = 0, binding = 1, scalar) readonly buffer MaterialInfo {
    uint numLayers;
    uint materialFlags;
    float globalOpacity;
    uint padding;
} materialInfo;

//===============================================================================
// Textures
//===============================================================================

layout(set = 1, binding = 0) uniform sampler2D textures[MAX_TEXTURES];

//===============================================================================
// Push Constants
//===============================================================================

layout(push_constant, scalar) uniform PushConstants {
    mat4 modelMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
    mat4 normalMatrix;

    vec3 cameraPosition;
    float time;

    vec3 lightDirection;
    float lightIntensity;

    vec3 ambientColor;
    float exposure;
} pushConstants;

//===============================================================================
// Procedural Texture Functions
//===============================================================================

// Simple hash function
uint hash(uint x, uint y, uint seed) {
    uint h = seed;
    h ^= x;
    h *= 0x9e3779b9u;
    h ^= y;
    h *= 0x9e3779b9u;
    h ^= h >> 16;
    return h;
}

// Smooth interpolation
float smoothstep(float t) {
    return t * t * (3.0 - 2.0 * t);
}

// Linear interpolation
float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

// Gradient noise
float gradientNoise(vec2 pos, uint seed) {
    ivec2 ipos = ivec2(floor(pos));
    vec2 fpos = pos - vec2(ipos);

    // Corner gradients
    uint h00 = hash(uint(ipos.x), uint(ipos.y), seed);
    uint h10 = hash(uint(ipos.x + 1), uint(ipos.y), seed);
    uint h01 = hash(uint(ipos.x), uint(ipos.y + 1), seed);
    uint h11 = hash(uint(ipos.x + 1), uint(ipos.y + 1), seed);

    // Convert to gradients (-1 to 1)
    float g00 = (h00 % 256) / 127.5 - 1.0;
    float g10 = (h10 % 256) / 127.5 - 1.0;
    float g01 = (h01 % 256) / 127.5 - 1.0;
    float g11 = (h11 % 256) / 127.5 - 1.0;

    // Dot products
    float d00 = fpos.x * g00 + fpos.y * g00;
    float d10 = (fpos.x - 1.0) * g10 + fpos.y * g10;
    float d01 = fpos.x * g01 + (fpos.y - 1.0) * g01;
    float d11 = (fpos.x - 1.0) * g11 + (fpos.y - 1.0) * g11;

    // Bilinear interpolation
    float sx = smoothstep(fpos.x);
    float sy = smoothstep(fpos.y);

    float a = lerp(d00, d10, sx);
    float b = lerp(d01, d11, sy);

    return lerp(a, b, sy);
}

// Procedural noise functions
float proceduralNoise(vec2 uv, int type, float frequency, float amplitude, int octaves, uint seed) {
    vec2 pos = uv * frequency;

    switch (type) {
        case 0: // Perlin noise
        {
            float value = 0.0;
            float amp = amplitude;
            float freq = 1.0;

            for (int i = 0; i < octaves; ++i) {
                value += gradientNoise(pos * freq, seed + uint(i)) * amp;
                amp *= 0.5;
                freq *= 2.0;
            }

            return (value + 1.0) * 0.5; // Normalize to 0-1
        }

        case 1: // Checkerboard
        {
            ivec2 ipos = ivec2(floor(uv * 8.0));
            return ((ipos.x + ipos.y) % 2) == 0 ? 1.0 : 0.0;
        }

        case 2: // Gradient (radial)
        {
            vec2 center = vec2(0.5);
            float dist = length(uv - center) * 2.0;
            return clamp(1.0 - dist, 0.0, 1.0);
        }

        case 3: // Wave
        {
            float wave = sin(uv.x * 10.0) * cos(uv.y * 10.0);
            return (wave + 1.0) * 0.5;
        }

        default:
            return 0.5;
    }
}

//===============================================================================
// UV Transformation Functions
//===============================================================================

vec2 transformUV(vec2 uv, vec2 offset, vec2 scale, float rotation, vec2 scrollSpeed, float rotationSpeed, float time) {
    // Apply scroll animation
    vec2 animatedUV = uv + scrollSpeed * time;

    // Apply scale
    animatedUV *= scale;

    // Apply rotation
    if (rotation != 0.0 || rotationSpeed != 0.0) {
        float rot = rotation + rotationSpeed * time;
        float cosRot = cos(rot);
        float sinRot = sin(rot);
        mat2 rotMatrix = mat2(cosRot, -sinRot, sinRot, cosRot);
        animatedUV = rotMatrix * (animatedUV - 0.5) + 0.5;
    }

    // Apply offset
    animatedUV += offset;

    return animatedUV;
}

//===============================================================================
// Texture Sampling Functions
//===============================================================================

vec4 sampleTexture(int textureIndex, vec2 uv) {
    if (textureIndex < 0) {
        return vec4(1.0); // No texture
    }

    return texture(textures[textureIndex], uv);
}

vec3 sampleProcedural(int proceduralType, vec2 uv, float frequency, float amplitude, int octaves, uint seed) {
    float value = proceduralNoise(uv, proceduralType, frequency, amplitude, octaves, seed);
    return vec3(value);
}

//===============================================================================
// Material Layer Blending Functions
//===============================================================================

vec3 blendColors(vec3 base, vec3 blend, int mode) {
    switch (mode) {
        case BLEND_OPAQUE:
            return blend;

        case BLEND_ALPHA:
            return blend; // Alpha handled separately

        case BLEND_ADDITIVE:
            return base + blend;

        case BLEND_MULTIPLY:
            return base * blend;

        case BLEND_SCREEN:
            return 1.0 - (1.0 - base) * (1.0 - blend);

        case BLEND_OVERLAY:
            return mix(
                2.0 * base * blend,
                1.0 - 2.0 * (1.0 - base) * (1.0 - blend),
                step(0.5, base)
            );

        case BLEND_SOFT_LIGHT:
            return mix(
                2.0 * base * blend + base * base * (1.0 - 2.0 * blend),
                sqrt(base) * (2.0 * blend - 1.0) + 2.0 * base * (1.0 - blend),
                step(0.5, blend)
            );

        case BLEND_HARD_LIGHT:
            return mix(
                2.0 * base * blend,
                1.0 - 2.0 * (1.0 - base) * (1.0 - blend),
                step(0.5, blend)
            );

        case BLEND_DIFFERENCE:
            return abs(base - blend);

        case BLEND_EXCLUSION:
            return base + blend - 2.0 * base * blend;

        case BLEND_COLOR_DODGE:
            return mix(
                min(base / (1.0 - blend), 1.0),
                vec3(1.0),
                step(1.0 - 1e-6, blend)
            );

        case BLEND_COLOR_BURN:
            return mix(
                1.0 - min((1.0 - base) / blend, 1.0),
                vec3(0.0),
                step(1e-6, blend)
            );

        default:
            return blend;
    }
}

vec3 blendNormals(vec3 base, vec3 blend, float strength) {
    // Convert to tangent space normal maps (-1 to 1)
    vec3 n1 = base * 2.0 - 1.0;
    vec3 n2 = blend * 2.0 - 1.0;

    // Blend in tangent space
    vec3 result = normalize(mix(n1, n2, strength));

    // Convert back to texture space (0 to 1)
    return result * 0.5 + 0.5;
}

//===============================================================================
// Main Fragment Shader
//===============================================================================

void main() {
    // Start with default material properties
    vec3 finalColor = vec3(0.5);
    vec3 finalNormal = normalize(inNormal);
    float finalMetallic = 0.0;
    float finalRoughness = 0.5;
    vec3 finalEmissive = vec3(0.0);
    float finalOcclusion = 1.0;

    uint layerStart = inMaterialID * MAX_MATERIAL_LAYERS;
    uint layerCount = min(materialInfo.numLayers, MAX_MATERIAL_LAYERS);

    // Process each material layer
    for (uint layerIndex = 0; layerIndex < layerCount; ++layerIndex) {
        MaterialLayer layer = materialLayers.layers[layerStart + layerIndex];

        // Transform UV coordinates
        vec2 uv = transformUV(inTexCoord, layer.uvOffset, layer.uvScale,
                            layer.uvRotation, layer.scrollSpeed,
                            layer.rotationSpeed, pushConstants.time);

        // Sample diffuse/albedo
        vec3 layerColor = layer.baseColor;
        if (layer.diffuseTexture >= 0) {
            vec4 texColor = sampleTexture(layer.diffuseTexture, uv);
            layerColor *= texColor.rgb;
        } else if (layer.diffuseTexture == -2) {
            // Procedural texture
            layerColor *= sampleProcedural(0, uv, 4.0, 1.0, 4, 0);
        }

        // Sample normal map
        vec3 layerNormal = finalNormal;
        if (layer.normalTexture >= 0) {
            vec4 normalTex = sampleTexture(layer.normalTexture, uv);
            vec3 tangentNormal = normalTex.rgb * 2.0 - 1.0;

            // Transform to world space
            vec3 N = normalize(inNormal);
            vec3 T = normalize(inTangent);
            vec3 B = normalize(inBitangent);
            mat3 TBN = mat3(T, B, N);

            layerNormal = normalize(TBN * tangentNormal);
        }

        // Sample PBR maps
        float layerMetallic = layer.metallic;
        if (layer.metallicTexture >= 0) {
            layerMetallic *= sampleTexture(layer.metallicTexture, uv).r;
        }

        float layerRoughness = layer.roughness;
        if (layer.roughnessTexture >= 0) {
            layerRoughness *= sampleTexture(layer.roughnessTexture, uv).r;
        }

        vec3 layerEmissive = vec3(0.0);
        if (layer.emissiveTexture >= 0) {
            layerEmissive = sampleTexture(layer.emissiveTexture, uv).rgb * layer.emissiveIntensity;
        }

        float layerOcclusion = 1.0;
        if (layer.occlusionTexture >= 0) {
            layerOcclusion = sampleTexture(layer.occlusionTexture, uv).r;
        }

        // Blend with previous layers
        switch (layer.blendMode) {
            case BLEND_NORMAL_MAP:
                finalNormal = blendNormals(finalNormal, layerNormal, layer.opacity);
                break;

            case BLEND_MASK:
                // Use as alpha mask
                finalColor = mix(finalColor, layerColor, layer.opacity * layerColor.r);
                break;

            default:
                // Standard color blending
                finalColor = blendColors(finalColor, layerColor, layer.blendMode);
                finalMetallic = mix(finalMetallic, layerMetallic, layer.opacity);
                finalRoughness = mix(finalRoughness, layerRoughness, layer.opacity);
                finalEmissive = mix(finalEmissive, layerEmissive, layer.opacity);
                finalOcclusion = mix(finalOcclusion, layerOcclusion, layer.opacity);
                break;
        }
    }

    // Apply global opacity
    finalColor *= materialInfo.globalOpacity;

    // Simple lighting (placeholder - would use proper PBR in full implementation)
    vec3 lightDir = normalize(pushConstants.lightDirection);
    float NdotL = max(dot(finalNormal, lightDir), 0.0);

    vec3 ambient = finalColor * pushConstants.ambientColor * finalOcclusion;
    vec3 diffuse = finalColor * NdotL * pushConstants.lightIntensity;

    vec3 litColor = ambient + diffuse + finalEmissive;

    // Tone mapping (simple Reinhard)
    litColor = litColor / (litColor + vec3(1.0));

    // Gamma correction
    litColor = pow(litColor, vec3(1.0 / 2.2));

    outColor = vec4(litColor, 1.0);
}