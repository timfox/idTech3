#version 450
#extension GL_GOOGLE_include_directive : enable

precision mediump int;
precision mediump float;

#include "shader_constants.glsl"

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 1) in vec4 frag_color;

layout(location = 0) out PRECISION_MEDIUMP vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D texture_sampler;

void main() {
    // Sample texture and multiply by vertex color
    PRECISION_MEDIUMP vec4 tex_color = texture(texture_sampler, frag_tex_coord);
    out_color = tex_color * frag_color;
}