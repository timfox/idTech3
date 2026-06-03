#version 450

layout(set = 0, binding = 0) uniform sampler2D litTex;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

void main() {
	out_color = vec4( texture( litTex, frag_tex_coord ).rgb, 1.0 );
}
