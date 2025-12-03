#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

precision highp float;
precision highp int;

#include "rt_defines.glsl"
#include "rt_helpers.glsl"

hitAttributeEXT vec2 attribs;

// Ray payload for reflections
struct ReflectionPayload {
    vec3 color;
    vec3 hitNormal;
    vec3 hitPosition;
    float hitDistance;
    uint depth;
};

layout(location = RT_REFLECTION_RAY_INDEX) rayPayloadInEXT ReflectionPayload reflectionPayload;

layout(binding = 3, set = 0) uniform sampler2D textures[];
layout(binding = 4, set = 0) uniform MaterialBuffer {
    vec4 albedo;
    vec4 emissive;
    float roughness;
    float metallic;
    float normalScale;
    int albedoTextureIndex;
    int normalTextureIndex;
    int roughnessTextureIndex;
    int metallicTextureIndex;
    int emissiveTextureIndex;
} material;

void main()
{
    // Get hit information
    vec3 worldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    vec3 worldNormal = normalize(gl_WorldRayDirectionEXT * gl_HitTEXT);
    
    // Sample material properties
    vec3 albedoColor = material.albedo.rgb;
    if (material.albedoTextureIndex >= 0) {
        albedoColor *= texture(textures[material.albedoTextureIndex], attribs).rgb;
    }
    
    float roughness = material.roughness;
    if (material.roughnessTextureIndex >= 0) {
        roughness *= texture(textures[material.roughnessTextureIndex], attribs).g;
    }
    
    float metallic = material.metallic;
    if (material.metallicTextureIndex >= 0) {
        metallic *= texture(textures[material.metallicTextureIndex], attribs).b;
    }
    
    // Get surface normal
    vec3 normal = worldNormal;
    if (material.normalTextureIndex >= 0) {
        vec3 tangentNormal = texture(textures[material.normalTextureIndex], attribs).xyz * 2.0 - 1.0;
        // TODO: Apply normal mapping with TBN matrix
        normal = normalize(normal + tangentNormal * material.normalScale);
    }
    
    // Calculate reflection direction
    vec3 viewDir = normalize(-gl_WorldRayDirectionEXT);
    vec3 reflectDir = reflect(-viewDir, normal);
    
    // Apply roughness-based blur
    if (roughness > 0.01) {
        // Sample multiple directions for rough reflections
        vec3 sampleDir = reflectDir;
        // TODO: Add importance sampling based on roughness
    }
    
    // Store hit information
    reflectionPayload.hitPosition = worldPos;
    reflectionPayload.hitNormal = normal;
    reflectionPayload.hitDistance = gl_HitTEXT;
    reflectionPayload.depth = reflectionPayload.depth + 1;
    
    // Calculate reflected color (simplified - would trace reflection ray)
    vec3 F0 = mix(vec3(0.04), albedoColor, metallic);
    vec3 F = fresnelSchlick(max(dot(normal, viewDir), 0.0), F0);
    
    // For now, return material color
    // In full implementation, would trace reflection ray recursively
    reflectionPayload.color = albedoColor * (1.0 - F) + F * vec3(1.0); // Placeholder
}

