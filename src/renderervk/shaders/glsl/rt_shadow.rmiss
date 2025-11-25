#version 460
#extension GL_EXT_ray_tracing : require

#include "rt_defines.glsl"

layout(location = 0) rayPayloadInEXT float shadowFactor;

void main()
{
    // Ray hit nothing, so fully lit
    shadowFactor = 1.0;
}

