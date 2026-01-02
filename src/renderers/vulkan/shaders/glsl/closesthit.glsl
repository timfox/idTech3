#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

#include "rt_defines.glsl"

// Ray payload
struct RayPayload {
    vec3 color;
    vec3 normal;
    vec2 uv;
    uint material_id;
};

layout(location = 0) rayPayloadInEXT RayPayload payload;
struct ShadowPayload {
    float shadowFactor;
};

layout(location = 1) rayPayloadInEXT ShadowPayload shadow_payload;

hitAttributeEXT vec2 hit_attribs;

// Use existing material system - matches Vulkan renderer
struct MaterialData {
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
    float anisotropy;
    vec3 anisotropyDirection;
    float _pad2;
    vec3 subsurfaceColor;
    float subsurfaceScale;
    vec3 sheenColor;
    float sheenRoughness;

    // Procedural parameters
    vec4 customParams[4];

    // System flags
    uint flags;
};

layout(binding = 3, set = 0) buffer Materials { MaterialData materials[]; };

// Per-surface material indices
layout(binding = 7, set = 0) buffer SurfaceMaterialIndices { uint surfaceMaterialIndices[]; };

void main() {
    // Get material for this hit using per-surface material indices
    uint surface_index = gl_PrimitiveID;
    uint material_id = surfaceMaterialIndices[surface_index];
    MaterialData material = materials[material_id];

    // Interpolate normal and UV (simplified - would use actual vertex attributes)
    vec3 normal = normalize(gl_ObjectToWorldEXT * vec4(0.0, 1.0, 0.0, 0.0)).xyz;
    vec2 uv = hit_attribs;

    // Store hit information in payload using material system
    payload.color = material.baseColor.rgb;
    payload.normal = normal;
    payload.uv = uv;
    payload.material_id = material_id;
}

void shadow_hit() {
    shadow_payload.shadowFactor = 0.0; // Fully shadowed
}