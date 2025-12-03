// Material Parameter System
// Runtime material properties that can be modified by gameplay/scripts

#ifndef MATERIAL_PARAMS_GLSL
#define MATERIAL_PARAMS_GLSL

// Material parameter structure (matches CPU layout)
struct MaterialParams {
    float wetness;          // 0.0 = dry, 1.0 = fully wet
    float damage;          // 0.0 = pristine, 1.0 = destroyed
    float corruption;      // 0.0 = clean, 1.0 = corrupted
    float magicGlow;       // 0.0 = no glow, 1.0 = full glow
    float temperature;     // Temperature for thermal effects
    float time;            // Time-based animation parameter
    
    vec3 magicColor;       // Magical glow color
    vec3 damageColor;      // Damage tint color
    
    uint flags;            // Material flags
    uint stateHash;        // Hash of current state
};

// Material flags
#define MATERIAL_WET       0x01
#define MATERIAL_DAMAGED   0x02
#define MATERIAL_MAGICAL   0x04
#define MATERIAL_CORRUPTED 0x08
#define MATERIAL_EMISSIVE  0x10
#define MATERIAL_DYNAMIC   0x20

// Material parameter buffer
layout(std430, binding = 10) restrict readonly buffer MaterialParamsBuffer {
    MaterialParams materials[];
};

// Apply material parameters to PBR properties
vec3 applyMaterialParams(uint materialIndex, vec3 baseColor, float roughness, float metallic, out vec3 emissive)
{
    MaterialParams params = materials[materialIndex];
    emissive = vec3(0.0);
    
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
    float roughness = baseRoughness;
    
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
    float metallic = baseMetallic;
    
    // Corruption can make materials more metallic-looking
    if ((params.flags & MATERIAL_CORRUPTED) != 0) {
        metallic = mix(metallic, 0.8, params.corruption * 0.3);
    }
    
    return clamp(metallic, 0.0, 1.0);
}

#endif // MATERIAL_PARAMS_GLSL

