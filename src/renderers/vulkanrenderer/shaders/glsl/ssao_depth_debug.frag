#version 450

layout(set = 0, binding = 0) uniform sampler2D depthTex;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

void main()
{
	float d = texture(depthTex, frag_tex_coord).r;
	d = 1.0 - d; // show reversed-depth clearly too
	out_color = vec4(d, d, d, 1.0);
}
