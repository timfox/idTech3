#version 460
#extension GL_GOOGLE_include_directive : enable

precision highp float;
precision highp int;

layout(push_constant) uniform DecalConstants {
    mat4 mvp_matrix;
    vec4 view_pos;
    float time;
} pc;

layout(set = 0, binding = 0) uniform sampler2D decal_texture;

layout(location = 0) in vec3 world_pos;
layout(location = 1) in vec2 tex_coord;
layout(location = 2) in float alpha;

layout(location = 0) out vec4 out_color;

void main()
{
    // Sample decal texture
    vec4 tex_color = texture(decal_texture, tex_coord);

    // Apply alpha
    tex_color.a *= alpha;

    // Simple circular fade-out from edges
    float dist = length(tex_coord - vec2(0.5));
    float fade = 1.0 - smoothstep(0.4, 0.5, dist);
    tex_color.a *= fade;

    // Discard fully transparent pixels
    if (tex_color.a < 0.001) {
        discard;
    }

    out_color = tex_color;
}