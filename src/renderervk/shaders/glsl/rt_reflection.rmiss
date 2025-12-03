#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

precision highp float;
precision highp int;

#include "rt_defines.glsl"

struct ReflectionPayload {
    vec3 color;
    vec3 hitNormal;
    vec3 hitPosition;
    float hitDistance;
    uint depth;
};

layout(location = RT_REFLECTION_RAY_INDEX) rayPayloadInEXT ReflectionPayload reflectionPayload;

void main()
{
    // Ray missed - sample sky/environment
    vec3 rayDir = normalize(gl_WorldRayDirectionEXT);
    
    // Sample sky color (simplified - would use environment map)
    vec3 skyColor = vec3(0.5, 0.7, 1.0); // Simple sky gradient
    
    // Apply atmospheric scattering approximation
    float sunAngle = dot(rayDir, vec3(0.0, 1.0, 0.0));
    float sunIntensity = max(0.0, sunAngle);
    skyColor = mix(skyColor, vec3(1.0, 0.9, 0.7), sunIntensity * 0.5);
    
    reflectionPayload.color = skyColor;
    reflectionPayload.hitNormal = vec3(0.0);
    reflectionPayload.hitPosition = vec3(0.0);
    reflectionPayload.hitDistance = MAX_DISTANCE;
    reflectionPayload.depth = reflectionPayload.depth;
}

