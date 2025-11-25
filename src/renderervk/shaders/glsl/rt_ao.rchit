#version 460
#extension GL_EXT_ray_tracing : require

#include "rt_defines.glsl"
#include "rt_helpers.glsl"

layout(location = 0) rayPayloadInEXT float aoFactor;

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
    // Calculate occlusion based on distance
    float hitDistance = gl_HitTEXT;
    aoFactor = calculateAO(hitDistance, AO_MAX_DISTANCE);
}

