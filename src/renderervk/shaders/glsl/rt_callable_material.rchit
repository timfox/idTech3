#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

precision highp float;
precision highp int;

#include "rt_defines.glsl"
#include "rt_helpers.glsl"

// Callable shader for material evaluation
// This allows dynamic material selection and evaluation

hitAttributeEXT vec2 attribs;

struct MaterialData {
    vec3 albedo;
    vec3 normal;
    float roughness;
    float metallic;
    vec3 emissive;
    float alpha;
};

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

// Callable function to evaluate material
void evaluateMaterial(out MaterialData matData)
{
    // Sample albedo
    matData.albedo = material.albedo.rgb;
    if (material.albedoTextureIndex >= 0) {
        vec4 texColor = texture(textures[material.albedoTextureIndex], attribs);
        matData.albedo *= texColor.rgb;
        matData.alpha = texColor.a;
    } else {
        matData.alpha = material.albedo.a;
    }
    
    // Sample roughness
    matData.roughness = material.roughness;
    if (material.roughnessTextureIndex >= 0) {
        matData.roughness *= texture(textures[material.roughnessTextureIndex], attribs).g;
    }
    
    // Sample metallic
    matData.metallic = material.metallic;
    if (material.metallicTextureIndex >= 0) {
        matData.metallic *= texture(textures[material.metallicTextureIndex], attribs).b;
    }
    
    // Sample normal
    vec3 worldNormal = normalize(gl_WorldRayDirectionEXT * gl_HitTEXT);
    matData.normal = worldNormal;
    if (material.normalTextureIndex >= 0) {
        vec3 tangentNormal = texture(textures[material.normalTextureIndex], attribs).xyz * 2.0 - 1.0;
        // TODO: Apply TBN transformation
        matData.normal = normalize(matData.normal + tangentNormal * material.normalScale);
    }
    
    // Sample emissive
    matData.emissive = material.emissive.rgb;
    if (material.emissiveTextureIndex >= 0) {
        matData.emissive *= texture(textures[material.emissiveTextureIndex], attribs).rgb;
    }
}

