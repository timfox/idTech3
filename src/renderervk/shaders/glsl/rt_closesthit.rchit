#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

precision highp float;
precision highp int;

#include "rt_defines.glsl"
#include "rt_helpers.glsl"

layout(location = 0) rayPayloadInEXT vec3 hitValue;
layout(location = 1) rayPayloadInEXT float hitDistance;
layout(location = 2) rayPayloadEXT float aoValue;
hitAttributeEXT vec2 attribs;

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
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

layout(binding = 5, set = 0) uniform AOBuffer {
    float aoRadius;
    float aoMaxDistance;
    float aoBias;
    float aoPower;
    int aoEnabled;
    int maoEnabled;
    int aoNumSamples;
    int maoNumBounces;
} aoSettings;

layout(binding = 2, set = 0) uniform UniformBuffer {
    mat4 viewInverse;
    mat4 projInverse;
    vec4 cameraPos;
    vec2 resolution;
    float time;
    float nearPlane;
    float farPlane;
    float exposure;
    int frameIndex;
    int samplesPerPixel;
} ubo;

struct VertexData {
    vec3 position;
    vec3 normal;
    vec3 tangent;
    vec2 texCoord;
};

// Forward declarations for AO functions
float calculateAmbientOcclusion(vec3 position, vec3 normal);
float calculateMultiBounceAO(vec3 position, vec3 normal, vec3 albedo);

void main()
{
    // Get hit information
    vec3 worldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    vec3 rayDir = normalize(gl_WorldRayDirectionEXT);
    
    // Reconstruct normal (simplified - in real implementation, fetch from vertex buffer)
    vec3 N = normalize(gl_ObjectRayOriginEXT + gl_ObjectRayDirectionEXT * gl_HitTEXT);
    
    // Get texture coordinates from hit attributes
    vec2 texCoord = attribs;
    
    // Sample textures
    vec3 albedo = material.albedo.rgb;
    if (material.albedoTextureIndex >= 0) {
        albedo *= texture(textures[material.albedoTextureIndex], texCoord).rgb;
    }
    
    float roughness = material.roughness;
    if (material.roughnessTextureIndex >= 0) {
        roughness *= texture(textures[material.roughnessTextureIndex], texCoord).r;
    }
    roughness = clamp(roughness, MIN_ROUGHNESS, MAX_ROUGHNESS);
    
    float metallic = material.metallic;
    if (material.metallicTextureIndex >= 0) {
        metallic *= texture(textures[material.metallicTextureIndex], texCoord).r;
    }
    metallic = clamp(metallic, MIN_METALLIC, MAX_METALLIC);
    
    vec3 emissive = material.emissive.rgb;
    if (material.emissiveTextureIndex >= 0) {
        emissive *= texture(textures[material.emissiveTextureIndex], texCoord).rgb;
    }
    
    // View direction
    vec3 V = normalize(-rayDir);
    
    // Simple lighting setup (will be enhanced with actual light sources)
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    vec3 lightColor = vec3(1.0, 0.95, 0.8) * 2.0;
    
    // Calculate PBR lighting
    vec3 Lo = calculatePBR(albedo, metallic, roughness, N, V, lightDir, lightColor);
    
    // Add ambient lighting (IBL approximation)
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 kS = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;
    
    vec3 ambient = kD * albedo * getSkyColor(N) * 0.1;
    
    // Calculate Ambient Occlusion
    float ao = 1.0;
    if (aoSettings.aoEnabled != 0 && aoSettings.maoEnabled == 0) {
        // Forward declaration - function defined below
        ao = calculateAmbientOcclusion(worldPos, N);
    } else if (aoSettings.maoEnabled != 0) {
        ao = calculateMultiBounceAO(worldPos, N, albedo);
    }
    
    // Apply AO to ambient lighting
    ambient *= ao;
    
    // Combine lighting
    hitValue = ambient + Lo + emissive;
    hitDistance = gl_HitTEXT;
    
    // Note: Reflection rays would be traced here in a more advanced implementation
    // For now, we use environment mapping approximation
    if (roughness < 0.5) {
        vec3 R = reflectRay(-V, N);
        vec3 reflectionColor = getSkyColor(R) * 0.3;
        float fresnel = fresnelSchlick(max(dot(N, V), 0.0), F0).r;
        hitValue = mix(hitValue, reflectionColor, fresnel * (1.0 - roughness) * 0.5);
    }
}

