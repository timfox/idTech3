#version 460
#extension GL_EXT_ray_tracing : require

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
} ubo;

void main()
{
    vec3 rayDir = normalize(gl_WorldRayDirectionEXT);
    
    // Get sky color based on ray direction
    hitValue = getSkyColor(rayDir);
    
    // Add sun disk if looking towards sun
    vec3 sunDir = normalize(vec3(0.3, 0.8, 0.5));
    float sunAngle = dot(rayDir, sunDir);
    if (sunAngle > 0.99) {
        float sunIntensity = pow(sunAngle, 256.0) * 5.0;
        hitValue += vec3(1.0, 0.95, 0.8) * sunIntensity;
    }
    
    // Set hit distance to maximum
    hitDistance = MAX_DISTANCE;
}
