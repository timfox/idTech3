// Ray tracing light sampling utilities
// Based on q3rtx light sampling patterns with cluster-based culling

#ifndef RT_LIGHT_SAMPLING_GLSL
#define RT_LIGHT_SAMPLING_GLSL

#include "rt_helpers.glsl"
#include "rt_random.glsl"

// Light structure matching q3rtx pattern
struct DirectionalLight {
    vec3 pos;
    vec3 normal;
    vec3 color;
    float mag;
    vec3 AB;  // Edge vectors for area light sampling
    vec3 AC;
};

struct Light {
    vec4 pos;      // xyz = position, w = type
    vec4 normal;   // xyz = normal, w = size
    vec4 color;    // xyz = color, w = intensity
    vec4 AB;       // xyz = edge vector AB, w = unused
    vec4 AC;       // xyz = edge vector AC, w = unused
    uint cluster;  // BSP cluster ID for visibility culling
};

// Light list buffer (matches q3rtx structure)
layout(binding = 0, set = 0) buffer LightList {
    uint numLights;
    Light lights[];
} uboLights;

// Visibility data (cluster-based light culling)
layout(binding = 1, set = 0, r8ui) uniform uimage2D vis_data;        // PVS visibility data
layout(binding = 2, set = 0, r32ui) uniform uimage2D lightVis_data; // Light visibility per cluster
layout(binding = 3, set = 0, r32ui) uniform uimage2D lightVis_data2; // Alternative light visibility

// Check if light is visible from cluster using PVS
bool lightVisible(int lightIndex, uint hitCluster) {
    uint lightCluster = uboLights.lights[lightIndex].cluster;
    if (lightCluster == 0xFFFFFFFF) {
        return true; // No cluster assigned, assume visible
    }
    
    // Check PVS bit
    uint clusterByte = lightCluster >> 3;
    uint clusterBit = lightCluster & 7u;
    uint visByte = imageLoad(vis_data, ivec2(clusterByte, hitCluster)).r;
    return (visByte & (1u << clusterBit)) != 0u;
}

// Sample triangle for area light (uniform triangle sampling)
vec3 sample_triangle(vec2 xi) {
    float sqrt_xi = sqrt(xi.x);
    return vec3(
        1.0 - sqrt_xi,
        sqrt_xi * (1.0 - xi.y),
        sqrt_xi * xi.y
    );
}

// Get directional light with optional random sampling
DirectionalLight getLight2(in Light l, ivec2 rng, bool random) {
    float rng_x = 0.5;
    float rng_y = 0.5;
    
    if (random) {
        // Use blue noise or hash-based RNG
        #ifdef USE_BLUE_NOISE
        rng_x = rt_getRNG(false, RNG_LP_X(rng.x), uvec2(rng), 0u);
        rng_y = rt_getRNG(false, RNG_LP_Y(rng.x), uvec2(rng), 0u);
        #else
        rng_x = random(hash(rng.x + rng.y * 1000u));
        rng_y = random(hash(rng.x + rng.y * 1000u + 1u));
        #endif
    }
    
    DirectionalLight light;
    light.normal = l.normal.xyz;
    light.mag = l.normal.w;
    light.pos = l.pos.xyz + (rng_x * l.AB.xyz + rng_y * l.AC.xyz);
    light.color = l.color.xyz;
    light.AB = l.AB.xyz;
    light.AC = l.AC.xyz;
    
    return light;
}

// Calculate shading with cluster-based light culling
// Based on q3rtx calcShading function
vec3 calcShading(
    in vec4 primary_albedo,
    in vec3 P,
    in vec3 N,
    in uint cluster,
    in uint material,
    in int cullLights,      // 0=none, 1=lightVis_data, 2=lightVis_data2
    in int numRandomDL,    // Number of random lights to sample (0 = all)
    in bool randSampleLight, // Whether to randomly sample light positions
    in uint frameIndex
) {
    vec3 shadeColor = vec3(0.0);
    
    float amplification = 1.0;
    uint numLights;
    
    // Get number of visible lights for this cluster
    if (cullLights == 1) {
        numLights = imageLoad(lightVis_data, ivec2(0, cluster)).r;
    } else if (cullLights == 2) {
        numLights = imageLoad(lightVis_data2, ivec2(0, cluster)).r;
    } else {
        numLights = uboLights.numLights;
    }
    
    uint totalNumLights = numLights;
    
    // Limit number of lights to sample
    if (numRandomDL > 0) {
        numLights = min(numLights, uint(numRandomDL));
        amplification = float(totalNumLights) / float(numLights);
    }
    
    // Sample lights
    for (int i = 0; i < int(numLights); i++) {
        uint lightIndex;
        
        // Get light index (either from visibility buffer or random)
        if (cullLights > 0) {
            if (numRandomDL > 0) {
                // Random light selection
                float rand = 0.0;
                #ifdef USE_BLUE_NOISE
                rand = rt_getRNG(false, RNG_NEE_LH(i), uvec2(i, frameIndex), frameIndex);
                #else
                rand = random(hash(uint(i) + frameIndex * 1000u));
                #endif
                uint randIdx = uint(round(rand * float(totalNumLights)));
                
                if (cullLights == 1) {
                    lightIndex = imageLoad(lightVis_data, ivec2(randIdx + 1, cluster)).r;
                } else {
                    lightIndex = imageLoad(lightVis_data2, ivec2(randIdx + 1, cluster)).r;
                }
            } else {
                // Sequential light selection
                if (cullLights == 1) {
                    lightIndex = imageLoad(lightVis_data, ivec2(i + 1, cluster)).r;
                } else {
                    lightIndex = imageLoad(lightVis_data2, ivec2(i + 1, cluster)).r;
                }
            }
        } else {
            // No culling - use all lights
            if (numRandomDL > 0) {
                float rand = 0.0;
                #ifdef USE_BLUE_NOISE
                rand = rt_getRNG(false, RNG_NEE_LH(i), uvec2(i, frameIndex), frameIndex);
                #else
                rand = random(hash(uint(i) + frameIndex * 1000u));
                #endif
                lightIndex = uint(round(rand * float(totalNumLights)));
            } else {
                lightIndex = uint(i);
            }
        }
        
        // Get light with optional random position sampling
        DirectionalLight light = getLight2(
            uboLights.lights[lightIndex],
            ivec2(i, int(frameIndex)),
            randSampleLight
        );
        
        vec3 posLight = light.pos;
        vec3 toLight = posLight - P;
        vec3 L = normalize(toLight);
        float distToLight = length(toLight);
        
        // Trace shadow ray (this would call trace_shadow_ray in actual implementation)
        // For now, assume fully lit
        float shadowMult = 1.0;
        // shadowMult = trace_shadow_ray(P, L, 0.01, distToLight, isPlayer(material));
        
        if (shadowMult == 0.0) {
            continue;
        }
        
        shadowMult *= amplification; // Compensate for reduced light count
        
        float lightStrength = min(light.mag / distToLight, 5.0);
        vec3 lightIntensity = light.color * lightStrength;
        
        float LdotN = clamp(dot(normalize(N), L), 0.0, 1.0);
        shadeColor += shadowMult * LdotN * lightIntensity;
    }
    
    return shadeColor / M_PI;
}

#endif // RT_LIGHT_SAMPLING_GLSL
