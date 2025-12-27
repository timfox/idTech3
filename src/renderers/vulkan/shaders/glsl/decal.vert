#version 460
#extension GL_GOOGLE_include_directive : enable

precision highp float;
precision highp int;

layout(push_constant) uniform DecalConstants {
    mat4 mvp_matrix;
    vec4 view_pos;
    float time;
} pc;

// Per-instance data (passed via vertex attributes for now)
// In a more advanced implementation, this would be in a uniform buffer
layout(location = 0) in vec4 position; // x, y, z, w (w=1.0 for position)

layout(location = 0) out vec3 world_pos;
layout(location = 1) out vec2 tex_coord;
layout(location = 2) out float alpha;

void main()
{
    // Transform position to world space
    vec3 local_pos = position.xyz;

    // Simple quad positioning - in a real implementation,
    // we'd transform by the decal's orientation matrix
    world_pos = local_pos;

    // Generate texture coordinates from position
    tex_coord = (local_pos.xy + vec2(0.5)) * vec2(1.0, -1.0) + vec2(0.0, 1.0);

    // For now, use full alpha
    alpha = 1.0;

    // Transform to clip space
    gl_Position = pc.mvp_matrix * vec4(world_pos, 1.0);
}