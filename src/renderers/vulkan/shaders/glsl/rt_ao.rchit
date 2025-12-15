#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

precision highp float;
precision highp int;

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
    int pathTracing;
} ubo;

void main()
{
    // Calculate occlusion based on distance
    float hitDistance = gl_HitTEXT;
    aoFactor = calculateAO(hitDistance, AO_MAX_DISTANCE);
}

