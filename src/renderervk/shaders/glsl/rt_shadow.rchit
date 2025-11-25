#version 460
#extension GL_EXT_ray_tracing : require

#include "rt_defines.glsl"

layout(location = 0) rayPayloadInEXT float shadowFactor;

void main()
{
    // Ray hit something, so in shadow
    shadowFactor = 0.0;
}

