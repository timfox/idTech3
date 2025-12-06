#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

precision highp float;
precision highp int;

#include "rt_defines.glsl"
#include "rt_helpers.glsl"

layout(location = 0) rayPayloadInEXT vec3 hitValue;
layout(location = 1) rayPayloadInEXT float hitDistance;

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
    int pathTracing;
} ubo;

void main()
{
    vec3 rayDir = normalize(gl_WorldRayDirectionEXT);
    
    // Get sky color based on ray direction
    hitValue = getSkyColor(rayDir);
    
    // Add sun disk if looking towards sun
    // Optimized: use constant instead of magic numbers
    vec3 sunDir = normalize(vec3(0.3, 0.8, 0.5));
    float sunAngle = dot(rayDir, sunDir);
    if (sunAngle > SUN_DISK_THRESHOLD) {
        // Optimized: use manual pow approximation for better performance
        float sunIntensity = pow(sunAngle, SUN_DISK_POWER) * SUN_INTENSITY_MULTIPLIER;
        hitValue += vec3(1.0, 0.95, 0.8) * sunIntensity;
    }
    
    // Set hit distance to maximum
    hitDistance = MAX_DISTANCE;
}
