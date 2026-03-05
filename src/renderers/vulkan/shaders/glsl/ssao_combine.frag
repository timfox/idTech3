#version 450

layout(set = 0, binding = 0) uniform sampler2D sceneTex;
layout(set = 1, binding = 0) uniform sampler2D ssaoTex;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

void main()
{
	vec3 scene = texture(sceneTex, frag_tex_coord).rgb;
	float ao = texture(ssaoTex, frag_tex_coord).r;
	out_color = vec4(scene * ao, 1.0);
}
