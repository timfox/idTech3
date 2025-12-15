#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

precision highp float;
precision highp int;

#include "rt_defines.glsl"
#include "rt_helpers.glsl"

hitAttributeEXT vec2 attribs;

// Ray payload for refractions
struct RefractionPayload {
    vec3 color;
    vec3 hitNormal;
    vec3 hitPosition;
    float hitDistance;
    float ior; // Index of refraction
    uint depth;
};

layout(location = RT_REFRACTION_RAY_INDEX) rayPayloadInEXT RefractionPayload refractionPayload;

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
    
    // Get surface normal
    vec3 normal = worldNormal;
    if (material.normalTextureIndex >= 0) {
        vec3 tangentNormal = texture(textures[material.normalTextureIndex], attribs).xyz * 2.0 - 1.0;
        normal = normalize(normal + tangentNormal * material.normalScale);
    }
    
    // Calculate refraction direction using Snell's law
    vec3 viewDir = normalize(-gl_WorldRayDirectionEXT);
    float ior = refractionPayload.ior;
    float eta = 1.0 / ior; // Assuming entering material
    
    vec3 refractedDir = refract(-viewDir, normal, eta);
    
    // If total internal reflection, use reflection instead
    if (length(refractedDir) < 0.001) {
        refractedDir = reflect(-viewDir, normal);
    }
    
    // Store hit information
    refractionPayload.hitPosition = worldPos;
    refractionPayload.hitNormal = normal;
    refractionPayload.hitDistance = gl_HitTEXT;
    refractionPayload.depth = refractionPayload.depth + 1;
    
    // For now, return material color
    // In full implementation, would trace refraction ray recursively
    refractionPayload.color = albedoColor;
}

