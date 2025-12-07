// Material Parameter System
// Runtime material properties that can be modified by gameplay/scripts

#ifndef MATERIAL_PARAMS_GLSL
#define MATERIAL_PARAMS_GLSL

// Material parameter structure (matches CPU layout)
struct MaterialParams {
    // Dynamic state
    float wetness;
    float damage;
    float corruption;
    float magicGlow;
    float temperature;
    float time;

    vec3 magicColor;
    float _pad0;
    vec3 damageColor;
    float _pad1;

    // Layered/PBR baseline
    vec3 baseColor;
    float roughness;
    vec3 emissive;
    float metallic;
    float normalScale;
    float clearcoat;
    float clearcoatRoughness;
    float anisotropy;          // -1..1
    vec3 anisotropyDir;        // tangent-space direction
    float sheen;               // 0..1
    vec3 sheenColor;           // sheen tint
    float subsurface;          // 0..1
    vec3 subsurfaceColor;      // SSS color
    float microfacet;          // 0..1 tighten highlights
    float microfacetSharpness; // >0, 1=neutral
    float layerWeight;

    // Metadata
    uint flags;
    uint stateHash;
    uint layerCount;
    uint debugFlags;
    float layerCost;
    vec3 _pad2;
};

// Material flags
#define MATERIAL_WET       0x01
#define MATERIAL_DAMAGED   0x02
#define MATERIAL_MAGICAL   0x04
#define MATERIAL_CORRUPTED 0x08
#define MATERIAL_EMISSIVE  0x10
#define MATERIAL_DYNAMIC   0x20
#define MATERIAL_HAS_LAYERS 0x40

// Material parameter buffer (set 9, binding 0)
layout(set = 9, binding = 0, std430) restrict readonly buffer MaterialParamsBuffer {
    MaterialParams materials[];
};

// Apply material parameters to PBR properties
vec3 applyMaterialParams(uint materialIndex, vec3 baseColor, float roughness, float metallic, out vec3 emissive)
{
    MaterialParams params = materials[materialIndex];
    emissive = params.emissive;
    
    // Layered base override when provided
    if ((params.flags & MATERIAL_HAS_LAYERS) != 0) {
        baseColor = params.baseColor;
        roughness = params.roughness;
        metallic = params.metallic;
    }
    
    // Apply wetness (reduces roughness)
    float finalRoughness = roughness;
    if ((params.flags & MATERIAL_WET) != 0) {
        finalRoughness = mix(roughness, 0.1, params.wetness);
        // Add specular reflection for wet surfaces
        baseColor *= (1.0 - params.wetness * 0.1);
    }
    
    // Apply damage (adds roughness and darkens)
    if ((params.flags & MATERIAL_DAMAGED) != 0) {
        finalRoughness = mix(finalRoughness, 1.0, params.damage * 0.5);
        baseColor = mix(baseColor, params.damageColor, params.damage * 0.3);
        baseColor *= (1.0 - params.damage * 0.2);
    }
    
    // Apply corruption (tints color, adds emissive)
    if ((params.flags & MATERIAL_CORRUPTED) != 0) {
        baseColor = mix(baseColor, vec3(0.2, 0.8, 0.2), params.corruption * 0.2);
        emissive += vec3(0.1, 0.3, 0.1) * params.corruption;
    }
    
    // Apply magic glow (adds emissive)
    if ((params.flags & MATERIAL_MAGICAL) != 0) {
        emissive += params.magicColor * params.magicGlow;
    }
    
    // Time-based effects (pulsing, etc.)
    float timePulse = sin(params.time * 2.0) * 0.5 + 0.5;
    if ((params.flags & MATERIAL_MAGICAL) != 0) {
        emissive *= (0.8 + timePulse * 0.2);
    }
    
    return baseColor;
}

// Get modified roughness with material parameters
float getMaterialRoughness(uint materialIndex, float baseRoughness)
{
    MaterialParams params = materials[materialIndex];
    float roughness = ((params.flags & MATERIAL_HAS_LAYERS) != 0) ? params.roughness : baseRoughness;
    
    // Wetness reduces roughness
    if ((params.flags & MATERIAL_WET) != 0) {
        roughness = mix(roughness, 0.1, params.wetness);
    }
    
    // Damage increases roughness
    if ((params.flags & MATERIAL_DAMAGED) != 0) {
        roughness = mix(roughness, 1.0, params.damage * 0.5);
    }
    
    return clamp(roughness, 0.04, 1.0);
}

// Get modified metallic with material parameters
float getMaterialMetallic(uint materialIndex, float baseMetallic)
{
    MaterialParams params = materials[materialIndex];
    float metallic = ((params.flags & MATERIAL_HAS_LAYERS) != 0) ? params.metallic : baseMetallic;
    
    // Corruption can make materials more metallic-looking
    if ((params.flags & MATERIAL_CORRUPTED) != 0) {
        metallic = mix(metallic, 0.8, params.corruption * 0.3);
    }
    
    return clamp(metallic, 0.0, 1.0);
}

// Advanced: anisotropy (returns -1..1) and direction (tangent-space)
float getMaterialAnisotropy(uint materialIndex)
{
    MaterialParams params = materials[materialIndex];
    return clamp(params.anisotropy, -1.0, 1.0);
}

vec3 getMaterialAnisotropyDir(uint materialIndex)
{
    MaterialParams params = materials[materialIndex];
    return normalize(params.anisotropyDir);
}

// Advanced: sheen
vec3 getMaterialSheen(uint materialIndex)
{
    MaterialParams params = materials[materialIndex];
    return clamp(params.sheen, 0.0, 1.0) * params.sheenColor;
}

// Advanced: subsurface scattering strength/color
vec3 getMaterialSubsurface(uint materialIndex)
{
    MaterialParams params = materials[materialIndex];
    return clamp(params.subsurface, 0.0, 1.0) * params.subsurfaceColor;
}

// Advanced: microfaceting controls — tighten or soften spec lobes
float getMaterialMicrofacet(uint materialIndex)
{
    MaterialParams params = materials[materialIndex];
    return clamp(params.microfacet, 0.0, 1.0);
}

float getMaterialMicrofacetSharpness(uint materialIndex)
{
    MaterialParams params = materials[materialIndex];
    return max(params.microfacetSharpness, 0.1);
}

#endif // MATERIAL_PARAMS_GLSL

