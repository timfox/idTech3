#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

precision highp float;
precision highp int;

#include "rt_defines.glsl"

struct RefractionPayload {
    vec3 color;
    vec3 hitNormal;
    vec3 hitPosition;
    float hitDistance;
    float ior;
    uint depth;
};

layout(location = RT_REFRACTION_RAY_INDEX) rayPayloadInEXT RefractionPayload refractionPayload;

void main()
{
    // Ray missed - sample sky/environment
    vec3 rayDir = normalize(gl_WorldRayDirectionEXT);
    
    // Sample sky color
    vec3 skyColor = vec3(0.5, 0.7, 1.0);
    float sunAngle = dot(rayDir, vec3(0.0, 1.0, 0.0));
    float sunIntensity = max(0.0, sunAngle);
    skyColor = mix(skyColor, vec3(1.0, 0.9, 0.7), sunIntensity * 0.5);
    
    refractionPayload.color = skyColor;
    refractionPayload.hitNormal = vec3(0.0);
    refractionPayload.hitPosition = vec3(0.0);
    refractionPayload.hitDistance = MAX_DISTANCE;
    refractionPayload.depth = refractionPayload.depth;
}

