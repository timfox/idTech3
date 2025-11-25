#version 460
#extension GL_EXT_ray_tracing : require

#include "rt_defines.glsl"

layout(location = 0) rayPayloadInEXT float aoFactor;

void main()
{
    // Ray hit nothing, so fully visible (no occlusion)
    aoFactor = 1.0;
}