// Calculate Ambient Occlusion
float calculateAmbientOcclusion(vec3 position, vec3 normal) {
    float occlusion = 0.0;
    uint seed = uint(gl_LaunchIDEXT.x + gl_LaunchIDEXT.y * uint(gl_LaunchSizeEXT.x) + ubo.frameIndex);
    
    int numSamples = aoSettings.aoNumSamples > 0 ? aoSettings.aoNumSamples : AO_NUM_SAMPLES;
    float maxDist = aoSettings.aoMaxDistance > 0.0 ? aoSettings.aoMaxDistance : AO_MAX_DISTANCE;
    float radius = aoSettings.aoRadius > 0.0 ? aoSettings.aoRadius : AO_RADIUS;
    
    for (int i = 0; i < numSamples; i++) {
        vec3 sampleDir = getHemisphereSample(uint(i), normal, seed + uint(i));
        vec3 samplePos = position + normal * aoSettings.aoBias + sampleDir * radius;
        
        float aoFactor = 1.0;
        
        traceRayEXT(
            topLevelAS,
            gl_RayFlagsOpaqueEXT,
            0xFF,
            RT_AO_RAY_INDEX,
            0,
            RT_AO_RAY_INDEX,
            samplePos,
            0.0,
            -sampleDir,
            maxDist,
            2
        );
        
        aoFactor = aoValue;
        occlusion += 1.0 - aoFactor;
    }
    
    occlusion /= float(numSamples);
    return 1.0 - occlusion;
}

// Calculate Multi-Bounce Ambient Occlusion
float calculateMultiBounceAO(vec3 position, vec3 normal, vec3 albedo) {
    float ao = 1.0;
    uint seed = uint(gl_LaunchIDEXT.x + gl_LaunchIDEXT.y * uint(gl_LaunchSizeEXT.x) + ubo.frameIndex);
    
    int numBounces = aoSettings.maoNumBounces > 0 ? aoSettings.maoNumBounces : MAO_NUM_BOUNCES;
    int samplesPerBounce = MAO_NUM_SAMPLES_PER_BOUNCE;
    float maxDist = aoSettings.aoMaxDistance > 0.0 ? aoSettings.aoMaxDistance : AO_MAX_DISTANCE;
    float radius = aoSettings.aoRadius > 0.0 ? aoSettings.aoRadius : AO_RADIUS;
    
    vec3 currentPos = position;
    vec3 currentNormal = normal;
    float currentAO = 1.0;
    
    for (int bounce = 0; bounce < numBounces; bounce++) {
        float bounceOcclusion = 0.0;
        vec3 bounceAlbedo = albedo;
        
        for (int i = 0; i < samplesPerBounce; i++) {
            vec3 sampleDir = getMAOSample(currentPos, currentNormal, uint(bounce), uint(i), seed);
            vec3 samplePos = currentPos + currentNormal * aoSettings.aoBias + sampleDir * radius;
            
            aoValue = 1.0;
            
            traceRayEXT(
                topLevelAS,
                gl_RayFlagsOpaqueEXT,
                0xFF,
                RT_AO_RAY_INDEX,
                0,
                RT_AO_RAY_INDEX,
                samplePos,
                0.0,
                -sampleDir,
                maxDist,
                2
            );
            
            float aoFactor = aoValue;
            
            // For multi-bounce, consider the albedo at hit point
            // This is simplified - in full implementation, would sample material at hit
            float visibility = aoFactor;
            bounceOcclusion += (1.0 - visibility) * dot(bounceAlbedo, vec3(1.0)) / 3.0;
        }
        
        bounceOcclusion /= float(samplesPerBounce);
        currentAO *= (1.0 - bounceOcclusion);
        
        // Update position and normal for next bounce (simplified)
        // In full implementation, would use actual hit position and normal
        if (currentAO < 0.1) break; // Early exit if too occluded
    }
    
    return currentAO;
}
