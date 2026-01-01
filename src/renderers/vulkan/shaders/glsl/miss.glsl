#version 460
#extension GL_EXT_ray_tracing : enable

#include "rt_defines.glsl"

// Ray payload
struct RayPayload {
    vec3 color;
    uint recursion_depth;
    vec3 normal;
    vec2 uv;
    uint material_id;
};

layout(location = 0) rayPayloadInEXT RayPayload payload;

vec3 sample_environment(vec3 dir) {
    // Simple sky gradient
    float t = 0.5 * (dir.y + 1.0);
    return mix(vec3(1.0, 1.0, 1.0), vec3(0.5, 0.7, 1.0), t) * 0.5;
}

void main() {
    // Environment sampling
    payload.color = sample_environment(gl_WorldRayDirectionEXT);
    payload.normal = vec3(0.0);
    payload.uv = vec2(0.0);
    payload.material_id = 0;
}